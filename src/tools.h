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
};

struct ToolEnv {
    const Authority* auth = nullptr;
    const Config* cfg = nullptr;
    std::string workspace;
    std::string sessionTmp;
    std::string sandboxHome;
    bool allowNet = false;
    bool unsafe = false;
    bool interactive = false;
    bool allowDestructive = false;  // non-interactive explicit override only
    // Interactive approval for guard-flagged commands. Must be human-driven.
    std::function<bool(const std::string& cmd, const std::string& reason)> askApproval;
    std::atomic<bool>* cancel = nullptr;
    std::function<void(const std::string& line)> onEvent;
    std::function<void(const std::string& name, bool ok, const std::string& summary)> onToolDone;
    std::vector<UndoEntry> undo;              // newest last, max kMaxUndo
    std::vector<std::string> changedFiles;    // paths written this turn (agent resets)
    long bashRuns = 0;                        // successful+failed bash calls (verification signal)
    double sideCost = 0;                      // USD spent by tool-side judges (agent collects)
};

inline constexpr size_t kMaxUndo = 64;

struct ToolResult {
    bool ok = false;
    std::string output;  // bounded, human/model-readable
};

// Schemas advertised to the provider (also used by /help and tests).
std::vector<ToolDef> nativeToolDefs();

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

}  // namespace pocket
