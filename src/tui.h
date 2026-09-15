// PocketHarness - native Linux TUI: termios + ANSI, no frameworks.
// Scrollback-based chat rendering (no alternate screen, no dashboard clutter)
// with a readline-like multiline input, streaming Markdown-ish output, and
// prompt-embedded status (model, thinking, context %).
#pragma once

#include <string>

#include "agent.h"
#include "common.h"
#include "config.h"

namespace pocket {

struct TuiOpts {
    Agent* agent = nullptr;      // not owned
    ToolEnv* tools = nullptr;    // not owned
    Config* cfg = nullptr;       // not owned
    ResolvedModel model;
    std::string thinking = "off";
    std::string workspace;
    std::string sessionId;
    std::string systemSource;  // "" = built-in base prompt
    bool unsafe = false;
    bool allowNet = false;
};

// Full interactive session. Returns process exit code.
int tuiRun(TuiOpts& opts);

// Plain line-based fallback when stdin/stdout are not TTYs.
int lineRun(TuiOpts& opts);

// Human-driven destructive-command approval. Returns true = allow once.
bool askApprovalCli(const std::string& cmd, const std::string& reason);

// Cheap Markdown-ish line renderer (headers, fences, bullets, `code`).
// Exposed for tests.
std::string renderLine(const std::string& line, bool& inFence);

}  // namespace pocket
