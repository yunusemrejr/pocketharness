// PocketHarness - the overseer's senses: a yes/no judge and environment
// awareness. The judge cascades free-and-local to cheap-and-remote:
//   native naive Bayes (brain.h, always) -> local llama.cpp Qwen (started on
//   demand, loopback) -> OpenRouter Jev decisions (~$0.00001 per call).
// Each layer refines, none gates: when all are unavailable, callers keep
// their native heuristic.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "config.h"
#include "json.h"

namespace pocket {

// P(yes) that `question` holds for `text`; -1 when no judge answered.
// cost (if non-null) receives the USD spent (0 local, reported for Jev).
double judgeYes(const Config& cfg, const std::string& question, const std::string& text,
                double* cost = nullptr);

// Batched structured judgment on OpenRouter's decisions API: one call
// answers many yes/no questions with calibrated probabilities.
//   transcript=true: Span (respan/span-01) judges a conversation; `state`
//     must be {"input":[{role,content}...],"output":{role:assistant,...}}.
//   transcript=false: Jev (typesafe) judges any content object.
// Each route falls back to the other. Returns id -> P(yes); missing ids
// mean "unknown". Cached per process; cost += USD reported.
struct Question {
    std::string id, text;
};
std::map<std::string, double> decide(const Config& cfg, const json::Value& state,
                                     const std::vector<Question>& qs, bool transcript,
                                     double* cost = nullptr);
bool decideAvailable(const Config& cfg);
std::string judgeStatus(const Config& cfg);  // which judges are live
void judgeShutdown();                        // stop a llama-server this process started

// P(yes) from a llama.cpp /completion body with n_probs (top_logprobs or
// probs shapes); -1 when yes/no are absent. Pure, unit-tested.
double parseYesProbability(const std::string& body);

// Environment + guardrail awareness, frozen into the system prompt at
// session start (cache-stable: nothing here changes mid-session).
std::string awarenessBlock(const std::string& workspace, const Config& cfg, bool allowNet,
                           bool unsafe, bool interactive, int maxRounds);
// Best verification command for the workspace ("" when unknown).
std::string verifyCommand(const std::string& workspace);

}  // namespace pocket
