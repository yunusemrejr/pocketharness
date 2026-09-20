// PocketHarness - the agent loop: terminal -> model -> tools -> model.
// Deliberately boring: one conversation, five tools, summary compaction.
#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "common.h"
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
std::string buildSystemPrompt(const std::string& workspace);
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
    std::atomic<bool>* cancel = nullptr;
    std::function<void(std::string_view token)> onToken;
    std::function<void(std::string_view chunk)> onReasoning;  // live thinking preview
    std::function<void(const std::string&)> onNotice;  // compaction, retries, etc.
    std::function<Result<ChatResponse>(const ChatRequest&, const ChatCallbacks&)> request = chatRequest;
};

struct AgentStats {
    long inTokens = 0;  // summed when the provider reports usage, else stays 0
    long outTokens = 0;
    long lastPrompt = -1;  // exact prompt tokens of the latest request (-1 unknown)
    long genMs = 0;  // provider wall-time of successful requests (no tool time)
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
};

inline constexpr size_t kCacheWindow = 20;
// Record one request's reported (hit, miss) pair; negative = unreported.
void noteCacheSample(AgentStats& st, long hit, long miss);

class Agent {
  public:
    explicit Agent(AgentOpts opts);

    // Restore history from a session file (resume).
    VoidResult restore(const std::string& sessionId);

    // Run one user turn to completion (may involve many model<->tool rounds).
    // Returns error text, or "" on success. Partial work is already in the
    // session either way.
    std::string runTurn(const std::string& userText);

    // Force compaction now (used by /compact). Error text or "".
    std::string compactNow();

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
    long contextUsed() const;  // estimated tokens in the next request
    long contextMax() const { return opts_.model.context; }
    const AgentStats& stats() const { return stats_; }
    size_t messageCount() const { return messages_.size(); }
    const std::vector<ChatMessage>& messages() const { return messages_; }
    // The frozen request prefix and sticky routing tag for this session.
    const std::string& systemPrompt() const { return system_; }
    const std::string& sessionTag() const { return orSessionId_; }

  private:
    Result<ChatResponse> requestOnce();
    std::string maybeCompact();
    long completionBudget() const;
    long estimateContext() const;
    void recordResponse(const ChatResponse& response, long elapsedMs);
    void appendSession(const SessionEvent& ev);
    void saveStats();
    // Load the frozen prefix from the session sidecar, or freeze it now.
    // Empty sessionId (unit tests) skips persistence but still builds once.
    void ensureMeta();

    AgentOpts opts_;
    std::string system_;
    std::string orSessionId_;
    std::vector<ChatMessage> messages_;
    std::vector<ChatImage> pendingImages_;
    AgentStats stats_;
    std::vector<ToolDef> toolDefs_;
    long lastEstimate_ = 0;
    std::string persistenceError_;
};

}  // namespace pocket
