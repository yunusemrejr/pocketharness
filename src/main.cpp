// PocketHarness - entry point: CLI, privilege checks, workspace setup, modes.
#include <ftw.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include <atomic>
#include <algorithm>
#include <cstdio>

#include "agent.h"
#include "brain.h"
#include "catalog.h"
#include "kit.h"
#include "oversee.h"
#include "skills.h"
#include "common.h"
#include "config.h"
#include "process.h"
#include "provider.h"
#include "sandbox.h"
#include "session.h"
#include "tui.h"

namespace pocket {
namespace {

std::atomic<bool> printCancel{false};
static_assert(std::atomic<bool>::is_always_lock_free);
void cancelPrint(int) { printCancel.store(true); }
// Any fatal exit (terminal closed, kill) takes this session's processes and,
// when no other session uses it, the local model down with it.
void onExitSignal(int sig) {
    killSessionProcesses(300);
    judgeShutdownOnSignal();
    signal(sig, SIG_DFL);
    raise(sig);
}

int positiveOption(const std::string& text, int max) {
    if (text.empty() || text.size() > 9 || text.find_first_not_of("0123456789") != std::string::npos) return 0;
    long n = std::stol(text);
    return n > 0 && n <= max ? (int)n : 0;
}

void usage() {
    printf(
        "pocket %s — tiny Linux-native coding-agent harness\n"
        "\n"
        "usage:\n"
        "  pocket [path]                 interactive agent in workspace (default: .)\n"
        "  pocket -p \"prompt\"            non-interactive single turn (composable)\n"
        "  pocket --resume [id]          resume newest (or given) session\n"
        "  pocket --sessions             list sessions\n"
        "  pocket --models [query]       search the live model catalog\n"
        "  pocket kit                    native superpowers (web, search, browser, media, code index)\n"
        "\n"
        "options:\n"
        "  -m, --model SPEC     provider:model[@routing] or alias (default: config)\n"
        "  -t, --thinking LVL   adaptive|auto|off|none|minimal|low|medium|high|xhigh|max\n"
        "  -p, --print PROMPT   non-interactive prompt (stdout = final answer)\n"
        "  -g, --goal GOAL      non-interactive goal: work + audit until verifiably met\n"
        "  --refresh-catalog    refetch every keyed provider's model list now\n"
        "  --image PATH         attach an image (PNG/JPEG/GIF/WebP, max 5 MiB, repeatable)\n"
        "  --resume [id]        resume a session (interactive unless -p)\n"
        "  --sessions           list sessions and exit\n"
        "  --network            allow network access for tools (on by default)\n"
        "  --no-network, --offline  deny network access for tools\n"
        "  --allow-read PATH    extra read root for native tools (repeatable)\n"
        "  --allow-write PATH   extra write root for native tools (repeatable)\n"
        "  --allow-destructive  -p mode: permit guard-flagged commands (explicit)\n"
        "  --max-rounds N       cap model tool rounds per turn (default 100)\n"
        "  --max-tokens N       completion budget (default: model config, fits context)\n"
        "  --unsafe             disable containment (conspicuous, never persisted)\n"
        "  --allow-root         permit agent execution as UID 0 (dangerous)\n"
        "  --help               this text\n"
        "  --version            version\n"
        "\n"
        "Recursive use (subagents via Linux processes):\n"
        "  pocket -p \"review auth\" > /tmp/a & pocket -p \"review io\" > /tmp/b & wait\n",
        kVersion);
}

// Parent state for a recursive `pocket`: depth, parent workspace and the net
// grant. Read from $TMPDIR/pocket.parent (written by the parent into its
// own 0700 sessionTmp), NEVER from env: env is model-visible and untrusted.
// The file must be an euid-owned regular file, else it is ignored outright.
struct ParentState {
    int depth = 0;
    std::string workspace;
    bool net = false;
    std::string coordination;
};
ParentState readParentState() {
    ParentState ps;
    const char* td = getenv("TMPDIR");
    if (!td || !*td) return ps;
    std::string p = std::string(td) + "/pocket.parent";
    struct stat st;
    if (lstat(p.c_str(), &st) != 0) return ps;
    if (!S_ISREG(st.st_mode) || st.st_uid != geteuid()) return ps;
    auto t = readFileBounded(p, 16384);
    if (!t.ok) return ps;
    auto data = json::parse(t.value);
    if (data.ok && data.value.isObj()) {
        ps.depth = (int)data.value.at("depth").asInt();
        ps.workspace = data.value.at("workspace").asStr();
        ps.net = data.value.at("net").asBool();
        ps.coordination = data.value.at("coordination").asStr();
        return ps;
    }
    for (const std::string& ln : splitLines(t.value)) {
        if (startsWith(ln, "depth=")) ps.depth = atoi(ln.c_str() + 6);
        else if (startsWith(ln, "workspace=")) ps.workspace = ln.substr(10);
        else if (ln == "net=1") ps.net = true;
    }
    return ps;
}

bool startsWithDash(const std::string& s) { return !s.empty() && s[0] == '-'; }

int removeTmp(const char* path, const struct stat*, int type, struct FTW*) {
    (void)type;
    if (remove(path) != 0 && errno != ENOENT) return -1;
    return 0;
}

// Called before model tools can access this new private scratch directory.
// Stream the running inode: installed binaries can be replaced while a session
// is open, and instrumented/debug builds can be much larger than release builds.
VoidResult stageExecutable(const std::string& path) {
    int source = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
    if (source < 0) return VoidResult::Err("cannot open running executable");
    int target = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0700);
    if (target < 0) { close(source); return VoidResult::Err("cannot create staged executable"); }
    bool copied = fchmod(target, 0700) == 0;
    while (copied) {
        ssize_t n = sendfile(target, source, nullptr, 1u << 20);
        if (n > 0 || (n < 0 && errno == EINTR)) continue;
        copied = n == 0;
        break;
    }
    close(source);
    if (close(target) != 0) copied = false;
    if (!copied) {
        unlink(path.c_str());
        return VoidResult::Err("cannot copy running executable into sandbox");
    }
    return VoidResult::Ok();
}

}  // namespace

int pocketMain(int argc, char** argv);

}  // namespace pocket

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    // Keep terminal Unicode behavior, but JSON/CSS and tool numeric arguments
    // always use a decimal point, regardless of the desktop's locale.
    setlocale(LC_NUMERIC, "C");
    if (argc > 1 && std::string(argv[1]) == "kit") return pocket::kitMain(argc - 1, argv + 1);
    return pocket::pocketMain(argc, argv);
}

namespace pocket {

int pocketMain(int argc, char** argv) {
    std::string positional;
    std::string prompt;
    std::string modelSpec;
    std::string thinkingCli;
    std::string resumeId;
    bool resume = false, listSessions = false, listModels = false, refreshCatalog = false;
    std::string modelQuery, goalText;
    bool optNetwork = false, optUnsafe = false, optAllowRoot = false, optNoNetwork = false;
    bool optAllowDestructive = false;
    bool optHelp = false, optVersion = false;
    bool printRequested = false, goalRequested = false, endOptions = false;
    int optMaxRounds = 0, optMaxTokens = 0;  // 0 = model/config defaults
    std::vector<std::string> allowRead, allowWrite, optImages;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto needVal = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                fprintf(stderr, "pocket: %s needs a value\n", flag);
                exit(2);
            }
            return argv[++i];
        };
        if (endOptions) {
            if (!positional.empty()) { fprintf(stderr, "pocket: unexpected workspace argument\n"); return 2; }
            positional = a;
        }
        else if (a == "--") endOptions = true;
        else if (a == "--help" || a == "-h") optHelp = true;
        else if (a == "--version" || a == "-v") optVersion = true;
        else if (a == "-p" || a == "--print") { printRequested = true; prompt = needVal("-p"); }
        else if (a == "-g" || a == "--goal") { goalRequested = true; prompt = goalText = needVal("--goal"); }
        else if (a == "--refresh-catalog") refreshCatalog = true;
        else if (a == "--models") {
            listModels = true;
            if (i + 1 < argc && !startsWithDash(argv[i + 1])) modelQuery = argv[++i];
        }
        else if (a == "-m" || a == "--model") modelSpec = needVal("-m");
        else if (a == "-t" || a == "--thinking") thinkingCli = needVal("-t");
        else if (a == "--resume") {
            resume = true;
            if (i + 1 < argc && !startsWithDash(argv[i + 1])) resumeId = argv[++i];
        } else if (a == "--sessions") listSessions = true;
        else if (a == "--network") optNetwork = true;
        else if (a == "--no-network" || a == "--offline") optNoNetwork = true;
        else if (a == "--unsafe") optUnsafe = true;
        else if (a == "--allow-root") optAllowRoot = true;
        else if (a == "--allow-destructive") optAllowDestructive = true;
        else if (a == "--max-rounds") {
            optMaxRounds = positiveOption(needVal("--max-rounds"), 1000);
            if (optMaxRounds < 1 || optMaxRounds > 1000) {
                fprintf(stderr, "pocket: --max-rounds needs 1..1000\n");
                return 2;
            }
        } else if (a == "--max-tokens") {
            optMaxTokens = positiveOption(needVal("--max-tokens"), 1048576);
            if (!optMaxTokens) { fprintf(stderr, "pocket: --max-tokens needs 1..1048576\n"); return 2; }
        } else if (a == "--allow-read") allowRead.push_back(needVal("--allow-read"));
        else if (a == "--allow-write") allowWrite.push_back(needVal("--allow-write"));
        else if (a == "--image") optImages.push_back(needVal("--image"));
        else if (startsWithDash(a)) {
            fprintf(stderr, "pocket: unknown flag %s (see --help)\n", a.c_str());
            return 2;
        } else if (positional.empty()) {
            positional = a;
        } else {
            fprintf(stderr, "pocket: unexpected argument %s\n", a.c_str());
            return 2;
        }
    }

    if (optHelp) {
        usage();
        return 0;
    }
    if (optVersion) {
        printf("pocket %s\n", kVersion);
        return 0;
    }
    if ((printRequested && goalRequested) || ((printRequested || goalRequested) && trim(prompt).empty())) {
        fprintf(stderr, "pocket: use either --print or --goal with a nonempty prompt\n");
        return 2;
    }

    // --- privilege boundary: refuse root agent execution by default ---
    if (geteuid() == 0 && !optAllowRoot) {
        fprintf(stderr,
                "PocketHarness refuses agent execution as root.\n"
                "Use --allow-root only if you explicitly accept the risk.\n");
        return 1;
    }

    // --- recursion guards (defense in depth; kernel confinement inherits) ---
    ParentState ps = readParentState();
    int depth = ps.depth;
    if (depth < 0) depth = 0;
    if (depth > kMaxPocketDepth) {
        fprintf(stderr, "pocket: nesting too deep (depth=%d, max %d)\n", depth, kMaxPocketDepth);
        return 1;
    }
    std::string parentWs = ps.workspace;
    bool parentNet = ps.net;
    if (depth > 0) {
        if (optUnsafe) {
            fprintf(stderr, "pocket: --unsafe refused in a child instance (depth %d)\n", depth);
            return 1;
        }
        if (optNetwork && !parentNet) {
            fprintf(stderr, "pocket: --network refused: parent did not grant tool networking\n");
            return 1;
        }
    }

    // --- workspace ---
    std::string wsArg = positional.empty() ? "." : positional;
    char wsReal[PATH_MAX];
    if (!realpath(wsArg.c_str(), wsReal)) {
        fprintf(stderr, "pocket: cannot resolve workspace %s\n", wsArg.c_str());
        return 1;
    }
    struct stat wsst;
    if (stat(wsReal, &wsst) != 0 || !S_ISDIR(wsst.st_mode)) {
        fprintf(stderr, "pocket: not a directory: %s\n", wsArg.c_str());
        return 1;
    }
    std::string workspace = wsReal;
    if (!parentWs.empty() && depth > 0) {
        if (workspace != parentWs && !startsWith(workspace, parentWs + "/")) {
            fprintf(stderr, "pocket: child workspace %s is outside parent %s; refused\n",
                    workspace.c_str(), parentWs.c_str());
            return 1;
        }
    }

    // Provider keys from the user env file (never overriding the shell).
    loadEnvFile(userEnvPath());

    // --- config (CLI grants authority; project config cannot escalate) ---
    auto cfgR = loadConfig(workspace);
    if (!cfgR.ok) {
        fprintf(stderr, "pocket: %s\n", cfgR.error.c_str());
        return 1;
    }
    Config cfg = cfgR.value;
    bool allowNet = (cfg.toolNetwork || optNetwork) && !optNoNetwork;
    // A child never out-networks its parent: under an --offline parent the
    // default-on tool network stays off, whatever the child config says.
    if (depth > 0 && !parentNet) allowNet = false;
    for (const auto& p : allowRead) cfg.allowRead.push_back(expandHome(p));
    for (const auto& p : allowWrite) cfg.allowWrite.push_back(expandHome(p));
    if (depth > 0 && !parentWs.empty()) {
        for (const auto& r : cfg.allowRead) {
            char buf[PATH_MAX];
            std::string canon = realpath(r.c_str(), buf) ? buf : r;
            if (canon != parentWs && !startsWith(canon, parentWs + "/")) {
                fprintf(stderr, "pocket: child --allow-read outside parent workspace; refused\n");
                return 1;
            }
        }
        for (const auto& r : cfg.allowWrite) {
            char buf[PATH_MAX];
            std::string canon = realpath(r.c_str(), buf) ? buf : r;
            if (canon != parentWs && !startsWith(canon, parentWs + "/")) {
                fprintf(stderr, "pocket: child --allow-write outside parent workspace; refused\n");
                return 1;
            }
        }
    }

    std::string thinkingFlag;
    if (!thinkingCli.empty()) {
        thinkingFlag = toLower(thinkingCli);
        if (!validThinking(thinkingFlag)) {
            fprintf(stderr, "pocket: bad --thinking (adaptive|auto|off|none|minimal|low|medium|high|xhigh|max)\n");
            return 2;
        }
    }

    if (refreshCatalog || listModels) {
        if (refreshCatalog) fprintf(stderr, "%s\n", catalogRefresh(cfg).c_str());
        if (!listModels) return 0;
        auto all = catalogLoad(cfg);
        if (all.empty()) fprintf(stderr, "catalog empty: run pocket --refresh-catalog\n");
        std::vector<std::string> labels;
        for (const auto& m : all) labels.push_back(catalogLabel(m));
        for (size_t i : fuzzyRank(labels, modelQuery)) printf("%s\n", labels[i].c_str());
        return 0;
    }

    if (listSessions) {
        auto list = sessionList();
        if (list.empty()) {
            printf("no sessions yet\n");
            return 0;
        }
        for (const auto& s : list)
            printf("%s  %s  %s  %s\n", s.id.c_str(), s.active ? "active" : "idle",
                   sanitizeTerminal(s.workspace).c_str(), sanitizeTerminal(s.firstLine).c_str());
        return 0;
    }

    const Config baseConfig = cfg;
    struct PendingSession {
        int fd = -1;
        std::string id;
        ~PendingSession() { if (fd >= 0) close(fd); }
    } pendingSession;
    // A TUI handoff leaves the old terminal, agent, scratch and lease before
    // entering the same startup/restore path used by --resume.
    for (;;) {
    cfg = baseConfig;
    // --- session first: resume restores its own model/thinking, so
    // concurrent sessions never observe each other (no shared UI state). ---
    std::string sessionId;
    if (resume) {
        auto s = sessionResolve(resumeId, workspace);
        if (!s.ok) {
            fprintf(stderr, "pocket: %s\n", s.error.c_str());
            return 1;
        }
        sessionId = s.value;
    } else {
        auto s = sessionCreate();
        if (!s.ok) {
            fprintf(stderr, "pocket: %s\n", s.error.c_str());
            return 1;
        }
        sessionId = s.value;
    }
    auto lease = pendingSession.fd >= 0 && pendingSession.id == sessionId ?
        Result<int>::Ok(pendingSession.fd) : sessionLock(sessionId);
    if (!lease.ok) { fprintf(stderr, "pocket: %s\n", lease.error.c_str()); return 1; }
    if (lease.value == pendingSession.fd) { pendingSession.fd = -1; pendingSession.id.clear(); }
    struct Lease { int fd; ~Lease() { close(fd); } } sessionLease{lease.value};
    auto loadedMeta = sessionLoadMeta(sessionId);
    if (!loadedMeta.ok) { fprintf(stderr, "pocket: %s\n", loadedMeta.error.c_str()); return 1; }
    SessionMeta sm = loadedMeta.value;
    if (sm.rolesSet) cfg.roles = sm.roles;
    // Freeze actual model identities, not aliases whose definitions may change
    // in another session's user/project config before this one resumes.
    for (auto& [name, selections] : cfg.roles) {
        (void)name;
        std::string list = selections;
        std::replace(list.begin(), list.end(), ',', '\n');
        std::vector<std::string> specs;
        for (const auto& entry : splitLines(list)) {
            if (trim(entry).empty()) continue;
            auto resolved = resolveModel(cfg, trim(entry));
            specs.push_back(resolved.ok ? resolved.value.spec : trim(entry));
        }
        selections = join(specs, ",");
    }
    sm.roles = cfg.roles;
    sm.rolesSet = true;
    if (!sm.workspace.empty() && sm.workspace != workspace) {
        fprintf(stderr, "pocket: session belongs to workspace %s; run pocket there to resume\n",
                sanitizeTerminal(sm.workspace).c_str());
        return 1;
    }
    sm.workspace = workspace;
    if (auto saved = sessionSaveMeta(sessionId, sm); !saved.ok) {
        fprintf(stderr, "pocket: %s\n", saved.error.c_str()); return 1;
    }

    // --- model + thinking: explicit CLI wins, then this session, then config ---
    std::string wantModel = !modelSpec.empty() ? modelSpec : sm.modelSpec;
    auto rmR = resolveModel(cfg, wantModel);
    if (!rmR.ok && modelSpec.empty() && !wantModel.empty() && wantModel != cfg.defaultModel) {
        fprintf(stderr, "pocket: %s; falling back to default\n", rmR.error.c_str());
        rmR = resolveModel(cfg, "");
    }
    if (!rmR.ok) {
        fprintf(stderr, "pocket: %s\n", rmR.error.c_str());
        return 1;
    }
    ResolvedModel model = rmR.value;
    std::string thinking = !thinkingFlag.empty() ? thinkingFlag : sm.thinking;
    if (!validThinking(thinking)) thinking = cfg.thinking;
    cfg.defaultModel = model.spec;
    cfg.thinking = thinking;

    if (!curlAvailable()) {
        fprintf(stderr, "pocket: the `curl` executable is required but not runnable\n");
        return 1;
    }

    // --- authority ---
    if (depth > 0 && !ps.coordination.empty()) {
        auto shared = sessionSetWorkspaceCoordinationDir(ps.coordination);
        if (!shared.ok) { fprintf(stderr, "pocket: %s\n", shared.error.c_str()); return 1; }
    }
    auto coordinationReady = sessionWorkspaceDirectory(workspace);
    if (!coordinationReady.ok) { fprintf(stderr, "pocket: %s\n", coordinationReady.error.c_str()); return 1; }
    std::string coordination = coordinationReady.value;
    std::vector<std::string> readRoots = cfg.allowRead, writeRoots = cfg.allowWrite;
    // Shell startup files are intentionally not sourced. Keep selected Node
    // toolchains usable without granting access to the rest of the real HOME.
    // passwd (rather than the fake child HOME) also works for recursive Pocket.
    const passwd* account = getpwuid(geteuid());
    for (const auto& runtime : discoverRuntimeRoots(getenv("PATH"), account ? account->pw_dir : homeDir()))
        readRoots.push_back(runtime);
    // Only bounded informational notices are shared; transcripts and credentials
    // stay in the private state directory outside this narrow grant.
    readRoots.push_back(coordination);
    writeRoots.push_back(coordination);
    auto authR = authorityInit(workspace, readRoots, writeRoots, optUnsafe);
    if (!authR.ok) {
        fprintf(stderr, "pocket: %s\n", authR.error.c_str());
        return 1;
    }
    Authority auth = authR.value;

    // --- session scratch: TMPDIR, fake HOME, parent-state file ---
    // No key material is ever staged here: this dir is child-visible, so it
    // may only hold non-secret staging. Provider header files live in a
    // per-request parent-only dir under the state dir (see provStaging).
    std::string tmpBase = getenv("TMPDIR") && *getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
    std::string tmpTpl = tmpBase + "/pocket-" + sessionId + "-XXXXXX";
    std::vector<char> tpl(tmpTpl.begin(), tmpTpl.end());
    tpl.push_back('\0');
    if (!mkdtemp(tpl.data())) {
        fprintf(stderr, "pocket: cannot create session temp dir\n");
        return 1;
    }
    chmod(tpl.data(), 0700);
    std::string sessionTmp = tpl.data();
    struct Scratch { std::string path; ~Scratch() { nftw(path.c_str(), removeTmp, 16, FTW_DEPTH | FTW_PHYS); } } scratch{sessionTmp};
    std::string sandboxHome = sessionTmp + "/home";
    ensureDir(sandboxHome, 0700);
    // All native file tools and bash share the same private scratch grant.
    if (auto rr = authorityAddWriteRoot(auth, sessionTmp); !rr.ok) {
        fprintf(stderr, "pocket: %s\n", rr.error.c_str());
        authorityClose(auth);
        return 1;
    }
    // Parent-state file for a recursive `pocket` (depth/workspace/net grant).
    // Keys are deliberately NOT inherited: recursive instances authenticate
    // via explicit expose_env passthrough only.
    auto parentSaved = atomicWriteFile(sessionTmp + "/pocket.parent",
                    json::stringify(json::Object{{"depth", depth + 1}, {"workspace", workspace},
                                                {"net", allowNet}, {"coordination", coordination}}), 0600);
    if (!parentSaved.ok) { fprintf(stderr, "pocket: %s\n", parentSaved.error.c_str()); return 1; }

    // Catalog: live window/reasoning/prices; daily background refresh.
    catalogApply(cfg, model);
    if (depth == 0) catalogRefreshIfStale(cfg);

    // Stage this binary on the sandbox PATH: `pocket kit` and recursive
    // `pocket -p` must work inside confinement, wherever pocket is installed.
    auto staged = ensureDir(sessionTmp + "/bin", 0700);
    if (staged.ok) staged = stageExecutable(sessionTmp + "/bin/pocket");
    if (!staged.ok) {
        fprintf(stderr, "pocket: %s\n", staged.error.c_str());
        return 1;
    }

    ToolEnv tools;
    tools.auth = &auth;
    tools.cfg = &cfg;
    tools.workspace = workspace;
    tools.sessionTmp = sessionTmp;
    tools.sandboxHome = sandboxHome;
    tools.sessionId = sessionId;
    tools.depth = depth;
    tools.allowNet = allowNet;
    tools.unsafe = optUnsafe;
    tools.interactive = prompt.empty();
    tools.allowDestructive = optAllowDestructive;

    // Roles resolve once; a broken role is reported and skipped, never fatal.
    auto role = [&](const std::string& name) {
        std::vector<ResolvedModel> out;
        if (!cfg.roles.count(name)) return out;
        for (std::string spec : splitLines(cfg.roles[name])) {
            for (size_t p; (p = spec.find(',')) != std::string::npos;) spec[p] = '\n';
            for (const auto& one : splitLines(spec)) {
                if (trim(one).empty()) continue;
                auto r = resolveModel(cfg, trim(one));
                if (!r.ok) { fprintf(stderr, "pocket: role %s: %s\n", name.c_str(), r.error.c_str()); continue; }
                catalogApply(cfg, r.value);
                out.push_back(r.value);
            }
        }
        return out;
    };

    AgentOpts ao;
    ao.model = model;
    ao.fast = role("fast");
    ao.fallback = role("fallback");
    ao.reviewers = role("review");
    ao.autonomy = cfg.autonomy;
    ao.review = cfg.review;
    ao.brief = cfg.review;
    if (cfg.hooks.count("stop")) ao.stopHooks = cfg.hooks["stop"];
    ao.awareness = awarenessBlock(workspace, cfg, allowNet, optUnsafe, prompt.empty(),
                                  optMaxRounds > 0 ? optMaxRounds : cfg.maxRounds);
    ao.judge = [&cfg, &tools](const std::string& q, const std::string& text, double* cost) {
        return judgeYes(cfg, q, text, cost, tools.cancel, tools.onEvent);
    };
    ao.decide = [&cfg, &tools](const json::Value& state, const std::vector<Question>& qs, bool transcript, double* cost) {
        return decide(cfg, state, qs, transcript, cost, tools.cancel, tools.onEvent);
    };
    ao.hint = [&cfg, &tools, workspace](const std::string& text, double* cost) -> std::string {
        if (!needsBrief(text)) return "";
        auto hits = skillSearch(skillDiscover(workspace), text);
        if (hits.size() > 3) hits.resize(3);
        if (hits.empty()) return "";
        std::vector<Question> qs;
        for (size_t i = 0; i < hits.size(); ++i)
            qs.push_back({"s" + std::to_string(i), "Would the guide \"" + hits[i].name + ": " + hits[i].preview.substr(0, 200) +
                                                       "\" materially help an expert do this task well?"});
        auto p = decide(cfg, json::Object{{"task", text.substr(0, 2000)}}, qs, false, cost, tools.cancel, tools.onEvent);
        std::vector<std::string> keep;
        for (size_t i = 0; i < hits.size(); ++i)
            // Without a verdict, only a skill the request names (fuzzily) is offered:
            // a bare BM25 word overlap is noise, not relevance.
            if (p.empty() ? i == 0 && fuzzyScore(hits[i].name, text) >= 0.8 : p["s" + std::to_string(i)] >= 0.7)
                keep.push_back(hits[i].name);
        if (keep.empty()) return "";
        return "[harness] Relevant skill" + std::string(keep.size() > 1 ? "s: " : ": ") + join(keep, ", ") +
               " — load with skill(action=load, name=...) before starting.";
    };
    ao.thinking = thinking;
    ao.maxRounds = optMaxRounds > 0 ? optMaxRounds : cfg.maxRounds;
    ao.workingContextTokens = cfg.workingContextTokens;
    ao.progress = [&cfg, &tools](const std::string& recent, double*) {
        return progressHint(cfg, recent, tools.cancel, tools.onEvent);
    };
    ao.maxTokens = optMaxTokens;
    ao.tools = &tools;
    ao.sessionId = sessionId;
    if (depth > 0 && getenv("TMPDIR")) ao.parentUsageDir = getenv("TMPDIR");
    Agent agent(ao);
    judgeWarm(cfg);  // registers as a local-LM user; warms only when needed
    for (int sig : {SIGHUP, SIGTERM, SIGQUIT}) {
        struct sigaction sa{};
        sa.sa_handler = onExitSignal;
        sigemptyset(&sa.sa_mask);
        sigaction(sig, &sa, nullptr);
    }
    if (resume) {
        auto r = agent.restore(sessionId);
        if (!r.ok) { fprintf(stderr, "pocket: cannot resume: %s\n", r.error.c_str()); return 1; }
    }
    for (const auto& img : optImages) {
        std::string err = agent.attachImage(img);
        if (!err.empty()) {
            fprintf(stderr, "pocket: --image %s: %s\n", img.c_str(), err.c_str());
            return 1;
        }
    }
    if (!modelSpec.empty() || !thinkingCli.empty()) {
        SessionMeta m = sessionLoadMeta(sessionId).value;
        if (!modelSpec.empty()) m.modelSpec = model.spec;
        if (!thinkingCli.empty()) m.thinking = thinking;
        sessionSaveMeta(sessionId, m);
    }

    std::string sysSrc = sessionLoadMeta(sessionId).value.systemSource;
    int rc = 0;
    std::string nextSession;
    if (!prompt.empty()) {
        // --- non-interactive: stdout = streamed answer, stderr = activity ---
        if (!sysSrc.empty()) fprintf(stderr, "pocket: system prompt: %s\n", sysSrc.c_str());
        tools.askApproval = nullptr;  // fail closed (or --allow-destructive)
        struct sigaction sa{}, oldInt{}, oldTerm{};
        sa.sa_handler = cancelPrint;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT, &sa, &oldInt);
        sigaction(SIGTERM, &sa, &oldTerm);
        struct sigaction oldHup{};
        sigaction(SIGHUP, &sa, &oldHup);  // a closed terminal cancels cleanly too
        tools.cancel = &printCancel;
        agent.setCancel(&printCancel);
        bool errTty = isatty(STDERR_FILENO);
        bool midLine = false;  // notices must start on their own line
        agent.setCallbacks(
            [&](std::string_view tok) {
                std::string s = sanitizeTerminal(std::string(tok));
                (void)!fwrite(s.data(), 1, s.size(), stdout);
                fflush(stdout);
                if (!s.empty()) midLine = s.back() != '\n';
            },
            [&](const std::string& note) {
                fprintf(stderr, "%s(%s)\n", midLine ? "\n" : "", sanitizeTerminal(note).c_str());
                midLine = false;
            },
            [&](std::string_view chunk) {
                // Thinking streams to stderr so stdout stays the pure answer.
                std::string s = sanitizeTerminal(std::string(chunk));
                if (errTty) s = "\033[2m" + s + "\033[0m";
                (void)!fwrite(s.data(), 1, s.size(), stderr);
            });
        tools.onEvent = [&](const std::string& line) { fprintf(stderr, "⚙ %s\n", sanitizeTerminal(line).c_str()); };
        tools.onToolDone = [&](const std::string& name, bool ok, const std::string&) {
            fprintf(stderr, "%s %s\n", ok ? "✓" : "✗", name.c_str());
        };
        std::string err = !goalText.empty() ? agent.runGoal(goalText) :
                          agent.goalPaused() ? agent.resumeGoal(prompt) : agent.runTurn(prompt);
        sigaction(SIGINT, &oldInt, nullptr);
        sigaction(SIGTERM, &oldTerm, nullptr);
        sigaction(SIGHUP, &oldHup, nullptr);
        printf("\n");
        const AgentStats& st = agent.stats();
        if (st.cacheSeen) {
            long denom = st.cacheHit + st.cacheMiss;
            if (denom <= 0) denom = st.inTokens;
            long pct = denom > 0 ? st.cacheHit * 100 / denom : 0;
            fprintf(stderr, "cache: %ld hit / %ld miss (%ld%% reported reuse)\n", st.cacheHit,
                    st.cacheMiss, pct);
        }
        if (st.costSeen)
            fprintf(stderr, "cost: %s$%.4f%s (overseer $%.4f; %ld child sessions)\n",
                    st.costEstimated ? "~" : "", st.cost, st.costIncomplete ? " + unreported" : "",
                    st.sideCost, st.childSessions);
        else if (st.costIncomplete) fprintf(stderr, "cost: unreported\n");
        if (err == "cancelled") {
            fprintf(stderr, "cancelled\n");
            rc = 130;
        } else if (!err.empty()) {
            fprintf(stderr, "pocket: %s\n", sanitizeTerminal(err).c_str());
            rc = 1;
        }
    } else {
        // --- interactive ---
        if (depth > 0 && !isatty(STDIN_FILENO)) {
            fprintf(stderr, "pocket: child interactive session without a TTY; refusing\n");
            rc = 1;
        } else {
            TuiOpts to;
            to.agent = &agent;
            to.tools = &tools;
            to.cfg = &cfg;
            to.model = model;
            to.thinking = thinking;
            to.workspace = workspace;
            to.sessionId = sessionId;
            to.systemSource = sysSrc;
            to.unsafe = optUnsafe;
            to.allowNet = allowNet;
            to.prepareResume = [&](const std::string& id) -> std::string {
                if (id == sessionId) return "this session is already open";
                auto held = sessionLock(id);
                if (!held.ok) return held.error;
                Lease target{held.value};
                auto metadata = sessionLoadMeta(id);
                if (!metadata.ok) return metadata.error;
                if (!metadata.value.workspace.empty() && metadata.value.workspace != workspace)
                    return "session belongs to another workspace";
                Config targetConfig = baseConfig;
                if (metadata.value.rolesSet) targetConfig.roles = metadata.value.roles;
                auto selected = resolveModel(targetConfig, metadata.value.modelSpec);
                if (!selected.ok) return selected.error;
                // Validate with the existing restore implementation while the
                // old UI is still available to report a failure. No requests.
                AgentOpts check = ao;
                // Preflight must not consume child receipts, stage images or
                // publish usage through the source session's accounting state.
                ToolEnv validationTools;
                validationTools.workspace = workspace;
                check.tools = &validationTools;
                check.parentUsageDir.clear();
                check.sessionId = id;
                check.model = selected.value;
                check.thinking = validThinking(metadata.value.thinking) ? metadata.value.thinking : targetConfig.thinking;
                check.onNotice = {};
                Agent candidate(check);
                auto restored = candidate.restore(id);
                if (!restored.ok) return restored.error;
                std::string usageError = agent.flushUsage();
                if (!usageError.empty()) return usageError;
                if (pendingSession.fd >= 0) close(pendingSession.fd);
                pendingSession.fd = target.fd;
                pendingSession.id = id;
                target.fd = -1;
                return "";
            };
            if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO))
                rc = tuiRun(to);
            else
                rc = lineRun(to);
            nextSession = to.resumeId;
        }
    }

    std::string usageError = agent.flushUsage();
    if (!usageError.empty()) {
        fprintf(stderr, "pocket: %s\n", sanitizeTerminal(usageError).c_str());
        rc = 1;
    }
    killSessionProcesses();
    judgeShutdown();
    authorityClose(auth);
    if (rc == kTuiResume && !nextSession.empty()) {
        resume = true;
        resumeId = std::move(nextSession);
        modelSpec.clear();
        thinkingCli.clear();
        thinkingFlag.clear();
        optImages.clear();
        continue;
    }
    return rc;
    }
}

}  // namespace pocket
