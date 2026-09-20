// PocketHarness - subprocess implementation.
#include "process.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace pocket {

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
    bool killed = false;
    char buf[65536];
    int status = 0;
    bool reaped = false;

    auto elapsed = [&]() { return nowMs() - start; };
    while (!reaped || pout[0] >= 0 || perr[0] >= 0) {
        if ((opts.timeoutMs > 0 && elapsed() >= opts.timeoutMs) ||
            (opts.cancel && opts.cancel->load())) {
            if (!killed) {
                killed = true;
                if (opts.timeoutMs > 0 && elapsed() >= opts.timeoutMs)
                    r.timedOut = true;
                else
                    r.cancelled = true;
                kill(-pid, SIGKILL);  // whole process group
                if (!reaped) kill(pid, SIGKILL);
            }
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
        if (pr < 0 && errno == EINTR) continue;
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
        if (pr < 0) continue;
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
                        if (opts.stopOnLimit) {
                            kill(-pid, SIGKILL);
                            if (!reaped) kill(pid, SIGKILL);
                        }
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
        if (killed && reaped) break;  // escaped descendants cannot hold pipes open forever
        if (reaped) continue;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            reaped = true;
        } else if (w < 0 && errno == EINTR) {
            continue;
        } else if (w < 0) {
            r.error = "waitpid failed";
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
    if (!reaped) return r;
    if (WIFEXITED(status)) {
        r.exitCode = WEXITSTATUS(status);
        r.ok = !r.timedOut && !r.cancelled;
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
