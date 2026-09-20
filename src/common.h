// PocketHarness - tiny Linux-native coding-agent TUI harness.
// common.h: small shared helpers. No frameworks, just functions.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pocket {

// Version of the harness binary.
inline constexpr const char* kVersion = "0.3.1";

// ---------------------------------------------------------------------------
// Result<T>: minimal error-or-value. Errors are human-readable strings.
// ---------------------------------------------------------------------------
template <typename T>
struct Result {
    bool ok = false;
    T value{};
    std::string error;

    static Result<T> Ok(T v) { return {true, std::move(v), {}}; }
    static Result<T> Err(std::string e) { return {false, T{}, std::move(e)}; }
};

template <>
struct Result<void> {
    bool ok = false;
    std::string error;

    static Result<void> Ok() { return {true, {}}; }
    static Result<void> Err(std::string e) { return {false, std::move(e)}; }
};

using VoidResult = Result<void>;

// ---------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------
std::string trim(std::string_view s);
bool startsWith(std::string_view s, std::string_view prefix);
bool endsWith(std::string_view s, std::string_view suffix);
std::vector<std::string> splitLines(const std::string& s);
std::string join(const std::vector<std::string>& parts, const std::string& sep);
std::string toLower(std::string_view s);

// Rough token estimate for context accounting (~4 chars per token).
inline long estTokens(const std::string& s) { return (long)(s.size() + 3) / 4; }

// Strip dangerous terminal control sequences from untrusted output.
// Removes ESC, CSI sequences, OSC sequences, and C0 controls except \n and \t.
std::string sanitizeTerminal(std::string_view in);

// Expand a leading "~" or "~/" using $HOME.
std::string expandHome(std::string_view path);

// Read an entire file (bounded). Error if larger than maxBytes.
Result<std::string> readFileBounded(const std::string& path, size_t maxBytes);

// Write data atomically: tmp file in same directory + rename + fsync.
VoidResult atomicWriteFile(const std::string& path, const std::string& data, mode_t mode = 0644);

// Append a line + fsync (for JSONL sessions).
VoidResult appendLine(const std::string& path, const std::string& line);

// Ensure a directory exists (mkdir -p). mode applies to created dirs.
VoidResult ensureDir(const std::string& path, mode_t mode = 0755);

// $HOME, never empty (falls back to passwd entry).
std::string homeDir();

// Milliseconds since monotonic epoch.
int64_t nowMs();

// Random hex string (from /dev/urandom, fallback to rand()).
std::string randHex(size_t bytes);

// Basename of a path.
std::string baseName(const std::string& path);

// Standard base64 encoding (RFC 4648, with padding). Used for vision
// image payloads; the harness never needs to decode base64.
std::string base64Encode(std::string_view in);

}  // namespace pocket
