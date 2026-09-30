// PocketHarness - native intelligence implementation.
#include "brain.h"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <set>

#include "common.h"
#include "config.h"
#include "json.h"

namespace pocket {

std::vector<std::string> words(std::string_view s) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s) {
        unsigned char c = (unsigned char)ch;
        if (isalnum(c) || c >= 0x80) cur.push_back((char)tolower(c));
        else if (!cur.empty()) out.push_back(std::move(cur)), cur.clear();
    }
    if (!cur.empty()) out.push_back(std::move(cur));
    return out;
}

namespace {

bool boundary(const std::string& t, size_t i) {
    return i == 0 || !isalnum((unsigned char)t[i - 1]);
}

std::set<std::string> trigrams(const std::string& s) {
    std::set<std::string> g;
    std::string p = " " + s + " ";
    for (size_t i = 0; i + 3 <= p.size(); ++i) g.insert(p.substr(i, 3));
    return g;
}

// One query word against lowercase text.
double wordScore(const std::string& w, const std::string& t) {
    size_t pos = t.find(w);
    if (pos != std::string::npos) return boundary(t, pos) ? 1.0 : 0.9;
    // Subsequence alignment: contiguity and word starts earn bonuses.
    double pts = 0;
    size_t j = 0, last = std::string::npos;
    for (char c : w) {
        while (j < t.size() && t[j] != c) ++j;
        if (j == t.size()) { pts = -1; break; }
        pts += 1 + (last != std::string::npos && j == last + 1) + boundary(t, j);
        last = j++;
    }
    // ~2 points per char is a good typo match (match + contiguity or word start).
    double sub = pts < 0 ? 0 : std::min(0.9, 0.9 * pts / (2.2 * (double)w.size()));
    if (w.size() < 3) return sub;
    auto q = trigrams(w), tt = trigrams(t);
    size_t hit = 0;
    for (const auto& g : q) hit += tt.count(g);
    double tri = 0.7 * (double)hit / (double)q.size();
    return std::max(sub, tri);
}

}  // namespace

double fuzzyScore(std::string_view query, std::string_view text) {
    std::string t = toLower(text);
    std::vector<std::string> ws = words(query);
    if (ws.empty()) return 1;
    std::string whole = toLower(trim(query));
    if (t.find(whole) != std::string::npos) return 1;
    double s = 1;
    for (const auto& w : ws) s = std::min(s, wordScore(w, t));
    return s;
}

std::vector<size_t> fuzzyRank(const std::vector<std::string>& labels, std::string_view query,
                              double minScore) {
    std::vector<std::pair<double, size_t>> scored;
    bool all = words(query).empty();
    for (size_t i = 0; i < labels.size(); ++i) {
        double s = all ? 1 : fuzzyScore(query, labels[i]);
        if (s >= minScore) scored.push_back({s, i});
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<size_t> out;
    for (const auto& [s, i] : scored) out.push_back(i);
    return out;
}

std::vector<double> bm25(const std::vector<std::string>& docs, std::string_view query) {
    std::vector<std::vector<std::string>> toks;
    std::map<std::string, int> df;
    double avg = 0;
    for (const auto& d : docs) {
        toks.push_back(words(d));
        avg += (double)toks.back().size();
        for (const auto& w : std::set<std::string>(toks.back().begin(), toks.back().end())) ++df[w];
    }
    double n = (double)docs.size();
    avg = n > 0 ? std::max(1.0, avg / n) : 1;
    std::vector<double> out(docs.size(), 0);
    std::vector<std::string> qs = words(query);
    std::set<std::string> uq(qs.begin(), qs.end());
    for (size_t i = 0; i < docs.size(); ++i) {
        std::map<std::string, int> tf;
        for (const auto& w : toks[i]) ++tf[w];
        double len = (double)toks[i].size();
        for (const auto& q : uq) {
            auto it = tf.find(q);
            if (it == tf.end()) continue;
            double idf = std::log((n - df[q] + 0.5) / (df[q] + 0.5) + 1);
            double f = it->second;
            out[i] += idf * f * 2.2 / (f + 1.2 * (0.25 + 0.75 * len / avg));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Naive Bayes stop classifier. The seed set is the model: tiny, readable,
// and deterministic. Extend it with real misclassifications, not rules.
// ---------------------------------------------------------------------------
namespace {

const char* kSeedDone[] = {
    "All tests pass. The bug was an off-by-one error in the loop bound.",
    "Done. I updated the config and verified the build succeeds.",
    "The function now handles empty input and the tests pass.",
    "Summary of changes: renamed the module and updated imports; build passes.",
    "The project builds cleanly and all 42 tests passed.",
    "I fixed the issue in parser.c and added a regression test.",
    "This file defines the HTTP router and its middleware chain.",
    "Implemented the feature and verified it with the full test suite.",
    "No changes were needed; the code already handles this case.",
    "The error comes from a missing environment variable in the service unit.",
    "The answer is 42 because the loop runs six times seven.",
    "Verified: the server starts and responds with status 200.",
    "Here is what I found: the cache key ignores the locale.",
    "Everything is committed and the working tree is clean.",
    "The report is written to docs/audit.md with all findings.",
    "It works now. Output matches the expected results exactly.",
};
const char* kSeedAnnounce[] = {
    "Let me now run the tests.",
    "Now I'll update the config file.",
    "Next, I will implement the parser.",
    "I'll start by reading the main file.",
    "Let me check the build output.",
    "I will now fix the remaining errors.",
    "Now let me edit the function accordingly.",
    "Let me look at the logs next.",
    "I'm going to refactor this module now.",
    "First, I'll inspect the directory structure.",
    "Proceeding to write the tests now.",
    "Let me verify that by running make.",
    "Next step: apply the same change to the other files.",
    "I need to update the remaining call sites now.",
    "Let me search for other usages of this function.",
    "Now I will create the migration script.",
};
const char* kSeedPermission[] = {
    "Would you like me to proceed?",
    "Should I go ahead and apply these changes?",
    "Do you want me to implement this?",
    "Let me know if you'd like me to continue.",
    "Shall I run the tests?",
    "If you want, I can also update the documentation.",
    "Can I proceed with the refactor?",
    "Please confirm before I make these changes.",
    "Would you like me to fix the other issues too?",
    "I can do that next if you'd like.",
    "Should I continue with the remaining files?",
    "Let me know how you would like to proceed.",
    "Do you want me to go ahead with option A or B?",
    "I can implement this now. Want me to?",
    "Ready to apply the fix when you give the go-ahead.",
    "Would you like me to write tests for this as well?",
};

std::vector<std::string> nbFeatures(std::string_view text) {
    std::string tail(text.size() > 240 ? text.substr(text.size() - 240) : text);
    std::vector<std::string> ws = words(tail);
    std::vector<std::string> f = ws;
    for (size_t i = 0; i + 1 < ws.size(); ++i) f.push_back(ws[i] + "_" + ws[i + 1]);
    std::string t = trim(tail);
    if (!t.empty() && t.back() == '?') f.push_back("?end");
    if (!t.empty() && t.back() == ':') f.push_back(":end");
    return f;
}

struct NbModel {
    std::map<std::string, double> count[3];
    double total[3] = {0, 0, 0};
    std::set<std::string> vocab;
    NbModel() {
        auto train = [&](int k, const char* const* seeds, size_t n) {
            for (size_t i = 0; i < n; ++i)
                for (const auto& f : nbFeatures(seeds[i])) {
                    count[k][f] += 1;
                    total[k] += 1;
                    vocab.insert(f);
                }
        };
        train(0, kSeedDone, std::size(kSeedDone));
        train(1, kSeedAnnounce, std::size(kSeedAnnounce));
        train(2, kSeedPermission, std::size(kSeedPermission));
    }
};

}  // namespace

StopGuess classifyStop(std::string_view finalText) {
    static const NbModel m;
    std::vector<std::string> f = nbFeatures(finalText);
    if (f.empty()) return {};
    const double prior[3] = {0.6, 0.2, 0.2};  // finishing is the common case
    double lp[3];
    double v = (double)m.vocab.size();
    for (int k = 0; k < 3; ++k) {
        lp[k] = std::log(prior[k]);
        for (const auto& x : f) {
            if (!m.vocab.count(x)) continue;  // unseen words carry no evidence for any class
            auto it = m.count[k].find(x);
            double c = it == m.count[k].end() ? 0 : it->second;
            lp[k] += std::log((c + 0.5) / (m.total[k] + 0.5 * v));
        }
    }
    int best = (int)(std::max_element(lp, lp + 3) - lp);
    double z = 0;
    for (int k = 0; k < 3; ++k) z += std::exp(lp[k] - lp[best]);
    return {(StopKind)best, 1 / z};
}

const char* stopKindName(StopKind k) {
    return k == StopKind::Done ? "done" : k == StopKind::Announce ? "announce" : "permission";
}

TaskPolicy taskPolicy(std::string_view request, const TaskObservation& observed) {
    // Whole words prevent e.g. "authentication" from becoming a match for
    // "cat". The policy only allocates effort; it never certifies success.
    auto tokens = words(request);
    std::set<std::string> ws(tokens.begin(), tokens.end());
    auto has = [&](std::initializer_list<const char*> terms) {
        for (const char* term : terms) if (ws.count(term)) return true;
        return false;
    };
    bool critical = has({"production", "deploy", "deployment", "release", "publish", "credentials", "auth", "oauth",
                         "authentication", "authorization", "security", "payment", "payments",
                         "destructive", "irreversible"});
    bool uncertain = has({"debug", "diagnose", "investigate", "race", "deadlock", "corruption",
                          "regression", "intermittent", "algorithm", "algorithms", "training", "finetune",
                          "finetuning", "qlora", "lora", "peft", "sft", "colab"});
    bool tuning = has({"finetune", "finetuning", "training"}) ||
                  (ws.count("fine") && has({"tune", "tuning"})) ||
                  (has({"qlora", "lora", "peft", "sft", "colab"}) && has({"train", "create", "build", "tune"}));
    bool mediaPipeline = has({"video", "animation", "film", "music", "soundtrack"}) &&
                         has({"create", "build", "compose", "produce", "animate"});
    bool broad = has({"architecture", "redesign", "overhaul", "migrate", "migration", "substantially",
                      "comprehensive", "pipeline", "benchmark", "benchmarks"}) || tokens.size() > 80 ||
                 tuning || mediaPipeline;
    int acceptance = 0;
    for (const char* term : {"implement", "test", "tests", "verify", "documentation", "benchmark", "deliver"})
        acceptance += ws.count(term);
    bool multi = acceptance >= 3 || (ws.count("across") && has({"modules", "services", "repositories", "platforms"}));
    TaskScale scale = critical ? TaskScale::Critical : broad || multi ? TaskScale::Complex :
                      uncertain || tokens.size() > 24 ? TaskScale::Standard : TaskScale::Simple;
    scale = std::max(scale, observed.actionScale);
    if (observed.changedFiles > 3 || observed.toolRounds >= 12 || observed.failures >= 3)
        scale = std::max(scale, TaskScale::Complex);
    else if (observed.changedFiles || observed.failures || observed.toolRounds >= 4)
        scale = std::max(scale, TaskScale::Standard);
    TaskPolicy p;
    p.scale = scale;
    p.thinking = scale >= TaskScale::Complex || observed.failures ? "high" :
                 scale == TaskScale::Standard ? "medium" : "low";
    p.planningBrief = scale >= TaskScale::Complex;
    p.fastModel = scale == TaskScale::Simple && !observed.failures && !observed.changedFiles;
    bool explanation = !tokens.empty() && (tokens[0] == "what" || tokens[0] == "why" || tokens[0] == "how");
    for (size_t i = 0; i < tokens.size() && i < 8; ++i)
        explanation |= tokens[i] == "explain" || tokens[i] == "describe" || tokens[i] == "summarize";
    bool artifact = has({"file", "files", "app", "application", "website", "ui", "code", "function", "module",
                         "project", "parser", "repository", "script", "program", "video", "animation", "music",
                         "soundtrack", "pdf", "mp4", "docx", "xlsx", "pptx", "document", "sheet", "deck",
                         "image", "logo", "illustration", "png", "svg", "wav", "mp3", "audio"});
    bool action = has({"fix", "edit", "implement", "refactor", "migrate", "deploy", "redesign", "rewrite",
                       "optimize", "optimise", "upgrade", "install", "remove", "delete", "rename", "update",
                       "build", "develop", "repair", "publish", "release"});
    // Writing a poem or correcting a sentence can finish in chat. Writing a
    // requested program/artifact, or fixing a project, needs observed work.
    bool prose = has({"poem", "sentence", "grammar", "wording", "email", "haiku"}) && !artifact;
    p.workspaceWork = !explanation && !prose &&
                      (action || tuning || mediaPipeline ||
                       (artifact && has({"create", "make", "write", "produce", "compose"})));
    p.reviewers = scale >= TaskScale::Complex ? 3 : 1;
    return p;
}

const char* taskScaleName(TaskScale scale) {
    switch (scale) {
        case TaskScale::Simple: return "simple";
        case TaskScale::Standard: return "standard";
        case TaskScale::Complex: return "complex";
        case TaskScale::Critical: return "critical";
    }
    return "standard";
}

// ---------------------------------------------------------------------------
// Learned state.
// ---------------------------------------------------------------------------
namespace {

std::mutex g_brainMu;
// Reload under an interprocess lock on every operation. Provider learning is
// tiny and infrequent; this avoids stale per-process snapshots overwriting
// another active session's observations or following it into another HOME.
struct BrainLock {
    int fd = -1;
    BrainLock() {
        if (!ensureDir(stateDir(), 0700).ok) return;
        fd = open((stateDir() + "/brain.lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd < 0) return;
        int rc;
        do { rc = flock(fd, LOCK_EX); } while (rc < 0 && errno == EINTR);
        if (rc < 0) { close(fd); fd = -1; }
    }
    ~BrainLock() { if (fd >= 0) close(fd); }
};

std::string brainPath() { return stateDir() + "/brain.json"; }

json::Value brainLocked() {
    auto t = readFileBounded(brainPath(), 1 << 20);
    auto v = t.ok ? json::parse(t.value) : Result<json::Value>::Err("");
    json::Value brain = v.ok && v.value.isObj() ? std::move(v.value) : json::Value(json::obj());
    for (const char* k : {"quirks", "health"})
        if (!brain.at(k).isObj()) brain.asObj()[k] = json::obj();
    return brain;
}

void brainSaveLocked(const json::Value& brain) {
    (void)atomicWriteFile(brainPath(), json::stringify(brain, true), 0600);
}

}  // namespace

std::vector<std::string> brainQuirks(const std::string& modelKey) {
    std::lock_guard<std::mutex> lk(g_brainMu);
    BrainLock disk;
    auto brain = brainLocked();
    std::vector<std::string> out;
    for (const auto& q : brain.at("quirks").at(modelKey).asArr()) out.push_back(q.asStr());
    return out;
}

void brainNoteQuirk(const std::string& modelKey, const std::string& quirk) {
    std::lock_guard<std::mutex> lk(g_brainMu);
    BrainLock disk;
    if (disk.fd < 0) return;
    auto brain = brainLocked();
    auto& qs = brain.asObj()["quirks"].asObj()[modelKey];
    if (!qs.isArr()) qs = json::Array{};
    for (const auto& q : qs.asArr())
        if (q.asStr() == quirk) return;
    // Mutually exclusive token parameter corrections replace one another.
    if (quirk == "max_tokens" || quirk == "max_completion_tokens") {
        auto& a = qs.asArr();
        a.erase(std::remove_if(a.begin(), a.end(), [](const json::Value& q) {
            return q.asStr() == "max_tokens" || q.asStr() == "max_completion_tokens";
        }), a.end());
    }
    qs.asArr().push_back(quirk);
    brainSaveLocked(brain);
}

void brainNoteHealth(const std::string& provider, bool ok, long ms) {
    std::lock_guard<std::mutex> lk(g_brainMu);
    BrainLock disk;
    if (disk.fd < 0) return;
    auto brain = brainLocked();
    auto& h = brain.asObj()["health"].asObj()[provider];
    if (!h.isObj()) h = json::Object{{"ok", 1.0}, {"ms", (double)ms}, {"n", 0L}};
    auto& o = h.asObj();
    o["ok"] = 0.8 * o["ok"].asNum(1) + 0.2 * (ok ? 1.0 : 0.0);
    if (ok) o["ms"] = std::round(0.8 * o["ms"].asNum((double)ms) + 0.2 * (double)ms);
    o["n"] = o["n"].asInt(0) + 1;
    brainSaveLocked(brain);
}

double brainHealth(const std::string& provider) {
    std::lock_guard<std::mutex> lk(g_brainMu);
    BrainLock disk;
    return brainLocked().at("health").at(provider).at("ok").asNum(1);
}

bool brainPreferFast(const std::string& mainProvider, const std::string& fastProvider) {
    if (mainProvider == fastProvider) return true; // health is provider-level
    std::lock_guard<std::mutex> lk(g_brainMu);
    BrainLock disk;
    const auto brain = brainLocked(); // one coherent cross-process snapshot
    const auto& fast = brain.at("health").at(fastProvider);
    constexpr long sufficientSamples = 8;
    if (fast.at("n").asInt(0) < sufficientSamples) return true;
    double fastHealth = fast.at("ok").asNum(1);
    if (fastHealth < .8) return false;
    const auto& main = brain.at("health").at(mainProvider);
    if (main.at("n").asInt(0) < sufficientSamples || main.at("ok").asNum(1) < .8) return true;
    double mainMs = main.at("ms").asNum(0), fastMs = fast.at("ms").asNum(0);
    // Latency varies with prompt size. Reject only a large, well-observed
    // disadvantage; do not turn a small timing difference into a model verdict.
    return mainMs <= 0 || fastMs <= mainMs * 3 || fastMs <= mainMs + 1000;
}

std::string brainStatus() {
    std::lock_guard<std::mutex> lk(g_brainMu);
    BrainLock disk;
    auto brain = brainLocked();
    std::string s;
    for (const auto& [p, h] : brain.at("health").asObj()) {
        char b[160];
        snprintf(b, sizeof b, "  %-24s health %3.0f%%  ~%ldms  n=%ld\n", p.c_str(),
                 h.at("ok").asNum(1) * 100, h.at("ms").asInt(0), h.at("n").asInt(0));
        s += b;
    }
    for (const auto& [m, q] : brain.at("quirks").asObj()) {
        std::vector<std::string> qs;
        for (const auto& x : q.asArr()) qs.push_back(x.asStr());
        s += "  quirk " + m + ": " + join(qs, ",") + "\n";
    }
    return s.empty() ? "  (nothing learned yet)\n" : s;
}

std::string quirkFromError(const std::string& err, bool sentReasoning,
                           const std::string& tokenParameter) {
    if (!startsWith(err, "HTTP 400") && !startsWith(err, "HTTP 422")) return "";
    std::string e = toLower(err);
    if (e.find("stream_options") != std::string::npos) return "no_stream_usage";
    if (tokenParameter == "max_completion_tokens" && e.find("max_completion_tokens") != std::string::npos)
        return "max_tokens";
    if (tokenParameter == "max_tokens" && e.find("max_completion_tokens") != std::string::npos)
        return "max_completion_tokens";
    // A history-shape complaint ("reasoning_content ... must be passed back")
    // is not a missing capability: retry this request without thinking, learn nothing.
    if (e.find("passed back") != std::string::npos && e.find("reasoning") != std::string::npos)
        return "thinking_off_once";
    if (sentReasoning && (e.find("reasoning") != std::string::npos ||
                          e.find("thinking") != std::string::npos ||
                          e.find("effort") != std::string::npos))
        return "no_reasoning";
    return "";
}

}  // namespace pocket
