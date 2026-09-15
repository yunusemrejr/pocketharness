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

// Visible terminal columns of a string (ANSI zero-width, tabs = tab stops).
// Exposed for tests.
size_t visibleWidth(const std::string& s);

// Compact counts for the status line: 999, 1k, 12.4k, 200k, 1.5M.
// Exposed for tests.
std::string fmtK(long n);

// Picker matching: indices of labels containing filter (case-insensitive;
// empty filter matches all). Exposed for tests.
std::vector<size_t> pickFilter(const std::vector<std::string>& labels,
                               const std::string& filter);

// Byte-truncate without splitting a UTF-8 sequence. Exposed for tests.
std::string cutBytes(const std::string& s, size_t maxB);

}  // namespace pocket
