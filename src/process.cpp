// PocketHarness - subprocess implementation.
#include "process.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pocket {

std::string whichExe(const std::string& name) {
    if (name.find('/') != std::string::npos) return name;
    const char* path = getenv("PATH");
    std::string dirs = path ? path : "/usr/bin:/bin";
    size_t i = 0;
    while (i <= dirs.size()) {
        size_t j = dirs.find(':', i);
        if (j == std::string::npos) j = dirs.size();
        std::string cand = dirs.substr(i, j - i) + "/" + name;
        if (access(cand.c_str(), X_OK) == 0) return cand;
        i = j + 1;
    }
    return name;
}

SpawnResult spawn(const SpawnOpts& opts) {
    SpawnResult r;
    int pin[2] = {-1, -1}, pout[2] = {-1, -1}, perr[2] = {-1, -1};
    if (pipe2(pin, O_CLOEXEC) != 0 || pipe2(pout, O_CLOEXEC) != 0 || pipe2(perr, O_CLOEXEC) != 0) {
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
        std::vector<char*> argv;
        for (const auto& a : opts.argv) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        if (!opts.env.empty()) {
            std::vector<char*> envp;
            for (const auto& e : opts.env) envp.push_back(const_cast<char*>(e.c_str()));
            envp.push_back(nullptr);
            if (opts.exe.find('/') != std::string::npos)
                execve(opts.exe.c_str(), argv.data(), envp.data());
            else {
                // PATH lookup for execve: use execvpe.
                execvpe(opts.exe.c_str(), argv.data(), envp.data());
            }
        } else {
            if (opts.exe.find('/') != std::string::npos)
                execv(opts.exe.c_str(), argv.data());
            else
                execvp(opts.exe.c_str(), argv.data());
        }
        _exit(127);
    }
    // ---- parent ----
    close(pin[0]);
    close(pout[1]);
    close(perr[1]);

    size_t inOff = 0;
    bool inOpen = true;
    int64_t start = nowMs();
    bool killed = false;
    char buf[65536];
    int status = 0;
    bool reaped = false;

    auto elapsed = [&]() { return nowMs() - start; };
    while (!reaped) {
        if ((opts.timeoutMs > 0 && elapsed() >= opts.timeoutMs) ||
            (opts.cancel && opts.cancel->load())) {
            if (!killed) {
                killed = true;
                if (opts.timeoutMs > 0 && elapsed() >= opts.timeoutMs)
                    r.timedOut = true;
                else
                    r.cancelled = true;
                kill(-pid, SIGKILL);  // whole process group
                kill(pid, SIGKILL);
            }
        }
        struct pollfd fds[3];
        fds[0].fd = pout[0];
        fds[0].events = POLLIN;
        fds[1].fd = perr[0];
        fds[1].events = POLLIN;
        fds[2].fd = inOpen ? pin[1] : -1;
        fds[2].events = POLLOUT;
        int pr = poll(fds, 3, 50);
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
        auto drain = [&](int fd, std::string& dst, bool isErr) {
            for (;;) {
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n > 0) {
                    if (opts.onChunk)
                        opts.onChunk(std::string_view(buf, (size_t)n), isErr);
                    if (dst.size() < opts.outLimit) {
                        size_t room = opts.outLimit - dst.size();
                        dst.append(buf, (size_t)n > room ? room : (size_t)n);
                        if ((size_t)n > room) r.truncated = true;
                    } else {
                        r.truncated = true;
                    }
                } else if (n == 0) {
                    return true;  // EOF
                } else {
                    if (errno == EAGAIN || errno == EINTR) return false;
                    return true;
                }
            }
        };
        bool outEof = false, errEof = false;
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) outEof = drain(pout[0], r.out, false);
        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) errEof = drain(perr[0], r.err, true);
        (void)outEof;
        (void)errEof;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            reaped = true;
            // Drain whatever remains after exit.
            drain(pout[0], r.out, false);
            drain(perr[0], r.err, true);
        } else if (w < 0 && errno == EINTR) {
            continue;
        } else if (w < 0) {
            break;
        }
    }
    if (pin[1] >= 0) close(pin[1]);
    close(pout[0]);
    close(perr[0]);
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
