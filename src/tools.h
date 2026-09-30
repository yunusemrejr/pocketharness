// PocketHarness - the five native model tools: read/write/edit/bash/skill.
// There must be no sixth tool without an entry in the README justifying
// why Linux itself could not supply it.
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "common.h"
#include "config.h"
#include "provider.h"
#include "sandbox.h"

namespace pocket {

// Pre-image of a file changed by write/edit, for /undo (in memory, bounded).
struct UndoEntry {
    std::string path;
    bool existed = false;
    std::string content;
    std::string written;  // refuse undo after another writer changes the file
};

struct ChildUsage {
    double cost = 0, sideCost = 0;
    long count = 0;
    bool estimated = false, seen = false, incomplete = false;
};

struct ToolEnv {
    const Authority* auth = nullptr;
    const Config* cfg = nullptr;
    std::string workspace;
    std::string sessionTmp;
    std::string sandboxHome;
    std::string sessionId;
    int depth = 0;
    bool allowNet = false;
    bool unsafe = false;
    bool interactive = false;
    bool allowDestructive = false;  // non-interactive explicit override only
    bool readOnly = false;  // /double evidence passes: read + proven-read-only bash only
    // Interactive approval for guard-flagged commands. Must be human-driven.
    std::function<bool(const std::string& cmd, const std::string& reason)> askApproval;
    std::atomic<bool>* cancel = nullptr;
    std::function<void(const std::string& line)> onEvent;
    std::function<void(const std::string& name, bool ok, const std::string& summary)> onToolDone;
    std::vector<UndoEntry> undo;              // newest last, max kMaxUndo
    std::vector<std::string> changedFiles;    // paths written this turn (agent resets)
    std::string taskIntent;                   // current user request, for tool-side assessment
    std::map<std::string, std::vector<std::string>> qualitySeen;  // current findings per file
    struct SemanticCheck { int calls = 0; };
    std::map<std::string, SemanticCheck> semanticChecks;  // material revisions only, bounded per turn
    long bashRuns = 0;                        // successful+failed bash calls (verification signal)
    bool uiDocLoaded = false;                 // ai-design-slop read this session (UI work gate)
    bool videoDocLoaded = false;              // video-studio read this session (video work gate)
    bool videoNagged = false;                 // the first kit video render was already stopped once
    std::vector<std::string> loadedSkills;    // successfully loaded this session; suppress duplicate hints
    double sideCost = 0;                      // USD spent by tool-side judges (agent collects)
    std::vector<ChatImage> viewImages;        // images `read` this batch (agent attaches them)
    std::map<std::string, ChildUsage> childUsage;
};

inline constexpr size_t kMaxUndo = 64;
// Bundled UI-design doctrine: agents must read it before UI/UX/GUI work.
inline constexpr const char* kUiDocSkill = "ai-design-slop";
// Offline fallback for the Jev "is this UI work?" verdict.
bool looksLikeUiWork(const std::string& text);
// Bundled video doctrine: publish-grade motion video pipeline, art direction, audio and QA gate.
inline constexpr const char* kVideoDocSkill = "video-studio";
bool looksLikeVideoWork(const std::string& text);

struct ToolResult {
    bool ok = false;
    std::string output;  // bounded, human/model-readable
    bool changed = false;  // a successful write/edit actually changed file contents
};

// Schemas advertised to the provider (also used by /help and tests).
std::vector<ToolDef> nativeToolDefs();

// Schemas offered to /double first passes: read plus a read-only bash.
// Not a sixth tool: the same two tools, advertised with their evidence
// limits and enforced by ToolEnv::readOnly inside runTool.
std::vector<ToolDef> readOnlyToolDefs();

// Dispatch one tool call. argsJson must be a JSON object (tolerates "").
ToolResult runTool(ToolEnv& env, const std::string& name, const std::string& argsJson);

// Run a configured hook command in the bash-tool sandbox. Hooks are user
// authored: no approval prompt, same confinement. {file}/{cmd} expand to
// shell-quoted values.
ToolResult runHook(ToolEnv& env, std::string cmd, const std::string& var = "",
                   const std::string& value = "");
std::string shellQuote(const std::string& s);

// Restore the newest undo entry. Returns a human summary or error text.
std::string undoLast(ToolEnv& env);
// Cumulative child receipts in this session's scratch; returns new usage only.
ChildUsage collectChildUsage(ToolEnv& env);

}  // namespace pocket
