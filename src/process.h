// PocketHarness - subprocess primitives: fork/exec, capture, timeout, cancel.
// All subprocess construction is argv-based; no shell string concatenation.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "common.h"

namespace pocket {

struct SpawnOpts {
    std::string exe;                       // executable path (execvp lookup if no '/')
    std::vector<std::string> argv;          // argv[0..]; argv[0] usually = exe basename
    // Complete "K=V" environment. Empty = inherit the parent environment.
    std::vector<std::string> env;
    std::string workdir;                   // "" = inherit
    std::string stdinData;                 // fed to child stdin, then EOF
    long timeoutMs = 0;                    // 0 = no timeout
    size_t outLimit = 1 << 20;             // per-stream capture cap
    bool stopOnLimit = false;             // provider streams: reject oversized responses
    std::function<void()> childSetup;      // runs in child before exec (async-safe only)
    std::function<void(std::string_view, bool isErr)> onChunk;  // capped at outLimit per stream
    std::atomic<bool>* cancel = nullptr;   // set true to kill child
    long terminateGraceMs = 750;           // TERM cleanup before KILL; bounded to 0..5000
    // After the leader exits, wait at most this long for inherited pipes to
    // close (background children keep them open). -1 = wait for EOF.
    long lingerMs = -1;
    // Disk guard: stop the tree when the command has consumed more than
    // diskBudgetBytes of free space on diskGuardPath's filesystem, or keeps
    // writing once free space is below diskReserveBytes. 0 = off.
    std::string diskGuardPath;
    uint64_t diskBudgetBytes = 0;
    uint64_t diskReserveBytes = 0;
};

struct SpawnResult {
    bool ok = false;        // process ran to completion (any exit code)
    int exitCode = -1;      // valid when ok && !timedOut && !cancelled && termSig==0
    int termSig = 0;        // killing signal, if any
    bool timedOut = false;
    bool cancelled = false;
    bool truncated = false;  // output hit outLimit
    bool detached = false;   // leader exited; background children still held the pipes
    std::string out;
    std::string err;
    std::string error;  // spawn/wait failure description
    std::string diskGuard;  // non-empty: the disk guard stopped the command (why)
};

// Run a child synchronously. Never invokes a shell.
SpawnResult spawn(const SpawnOpts& opts);
// Stop every process group this session spawned that is still alive (tool
// background servers included): TERM, up to graceMs, then KILL. Uses only
// async-signal-safe calls, so fatal-signal handlers may call it.
void killSessionProcesses(long graceMs = 1000);
// Session-wide backstop for background groups that outlive their tool call:
// while any tracked group lives, poll `paths` and stop every session group
// when free space is below reserveBytes and still falling fast. Idempotent;
// later calls replace the paths/reserve. reserveBytes 0 disables it.
void startDiskWatchdog(std::vector<std::string> paths, uint64_t reserveBytes);
// Free bytes available to unprivileged writers on path's filesystem, or -1.
int64_t diskAvail(const std::string& path);

// Resolve PATH in the child's environment and working directory, before fork.
std::string whichExe(const std::string& name, const std::string& workdir = "",
                     const std::vector<std::string>& env = {});

}  // namespace pocket
