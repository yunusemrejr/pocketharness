// PocketHarness - subprocess primitives: fork/exec, capture, timeout, cancel.
// All subprocess construction is argv-based; no shell string concatenation.
#pragma once

#include <atomic>
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
};

struct SpawnResult {
    bool ok = false;        // process ran to completion (any exit code)
    int exitCode = -1;      // valid when ok && !timedOut && !cancelled && termSig==0
    int termSig = 0;        // killing signal, if any
    bool timedOut = false;
    bool cancelled = false;
    bool truncated = false;  // output hit outLimit
    std::string out;
    std::string err;
    std::string error;  // spawn/wait failure description
};

// Run a child synchronously. Never invokes a shell.
SpawnResult spawn(const SpawnOpts& opts);

// Resolve PATH in the child's environment and working directory, before fork.
std::string whichExe(const std::string& name, const std::string& workdir = "",
                     const std::vector<std::string>& env = {});

}  // namespace pocket
