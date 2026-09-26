// PocketHarness - overseer senses implementation.
#include "oversee.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cmath>
#include <map>
#include <mutex>

#include "json.h"
#include "kit.h"
#include "process.h"
#include "provider.h"

namespace pocket {

namespace {

std::mutex g_judgeMu;
pid_t g_serverPid = 0;
int64_t g_localPausedUntil = 0, g_jevPausedUntil = 0;
std::map<std::string, double> g_judgeCache;

bool portOpen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bool ok = connect(fd, (sockaddr*)&a, sizeof a) == 0;
    close(fd);
    return ok;
}

std::string localKey(const Config& cfg) {
    if (cfg.localLm.keyFile.empty()) return "";
    auto t = readFileBounded(cfg.localLm.keyFile, 4096);
    return t.ok ? trim(t.value) : "";
}

// Start llama-server once, niced and thread-capped: the judge must never
// compete with the user's own work for the machine.
bool ensureLocalServer(const Config& cfg) {
    const auto& l = cfg.localLm;
    if (portOpen(l.port)) return true;
    if (g_serverPid || l.server.empty() || l.model.empty() || access(l.server.c_str(), X_OK) != 0 ||
        access(l.model.c_str(), R_OK) != 0)
        return false;
    std::vector<std::string> args = {l.server, "--model", l.model, "--host", "127.0.0.1", "--port",
                                     std::to_string(l.port), "--threads", std::to_string(l.threads),
                                     "--ctx-size", std::to_string(l.ctx), "--n-predict", "16",
                                     "--parallel", "1", "--no-webui", "--log-disable"};
    if (!l.keyFile.empty()) args.insert(args.end(), {"--api-key-file", l.keyFile});
    std::vector<char*> av;  // built before fork: the child may only exec
    for (auto& a : args) av.push_back(a.data());
    av.push_back(nullptr);
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        setsid();
        setpriority(PRIO_PROCESS, 0, 19);
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) dup2(devnull, 0), dup2(devnull, 1), dup2(devnull, 2);
        execv(av[0], av.data());
        _exit(127);
    }
    g_serverPid = pid;
    for (int i = 0; i < 150; ++i) {  // up to 15s for the model to map
        struct timespec ts{0, 100 * 1000 * 1000};
        nanosleep(&ts, nullptr);
        if (waitpid(pid, nullptr, WNOHANG) == pid) { g_serverPid = 0; return false; }
        if (portOpen(l.port)) return true;
    }
    return false;
}

double localJudge(const Config& cfg, const std::string& question, const std::string& text) {
    if (nowMs() < g_localPausedUntil || !ensureLocalServer(cfg)) return -1;
    std::string prompt =
        "Answer the question about the text with yes or no.\n"
        "Text: Would you like me to run the tests now?\nQuestion: Is the assistant asking permission for work it could do itself?\nAnswer: yes\n"
        "Text: All 12 tests pass; the fix is in parser.c.\nQuestion: Is the assistant asking permission for work it could do itself?\nAnswer: no\n"
        "Text: " + text.substr(text.size() > 1500 ? text.size() - 1500 : 0) + "\nQuestion: " + question + "\nAnswer:";
    json::Object body{{"prompt", prompt}, {"n_predict", 1L}, {"n_probs", 10L}, {"temperature", 0L},
                      {"cache_prompt", true}};
    std::string key = localKey(cfg);
    auto r = httpRequest("http://127.0.0.1:" + std::to_string(cfg.localLm.port) + "/completion",
                         key.empty() ? "" : "Authorization: Bearer " + key, json::stringify(body), 8000);
    double p = r.ok ? parseYesProbability(r.value) : -1;
    if (p < 0) g_localPausedUntil = nowMs() + 60000;  // a failing judge rests for a minute
    return p;
}

std::map<std::string, std::map<std::string, double>> g_decideCache;

std::map<std::string, double> decisionsCall(const std::string& model, const json::Value& state,
                                            const std::vector<Question>& qs, double* cost) {
    const char* key = getenv("OPENROUTER_API_KEY");
    std::map<std::string, double> out;
    json::Object questions;
    for (const auto& q : qs) questions[q.id] = json::Object{{"type", "noul"}, {"instructions", q.text}};
    json::Object body{{"model", model}, {"state", state}, {"questions", questions}};
    std::string payload = json::stringify(body);
    if (payload.size() > 60000) return out;  // decisions inputs are small by design
    auto r = httpRequest("https://openrouter.ai/api/alpha/decisions", std::string("Authorization: Bearer ") + key,
                         payload, 15000);
    if (!r.ok) return out;
    auto v = json::parse(r.value);
    if (!v.ok) return out;
    for (const auto& q : qs) {
        double p = v.value.at("answers").at(q.id).at("noul").asNum(-1);
        if (p >= 0 && p <= 1) out[q.id] = p;
    }
    if (cost) *cost += std::max(0.0, v.value.at("usage").at("cost").asNum(0));
    return out;
}

// Flatten a transcript state into plain text for Jev, which takes any object.
json::Value asContent(const json::Value& state) {
    if (!state.at("input").isArr()) return state;
    std::string t;
    for (const auto& m : state.at("input").asArr()) t += "[" + m.at("role").asStr() + "] " + m.at("content").asStr() + "\n";
    t += "[assistant final] " + state.at("output").at("content").asStr();
    return json::Object{{"transcript", t.substr(t.size() > 24000 ? t.size() - 24000 : 0)}};
}

}  // namespace

double parseYesProbability(const std::string& body) {
    auto v = json::parse(body);
    if (!v.ok) return -1;
    const auto& first = v.value.at("completion_probabilities").at((size_t)0);
    const json::Value* cands = &first.at("top_logprobs");
    if (!cands->isArr()) cands = &first.at("top_probs");
    if (!cands->isArr()) cands = &first.at("probs");
    double yes = 0, no = 0;
    for (const auto& c : cands->asArr()) {
        std::string tok = toLower(trim(c.at("token").isStr() ? c.at("token").asStr() : c.at("tok_str").asStr()));
        double p = c.has("logprob") ? std::exp(c.at("logprob").asNum(-100)) : c.at("prob").asNum(0);
        if (tok == "yes") yes += p;
        if (tok == "no") no += p;
    }
    return yes + no > 0 ? yes / (yes + no) : -1;
}

bool decideAvailable(const Config& cfg) {
    const char* key = getenv("OPENROUTER_API_KEY");
    return cfg.jev && key && *key && nowMs() >= g_jevPausedUntil;
}

std::map<std::string, double> decide(const Config& cfg, const json::Value& state,
                                     const std::vector<Question>& qs, bool transcript, double* cost) {
    std::map<std::string, double> out;
    if (!decideAvailable(cfg) || qs.empty()) return out;
    std::string ck = json::stringify(state) + (transcript ? "T" : "C");
    for (const auto& q : qs) ck += "\n" + q.id + ":" + q.text;
    {
        std::lock_guard<std::mutex> lk(g_judgeMu);
        if (auto it = g_decideCache.find(ck); it != g_decideCache.end()) return it->second;
    }
    // Span reads conversations; Jev reads content. Each backs up the other.
    const char* primary = transcript ? "respan/span-01" : "~typesafe/jev-latest";
    const char* backup = transcript ? "~typesafe/jev-latest" : "respan/span-01";
    out = decisionsCall(primary, state, qs, cost);
    if (out.empty()) {
        json::Value alt = transcript ? asContent(state)
                                     : json::Value(json::stringify(state).substr(0, 24000));  // Span takes a string
        out = decisionsCall(backup, alt, qs, cost);
    }
    std::lock_guard<std::mutex> lk(g_judgeMu);
    if (out.empty()) g_jevPausedUntil = nowMs() + 120000;  // both routes down: rest two minutes
    else g_decideCache[ck] = out;
    return out;
}

double judgeYes(const Config& cfg, const std::string& question, const std::string& text, double* cost) {
    {
        std::lock_guard<std::mutex> lk(g_judgeMu);
        if (auto it = g_judgeCache.find(question + "\n" + text); it != g_judgeCache.end()) return it->second;
    }
    // A single question goes to the free local model first, then Jev.
    double p;
    {
        std::lock_guard<std::mutex> lk(g_judgeMu);
        p = localJudge(cfg, question, text);
    }
    if (p < 0) {
        auto r = decide(cfg, json::Object{{"text", text.substr(text.size() > 6000 ? text.size() - 6000 : 0)}},
                        {{"q", question}}, false, cost);
        p = r.count("q") ? r["q"] : -1;
    }
    std::lock_guard<std::mutex> lk(g_judgeMu);
    if (p >= 0) g_judgeCache[question + "\n" + text] = p;
    return p;
}

std::string judgeStatus(const Config& cfg) {
    std::string s = "judge: native naive-Bayes (always)";
    if (!cfg.localLm.model.empty())
        s += " · local " + baseName(cfg.localLm.model) + (portOpen(cfg.localLm.port) ? " (running)" : " (on demand)");
    if (decideAvailable(cfg)) s += " · span-01 (transcripts) · jev (content)";
    return s;
}

void judgeShutdown() {
    if (g_serverPid > 0) {
        kill(g_serverPid, SIGTERM);
        waitpid(g_serverPid, nullptr, 0);
        g_serverPid = 0;
    }
}

std::string verifyCommand(const std::string& ws) {
    auto has = [&](const char* f) { return access((ws + "/" + f).c_str(), R_OK) == 0; };
    if (has("Makefile")) {
        auto t = readFileBounded(ws + "/Makefile", 1 << 20);
        return t.ok && t.value.find("\ntest:") != std::string::npos ? "make test" : "make";
    }
    if (has("Cargo.toml")) return "cargo test";
    if (has("go.mod")) return "go test ./...";
    if (has("package.json")) return "npm test";
    if (has("pyproject.toml") || has("pytest.ini") || has("setup.py")) return "python3 -m pytest -q";
    if (has("CMakeLists.txt")) return "cmake -B build && cmake --build build && ctest --test-dir build";
    if (has("pom.xml")) return "mvn -q test";
    if (has("build.gradle") || has("build.gradle.kts")) return "./gradlew test";
    return "";
}

std::string awarenessBlock(const std::string& ws, const Config& cfg, bool allowNet, bool unsafe,
                           bool interactive, int maxRounds) {
    std::string s = "\nEnvironment (captured at session start):\n";
    for (const auto& l : splitLines(hostProbe(ws))) s += "- " + l + "\n";
    char date[32];
    time_t now = time(nullptr);
    strftime(date, sizeof date, "%Y-%m-%d", localtime(&now));
    s += "- date: " + std::string(date) + "\n";
    SpawnOpts o;
    o.exe = "git";
    o.argv = {"git", "-C", ws, "status", "--porcelain=v1", "--branch"};
    o.timeoutMs = 3000;
    o.outLimit = 1 << 20;
    SpawnResult g = spawn(o);
    if (g.ok && g.exitCode == 0) {
        auto lines = splitLines(g.out);
        std::string branch = lines.empty() ? "" : lines[0].substr(std::min<size_t>(3, lines[0].size()));
        s += "- git: " + branch + ", " + std::to_string(lines.empty() ? 0 : lines.size() - 1) +
             " uncommitted path(s) at start (not yours: preserve them)\n";
    } else s += "- git: not a repository\n";
    std::string v = verifyCommand(ws);
    if (!v.empty()) s += "- verify with: " + v + "\n";
    s += "Guardrails (enforced by the harness and kernel, not by you):\n";
    s += unsafe ? "- UNSAFE mode: containment is off; act conservatively.\n"
                : "- writes: workspace, /tmp and configured roots only; secrets are never visible to tools\n";
    s += std::string("- tool network: ") + (allowNet ? "on" : "OFF (offline: never retry network commands)") + "\n";
    s += std::string("- destructive commands: ") +
         (interactive ? "a human approves them" : "blocked (non-interactive)") + "\n";
    s += "- at most " + std::to_string(maxRounds) + " tool rounds per turn; repeated identical calls are stopped\n";
    s += "- every write/edit is quality-scanned; ";
    s += cfg.review ? "an overseer reviews changed work before a turn ends\n" : "reviews are off\n";
    s += "Superpowers: `pocket kit` via bash — web, search, dom, shot, img, svg, spring, wav, audio, slop, "
         "find/sym/refs (code index), probe; run `pocket kit` for usage. To see a UI, "
         "`pocket kit shot file://$PWD/page.html out.png 1280x800`, then read out.png: images you read are "
         "shown to you.\n";
    std::string mem = projectDir(ws) + "/memory.md";
    auto m = readFileBounded(mem, 16384);
    s += "Project memory: " + mem + " — append short durable learnings (commands, pitfalls, decisions).\n";
    if (m.ok && !trim(m.value).empty()) s += "Project memory contents:\n" + m.value + "\n";
    return s;
}

}  // namespace pocket
