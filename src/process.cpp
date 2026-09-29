// PocketHarness - subprocess implementation.
#include "process.h"

#include <algorithm>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <deque>
#include <mutex>
#include <thread>

namespace pocket {

namespace {
// Process groups this session started that may still hold members (a tool's
// backgrounded server outlives its call). Lock-free slots: the fatal-signal
// path reads them.
constexpr int kGroupSlots = 256;
std::atomic<pid_t> g_groups[kGroupSlots];

bool groupAlive(pid_t pg) { return kill(-pg, 0) == 0 || errno != ESRCH; }

void trackGroup(pid_t pg) {
    // Claim a free slot with no syscalls first. Pruning costs one kill(2) per
    // occupied slot, and doing that on every spawn put ~512 syscalls between
    // each tool launch and its first poll.
    auto claim = [&] {
        for (auto& slot : g_groups) {
            pid_t empty = 0;
            if (slot.compare_exchange_strong(empty, pg)) return true;
        }
        return false;
    };
    if (claim()) return;
    // Table full: reclaim slots whose group is gone (pgids get reused).
    for (auto& slot : g_groups) {
        pid_t old = slot.load();
        if (old > 0 && !groupAlive(old)) slot.compare_exchange_strong(old, 0);
    }
    claim();  // still full only with 256 genuinely live groups
}

void untrackGroup(pid_t pg) {
    for (auto& slot : g_groups) {
        pid_t cur = pg;
        if (slot.compare_exchange_strong(cur, 0)) return;
    }
}

bool anyGroupAlive() {
    for (auto& slot : g_groups) {
        pid_t pg = slot.load();
        if (pg > 0 && groupAlive(pg)) return true;
    }
    return false;
}

std::string gib(int64_t bytes) {
    char b[32];
    snprintf(b, sizeof(b), "%.1f GiB", (double)bytes / (1LL << 30));
    return b;
}

// Below the reserve a command may still write this much before it is stopped,
// so short commands keep working on an already tight disk.
constexpr int64_t kLowDiskSlack = 256LL << 20;
// Watchdog: below the reserve, this much consumed within kTripWindowMs trips it.
constexpr int64_t kTripDrop = 1LL << 30;
constexpr int64_t kTripWindowMs = 30000;

std::atomic<unsigned> g_diskTrips{0};  // bumped each time the watchdog stops the session's groups
struct Watch {
    std::mutex m;
    std::vector<std::string> paths;
    uint64_t reserve = 0;
    bool started = false;
} g_watch;

void watchdogLoop() {
    std::vector<std::deque<std::pair<int64_t, int64_t>>> hist;  // per path: (ms, avail)
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::vector<std::string> paths;
        int64_t reserve;
        {
            std::lock_guard<std::mutex> lock(g_watch.m);
            paths = g_watch.paths;
            reserve = (int64_t)g_watch.reserve;
        }
        if (hist.size() != paths.size() || reserve <= 0 || !anyGroupAlive()) {
            hist.assign(paths.size(), {});
            continue;
        }
        int64_t now = nowMs();
        bool trip = false;
        for (size_t i = 0; i < paths.size(); ++i) {
            int64_t avail = diskAvail(paths[i]);
            if (avail < 0) continue;
            auto& h = hist[i];
            while (!h.empty() && now - h.front().first > kTripWindowMs) h.pop_front();
            h.emplace_back(now, avail);
            int64_t peak = 0;
            for (const auto& [t, a] : h) peak = std::max(peak, a);
            if (avail < reserve && peak - avail >= kTripDrop) trip = true;
        }
        if (trip) {
            g_diskTrips.fetch_add(1);
            killSessionProcesses(1000);
            hist.assign(paths.size(), {});
        }
    }
}
}  // namespace

int64_t diskAvail(const std::string& path) {
    struct statvfs sv{};
    if (path.empty() || statvfs(path.c_str(), &sv) != 0) return -1;
    return (int64_t)sv.f_bavail * (int64_t)sv.f_frsize;
}

void startDiskWatchdog(std::vector<std::string> paths, uint64_t reserveBytes) {
    std::lock_guard<std::mutex> lock(g_watch.m);
    g_watch.paths = std::move(paths);
    g_watch.reserve = reserveBytes;
    if (g_watch.started || reserveBytes == 0) return;
    g_watch.started = true;
    std::thread(watchdogLoop).detach();
}

void killSessionProcesses(long graceMs) {
    bool any = false;
    for (auto& slot : g_groups) {
        pid_t pg = slot.load();
        if (pg > 0 && kill(-pg, SIGTERM) == 0) { kill(-pg, SIGCONT); any = true; }
    }
    if (!any) return;
    for (long waited = 0; waited < graceMs; waited += 25) {
        bool alive = false;
        for (auto& slot : g_groups) {
            pid_t pg = slot.load();
            if (pg > 0 && groupAlive(pg)) alive = true;
        }
        if (!alive) break;
        struct timespec ts{0, 25 * 1000 * 1000};
        nanosleep(&ts, nullptr);
    }
    for (auto& slot : g_groups) {
        pid_t pg = slot.exchange(0);
        if (pg > 0) kill(-pg, SIGKILL);
    }
}

std::string whichExe(const std::string& name, const std::string& workdir,
                     const std::vector<std::string>& env) {
    if (name.find('/') != std::string::npos) return name;
    const char* path = env.empty() ? getenv("PATH") : nullptr;
    for (const auto& e : env)
        if (startsWith(e, "PATH=")) { path = e.c_str() + 5; break; }
    std::string dirs = path ? path : "/usr/bin:/bin";
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) return "";
    std::string base = workdir.empty() ? cwd : workdir;
    if (base[0] != '/') base = std::string(cwd) + "/" + base;
    size_t i = 0;
    while (i <= dirs.size()) {
        size_t j = dirs.find(':', i);
        if (j == std::string::npos) j = dirs.size();
        std::string dir = dirs.substr(i, j - i);
        if (dir.empty()) dir = ".";
        if (dir[0] != '/') dir = base + "/" + dir;
        std::string cand = dir + "/" + name;
        if (access(cand.c_str(), X_OK) == 0) return cand;
        i = j + 1;
    }
    return "";  // never fall back to an executable in an unsearched working directory
}

SpawnResult spawn(const SpawnOpts& opts) {
    SpawnResult r;
    if (opts.cancel && opts.cancel->load()) { r.cancelled = true; return r; }
    // Prepare allocations before fork: the TUI may have other threads.
    std::vector<char*> argv, envp;
    for (const auto& a : opts.argv) argv.push_back(const_cast<char*>(a.c_str()));
    if (argv.empty()) argv.push_back(const_cast<char*>(opts.exe.c_str()));
    argv.push_back(nullptr);
    for (const auto& e : opts.env) envp.push_back(const_cast<char*>(e.c_str()));
    envp.push_back(nullptr);
    std::string exe = whichExe(opts.exe, opts.workdir, opts.env);
    int pin[2] = {-1, -1}, pout[2] = {-1, -1}, perr[2] = {-1, -1};
    if (pipe2(pin, O_CLOEXEC) != 0 || pipe2(pout, O_CLOEXEC) != 0 || pipe2(perr, O_CLOEXEC) != 0) {
        for (int fd : {pin[0], pin[1], pout[0], pout[1], perr[0], perr[1]})
            if (fd >= 0) close(fd);
        r.error = "pipe failed";
        return r;
    }
    // Non-blocking parent ends.
    fcntl(pin[1], F_SETFL, O_NONBLOCK);
    fcntl(pout[0], F_SETFL, O_NONBLOCK);
    fcntl(perr[0], F_SETFL, O_NONBLOCK);

    pid_t spawningPid = getpid();
    pid_t pid = fork();
    if (pid < 0) {
        r.error = "fork failed";
        close(pin[0]);
        close(pin[1]);
        close(pout[0]);
        close(pout[1]);
        close(perr[0]);
        close(perr[1]);
        return r;
    }
    if (pid == 0) {
        // ---- child ----
        // A recursive harness must see termination before exec, too. Do not
        // inherit the parent's handler (which only sets its private flag), or
        // a blocked/ignored signal. Parent death closes the fork/prctl race.
        struct sigaction action{};
        action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask);
        sigaction(SIGTERM, &action, nullptr);
        sigaction(SIGINT, &action, nullptr);
        sigaction(SIGPIPE, &action, nullptr);
        sigset_t unblocked;
        sigemptyset(&unblocked);
        sigaddset(&unblocked, SIGTERM);
        sigaddset(&unblocked, SIGINT);
        sigaddset(&unblocked, SIGPIPE);
        sigprocmask(SIG_UNBLOCK, &unblocked, nullptr);
        if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0 || getppid() != spawningPid) _exit(125);
        dup2(pin[0], STDIN_FILENO);
        dup2(pout[1], STDOUT_FILENO);
        dup2(perr[1], STDERR_FILENO);
        close(pin[0]);
        close(pin[1]);
        close(pout[0]);
        close(pout[1]);
        close(perr[0]);
        close(perr[1]);
        if (!opts.workdir.empty()) {
            if (chdir(opts.workdir.c_str()) != 0) _exit(126);
        }
        // Start a process group so we can kill the whole tree on timeout.
        setpgid(0, 0);
        if (opts.childSetup) opts.childSetup();
        if (!opts.env.empty()) execve(exe.c_str(), argv.data(), envp.data());
        else execv(exe.c_str(), argv.data());
        _exit(127);
    }
    // ---- parent ----
    // Move the child into its own group NOW (the child does the same): without
    // this, kill(-pid) below could fire before the child's setpgid and hit
    // our own process group. Errors are harmless (child may have exited).
    setpgid(pid, pid);
    trackGroup(pid);
    int exitFd = -1;
#ifdef SYS_pidfd_open
    exitFd = (int)syscall(SYS_pidfd_open, pid, 0);  // wake on exit without polling EOF pipes
#endif
    close(pin[0]);
    close(pout[1]);
    close(perr[1]);

    // A child may close stdin early. Block SIGPIPE in this thread only;
    // never change the TUI's or another subprocess's signal disposition.
    sigset_t pipeSet, oldMask, pending;
    sigemptyset(&pipeSet);
    sigaddset(&pipeSet, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &pipeSet, &oldMask);
    sigpending(&pending);

    size_t inOff = 0;
    bool inOpen = true;
    int64_t start = nowMs();
    int64_t terminateAt = -1;
    bool killed = false;
    char buf[65536];
    int status = 0;
    bool reaped = false;
    int64_t reapedAt = -1;

    const int64_t diskStart = (opts.diskBudgetBytes || opts.diskReserveBytes) ? diskAvail(opts.diskGuardPath) : -1;
    int64_t nextDiskCheck = start;
    const unsigned tripsAtStart = g_diskTrips.load();

    auto elapsed = [&]() { return nowMs() - start; };
    auto terminate = [&] {
        if (terminateAt >= 0) return;
        terminateAt = nowMs();
        // Recursive Pocket handles TERM and cancels its separately grouped
        // tools, then writes final usage. KILL first would orphan those tools.
        kill(-pid, SIGTERM);
        if (!reaped) kill(pid, SIGTERM);
    };
    while (!reaped || pout[0] >= 0 || perr[0] >= 0 || (terminateAt >= 0 && !killed)) {
        if ((opts.timeoutMs > 0 && elapsed() >= opts.timeoutMs) ||
            (opts.cancel && opts.cancel->load())) {
            if (terminateAt < 0) {
                if (opts.timeoutMs > 0 && elapsed() >= opts.timeoutMs)
                    r.timedOut = true;
                else
                    r.cancelled = true;
                terminate();
            }
        }
        if (diskStart >= 0 && terminateAt < 0 && nowMs() >= nextDiskCheck) {
            nextDiskCheck = nowMs() + 250;
            int64_t avail = diskAvail(opts.diskGuardPath);
            int64_t used = avail >= 0 ? diskStart - avail : 0;
            if (opts.diskBudgetBytes && used > (int64_t)opts.diskBudgetBytes)
                r.diskGuard = "command consumed " + gib(used) + " of disk, over the " +
                              gib((int64_t)opts.diskBudgetBytes) + " per-command budget";
            else if (opts.diskReserveBytes && avail >= 0 && avail < (int64_t)opts.diskReserveBytes &&
                     used > kLowDiskSlack)
                r.diskGuard = "free disk fell to " + gib(avail) + ", below the " +
                              gib((int64_t)opts.diskReserveBytes) + " reserve, while the command kept writing";
            if (!r.diskGuard.empty()) terminate();
        }
        if (terminateAt >= 0 && !killed && nowMs() - terminateAt >= std::clamp(opts.terminateGraceMs, 0L, 5000L)) {
            killed = true;
            kill(-pid, SIGKILL);  // bounded grace, including a reaped leader's descendants
            if (!reaped) kill(pid, SIGKILL);
        }
        struct pollfd fds[4];
        fds[0].fd = pout[0];
        fds[0].events = POLLIN;
        fds[1].fd = perr[0];
        fds[1].events = POLLIN;
        fds[2].fd = inOpen ? pin[1] : -1;
        fds[2].events = POLLOUT;
        fds[3].fd = reaped ? -1 : exitFd;
        fds[3].events = POLLIN;
        int pr = poll(fds, 4, 50);
        if (pr < 0) continue;  // revents is only defined once poll succeeds
        // stdin feed
        if (inOpen && (fds[2].revents & (POLLOUT | POLLERR | POLLHUP))) {
            if (inOff < opts.stdinData.size()) {
                ssize_t n = write(pin[1], opts.stdinData.data() + inOff,
                                  opts.stdinData.size() - inOff);
                if (n > 0) {
                    inOff += (size_t)n;
                } else if (n < 0 && errno != EAGAIN && errno != EINTR) {
                    inOpen = false;
                    close(pin[1]);
                    pin[1] = -1;
                }
            } else {
                inOpen = false;
                close(pin[1]);
                pin[1] = -1;
            }
        }
        auto drain = [&](int& fd, std::string& dst, bool isErr) {
            // Fairness: an endless stdout writer must not starve stderr,
            // timeout checks or cancellation.
            for (int reads = 0; fd >= 0 && reads < 4; ++reads) {
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n > 0) {
                    size_t take = std::min((size_t)n, opts.outLimit - dst.size());
                    if (opts.onChunk && take) opts.onChunk(std::string_view(buf, take), isErr);
                    dst.append(buf, take);
                    if (take < (size_t)n) {
                        r.truncated = true;
                        if (opts.stopOnLimit) terminate();
                    }
                } else if (n == 0) {
                    close(fd); fd = -1;
                } else {
                    if (errno == EAGAIN || errno == EINTR) break;
                    close(fd); fd = -1;
                }
            }
        };
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) drain(pout[0], r.out, false);
        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) drain(perr[0], r.err, true);
        if (!reaped) {
            pid_t w = waitpid(pid, &status, WNOHANG);
            if (w == pid) {
                reaped = true;
                reapedAt = nowMs();
            } else if (w < 0 && errno != EINTR) {
                r.error = "waitpid failed";
                break;
            }
        }
        if (reaped && terminateAt >= 0) {
            if (killed) break;  // escaped descendants cannot hold pipes open forever
            if (pout[0] < 0 && perr[0] < 0 && kill(-pid, 0) != 0 && errno == ESRCH) break;
        }
        if (reaped && terminateAt < 0 && opts.lingerMs >= 0 && (pout[0] >= 0 || perr[0] >= 0) &&
            nowMs() - reapedAt >= opts.lingerMs) {
            r.detached = true;  // a backgrounded server must not pin the call until timeout
            break;
        }
    }
    if (pin[1] >= 0) close(pin[1]);
    if (exitFd >= 0) close(exitFd);
    if (pout[0] >= 0) close(pout[0]);
    if (perr[0] >= 0) close(perr[0]);
    if (!sigismember(&pending, SIGPIPE)) {
        struct timespec zero{};
        while (sigtimedwait(&pipeSet, nullptr, &zero) >= 0) {}
    }
    pthread_sigmask(SIG_SETMASK, &oldMask, nullptr);
    // A group whose members all exited is done; one with a live background
    // member stays tracked so session exit can stop it.
    if (reaped && !groupAlive(pid)) untrackGroup(pid);
    if (r.diskGuard.empty() && g_diskTrips.load() != tripsAtStart)
        r.diskGuard = "the session disk watchdog stopped all tool processes: free disk below the reserve and falling";
    if (!reaped) return r;
    if (WIFEXITED(status)) {
        r.exitCode = WEXITSTATUS(status);
        r.ok = !r.timedOut && !r.cancelled && r.diskGuard.empty() && !(opts.stopOnLimit && r.truncated);
    } else if (WIFSIGNALED(status)) {
        r.termSig = WTERMSIG(status);
        r.ok = false;
        if (r.termSig == SIGKILL && (r.timedOut || r.cancelled)) {
            r.error = r.timedOut ? "timed out" : "cancelled";
        }
    }
    if (r.exitCode == 127 && r.out.empty() && r.err.empty() && !r.timedOut && !r.cancelled) {
        r.error = "failed to execute: " + opts.exe;
        r.ok = false;
    }
    return r;
}

}  // namespace pocket
