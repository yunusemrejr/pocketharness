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
        "  --resume [id]        resume a session (interactive unless -p)\n"
        "  --sessions           list sessions and exit\n"
        "  --network            allow network access for model bash commands\n"
        "  --allow-read PATH    extra read root for native tools (repeatable)\n"
        "  --allow-write PATH   extra write root for native tools (repeatable)\n"
        "  --allow-destructive  -p mode: permit guard-flagged commands (explicit)\n"
        "  --unsafe             disable containment (conspicuous, never persisted)\n"
        "  --allow-root         permit agent execution as UID 0 (dangerous)\n"
        "  --help               this text\n"
        "  --version            version\n"
        "\n"
        "Recursive use (subagents via Linux processes):\n"
        "  pocket -p \"review auth\" > /tmp/a & pocket -p \"review io\" > /tmp/b & wait\n",
        kVersion);
}

int myDepth() {
    const char* d = getenv("POCKETHARNESS_DEPTH");
    if (!d || !*d) return 0;
    return atoi(d);
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
    bool optNetwork = false, optUnsafe = false, optAllowRoot = false;
    bool optAllowDestructive = false;
    bool optHelp = false, optVersion = false;
    std::vector<std::string> allowRead, allowWrite;

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
        else if (a == "--unsafe") optUnsafe = true;
        else if (a == "--allow-root") optAllowRoot = true;
        else if (a == "--allow-destructive") optAllowDestructive = true;
        else if (a == "--allow-read") allowRead.push_back(needVal("--allow-read"));
        else if (a == "--allow-write") allowWrite.push_back(needVal("--allow-write"));
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
    int depth = myDepth();
    if (depth < 0) depth = 0;
    if (depth > kMaxPocketDepth) {
        fprintf(stderr, "pocket: nesting too deep (POCKETHARNESS_DEPTH=%d, max %d)\n", depth,
                kMaxPocketDepth);
        return 1;
    }
    const char* parentWsEnv = getenv("POCKETHARNESS_WORKSPACE");
    std::string parentWs = parentWsEnv ? parentWsEnv : "";
    bool parentNet = getenv("POCKETHARNESS_PARENT_NET") &&
                     std::string(getenv("POCKETHARNESS_PARENT_NET")) == "1";
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
    bool allowNet = cfg.toolNetwork || optNetwork;
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

    // --- session scratch: TMPDIR, fake HOME, provider key file ---
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
    std::string keyfile = sessionTmp + "/provider.env";
    {
        // Merge parent keyfile (recursive pocket) with our own env; env wins.
        std::string content;
        if (const char* pkf = getenv("POCKETHARNESS_KEYFILE")) {
            if (pkf[0] && access(pkf, R_OK) == 0) {
                auto t = readFileBounded(pkf, 65536);
                if (t.ok) content = t.value;
            }
        }
        for (const auto& p : cfg.providers) {
            const char* v = getenv(p.keyEnv.c_str());
            if (!v || !*v) continue;
            std::string val(v);
            if (val.find('\n') != std::string::npos) continue;
            // Drop any older line for this name, then append.
            std::string next;
            for (const std::string& ln : splitLines(content)) {
                if (ln.substr(0, ln.find('=')) != p.keyEnv && !trim(ln).empty())
                    next += ln + "\n";
            }
            next += p.keyEnv + "=" + val + "\n";
            content = next;
        }
        if (!content.empty()) atomicWriteFile(keyfile, content, 0600);
    }

    ToolEnv tools;
    tools.auth = &auth;
    tools.cfg = &cfg;
    tools.workspace = workspace;
    tools.sessionTmp = sessionTmp;
    tools.sandboxHome = sandboxHome;
    tools.sessionId = sessionId;
    tools.keyfile = keyfile;
    tools.depth = depth + 1;
    tools.allowNet = allowNet;
    tools.unsafe = optUnsafe;
    tools.interactive = prompt.empty();
    tools.allowDestructive = optAllowDestructive;

    AgentOpts ao;
    ao.model = model;
    ao.thinking = thinking;
    ao.tools = &tools;
    ao.sessionId = sessionId;
    ao.tmpDir = sessionTmp;
    Agent agent(ao);
    if (resume) {
        auto r = agent.restore(sessionId);
        if (!r.ok) fprintf(stderr, "pocket: resume note: %s\n", r.error.c_str());
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
        agent.setCallbacks(
            [&](std::string_view tok) {
                std::string s = sanitizeTerminal(std::string(tok));
                (void)!fwrite(s.data(), 1, s.size(), stdout);
                fflush(stdout);
            },
            [&](const std::string& note) { fprintf(stderr, "(%s)\n", note.c_str()); });
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
        if (getenv("POCKETHARNESS_DEPTH") && depth > 0 && !isatty(STDIN_FILENO)) {
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
