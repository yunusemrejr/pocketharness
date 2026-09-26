// PocketHarness - boring append-only JSONL sessions.
#pragma once

#include <atomic>
#include <map>
#include <string>
#include <vector>

#include "common.h"
#include "json.h"

namespace pocket {

struct SessionEvent {
    std::string type;  // user|assistant|tool_call|tool_result|system|compact|image
    std::string text;
    std::string toolId;
    std::string toolName;
    std::string toolArgs;
    bool toolOk = true;
    // "image" events only: persisted attachment (0600, under the session
    // dir) reloaded on --resume. The JSONL line stays small; the bytes
    // live in the file, never inline.
    std::string imgFile{};
    std::string imgMime{};
    json::Value replay{};  // provider continuation data on assistant events
};

struct SessionInfo {
    std::string id;
    std::string path;
    std::string firstLine;  // short summary for --sessions
    std::string workspace;
    bool active = false;
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
std::vector<SessionInfo> sessionList(size_t max = 30, const std::string& workspace = "");

// Resolve "" or "last" to the newest session id, or validate a given id.
Result<std::string> sessionResolve(const std::string& idOrEmpty, const std::string& workspace = "");

// One writer per session. CLOEXEC lock survives crashes without stale PID files.
// Caller owns the returned fd until session exit; no lock file is unlinked.
Result<int> sessionLock(const std::string& id);

// A separate advisory lock for short workspace mutations (write/edit/undo).
// Caller closes the fd. Do not hold across bash/subprocess execution: a child
// may itself run Pocket. Waiting is interruptible through cancel.
Result<int> sessionWorkspaceLock(const std::string& workspace,
                                 std::atomic<bool>* cancel = nullptr);

// Recursive children may share only this coordination directory while keeping
// their HOME, transcripts and credentials isolated. Set during startup only,
// before threads start; the directory must already exist and be private/owned.
std::string sessionWorkspaceCoordinationDir();
VoidResult sessionSetWorkspaceCoordinationDir(const std::string& path);
// Create/return the scope to grant a workspace's model tools and descendants.
// Never grant the unscoped default coordination root to model tools.
Result<std::string> sessionWorkspaceDirectory(const std::string& workspace);

// Bounded notices between sessions in the same canonical workspace. These are
// coordination data, never another session's conversation or instructions.
struct WorkspaceEvent {
    long sequence = 0;
    std::string sessionId, type, text;
};
struct WorkspaceUpdates {
    long lastSequence = 0;
    bool missed = false;  // cursor predates retained notices; reread affected files
    std::vector<WorkspaceEvent> events;
};
VoidResult sessionWorkspacePublish(const std::string& workspace, const std::string& sessionId,
                                    const std::string& type, const std::string& text);
Result<WorkspaceUpdates> sessionWorkspaceRead(const std::string& workspace, long after = 0,
                                              const std::string& ownSession = "");

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
    std::string workspace;
    // Cumulative counters (restored on resume so /session tells the truth).
    long turns = 0, toolCalls = 0, compactions = 0;
    long inTokens = 0, outTokens = 0, cacheHit = 0, cacheMiss = 0, genMs = 0;
    long lastPrompt = -1;
    double cost = 0;
    bool cacheSeen = false, costSeen = false;
    double sideCost = 0;
    bool costEstimated = false;
    bool costIncomplete = false;
    long genTokens = 0;
    long childSessions = 0;
    std::map<std::string, std::string> roles;
    bool rolesSet = false;
};
Result<SessionMeta> sessionLoadMeta(const std::string& id);  // missing => Ok(empty)
VoidResult sessionSaveMeta(const std::string& id, const SessionMeta& m);

}  // namespace pocket
