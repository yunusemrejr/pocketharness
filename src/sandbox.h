// PocketHarness - sandbox: openat2 containment, Landlock, seccomp-net,
// NO_NEW_PRIVS, env sanitization, destructive-command guard.
//
// Contracts:
//  - Native file tools NEVER trust model paths; every open goes through
//    containedOpen() anchored at an already-open root fd.
//  - Model bash children get: Landlock fs rules + NO_NEW_PRIVS + sanitized
//    env + seccomp inet-socket denial (unless --network) + guard screening.
//  - Failures of a mechanism degrade to a loud warning + recorded cap flag,
//    never to silent insecurity (except Landlock/seccomp on kernels without
//    the feature, where the flag and /security show the gap honestly).
#pragma once

#include <string>
#include <vector>

#include "common.h"

namespace pocket {

struct SandboxCaps {
    bool openat2 = false;
    bool landlock = false;
    bool seccompNet = false;
    bool userNs = false;  // informational only; not required
};

// Probed once per process (forks test children for landlock/seccomp/userns).
const SandboxCaps& sandboxCaps();

// Extra roots granted via --allow-read/--allow-write or user config.
struct Authority {
    std::string workspace;                 // canonical absolute path
    std::vector<std::string> readRoots;    // canonical, workspace first
    std::vector<std::string> writeRoots;   // canonical, workspace first
    std::vector<int> readFds;              // O_PATH fds, parallel to readRoots
    std::vector<int> writeFds;             // O_PATH fds, parallel to writeRoots
    bool unsafe = false;                   // escape hatch: containment off
};

Result<Authority> authorityInit(const std::string& workspace,
                                const std::vector<std::string>& allowRead,
                                const std::vector<std::string>& allowWrite,
                                bool unsafe);
void authorityClose(Authority& a);

// Contained file operations for the native tools.
Result<std::string> boxRead(const Authority& a, const std::string& path, size_t maxBytes);
// Atomic write (tmp + rename). Creates parent dirs inside the owning root.
VoidResult boxWrite(const Authority& a, const std::string& path,
                    const std::string& data, mode_t mode = 0644);
Result<bool> boxExists(const Authority& a, const std::string& path);

// Child process confinement (called in the child after fork, before exec).
// Writes diagnostics to stderr and _exit()s on fatal failure.
struct ChildSpec {
    const Authority* auth = nullptr;
    std::string workspace;
    std::string sessionTmp;    // RW scratch (also contains the fake HOME)
    std::string stateDirPath;  // RW (own sessions/state)
    bool allowNet = false;
    bool unsafe = false;
    bool providerCurl = false;  // trusted harness networking: skip confinement
};
void childEnterSandbox(const ChildSpec& spec);

// Build the sanitized child environment (complete "K=V" list).
std::vector<std::string> buildChildEnv(const std::vector<std::string>& exposeEnv,
                                       const std::string& workspace,
                                       const std::string& tmpdir, const std::string& home,
                                       const std::string& keyfile, const std::string& sessionId,
                                       int depth, bool parentNet, bool parentUnsafe);

// True when an env name looks credential-like (defense in depth; the
// allowlist already drops everything not explicitly permitted).
bool looksSecretEnv(const std::string& name);

// Destructive-command guard: cheap last-resort screening of model commands.
enum class Verdict { Allow, Ask, Deny };
struct GuardResult {
    Verdict verdict = Verdict::Allow;
    std::string reason;
};
GuardResult classifyCommand(const std::string& cmd, const std::string& workspace, bool allowNet);

// Depth guard for recursive pocket instances.
inline constexpr int kMaxPocketDepth = 5;

}  // namespace pocket
