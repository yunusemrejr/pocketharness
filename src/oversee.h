// PocketHarness - the overseer's senses: a yes/no judge and environment
// awareness. The judge cascades free-and-local to cheap-and-remote:
//   native naive Bayes (brain.h, always) -> local llama.cpp Qwen (started on
//   demand, loopback) -> OpenRouter Jev decisions (reported usage cost).
// Each layer refines, none gates: when all are unavailable, callers keep
// their native heuristic.
#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "config.h"
#include "json.h"

namespace pocket {

// P(yes) that `question` holds for `text`; -1 when no judge answered.
// cost (if non-null) receives the USD spent (0 local, reported for Jev).
// Activity is synchronous on the calling thread, reports engine/status only,
// and distinguishes cached answers from model requests. No prompt data is sent.
double judgeYes(const Config& cfg, const std::string& question, const std::string& text,
                double* cost = nullptr, std::atomic<bool>* cancel = nullptr,
                const std::function<void(const std::string&)>& activity = {});

// Cheap advisory for long tool-only runs. Uses only an already-running local
// model, with a 1.5-second total budget; never starts a server, calls a remote
// model, stops work, or certifies completion. Empty means no reliable advice.
std::string progressHint(const Config& cfg, const std::string& recentTools,
                         std::atomic<bool>* cancel = nullptr,
                         const std::function<void(const std::string&)>& activity = {});

// Batched structured judgment on OpenRouter's decisions API: one call
// answers many yes/no questions with probability scores. Domain calibration
// must be evaluated separately; fixed policy thresholds are not guarantees.
//   transcript=true: Span (respan/span-01) judges a conversation; `state`
//     must be {"input":[{role,content}...],"output":{role:assistant,...}}.
//   transcript=false: Jev (typesafe) judges any content object.
// Each route falls back to the other. Returns id -> P(yes); missing ids
// mean "unknown". Local yes/no inference fills unavailable answers, with a
// bounded total budget. Local transcript probabilities stay in [0.2,0.8]
// (triage signals, not completion/review certification).
// Transcript quality ids premature/ignored/bad require a batched evidence
// sufficiency answer. Missing/uncertain evidence removes those ids (unknown),
// so a consumer must perform its normal review rather than infer approval.
// Cached per process; cost += USD reported even when
// a response only answers part of the batch. Cancellation stops every route.
struct Question {
    std::string id, text;
};
// Explicit policy bands preserve unknown rather than converting it to "no".
// Thresholds may be chosen on separate in-domain calibration data; the defaults
// are conservative routing policy, not calibrated error rates.
enum class DecisionBand { Unknown, Low, High };
DecisionBand decisionBand(double probability, double low = 0.2, double high = 0.8);
// Pure wire helpers used by the decision transport and protocol regressions.
json::Value decisionRequest(const std::string& model, const json::Value& state,
                            const std::vector<Question>& questions);
std::map<std::string, double> decisionAnswers(const json::Value& response,
                                             const std::vector<Question>& questions);
// Remove quality decisions unsupported by the separate evidence score. The
// remaining scores are unchanged. False means normal review is still needed.
bool retainSupportedQualityAnswers(std::map<std::string, double>& answers, double evidence);
std::map<std::string, double> decide(const Config& cfg, const json::Value& state,
                                     const std::vector<Question>& qs, bool transcript,
                                     double* cost = nullptr, std::atomic<bool>* cancel = nullptr,
                                     const std::function<void(const std::string&)>& activity = {});
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
