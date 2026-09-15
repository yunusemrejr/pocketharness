// PocketHarness - sandbox implementation, part 1: probing + contained files.
#include "sandbox.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>

#ifndef SYS_openat2
#define SYS_openat2 437
#endif
#ifdef __has_include
#if __has_include(<linux/openat2.h>)
#include <linux/openat2.h>
#endif
#endif
#ifndef RESOLVE_NO_XDEV
#define RESOLVE_NO_XDEV 0x01
#endif
#ifndef RESOLVE_NO_MAGICLINKS
#define RESOLVE_NO_MAGICLINKS 0x02
#endif
#ifndef RESOLVE_NO_SYMLINKS
#define RESOLVE_NO_SYMLINKS 0x04
#endif
#ifndef RESOLVE_BENEATH
#define RESOLVE_BENEATH 0x08
#endif
#ifndef RESOLVE_IN_ROOT
#define RESOLVE_IN_ROOT 0x10
#endif
#ifndef RESOLVE_CACHED
#define RESOLVE_CACHED 0x20
#endif

#ifndef HAVE_OPEN_HOW
struct pocket_open_how {
    uint64_t flags;
    uint64_t mode;
    uint64_t resolve;
};
#define HAVE_OPEN_HOW 1
#endif

extern char** environ;

namespace pocket {

namespace {

// Direct openat2 syscall. Returns fd or -1 (ENOSYS when unsupported).
// Uses our own layout-identical struct to avoid header-version issues.
int openat2raw(int dirfd, const char* path, uint64_t oflags, mode_t mode, uint64_t resolve) {
#ifdef __linux__
    pocket_open_how h{(uint64_t)oflags, (uint64_t)mode, (uint64_t)resolve};
    return (int)syscall(SYS_openat2, dirfd, path, &h, sizeof(h), 0);
#else
    (void)dirfd;
    (void)path;
    (void)oflags;
    (void)mode;
    (void)resolve;
    errno = ENOSYS;
    return -1;
#endif
}

bool probeOpenat2() {
    int fd = openat2raw(AT_FDCWD, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0,
                        RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS);
    if (fd >= 0) {
        close(fd);
        return true;
    }
    return errno != ENOSYS;
}

// Fork-based probes must be async-safe in the child: only syscalls + _exit.
bool forkProbe(bool (*childFn)()) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        bool r = childFn();
        _exit(r ? 0 : 1);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}  // namespace

// Forward decls from part 2 (same file, defined below).
bool probeLandlockChild();
bool probeSeccompChild();
bool probeUserNsChild();

const SandboxCaps& sandboxCaps() {
    static SandboxCaps caps;
    static bool done = false;
    if (done) return caps;
    done = true;
    caps.openat2 = probeOpenat2();
    caps.landlock = forkProbe(probeLandlockChild);
    caps.seccompNet = forkProbe(probeSeccompChild);
    caps.userNs = forkProbe(probeUserNsChild);
    return caps;
}

// ---------------------------------------------------------------------------
// Authority
// ---------------------------------------------------------------------------
namespace {

Result<std::string> canonicalDir(const std::string& p) {
    char buf[PATH_MAX];
    if (!realpath(p.c_str(), buf))
        return Result<std::string>::Err("cannot resolve path: " + p + " (" + strerror(errno) +
                                        ")");
    struct stat st;
    if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode))
        return Result<std::string>::Err("not a directory: " + p);
    return Result<std::string>::Ok(buf);
}

int openRootFd(const std::string& p) {
    return open(p.c_str(), O_PATH | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
}

}  // namespace

Result<Authority> authorityInit(const std::string& workspace,
                                const std::vector<std::string>& allowRead,
                                const std::vector<std::string>& allowWrite, bool unsafe) {
    Authority a;
    a.unsafe = unsafe;
    auto ws = canonicalDir(workspace);
    if (!ws.ok) return Result<Authority>::Err(ws.error);
    a.workspace = ws.value;
    a.readRoots.push_back(a.workspace);
    a.writeRoots.push_back(a.workspace);
    for (const auto& p : allowRead) {
        auto c = canonicalDir(expandHome(p));
        if (!c.ok) return Result<Authority>::Err("--allow-read: " + c.error);
        if (std::find(a.readRoots.begin(), a.readRoots.end(), c.value) == a.readRoots.end())
            a.readRoots.push_back(c.value);
    }
    for (const auto& p : allowWrite) {
        auto c = canonicalDir(expandHome(p));
        if (!c.ok) return Result<Authority>::Err("--allow-write: " + c.error);
        if (std::find(a.writeRoots.begin(), a.writeRoots.end(), c.value) == a.writeRoots.end()) {
            a.writeRoots.push_back(c.value);
            if (std::find(a.readRoots.begin(), a.readRoots.end(), c.value) == a.readRoots.end())
                a.readRoots.push_back(c.value);
        }
    }
    if (!unsafe) {
        for (const auto& r : a.readRoots) {
            int fd = openRootFd(r);
            if (fd < 0) {
                authorityClose(a);
                return Result<Authority>::Err("cannot open root fd for " + r);
            }
            a.readFds.push_back(fd);
        }
        for (const auto& r : a.writeRoots) {
            // Reuse the read fd when the root is shared.
            int fd = -1;
            for (size_t i = 0; i < a.readRoots.size(); ++i)
                if (a.readRoots[i] == r) {
                    fd = dup(a.readFds[i]);
                    break;
                }
            if (fd < 0) fd = openRootFd(r);
            if (fd < 0) {
                authorityClose(a);
                return Result<Authority>::Err("cannot open root fd for " + r);
            }
            a.writeFds.push_back(fd);
        }
    }
    return Result<Authority>::Ok(std::move(a));
}

void authorityClose(Authority& a) {
    for (int fd : a.readFds)
        if (fd >= 0) close(fd);
    for (int fd : a.writeFds)
        if (fd >= 0) close(fd);
    a.readFds.clear();
    a.writeFds.clear();
}

// ---------------------------------------------------------------------------
// Contained opens
// ---------------------------------------------------------------------------
namespace {

// Split an absolute path into components (no empties). ".." and "." are kept
// as components; the kernel enforces containment, so lexical games are moot.
std::vector<std::string> splitComps(const std::string& p) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < p.size()) {
        while (i < p.size() && p[i] == '/') ++i;
        size_t j = i;
        while (j < p.size() && p[j] != '/') ++j;
        if (j > i) out.push_back(p.substr(i, j - i));
        i = j;
    }
    return out;
}

// Open one component relative to dirfd.
// strictSymlinks=true refuses every symlink (writes); false allows symlinks
// that resolve inside the root (reads).
int openComp(int dirfd, const char* comp, int extraFlags, mode_t mode, bool isDir,
             bool strictSymlinks) {
    uint64_t resolve = RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS;
    if (strictSymlinks) resolve |= RESOLVE_NO_SYMLINKS;
    int flags = extraFlags | O_CLOEXEC | O_NOFOLLOW * (strictSymlinks ? 1 : 0);
    if (isDir) flags |= O_DIRECTORY;
    if (sandboxCaps().openat2) {
        int fd = openat2raw(dirfd, comp, (uint64_t)(unsigned)flags, mode, resolve);
        if (fd >= 0) return fd;
        if (errno == EXDEV || errno == ELOOP)
            errno = EACCES;  // normalize "escaped / symlink" to denial
        return -1;
    }
    // Fallback (ancient kernel): refuse all symlinks on every component.
    int fd = openat(dirfd, comp, flags | O_NOFOLLOW, mode);
    if (fd < 0 && (errno == ELOOP || errno == EXDEV)) errno = EACCES;
    return fd;
}

// Open the parent directory chain of relComps[0..n-2], creating dirs when
// createParents. Returns parent fd or -1.
int openParentChain(int rootfd, const std::vector<std::string>& comps, bool createParents,
                    bool strictSymlinks, std::string& err) {
    int cur = dup(rootfd);
    if (cur < 0) {
        err = "dup root: ";
        err += strerror(errno);
        return -1;
    }
    for (size_t i = 0; i + 1 < comps.size(); ++i) {
        const std::string& c = comps[i];
        if (c.empty() || c == "." || c == "..") {
            err = "invalid path component";
            close(cur);
            return -1;  // belt & suspenders; kernel would also confine
        }
        int nxt = openComp(cur, c.c_str(), O_RDONLY, 0, true, strictSymlinks);
        if (nxt < 0 && createParents && errno == ENOENT) {
            // mkdirat never follows a trailing symlink; reopen strictly.
            if (mkdirat(cur, c.c_str(), 0755) == 0) {
                nxt = openComp(cur, c.c_str(), O_RDONLY, 0, true, true);
            }
        }
        if (nxt < 0) {
            err = "cannot traverse '" + c + "': " + strerror(errno);
            close(cur);
            return -1;
        }
        close(cur);
        cur = nxt;
    }
    return cur;
}

struct Resolved {
    int rootIdx = -1;  // index into roots/fds
    std::vector<std::string> comps;
    bool absolute = false;
};

// Validate + relativize a model path against the given roots.
bool resolveModelPath(const std::vector<std::string>& roots, const std::string& raw,
                      Resolved& out, std::string& err) {
    std::string p = trim(raw);
    if (p.empty()) {
        err = "empty path";
        return false;
    }
    if (p.find('\0') != std::string::npos) {
        err = "NUL byte in path";
        return false;
    }
    p = expandHome(p);
    if (!p.empty() && p[0] == '/') {
        out.absolute = true;
        std::string rel;
        // selectRoot needs fds only for parity; pass dummy.
        std::vector<int> dummy;
        // Reimplement selection here to avoid coupling.
        size_t best = 0;
        int bestIdx = -1;
        for (size_t i = 0; i < roots.size(); ++i) {
            const std::string& r = roots[i];
            if (p == r) {
                bestIdx = (int)i;
                rel = ".";
                break;
            }
            if (p.size() > r.size() && startsWith(p, r) && p[r.size()] == '/') {
                if (r.size() > best) {
                    best = r.size();
                    bestIdx = (int)i;
                }
            }
        }
        (void)dummy;
        if (bestIdx < 0) {
            err = "path escapes allowed roots: " + p;
            return false;
        }
        out.rootIdx = bestIdx;
        if (rel == ".") {
            out.comps = {"."};
        } else {
            out.comps = splitComps(p.substr(best + 1));
            if (out.comps.empty()) out.comps = {"."};
        }
        return true;
    }
    out.absolute = false;
    out.rootIdx = 0;  // relative paths anchor at the workspace (roots[0])
    out.comps = splitComps(p);
    if (out.comps.empty()) {
        err = "empty path";
        return false;
    }
    return true;
}

std::string readAllFd(int fd, size_t maxBytes, bool& tooBig) {
    std::string out;
    char buf[65536];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        if (out.size() + (size_t)n > maxBytes) {
            tooBig = true;
            out.append(buf, maxBytes - out.size());
            break;
        }
        out.append(buf, (size_t)n);
    }
    return out;
}

}  // namespace

Result<std::string> boxRead(const Authority& a, const std::string& path, size_t maxBytes) {
    if (a.unsafe) {
        auto r = readFileBounded(expandHome(path), maxBytes);
        if (!r.ok) return Result<std::string>::Err(r.error);
        return r;
    }
    Resolved rs;
    std::string err;
    if (!resolveModelPath(a.readRoots, path, rs, err))
        return Result<std::string>::Err("read denied: " + err);
    int rootfd = a.readFds[(size_t)rs.rootIdx];
    int dirfd = openParentChain(rootfd, rs.comps, false, false, err);
    if (dirfd < 0) return Result<std::string>::Err("read denied: " + err);
    const std::string& leaf = rs.comps.back();
    int fd;
    if (leaf == ".") {
        fd = dirfd;  // reading a directory itself: fail cleanly below
    } else {
        fd = openComp(dirfd, leaf.c_str(), O_RDONLY, 0, false, false);
        if (fd < 0) {
            err = "cannot open '" + leaf + "': " + strerror(errno);
            close(dirfd);
            return Result<std::string>::Err("read denied: " + err);
        }
    }
    struct stat st;
    bool isDir = (leaf == ".") || (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode));
    if (isDir) {
        if (fd != dirfd) close(fd);
        close(dirfd);
        return Result<std::string>::Err("read denied: is a directory: " + path);
    }
    bool tooBig = false;
    std::string data = readAllFd(fd, maxBytes, tooBig);
    close(fd);
    if (fd != dirfd) close(dirfd);
    if (tooBig) data += "\n[... truncated: file exceeds limit ...]\n";
    return Result<std::string>::Ok(std::move(data));
}

VoidResult boxWrite(const Authority& a, const std::string& path, const std::string& data,
                    mode_t mode) {
    if (a.unsafe) return atomicWriteFile(expandHome(path), data, mode);
    Resolved rs;
    std::string err;
    if (!resolveModelPath(a.writeRoots, path, rs, err))
        return VoidResult::Err("write denied: " + err);
    const std::string& leaf = rs.comps.back();
    if (leaf == "." || leaf == ".." || leaf.empty())
        return VoidResult::Err("write denied: invalid name");
    int rootfd = a.writeFds[(size_t)rs.rootIdx];
    int dirfd = openParentChain(rootfd, rs.comps, true, true, err);
    if (dirfd < 0) return VoidResult::Err("write denied: " + err);
    // Refuse to replace a symlink, even though rename would stay contained:
    // silently swapping a link for a file hides what the model really did.
    struct stat leafSt;
    if (fstatat(dirfd, leaf.c_str(), &leafSt, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISLNK(leafSt.st_mode)) {
        close(dirfd);
        return VoidResult::Err("write denied: '" + leaf + "' is a symlink");
    }
    // Tmp file + rename inside the same directory (atomic for readers).
    std::string tmp = ".pocket-tmp-" + randHex(4);
    int fd;
    if (sandboxCaps().openat2) {
        fd = openat2raw(dirfd, tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode,
                        RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_SYMLINKS);
    } else {
        fd = openat(dirfd, tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                    mode);
    }
    if (fd < 0) {
        err = "cannot create temp file: " + std::string(strerror(errno));
        close(dirfd);
        return VoidResult::Err("write denied: " + err);
    }
    size_t off = 0;
    bool werr = false;
    while (off < data.size()) {
        ssize_t n = write(fd, data.data() + off, data.size() - off);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            werr = true;
            break;
        }
        off += (size_t)n;
    }
    if (!werr && fsync(fd) != 0) werr = true;
    close(fd);
    if (werr) {
        unlinkat(dirfd, tmp.c_str(), 0);
        close(dirfd);
        return VoidResult::Err("write denied: I/O error");
    }
    // renameat over a symlink replaces the link itself, never the target.
    if (renameat(dirfd, tmp.c_str(), dirfd, leaf.c_str()) != 0) {
        err = "rename failed: " + std::string(strerror(errno));
        unlinkat(dirfd, tmp.c_str(), 0);
        close(dirfd);
        return VoidResult::Err("write denied: " + err);
    }
    fsync(dirfd);
    close(dirfd);
    return VoidResult::Ok();
}

Result<bool> boxExists(const Authority& a, const std::string& path) {
    if (a.unsafe) {
        struct stat st;
        return Result<bool>::Ok(lstat(expandHome(path).c_str(), &st) == 0);
    }
    Resolved rs;
    std::string err;
    if (!resolveModelPath(a.readRoots, path, rs, err))
        return Result<bool>::Ok(false);  // outside roots => "does not exist" for model
    int rootfd = a.readFds[(size_t)rs.rootIdx];
    int dirfd = openParentChain(rootfd, rs.comps, false, false, err);
    if (dirfd < 0) {
        // Parent missing vs denied: both read as non-existent to the model.
        return Result<bool>::Ok(false);
    }
    const std::string& leaf = rs.comps.back();
    struct stat st;
    bool exists;
    if (leaf == ".") {
        exists = fstat(dirfd, &st) == 0;
        close(dirfd);
    } else {
        exists = faccessat(dirfd, leaf.c_str(), F_OK, AT_SYMLINK_NOFOLLOW) == 0;
        close(dirfd);
    }
    return Result<bool>::Ok(exists);
}

// ---------------------------------------------------------------------------
// Part 2: Landlock + seccomp-net + child setup + env + guard
// ---------------------------------------------------------------------------
#ifndef SYS_landlock_create_ruleset
#define SYS_landlock_create_ruleset 444
#define SYS_landlock_add_rule 445
#define SYS_landlock_restrict_self 446
#endif
#ifdef __has_include
#if __has_include(<linux/landlock.h>)
#include <linux/landlock.h>
#endif
#endif
// Fallback definitions when <linux/landlock.h> is absent or too old.
// Guarded on an ACCESS macro (always a #define in the UAPI header); the rule
// type enum and structs come from the same header version.
#ifndef LANDLOCK_ACCESS_FS_EXECUTE
enum { LANDLOCK_RULE_PATH_BENEATH = 1 };
struct landlock_ruleset_attr {
    uint64_t handled_access_fs;
    uint64_t handled_access_net;
};
struct landlock_path_beneath_attr {
    uint64_t allowed_access;
    int32_t parent_fd;
};
#define LANDLOCK_ACCESS_FS_EXECUTE (1ULL << 0)
#define LANDLOCK_ACCESS_FS_WRITE_FILE (1ULL << 1)
#define LANDLOCK_ACCESS_FS_READ_FILE (1ULL << 2)
#define LANDLOCK_ACCESS_FS_READ_DIR (1ULL << 3)
#define LANDLOCK_ACCESS_FS_REMOVE_DIR (1ULL << 4)
#define LANDLOCK_ACCESS_FS_REMOVE_FILE (1ULL << 5)
#define LANDLOCK_ACCESS_FS_MAKE_CHAR (1ULL << 6)
#define LANDLOCK_ACCESS_FS_MAKE_DIR (1ULL << 7)
#define LANDLOCK_ACCESS_FS_MAKE_REG (1ULL << 8)
#define LANDLOCK_ACCESS_FS_MAKE_SOCK (1ULL << 9)
#define LANDLOCK_ACCESS_FS_MAKE_FIFO (1ULL << 10)
#define LANDLOCK_ACCESS_FS_MAKE_BLOCK (1ULL << 11)
#define LANDLOCK_ACCESS_FS_MAKE_SYM (1ULL << 12)
#define LANDLOCK_ACCESS_FS_REFER (1ULL << 13)
#define LANDLOCK_ACCESS_FS_TRUNCATE (1ULL << 14)
#endif

namespace {

// All known fs rights (newer kernels define more bits; dropping the highest
// bit until create succeeds converges on any kernel version).
uint64_t allFsRights() {
    uint64_t m = LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE |
                 LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR |
                 LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |
                 LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR |
                 LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SOCK |
                 LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |
                 LANDLOCK_ACCESS_FS_MAKE_SYM | LANDLOCK_ACCESS_FS_REFER |
                 LANDLOCK_ACCESS_FS_TRUNCATE;
#ifdef LANDLOCK_ACCESS_FS_IOCTL
    m |= (uint64_t)LANDLOCK_ACCESS_FS_IOCTL;
#endif
    return m;
}

int landlockCreate(uint64_t handled) {
    struct landlock_ruleset_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.handled_access_fs = handled;
    return (int)syscall(SYS_landlock_create_ruleset, &attr, sizeof(attr), 0);
}

bool landlockAdd(int rsfd, int parentfd, uint64_t allowed) {
    struct landlock_path_beneath_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.allowed_access = allowed;
    attr.parent_fd = parentfd;
    return syscall(SYS_landlock_add_rule, rsfd, LANDLOCK_RULE_PATH_BENEATH, &attr, 0) == 0;
}

// Apply Landlock confinement. Returns 0 ok, 1 unsupported, -1 fatal.
int landlockConfine(const ChildSpec& spec, uint64_t* handledOut) {
    // Converge: clear the HIGHEST bit until the kernel accepts the set
    // (new rights are always added as higher bits, so this works on any ABI).
    uint64_t handled = allFsRights();
    int rsfd = -1;
    for (int i = 0; i < 64 && handled; ++i) {
        rsfd = landlockCreate(handled);
        if (rsfd >= 0) break;
        if (errno == ENOSYS) return 1;  // no Landlock in this kernel
        if (errno != EINVAL) return 1;
        handled &= ~(1ULL << (63 - __builtin_clzll(handled)));
    }
    if (rsfd < 0) return 1;
    if (handledOut) *handledOut = handled;
    uint64_t ro = LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR |
                  LANDLOCK_ACCESS_FS_EXECUTE;
    ro &= handled;
    uint64_t rw = handled;

    auto allowTree = [&](const char* path, uint64_t access, bool required) -> bool {
        int fd = open(path, O_PATH | O_CLOEXEC);
        if (fd < 0) return !required;
        bool ok = landlockAdd(rsfd, fd, access);
        close(fd);
        return ok || !required;
    };

    // Read-only system runtime.
    static const char* kRo[] = {"/usr", "/bin", "/sbin",  "/lib",  "/lib64", "/etc",
                                "/proc", "/sys", "/opt", "/snap", "/run",   nullptr};
    for (const char** p = kRo; *p; ++p) allowTree(*p, ro, false);
    // /dev: read-only, plus a few well-known writable nodes.
    allowTree("/dev", ro, false);
    static const char* kDevRw[] = {"/dev/null", "/dev/zero",  "/dev/full", "/dev/random",
                                   "/dev/urandom", "/dev/shm", "/dev/pts", nullptr};
    for (const char** p = kDevRw; *p; ++p) allowTree(*p, rw, false);
    // Writable: workspace, session scratch, own state, /tmp.
    if (!allowTree(spec.workspace.c_str(), rw, true)) return -1;
    if (!allowTree(spec.sessionTmp.c_str(), rw, true)) return -1;
    if (!spec.stateDirPath.empty()) allowTree(spec.stateDirPath.c_str(), rw, false);
    allowTree("/tmp", rw, false);
    if (spec.auth) {
        for (const auto& r : spec.auth->readRoots) allowTree(r.c_str(), ro, false);
        for (const auto& r : spec.auth->writeRoots) allowTree(r.c_str(), rw, false);
    }
    // /proc/self/fd etc. are under /proc (ro). Dynamic loader paths covered.
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) return -1;
    if (syscall(SYS_landlock_restrict_self, rsfd, 0) != 0) return -1;
    close(rsfd);
    return 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// seccomp: deny AF_INET/AF_INET6 socket() with EACCES (unprivileged, needs
// only NO_NEW_PRIVS). AF_UNIX and everything else is untouched.
// ---------------------------------------------------------------------------
#ifdef __has_include
#if __has_include(<linux/seccomp.h>)
#include <linux/seccomp.h>
#endif
#if __has_include(<linux/filter.h>)
#include <linux/filter.h>
#endif
#if __has_include(<linux/audit.h>)
#include <linux/audit.h>
#endif
#endif
#ifndef SECCOMP_MODE_FILTER
#define SECCOMP_MODE_FILTER 2
#endif
#ifndef SECCOMP_RET_ALLOW
#define SECCOMP_RET_ALLOW 0x7fff0000U
#define SECCOMP_RET_ERRNO 0x00050000U
#endif
#ifndef AUDIT_ARCH_X86_64
#define AUDIT_ARCH_X86_64 62
#endif
#ifndef AUDIT_ARCH_AARCH64
#define AUDIT_ARCH_AARCH64 183
#endif
#include <sys/socket.h>

namespace {

struct SeccompData {
    int nr;
    uint32_t arch;
    uint64_t ip;
    uint64_t args[6];
};

int seccompDenyInet() {
#if defined(__x86_64__)
    const uint32_t kArch = AUDIT_ARCH_X86_64;
    const int kSockNr = 41;  // __NR_socket
#elif defined(__aarch64__)
    const uint32_t kArch = AUDIT_ARCH_AARCH64;
    const int kSockNr = 198;
#else
    return 1;  // unknown arch: report unsupported
#endif
    struct sock_filter f[] = {
        // A = arch
        {(uint16_t)(BPF_LD | BPF_W | BPF_ABS), 0, 0, (uint32_t)__builtin_offsetof(SeccompData, arch)},
        // if A != kArch -> allow (foreign ABI; ignore)
        {(uint16_t)(BPF_JMP | BPF_JEQ | BPF_K), 0, 5, kArch},
        // A = syscall nr
        {(uint16_t)(BPF_LD | BPF_W | BPF_ABS), 0, 0, (uint32_t)__builtin_offsetof(SeccompData, nr)},
        // if nr != socket -> allow
        {(uint16_t)(BPF_JMP | BPF_JEQ | BPF_K), 0, 3, (uint32_t)kSockNr},
        // A = family (arg0, low 32 bits)
        {(uint16_t)(BPF_LD | BPF_W | BPF_ABS), 0, 0,
         (uint32_t)__builtin_offsetof(SeccompData, args)},
        // if family == AF_INET -> deny; if AF_INET6 -> deny; else allow
        {(uint16_t)(BPF_JMP | BPF_JEQ | BPF_K), 0, 1, (uint32_t)AF_INET},
        {(uint16_t)(BPF_RET | BPF_K), 0, 0, (uint32_t)(SECCOMP_RET_ERRNO | (EACCES & 0xffff))},
        {(uint16_t)(BPF_JMP | BPF_JEQ | BPF_K), 0, 1, (uint32_t)AF_INET6},
        {(uint16_t)(BPF_RET | BPF_K), 0, 0, (uint32_t)(SECCOMP_RET_ERRNO | (EACCES & 0xffff))},
        {(uint16_t)(BPF_RET | BPF_K), 0, 0, (uint32_t)SECCOMP_RET_ALLOW},
    };
    // Fix jump offsets: after arch check (idx1): jt=0 -> idx2, jf=5 -> idx7?
    // Recompute carefully: indexes 0..9. idx1: eq->next(2), ne->allow(9): jf=7.
    // idx3: eq->next(4), ne->allow(9): jf=5. idx5: eq->deny(6)? jt path...
    // Layout: 5: jeq AF_INET -> deny(6) else next(7)? We have two denies.
    // Simplify: 5: jeq INET, jt=0(jump to 6 deny), jf=1(skip to 7 check INET6).
    // 7: jeq INET6, jt=0(->8 deny), jf=1(->9 allow).
    f[1].jf = 7;  // 1 -> 9
    f[3].jf = 5;  // 3 -> 9
    f[5].jt = 0;  // 5 -> 6
    f[5].jf = 1;  // 5 -> 7
    f[7].jt = 0;  // 7 -> 8
    f[7].jf = 1;  // 7 -> 9
    struct sock_fprog {
        uint16_t len;
        struct sock_filter* filter;
    } prog{(uint16_t)(sizeof(f) / sizeof(f[0])), f};
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) return -1;
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        if (errno == EINVAL || errno == ENOSYS) return 1;
        return -1;
    }
    return 0;
}

void childWarn(const char* msg) {
    const char* p = "pocket-sandbox: ";
    // Best-effort diagnostics in the child; failures are intentionally ignored.
    ssize_t nw = write(STDERR_FILENO, p, strlen(p));
    nw += write(STDERR_FILENO, msg, strlen(msg));
    nw += write(STDERR_FILENO, "\n", 1);
    (void)nw;
}

}  // namespace

void childEnterSandbox(const ChildSpec& spec) {
    if (spec.providerCurl || spec.unsafe) {
        // Trusted harness networking, or explicit escape hatch: no confinement.
        // Still start clean (no-op here by design).
        return;
    }
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        childWarn("PR_SET_NO_NEW_PRIVS failed; refusing to run");
        _exit(127);
    }
    int ll = landlockConfine(spec, nullptr);
    if (ll < 0) {
        childWarn("Landlock setup failed; refusing to run");
        _exit(127);
    }
    if (ll > 0) childWarn("Landlock unavailable: filesystem running without kernel confinement");
    if (!spec.allowNet) {
        int sc = seccompDenyInet();
        if (sc < 0) {
            childWarn("seccomp setup failed; refusing to run");
            _exit(127);
        }
        if (sc > 0)
            childWarn("seccomp unavailable: model command runs WITHOUT network isolation");
    }
}

// ---------------------------------------------------------------------------
// Probes (run in forked children via forkProbe)
// ---------------------------------------------------------------------------
bool probeLandlockChild() {
    ChildSpec spec;
    Authority empty;
    spec.auth = &empty;
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) return false;
    spec.workspace = cwd;
    char tmp[] = "/tmp/pocket-probe-XXXXXX";
    if (!mkdtemp(tmp)) return false;
    spec.sessionTmp = tmp;
    uint64_t handled = 0;
    int r = landlockConfine(spec, &handled);
    rmdir(tmp);
    return r == 0;
}

bool probeSeccompChild() {
    if (seccompDenyInet() != 0) return false;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd >= 0) {
        close(fd);
        return false;  // filter did not block: broken
    }
    if (errno != EACCES) return false;
    int ufd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ufd < 0) return false;  // unix sockets must keep working
    close(ufd);
    return true;
}

bool probeUserNsChild() {
    // Informational only (documented; not part of the enforcement path).
    if (unshare(CLONE_NEWUSER | CLONE_NEWNET) != 0) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Environment sanitization: default-deny allowlist.
// ---------------------------------------------------------------------------
bool looksSecretEnv(const std::string& name) {
    std::string n = toLower(name);
    if (n.find("api_key") != std::string::npos) return true;
    if (endsWith(n, "_key") || endsWith(n, "key") || startsWith(n, "key_")) {
        // Narrow: only well-known credential holders, not every "*key".
        if (n.find("ssh") != std::string::npos || n.find("api") != std::string::npos ||
            n.find("secret") != std::string::npos || n.find("token") != std::string::npos)
            return true;
    }
    if (n.find("token") != std::string::npos) return true;
    if (n.find("secret") != std::string::npos) return true;
    if (n.find("password") != std::string::npos) return true;
    if (n.find("passwd") != std::string::npos) return true;
    if (startsWith(n, "aws_")) return true;
    if (startsWith(n, "ssh_")) return true;
    if (n == "github_token" || startsWith(n, "github_")) return true;
    if (n.find("credentials") != std::string::npos) return true;
    if (n.find("private_key") != std::string::npos) return true;
    if (n == "key_env" || endsWith(n, "_key_env")) return true;
    return false;
}

namespace {

bool validEnvName(const std::string& n) {
    if (n.empty() || !(n[0] == '_' || isalpha((unsigned char)n[0]))) return false;
    for (char c : n)
        if (!(c == '_' || isalnum((unsigned char)c))) return false;
    return true;
}

void copyIfSet(std::vector<std::string>& out, const char* name) {
    const char* v = getenv(name);
    if (v) {
        out.push_back(std::string(name) + "=" + v);
    }
}

}  // namespace

std::vector<std::string> buildChildEnv(const std::vector<std::string>& exposeEnv,
                                       const std::string& workspace, const std::string& tmpdir,
                                       const std::string& home, const std::string& keyfile,
                                       const std::string& sessionId, int depth, bool parentNet,
                                       bool parentUnsafe) {
    std::vector<std::string> env;
    // Fixed allowlist of ordinary tool variables.
    static const char* kKeep[] = {"PATH", "USER",       "LOGNAME", "LANG",     "LANGUAGE",
                                  "TERM", "COLORTERM",  "NO_COLOR", "TZ",      "EDITOR",
                                  "VISUAL", "PAGER",    "SHELL",   "TZDIR",    nullptr};
    for (const char** p = kKeep; *p; ++p) copyIfSet(env, *p);
    // Locale LC_* family.
    for (char** e = ::environ; e && *e; ++e) {
        std::string kv(*e);
        size_t eq = kv.find('=');
        if (eq == std::string::npos) continue;
        std::string name = kv.substr(0, eq);
        if (startsWith(name, "LC_")) env.push_back(kv);
    }
    // Explicit user-approved passthrough (user config only, never project).
    for (const auto& name : exposeEnv) {
        if (!validEnvName(name)) continue;
        const char* v = getenv(name.c_str());
        if (!v) continue;
        if (looksSecretEnv(name))
            fprintf(stderr, "pocket: exposing secret-looking variable %s (explicit user config)\n",
                    name.c_str());
        env.push_back(name + "=" + v);
    }
    // Harness-controlled values (override anything inherited).
    auto set = [&](const std::string& kv) {
        std::string name = kv.substr(0, kv.find('='));
        env.erase(std::remove_if(env.begin(), env.end(),
                                 [&](const std::string& e) {
                                     return e.size() > name.size() && e[name.size()] == '=' &&
                                            e.compare(0, name.size(), name) == 0;
                                 }),
                  env.end());
        env.push_back(kv);
    };
    set("PWD=" + workspace);
    set("TMPDIR=" + tmpdir);
    set("TMP=" + tmpdir);
    set("TEMP=" + tmpdir);
    set("HOME=" + home);
    set("SHELL=/bin/bash");
    const char* path = getenv("PATH");
    if (!path || !*path) set("PATH=/usr/local/bin:/usr/bin:/bin");
    set("POCKETHARNESS=1");
    set("POCKETHARNESS_WORKSPACE=" + workspace);
    set("POCKETHARNESS_SESSION=" + sessionId);
    set("POCKETHARNESS_DEPTH=" + std::to_string(depth));
    set("POCKETHARNESS_KEYFILE=" + keyfile);
    set(std::string("POCKETHARNESS_PARENT_NET=") + (parentNet ? "1" : "0"));
    set(std::string("POCKETHARNESS_PARENT_UNSAFE=") + (parentUnsafe ? "1" : "0"));
    return env;
}

// ---------------------------------------------------------------------------
// Destructive-command guard
// ---------------------------------------------------------------------------
namespace {

bool hasWord(const std::string& cmd, const std::string& word) {
    for (size_t i = 0; i + word.size() <= cmd.size(); ++i) {
        if (cmd.compare(i, word.size(), word) != 0) continue;
        bool left = i == 0 || (!isalnum((unsigned char)cmd[i - 1]) && cmd[i - 1] != '_' &&
                               cmd[i - 1] != '-' && cmd[i - 1] != '/');
        size_t e = i + word.size();
        bool right = e >= cmd.size() || (!isalnum((unsigned char)cmd[e]) && cmd[e] != '_' &&
                                        cmd[e] != '-');
        if (left && right) return true;
    }
    return false;
}

bool hasFlag(const std::string& cmd, char shortFlag, const char* longFlag) {
    // crude: look for "-x" inside a dash-group, or the long flag word
    for (size_t i = 0; i + 1 < cmd.size(); ++i) {
        if (cmd[i] == '-' && cmd[i + 1] != '-' && cmd[i + 1] != ' ' && cmd[i + 1] != '\'') {
            for (size_t j = i + 1; j < cmd.size() && cmd[j] != ' ' && cmd[j] != '\'' &&
                                       cmd[j] != '"' && cmd[j] != ';' && cmd[j] != '&';
                 ++j) {
                if (cmd[j] == shortFlag) return true;
            }
        }
    }
    if (longFlag && hasWord(cmd, longFlag)) return true;
    return false;
}

bool mentionsRootish(const std::string& cmd) {
    static const char* kRoots[] = {"/",        "/*",      "~",      "~/",     "$HOME",
                                   "${HOME}",  "$HOME/",  "/home",  "/root",  "/etc",
                                   "/usr",     "/var",    "/boot",  "/proc",  "/sys",
                                   "/dev",     "/tmp",    "/opt",   nullptr};
    for (const char** p = kRoots; *p; ++p) {
        std::string w(*p);
        // token-ish match: surrounded by whitespace/quotes/separators
        for (size_t i = 0; i + w.size() <= cmd.size(); ++i) {
            if (cmd.compare(i, w.size(), w) != 0) continue;
            bool left = i == 0 || strchr(" \t\n'\";=|&()$`", cmd[i - 1]) != nullptr;
            size_t e = i + w.size();
            bool right = e >= cmd.size() || strchr(" \t\n'\";/|&()`", cmd[e]) != nullptr;
            if (left && right) return true;
        }
    }
    return false;
}

bool mentionsDot(const std::string& cmd) {
    for (size_t i = 0; i < cmd.size(); ++i) {
        bool dotStar = i + 1 < cmd.size() && cmd[i] == '.' && cmd[i + 1] == '*';
        bool dotOnly = cmd[i] == '.' &&
                       (i + 1 >= cmd.size() || strchr(" \t\n'\";/|&()", cmd[i + 1])) &&
                       (i == 0 || strchr(" \t\n'\";=|&(/", cmd[i - 1]));
        bool dotdot = i + 2 <= cmd.size() && cmd.compare(i, 2, "..") == 0;
        if (dotStar || dotOnly || dotdot) return true;
    }
    return false;
}

}  // namespace

GuardResult classifyCommand(const std::string& cmd, const std::string& workspace, bool allowNet) {
    GuardResult r;
    std::string c = trim(cmd);
    if (c.empty()) {
        r.verdict = Verdict::Deny;
        r.reason = "empty command";
        return r;
    }
    // Network clients while tool networking is off: fail fast with guidance
    // (seccomp would block them anyway).
    if (!allowNet) {
        static const char* kNet[] = {"curl", "wget", "ssh",  "scp",  "sftp",  "rsync",
                                     "nc",   "ncat", "socat", "telnet", "ftp", nullptr};
        for (const char** p = kNet; *p; ++p) {
            if (hasWord(c, *p)) {
                r.verdict = Verdict::Deny;
                r.reason = std::string("tool networking is disabled; command uses '") + *p +
                           "' (run pocket --network to allow, or use skills/files instead)";
                return r;
            }
        }
        if (hasWord(c, "git") &&
            (hasWord(c, "clone") || hasWord(c, "push") || hasWord(c, "pull") ||
             hasWord(c, "fetch") || hasWord(c, "ls-remote"))) {
            r.verdict = Verdict::Deny;
            r.reason = "tool networking is disabled; git remote operation blocked";
            return r;
        }
    }
    // Fork bomb.
    if (c.find(":(){") != std::string::npos || c.find(": (){") != std::string::npos) {
        r.verdict = Verdict::Deny;
        r.reason = "fork-bomb pattern blocked";
        return r;
    }
    // Disk/format/partition tools.
    static const char* kDisk[] = {"mkfs", "mkfs.ext4", "mkfs.btrfs", "mkswap", "fdisk",
                                  "parted", "gdisk", "sgdisk", "blkdiscard", nullptr};
    for (const char** p = kDisk; *p; ++p)
        if (hasWord(c, *p)) {
            r.verdict = Verdict::Deny;
            r.reason = std::string("disk/partition tool blocked: ") + *p;
            return r;
        }
    if (hasWord(c, "dd") && (c.find("of=/dev") != std::string::npos)) {
        r.verdict = Verdict::Deny;
        r.reason = "raw disk write blocked (dd of=/dev...)";
        return r;
    }
    // Raw disk redirect: ">/dev/sdX" with or without spaces (>/dev/null stays OK).
    for (size_t i = 0; i < c.size(); ++i) {
        if (c[i] != '>') continue;
        size_t j = i + 1;
        while (j < c.size() && (c[j] == ' ' || c[j] == '\t')) ++j;
        std::string tail = c.substr(j, 12);
        if (startsWith(tail, "/dev/sd") || startsWith(tail, "/dev/hd") ||
            startsWith(tail, "/dev/nvme") || startsWith(tail, "/dev/vd") ||
            startsWith(tail, "/dev/mmcblk")) {
            r.verdict = Verdict::Deny;
            r.reason = "raw disk write blocked";
            return r;
        }
    }
    static const char* kPower[] = {"shutdown", "reboot", "halt", "poweroff", nullptr};
    for (const char** p = kPower; *p; ++p)
        if (hasWord(c, *p)) {
            r.verdict = Verdict::Deny;
            r.reason = std::string("power command blocked: ") + *p;
            return r;
        }
    // Broad recursive chmod/chown.
    if ((hasWord(c, "chmod") || hasWord(c, "chown")) && hasFlag(c, 'R', "--recursive") &&
        (mentionsRootish(c) || mentionsDot(c) || c.find(" -R /") != std::string::npos ||
         c.find(" -R ~") != std::string::npos || c.find(" -R .") != std::string::npos)) {
        r.verdict = Verdict::Ask;
        r.reason = "broad recursive chmod/chown needs approval";
        return r;
    }
    // rm -r[f]: the classic. Ask when the target looks broad; Deny when it
    // clearly points outside the workspace at a system/home root.
    if (hasWord(c, "rm") && (hasFlag(c, 'r', "--recursive") || hasFlag(c, 'R', "--recursive"))) {
        bool force = hasFlag(c, 'f', "--force");
        if (c.find("--no-preserve-root") != std::string::npos) {
            r.verdict = Verdict::Deny;
            r.reason = "rm --no-preserve-root blocked";
            return r;
        }
        // Absolute targets outside the workspace: deny outright.
        // (Inside-workspace absolute paths are fine and common.)
        for (size_t i = 0; i < c.size(); ++i) {
            if (c[i] == '/' && (i == 0 || strchr(" \t'\";=|&(", c[i - 1]))) {
                size_t e = i;
                while (e < c.size() && !strchr(" \t'\";|&()", c[e])) ++e;
                std::string tok = c.substr(i, e - i);
                if (tok == "/" || tok == "/*") {
                    r.verdict = Verdict::Deny;
                    r.reason = "recursive rm of filesystem root blocked";
                    return r;
                }
                if (!workspace.empty() && !startsWith(tok, workspace + "/") && tok != workspace &&
                    tok.find('*') == std::string::npos) {
                    // Absolute path outside workspace with rm -r: deny.
                    r.verdict = Verdict::Deny;
                    r.reason = "recursive rm outside the workspace blocked: " + tok;
                    return r;
                }
            }
        }
        if (force && (mentionsRootish(c) || mentionsDot(c) || c.find(" *") != std::string::npos ||
                      c.find("/*") != std::string::npos)) {
            r.verdict = Verdict::Ask;
            r.reason = "broad 'rm -rf' needs approval";
            return r;
        }
        if (!force && (mentionsDot(c) || mentionsRootish(c))) {
            r.verdict = Verdict::Ask;
            r.reason = "broad recursive rm needs approval";
            return r;
        }
    }
    // git destructive ops.
    if (hasWord(c, "git") && hasWord(c, "reset") && hasWord(c, "--hard")) {
        r.verdict = Verdict::Ask;
        r.reason = "'git reset --hard' can destroy uncommitted work; needs approval";
        return r;
    }
    if (hasWord(c, "git") && hasWord(c, "clean") && hasFlag(c, 'f', "--force")) {
        r.verdict = Verdict::Ask;
        r.reason = "'git clean -f' deletes untracked files; needs approval";
        return r;
    }
    if (hasWord(c, "git") && hasWord(c, "branch") && hasFlag(c, 'D', "--delete")) {
        r.verdict = Verdict::Ask;
        r.reason = "'git branch -D' needs approval";
        return r;
    }
    return r;  // Allow
}

}  // namespace pocket
