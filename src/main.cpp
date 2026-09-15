// PocketHarness - entry point: CLI, privilege checks, workspace setup, modes.
#include <ftw.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>

#include "agent.h"
#include "common.h"
#include "config.h"
#include "process.h"
#include "provider.h"
#include "sandbox.h"
#include "session.h"
#include "tui.h"

namespace pocket {
namespace {

void usage() {
    printf(
        "pocket %s — tiny Linux-native coding-agent harness\n"
        "\n"
        "usage:\n"
        "  pocket [path]                 interactive agent in workspace (default: .)\n"
        "  pocket -p \"prompt\"            non-interactive single turn (composable)\n"
        "  pocket --resume [id]          resume newest (or given) session\n"
        "  pocket --sessions             list sessions\n"
        "\n"
        "options:\n"
        "  -m, --model SPEC     provider:model[@routing] or alias (default: config)\n"
        "  -t, --thinking LVL   off|low|medium|high|max\n"
        "  -p, --print PROMPT   non-interactive prompt (stdout = final answer)\n"
        "  --image PATH         attach an image (PNG/JPEG/GIF/WebP, max 5 MiB, repeatable)\n"
        "  --resume [id]        resume a session (interactive unless -p)\n"
        "  --sessions           list sessions and exit\n"
        "  --network            allow network access for tools (on by default)\n"
        "  --no-network, --offline  deny network access for tools\n"
        "  --allow-read PATH    extra read root for native tools (repeatable)\n"
        "  --allow-write PATH   extra write root for native tools (repeatable)\n"
        "  --allow-destructive  -p mode: permit guard-flagged commands (explicit)\n"
        "  --max-rounds N       cap model tool rounds per turn (default 100)\n"
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
};
ParentState readParentState() {
    ParentState ps;
    const char* td = getenv("TMPDIR");
    if (!td || !*td) return ps;
    std::string p = std::string(td) + "/pocket.parent";
    struct stat st;
    if (lstat(p.c_str(), &st) != 0) return ps;
    if (!S_ISREG(st.st_mode) || st.st_uid != geteuid()) return ps;
    auto t = readFileBounded(p, 4096);
    if (!t.ok) return ps;
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

}  // namespace

int pocketMain(int argc, char** argv);

}  // namespace pocket

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    return pocket::pocketMain(argc, argv);
}

namespace pocket {

int pocketMain(int argc, char** argv) {
    std::string positional;
    std::string prompt;
    std::string modelSpec;
    std::string thinkingCli;
    std::string resumeId;
    bool resume = false, listSessions = false;
    bool optNetwork = false, optUnsafe = false, optAllowRoot = false, optNoNetwork = false;
    bool optAllowDestructive = false;
    bool optHelp = false, optVersion = false;
    int optMaxRounds = 0;  // 0 = no CLI override; config/default applies
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
        if (a == "--help" || a == "-h") optHelp = true;
        else if (a == "--version" || a == "-v") optVersion = true;
        else if (a == "-p" || a == "--print") prompt = needVal("-p");
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
            optMaxRounds = atoi(needVal("--max-rounds").c_str());
            if (optMaxRounds < 1 || optMaxRounds > 1000) {
                fprintf(stderr, "pocket: --max-rounds needs 1..1000\n");
                return 2;
            }
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

    auto thinkOk = [](const std::string& t) {
        return t == "off" || t == "low" || t == "medium" || t == "high" || t == "max";
    };
    std::string thinkingFlag;
    if (!thinkingCli.empty()) {
        thinkingFlag = toLower(thinkingCli);
        if (!thinkOk(thinkingFlag)) {
            fprintf(stderr, "pocket: bad --thinking (off|low|medium|high|max)\n");
            return 2;
        }
    }

    if (listSessions) {
        auto list = sessionList();
        if (list.empty()) {
            printf("no sessions yet\n");
            return 0;
        }
        for (const auto& s : list)
            printf("%s  (%ld events)  %s\n", s.id.c_str(), s.events, s.firstLine.c_str());
        return 0;
    }

    // --- session first: resume restores its own model/thinking, so
    // concurrent sessions never observe each other (no shared UI state). ---
    std::string sessionId;
    if (resume) {
        auto s = sessionResolve(resumeId);
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
    SessionMeta sm = sessionLoadMeta(sessionId).value;

    // --- model + thinking: explicit CLI wins, then this session, then config ---
    std::string wantModel = !modelSpec.empty() ? modelSpec : sm.modelSpec;
    auto rmR = resolveModel(cfg, wantModel);
    if (!rmR.ok && !wantModel.empty() && wantModel != cfg.defaultModel) {
        fprintf(stderr, "pocket: %s; falling back to default\n", rmR.error.c_str());
        rmR = resolveModel(cfg, "");
    }
    if (!rmR.ok) {
        fprintf(stderr, "pocket: %s\n", rmR.error.c_str());
        return 1;
    }
    ResolvedModel model = rmR.value;
    std::string thinking = !thinkingFlag.empty() ? thinkingFlag : sm.thinking;
    if (!thinkOk(thinking)) thinking = cfg.thinking;

    if (!curlAvailable()) {
        fprintf(stderr, "pocket: the `curl` executable is required but not runnable\n");
        return 1;
    }

    // --- authority ---
    auto authR = authorityInit(workspace, cfg.allowRead, cfg.allowWrite, optUnsafe);
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
    std::string sandboxHome = sessionTmp + "/home";
    ensureDir(sandboxHome, 0700);
    // The native `read` tool can see session scratch (pasted images land
    // here too); model bash children already could.
    if (auto rr = authorityAddReadRoot(auth, sessionTmp); !rr.ok)
        fprintf(stderr, "pocket: note: %s\n", rr.error.c_str());
    // Parent-state file for a recursive `pocket` (depth/workspace/net grant).
    // Keys are deliberately NOT inherited: recursive instances authenticate
    // via explicit expose_env passthrough only.
    atomicWriteFile(sessionTmp + "/pocket.parent",
                    "depth=" + std::to_string(depth + 1) + "\nworkspace=" + workspace +
                        "\nnet=" + (allowNet ? "1" : "0") + "\n",
                    0600);

    // Live context window unless config pins one explicitly.
    if (!hasExplicitContext(cfg, model.provider.name, model.model)) {
        long live = fetchModelContext(model.provider, model.model);
        if (live > 0) model.context = live;
    }

    ToolEnv tools;
    tools.auth = &auth;
    tools.cfg = &cfg;
    tools.workspace = workspace;
    tools.sessionTmp = sessionTmp;
    tools.sandboxHome = sandboxHome;
    tools.allowNet = allowNet;
    tools.unsafe = optUnsafe;
    tools.interactive = prompt.empty();
    tools.allowDestructive = optAllowDestructive;

    AgentOpts ao;
    ao.model = model;
    ao.thinking = thinking;
    ao.maxRounds = optMaxRounds > 0 ? optMaxRounds : cfg.maxRounds;
    ao.tools = &tools;
    ao.sessionId = sessionId;
    Agent agent(ao);
    if (resume) {
        auto r = agent.restore(sessionId);
        if (!r.ok) fprintf(stderr, "pocket: resume note: %s\n", r.error.c_str());
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
    if (!prompt.empty()) {
        // --- non-interactive: stdout = streamed answer, stderr = activity ---
        if (!sysSrc.empty()) fprintf(stderr, "pocket: system prompt: %s\n", sysSrc.c_str());
        tools.askApproval = nullptr;  // fail closed (or --allow-destructive)
        std::atomic<bool> cancel{false};
        tools.cancel = &cancel;
        agent.setCancel(&cancel);
        bool errTty = isatty(STDERR_FILENO);
        agent.setCallbacks(
            [&](std::string_view tok) {
                std::string s = sanitizeTerminal(std::string(tok));
                (void)!fwrite(s.data(), 1, s.size(), stdout);
                fflush(stdout);
            },
            [&](const std::string& note) { fprintf(stderr, "(%s)\n", note.c_str()); },
            [&](std::string_view chunk) {
                // Thinking streams to stderr so stdout stays the pure answer.
                std::string s = sanitizeTerminal(std::string(chunk));
                if (errTty) s = "\033[2m" + s + "\033[0m";
                (void)!fwrite(s.data(), 1, s.size(), stderr);
            });
        tools.onEvent = [&](const std::string& line) { fprintf(stderr, "⚙ %s\n", line.c_str()); };
        tools.onToolDone = [&](const std::string& name, bool ok, const std::string&) {
            fprintf(stderr, "%s %s\n", ok ? "✓" : "✗", name.c_str());
        };
        std::string err = agent.runTurn(prompt);
        printf("\n");
        const AgentStats& st = agent.stats();
        if (st.cacheSeen) {
            long denom = st.cacheHit + st.cacheMiss;
            if (denom <= 0) denom = st.inTokens;
            long pct = denom > 0 ? st.cacheHit * 100 / denom : 0;
            fprintf(stderr, "cache: %ld hit / %ld miss (%ld%% reported reuse)\n", st.cacheHit,
                    st.cacheMiss, pct);
        }
        if (err == "cancelled") {
            fprintf(stderr, "cancelled\n");
            rc = 130;
        } else if (!err.empty()) {
            fprintf(stderr, "pocket: %s\n", err.c_str());
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
            if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO))
                rc = tuiRun(to);
            else
                rc = lineRun(to);
        }
    }

    authorityClose(auth);
    nftw(sessionTmp.c_str(), removeTmp, 16, FTW_DEPTH | FTW_PHYS);
    return rc;
}

}  // namespace pocket
