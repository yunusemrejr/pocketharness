// PocketHarness - native intelligence. Small deterministic algorithms in
// place of heavyweight ML subsystems: fuzzy search, BM25 ranking, a naive
// Bayes stop classifier, and learned provider state (quirks + EWMA health).
// No models, no dependencies, microseconds per call.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace pocket {

// Lowercase alphanumeric words ("gpt-4o mini" -> gpt, 4o, mini).
std::vector<std::string> words(std::string_view s);

// Fuzzy score in [0,1]: exact substring > word-prefix subsequence > loose
// subsequence, blended with trigram overlap for typos. 0 = no match.
double fuzzyScore(std::string_view query, std::string_view text);

// Indices of labels ordered by fuzzyScore (best first); scores below
// minScore are dropped. Empty query keeps every label in order.
std::vector<size_t> fuzzyRank(const std::vector<std::string>& labels, std::string_view query,
                              double minScore = 0.35);

// Okapi BM25 (k1=1.2, b=0.75) of query against each doc; parallel to docs.
std::vector<double> bm25(const std::vector<std::string>& docs, std::string_view query);

// How a model ended its turn when it made no tool call. Multinomial naive
// Bayes over uni+bigrams of the message tail, trained on a built-in seed set.
enum class StopKind { Done, Announce, Permission };
struct StopGuess {
    StopKind kind = StopKind::Done;
    double p = 1;  // posterior of `kind`
};
StopGuess classifyStop(std::string_view finalText);
const char* stopKindName(StopKind k);

// Native execution policy: no network, no learned completion verdict. The
// request establishes a floor; observed failures and growing work escalate
// independently. Explicit model/thinking settings remain Agent's authority.
enum class TaskScale { Simple, Standard, Complex, Critical };
struct TaskObservation {
    int failures = 0;
    int changedFiles = 0;
    int toolRounds = 0;
    int stalledRounds = 0;
    TaskScale actionScale = TaskScale::Simple;
};
struct TaskPolicy {
    TaskScale scale = TaskScale::Simple;
    std::string thinking = "low";
    bool planningBrief = false;
    bool fastModel = true;
    bool workspaceWork = false;  // a text-only answer cannot complete this intent
    int reviewers = 1;
};
TaskPolicy taskPolicy(std::string_view request, const TaskObservation& observed = {});
const char* taskScaleName(TaskScale scale);

// Learned state, persisted in stateDir()/brain.json (small, atomic writes).
// quirks: per "provider:model" wire incompatibilities discovered from 400s
//   (no_reasoning, no_stream_usage, max_tokens, max_completion_tokens).
// health: per provider EWMA of request success and latency.
std::vector<std::string> brainQuirks(const std::string& modelKey);
void brainNoteQuirk(const std::string& modelKey, const std::string& quirk);
void brainNoteHealth(const std::string& provider, bool ok, long ms);
double brainHealth(const std::string& provider);  // EWMA success, 1 when unknown
// Advisory implicit routing only. A single snapshot compares sufficiently
// observed health/latency; unknown providers and same-provider models remain
// eligible. Never used to override a user's explicit model selection.
bool brainPreferFast(const std::string& mainProvider, const std::string& fastProvider);
std::string brainStatus();                        // human summary, one line per provider

// Map a provider 400 body to a quirk to learn ("" = nothing recognizable).
std::string quirkFromError(const std::string& err, bool sentReasoning,
                           const std::string& tokenParameter);

}  // namespace pocket
