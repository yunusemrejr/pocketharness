// PocketHarness - boring append-only JSONL sessions.
#pragma once

#include <string>
#include <vector>

#include "common.h"
#include "json.h"

namespace pocket {

struct SessionEvent {
    std::string type;  // user|assistant|tool_call|tool_result|system|compact
    std::string text;
    std::string toolId;
    std::string toolName;
    std::string toolArgs;
    bool toolOk = true;
};

struct SessionInfo {
    std::string id;
    std::string path;
    std::string firstLine;  // short summary for --sessions
    long events = 0;
};

// Create a new session file, return its id.
Result<std::string> sessionCreate();

// Append one event (fsync'd). A crash can only leave a partial final line,
// which sessionLoad detects and ignores.
VoidResult sessionAppend(const std::string& id, const SessionEvent& ev);

// Load all complete events. ok=false + error only on unreadable file;
// malformed lines are skipped with a count.
struct SessionLoad {
    std::vector<SessionEvent> events;
    long skipped = 0;
};
Result<SessionLoad> sessionLoad(const std::string& id);

// Newest-first list of sessions (bounded).
std::vector<SessionInfo> sessionList(size_t max = 30);

// Resolve "" or "last" to the newest session id, or validate a given id.
Result<std::string> sessionResolve(const std::string& idOrEmpty);

json::Value sessionEventToJson(const SessionEvent& ev);
SessionEvent sessionEventFromJson(const json::Value& v);

// Small sidecar (<id>.meta.json): data that must stay STABLE for the life of
// a session so provider prompt caches keep hitting. Written once at session
// start, reused verbatim on --resume.
struct SessionMeta {
    std::string systemPrompt;  // frozen request prefix (system + instructions)
    std::string orSessionId;   // stable OpenRouter sticky-routing id
    std::string modelSpec;     // model active at creation (restored on resume)
    std::string systemSource;  // "" = built-in base prompt, else override path
    std::string thinking;  // this session's level ("" = config default)
    // Cumulative counters (restored on resume so /session tells the truth).
    long turns = 0, toolCalls = 0, compactions = 0;
    long inTokens = 0, outTokens = 0, cacheHit = 0, cacheMiss = 0, genMs = 0;
    long lastPrompt = -1;
    double cost = 0;
    bool cacheSeen = false, costSeen = false;
};
Result<SessionMeta> sessionLoadMeta(const std::string& id);  // missing => Ok(empty)
VoidResult sessionSaveMeta(const std::string& id, const SessionMeta& m);

}  // namespace pocket
