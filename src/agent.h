// PocketHarness - the agent loop: terminal -> model -> tools -> model.
// Deliberately boring: one conversation, five tools, summary compaction.
#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "common.h"
#include "oversee.h"
#include "provider.h"
#include "session.h"
#include "tools.h"

namespace pocket {

// Compaction cut point: index of the first message to KEEP, or msgs.size()
// when compacting now would be wrong (too short, or no message boundary keeps
// tool_use/tool_result pairing intact). Pure and unit-tested.
size_t compactCutPoint(const std::vector<ChatMessage>& msgs, size_t keepLast = 8);

// History invariant: every tool message must answer a tool_call id issued by
// a PRECEDING assistant message. Returns "" when valid, else a description.
// Checked before every provider request so a corrupt history fails locally
// with a clear message instead of a provider 400.
std::string validateHistory(const std::vector<ChatMessage>& msgs);

// Small system prompt + project instructions (AGENTS.md / POCKET.md).
// Pure enough to unit test: takes workspace, returns prompt text.
// The base prompt is file-overridable: .pocket/system.md wins, then
// ~/.config/pocketharness/system.md, then the minimal built-in below.
std::string buildSystemPrompt(const std::string& workspace, const std::string& awareness = "");
// Which base prompt is active: "" = built-in, else the override file path.
std::string systemPromptSource(const std::string& workspace);

// Cap each result identically forever: aging must never rewrite a cached prefix.
inline constexpr size_t kWireToolCap = 12000;
std::vector<ChatMessage> trimWireHistory(const std::vector<ChatMessage>& msgs);

// Vision attachments. Pasted/dropped image paths are detected in TUI input
// (or given via --image), staged under the session dir + session tmp, and
// sent inline with the next user message as OpenAI image_url / Anthropic
// image blocks. Providers decide vision support; an attached image is
// explicit user intent, so it is always sent.
inline constexpr size_t kMaxImageBytes = 5 << 20;  // per image, raw file bytes
inline constexpr size_t kMaxImagesPerMessage = 4;
inline constexpr long kImageEstTokens = 2048;  // context estimate per image
// MIME from magic bytes (PNG/JPEG/GIF/WebP), or "" when not an image.
std::string sniffImageMime(std::string_view bytes);
// Read + validate + base64 one image file. Error text on failure.
Result<ChatImage> loadImageFile(const std::string& path);
// Quote-aware path tokens from free text that resolve to existing image
// files (token = original spelling for input rewriting, path = resolved).
struct ImageToken {
    std::string token;
    std::string path;
};
std::vector<ImageToken> collectImageTokens(const std::string& text,
                                           const std::string& workspace);

// The conversation driver. Owns message history; ToolEnv drives tools;
// session persistence happens here (one place, always consistent).
struct AgentOpts {
    ResolvedModel model;
    std::string thinking = "off";
    long maxTokens = 0;        // 0 = model config; also the compaction reserve
    int maxRounds = 100;       // model<->tool rounds per turn before stopping
    ToolEnv* tools = nullptr;  // not owned
    std::string sessionId;
    std::string parentUsageDir;  // recursive process reports cumulative spend to its parent scratch
    std::atomic<bool>* cancel = nullptr;
    std::function<void(std::string_view token)> onToken;
    std::function<void(std::string_view chunk)> onReasoning;  // live thinking preview
    std::function<void(const std::string&)> onNotice;  // compaction, retries, etc.
    std::function<Result<ChatResponse>(const ChatRequest&, const ChatCallbacks&)> request = chatRequest;
    // --- overseer (all optional; unset = plain agent loop) ---
    std::string awareness;           // frozen environment/guardrail block
    std::vector<ResolvedModel> fast;       // role "fast": summaries, goal audits (0..1)
    std::vector<ResolvedModel> fallback;   // role "fallback": provider failure (0..1)
    std::vector<ResolvedModel> reviewers;  // the council (majority decides)
    bool autonomy = false;           // nudge early stops / permission asks
    bool review = false;             // council reviews changed work
    std::vector<std::string> stopHooks;
    // P(yes) judge (local LM / Jev); -1 unavailable. cost += USD spent.
    std::function<double(const std::string& q, const std::string& text, double* cost)> judge;
    // Batched Span/Jev decisions (see oversee.h); empty map = unavailable.
    std::function<std::map<std::string, double>(const json::Value& state, const std::vector<Question>& qs,
                                                 bool transcript, double* cost)> decide;
    // Skill hint for a new user message ("" = none).
    std::function<std::string(const std::string& userText, double* cost)> hint;
    bool brief = false;              // expert brief before substantial requests
    long workingContextTokens = 96000;  // soft checkpoint independent of the model window; 0 disables
    // Cheap advisory check of a bounded recent tool trace. Never a completion
    // verdict or permission gate; empty means no change of direction suggested.
    std::function<std::string(const std::string& recentTools, double* cost)> progress;
};

// True for requests that deserve an expert brief (imperative work, not a
// quick question). Pure, unit-tested.
bool needsBrief(const std::string& userText);

struct AgentStats {
    long inTokens = 0;  // summed when the provider reports usage, else stays 0
    long outTokens = 0;
    long lastPrompt = -1;  // exact prompt tokens of the latest request (-1 unknown)
    long genMs = 0;  // provider wall-time of successful requests (no tool time)
    long genTokens = 0;  // main model only, matching genMs
    int turns = 0;
    int toolCalls = 0;
    int compactions = 0;
    // Measured prompt-cache reuse, exactly as reported (cacheSeen=false: unknown).
    long cacheHit = 0;
    long cacheMiss = 0;
    double cost = 0;
    bool cacheSeen = false;
    bool costSeen = false;
    // Rolling window over the last kCacheWindow requests that reported
    // cache usage: the steady-state hit rate once the prefix is warm.
    // In-memory only (not persisted); cumulative counters stay in the sidecar.
    std::deque<std::pair<long, long>> cacheWindow;
    long recentHit = 0;
    long recentMiss = 0;
    // Overseer accounting: side cost = judges, reviews, audits, summaries.
    double sideCost = 0;
    bool costEstimated = false;  // part of `cost` came from catalog prices
    bool costIncomplete = false; // some metered attempts could not be priced
    int nudges = 0, reviews = 0, fallbacks = 0, deduped = 0;
    long childSessions = 0;
};

inline constexpr size_t kCacheWindow = 20;
// Record one request's reported (hit, miss) pair; negative = unreported.
void noteCacheSample(AgentStats& st, long hit, long miss);

enum class GoalStatus { None, Active, Paused, Completed };

class Agent {
  public:
    explicit Agent(AgentOpts opts);

    // Restore history from a session file (resume).
    VoidResult restore(const std::string& sessionId);
    std::string flushUsage();  // idle handoff/exit: collect child receipts before scratch is removed

    // Run one user turn to completion (may involve many model<->tool rounds).
    // Returns error text, or "" on success. Partial work is already in the
    // session either way.
    std::string runTurn(const std::string& userText);

    // Force compaction now (used by /compact). Error text or "".
    std::string compactNow();

    // Goal mode: work, then audit, preserving progress on cancellation/error.
    // These methods and getters belong to the worker, or the idle UI after
    // joining it. While running, the UI may only change the cancel atomic.
    std::string runGoal(const std::string& goal, int maxCycles = 12);
    std::string resumeGoal(const std::string& followup = "", int maxCycles = 12);
    const std::string& goal() const { return goal_; }
    GoalStatus goalStatus() const { return goalStatus_; }
    bool goalPaused() const { return goalStatus_ == GoalStatus::Paused; }
    std::string pauseGoal();
    std::string clearGoal();
    // Called only at completed-turn boundaries. The UI callback must read
    // synchronized queue state and must not access this Agent concurrently.
    void setGoalYield(std::function<bool()> fn) { goalYield_ = std::move(fn); }

    // Queue an image file for the next user message ("", or error text).
    // Persists bytes under the session dir (resume-safe) plus a working
    // copy in the session tmp dir (visible to model tools).
    std::string attachImage(const std::string& path);
    size_t pendingImages() const { return pendingImages_.size(); }

    void setModel(const ResolvedModel& m, const std::string& thinking) {
        opts_.model = m;
        opts_.thinking = thinking;
        stats_.lastPrompt = -1;
        lastEstimate_ = 0;
    }
    void setCallbacks(std::function<void(std::string_view)> tok,
                      std::function<void(const std::string&)> notice,
                      std::function<void(std::string_view)> reasoning = {}) {
        opts_.onToken = std::move(tok);
        opts_.onNotice = std::move(notice);
        opts_.onReasoning = std::move(reasoning);
    }
    void setCancel(std::atomic<bool>* c) { opts_.cancel = c; }
    void setRole(const std::string& role, const std::vector<ResolvedModel>& v) {
        if (role == "fast") opts_.fast = v;
        else if (role == "fallback") opts_.fallback = v;
        else if (role == "review") opts_.reviewers = v;
    }
    long contextUsed() const;  // estimated tokens in the next request
    long contextMax() const { return opts_.model.context; }
    const AgentStats& stats() const { return stats_; }
    size_t messageCount() const { return messages_.size(); }
    const std::vector<ChatMessage>& messages() const { return messages_; }
    // The frozen request prefix and sticky routing tag for this session.
    const std::string& systemPrompt() const { return system_; }
    const std::string& sessionTag() const { return orSessionId_; }

  private:
    std::string runTurnImpl(const std::string& userText, bool continuation = false);
    void recordOutcome(const std::string& scope, const std::string& reason, const std::string& detail);
    Result<ChatResponse> requestOnce();
    Result<ChatResponse> sideRequest(const ResolvedModel& m, const std::string& system,
                                     const std::string& user, long maxTokens);
    std::string stopGate(const std::string& finalText);
    std::string councilReview();
    std::string makeBrief(const std::string& request);
    std::string turnDigest(size_t maxBytes) const;
    json::Value turnTranscript(const std::string& finalText) const;
    std::map<std::string, double> ask(const json::Value& state, const std::vector<Question>& qs, bool transcript);
    std::string distill(const std::string& output);
    void pushUser(const std::string& text);
    std::string maybeCompact();
    long completionBudget() const;
    long estimateContext() const;
    void recordResponse(const ChatResponse& response, long elapsedMs, const ResolvedModel* m = nullptr,
                        bool side = false);
    void appendSession(const SessionEvent& ev);
    void saveStats();
    void readWorkspaceUpdates();
    // Load the frozen prefix from the session sidecar, or freeze it now.
    // Empty sessionId (unit tests) skips persistence but still builds once.
    void ensureMeta();
    std::string continueGoal(int maxCycles);
    std::string finishGoal(std::string error, bool completed = false, const std::string& reason = "");

    AgentOpts opts_;
    std::string system_;
    std::string orSessionId_;
    std::vector<ChatMessage> messages_;
    std::vector<ChatImage> pendingImages_;
    AgentStats stats_;
    std::vector<ToolDef> toolDefs_;
    long lastEstimate_ = 0;
    std::string persistenceError_;
    std::string goal_;
    GoalStatus goalStatus_ = GoalStatus::None;
    std::string goalPhase_, goalBrief_, goalNext_, goalProgress_;
    std::function<bool()> goalYield_;
    bool goalYielded_ = false;
    std::string turnStopReason_;
    std::string lastStopReason_, lastStopDetail_;
    int64_t lastStoppedAtMs_ = 0;
    long compactAttemptTokens_ = -1;
    int64_t compactRetryAfterMs_ = 0;
    std::string originalRequest_, latestRequest_;
    bool turnMadeProgress_ = false;
    std::deque<size_t> goalObservations_;  // bounded fingerprints; never a completion verdict
    long outputBoost_ = 1;  // doubled when replies hit the output cap (session-wide)
    long workspaceSequence_ = 0;
    // Per-turn overseer state.
    size_t turnStart_ = 0;
    int turnNudges_ = 0, turnGates_ = 0;
    bool unverified_ = false, verifyNudged_ = false, reviewed_ = false, calmNext_ = false;
    std::vector<std::string> hookNagged_;
};

}  // namespace pocket
