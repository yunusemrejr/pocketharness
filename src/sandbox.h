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
// Grant one more read root after init (the session tmp dir, created after
// the authority). Dedupes; unsafe mode skips fds exactly like init.
VoidResult authorityAddReadRoot(Authority& a, const std::string& path);
VoidResult authorityAddWriteRoot(Authority& a, const std::string& path);

// Versioned Node/Python installations on the user's PATH or behind a workspace
// venv (.venv/bin/python -> ~/.local/share/uv/...). Grants only the
// selected runtime tree, never the containing home or manager configuration.
std::vector<std::string> discoverRuntimeRoots(const char* path, const std::string& home,
                                              const std::string& workspace = "");

// Contained file operations for the native tools.
Result<int> boxOpenRead(const Authority& a, const std::string& path);  // owned fd, regular files only
Result<std::string> boxRead(const Authority& a, const std::string& path, size_t maxBytes);
// Atomic write (tmp + rename). Creates parent dirs inside the owning root.
// Preserves the owner-execute bit when overwriting an executable file.
// With changed, an identical authorized regular file is left untouched; the
// flag records actual replacement. Write authority and symlink checks still apply.
VoidResult boxWrite(const Authority& a, const std::string& path,
                    const std::string& data, mode_t mode = 0644, bool* changed = nullptr);
Result<bool> boxExists(const Authority& a, const std::string& path);
// Remove a regular file using the same anchored, no-symlink write authority.
VoidResult boxRemove(const Authority& a, const std::string& path);

// Child process confinement (called in the child after fork, before exec).
// Writes diagnostics to stderr and _exit()s on fatal failure.
struct ChildSpec {
    const Authority* auth = nullptr;
    std::string workspace;
    std::string sessionTmp;   // RW scratch (also contains the fake HOME)
    std::string providerTmp;  // RW staging dir, provider curl only (parent-owned 0700)
    bool allowNet = false;
    bool unsafe = false;
    // Trusted harness networking: minimal Landlock/seccomp profile instead of
    // the full tool profile (system RO + staging RW, network allowed).
    bool providerCurl = false;
};
void childEnterSandbox(const ChildSpec& spec);

// Build the sanitized child environment (complete "K=V" list).
// Carries NO secrets and NO trusted harness state: no keyfile, no session
// id, no depth, no parent flags. A recursive `pocket` reads parent state
// (depth/workspace/net) from $TMPDIR/pocket.parent instead; keys flow only
// via explicit expose_env passthrough.
std::vector<std::string> buildChildEnv(const std::vector<std::string>& exposeEnv,
                                       const std::string& workspace,
                                       const std::string& tmpdir, const std::string& home,
                                       const Authority* auth = nullptr);

// Keep only PATH entries the sandbox can actually execute (Landlock would
// deny the rest with a confusing exit 126). Empty entries mean the child
// cwd, i.e. the workspace. Exposed for tests.
std::string filterChildPath(const char* path, const std::string& workspace,
                            const std::string& tmpdir, const Authority* auth = nullptr);

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

// Read-only prover for /double evidence passes: the strict inverse of the
// guard above. True only for a SINGLE simple command that is mechanically
// provable side-effect free: no pipes, redirection, lists, substitution, or
// backgrounding; argv[0] must be a bare name from a fixed binary allowlist,
// with per-binary denylists for flags that can execute or write (find
// -exec/-delete, git non-read subcommands/-c/--output, rg --pre, fd --exec).
// Anything else is rejected with a self-explaining *why. Pure, unit-tested.
bool isReadOnlyBash(const std::string& cmd, std::string* why = nullptr);

// Depth guard for recursive pocket instances.
inline constexpr int kMaxPocketDepth = 5;

}  // namespace pocket
