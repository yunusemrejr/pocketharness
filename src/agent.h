// PocketHarness - the agent loop: terminal -> model -> tools -> model.
// Deliberately boring: one conversation, five tools, summary compaction.
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "common.h"
#include "provider.h"
#include "session.h"
#include "tools.h"

namespace pocket {

// Compaction cut point: index of the first message to KEEP, or msgs.size()
// when compacting now would be wrong (too short, or no user boundary keeps
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

// The conversation driver. Owns message history; ToolEnv drives tools;
// session persistence happens here (one place, always consistent).
struct AgentOpts {
    ResolvedModel model;
    std::string thinking = "off";
    ToolEnv* tools = nullptr;  // not owned
    std::string sessionId;
    std::string tmpDir;
    std::atomic<bool>* cancel = nullptr;
    std::function<void(std::string_view token)> onToken;
    std::function<void(const std::string&)> onNotice;  // compaction, retries, etc.
};

struct AgentStats {
    long inTokens = 0;  // summed when the provider reports usage, else stays 0
    long outTokens = 0;
    int turns = 0;
    int toolCalls = 0;
    int compactions = 0;
    // Measured prompt-cache reuse, exactly as reported (cacheSeen=false: unknown).
    long cacheHit = 0;
    long cacheMiss = 0;
    double cost = 0;
    bool cacheSeen = false;
    bool costSeen = false;
};

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

    void setModel(const ResolvedModel& m, const std::string& thinking) {
        opts_.model = m;
        opts_.thinking = thinking;
    }
    void setCallbacks(std::function<void(std::string_view)> tok,
                      std::function<void(const std::string&)> notice) {
        opts_.onToken = std::move(tok);
        opts_.onNotice = std::move(notice);
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
    std::string requestOnce(std::vector<ToolCall>& callsOut, std::string& textOut);
    std::string maybeCompact();
    void appendSession(const SessionEvent& ev);
    // Load the frozen prefix from the session sidecar, or freeze it now.
    // Empty sessionId (unit tests) skips persistence but still builds once.
    void ensureMeta();

    AgentOpts opts_;
    std::string system_;
    std::string orSessionId_;
    std::vector<ChatMessage> messages_;
    AgentStats stats_;
    std::vector<ToolDef> toolDefs_;
};

}  // namespace pocket
