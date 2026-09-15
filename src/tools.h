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

struct ToolEnv {
    const Authority* auth = nullptr;
    const Config* cfg = nullptr;
    std::string workspace;
    std::string sessionTmp;
    std::string sandboxHome;
    std::string sessionId;
    std::string keyfile;
    int depth = 0;
    bool allowNet = false;
    bool unsafe = false;
    bool interactive = false;
    bool allowDestructive = false;  // non-interactive explicit override only
    // Interactive approval for guard-flagged commands. Must be human-driven.
    std::function<bool(const std::string& cmd, const std::string& reason)> askApproval;
    std::atomic<bool>* cancel = nullptr;
    std::function<void(const std::string& line)> onEvent;
    std::function<void(const std::string& name, bool ok, const std::string& summary)> onToolDone;
};

struct ToolResult {
    bool ok = false;
    std::string output;  // bounded, human/model-readable
};

// Schemas advertised to the provider (also used by /help and tests).
std::vector<ToolDef> nativeToolDefs();

// Dispatch one tool call. argsJson must be a JSON object (tolerates "").
ToolResult runTool(ToolEnv& env, const std::string& name, const std::string& argsJson);

}  // namespace pocket
