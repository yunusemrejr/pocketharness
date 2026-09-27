// PocketHarness tests - process capture/timeout/cancel.
#include "mini.h"

#include <signal.h>
#include <poll.h>
#include <sys/wait.h>

#include "../src/process.h"

using namespace pocket;
using namespace pocket::test;

TEST(process_Capture_Exit_Status) {
    SpawnOpts o;
    o.exe = "/bin/sh";
    o.argv = {"sh", "-c", "echo out; echo err >&2; exit 7"};
    SpawnResult r = spawn(o);
    CHECK(r.ok && r.exitCode == 7);
    CHECK_EQ(r.out, std::string("out\n"));
    CHECK_EQ(r.err, std::string("err\n"));
    return "";
}

TEST(process_Stdin_And_Truncate) {
    SpawnOpts o;
    o.exe = "/bin/cat";
    o.argv = {"cat"};
    o.stdinData = "hello";
    o.outLimit = 3;
    SpawnResult r = spawn(o);
    CHECK(r.ok && r.truncated);
    CHECK_EQ(r.out, std::string("hel"));
    return "";
}

TEST(process_Timeout_Kills_Tree) {
    SpawnOpts o;
    o.exe = "/bin/sh";
    o.argv = {"sh", "-c", "sleep 30"};
    o.timeoutMs = 300;
    int64_t t0 = nowMs();
    SpawnResult r = spawn(o);
    CHECK(r.timedOut && !r.ok);
    CHECK(nowMs() - t0 < 10000);
    return "";
}

TEST(process_Cancel) {
    SpawnOpts o;
    o.exe = "/bin/sh";
    o.argv = {"sh", "-c", "sleep 30"};
    std::atomic<bool> cancel{false};
    o.cancel = &cancel;
    // Cancel from this thread after 200ms via... spawn blocks; use timeout-like
    // trick: set cancel before spawn returns? Instead just pre-cancel:
    cancel.store(true);
    SpawnResult r = spawn(o);
    CHECK(r.cancelled);
    return "";
}

TEST(process_Signal_Death) {
    SpawnOpts o;
    o.exe = "/bin/sh";
    o.argv = {"sh", "-c", "kill -TERM $$"};
    SpawnResult r = spawn(o);
    CHECK(!r.ok && r.termSig == SIGTERM);
    return "";
}

TEST(process_Streaming_Callback) {
    SpawnOpts o;
    o.exe = "/bin/sh";
    o.argv = {"sh", "-c", "echo one; echo two"};
    std::string got;
    o.onChunk = [&](std::string_view sv, bool isErr) {
        if (!isErr) got.append(sv.data(), sv.size());
    };
    SpawnResult r = spawn(o);
    CHECK(r.ok && got == r.out && got == "one\ntwo\n");
    return "";
}

TEST(process_Closed_Stdin_Does_Not_Kill_Parent) {
    SpawnOpts o;
    o.exe = "/bin/sh";
    o.argv = {"sh", "-c", "exec 0<&-; sleep 0.02; echo alive"};
    o.stdinData = std::string(2 << 20, 'x');
    o.timeoutMs = 2000;
    auto r = spawn(o);
    CHECK(r.ok && r.out == "alive\n");
    return "";
}

TEST(process_Flood_Timeout_And_Bounded_Callbacks) {
    SpawnOpts o;
    o.exe = "/bin/sh";
    o.argv = {"sh", "-c", "while :; do printf 'lots of output\\n'; done"};
    o.outLimit = 128;
    o.timeoutMs = 150;
    size_t seen = 0;
    o.onChunk = [&](std::string_view s, bool) { seen += s.size(); };
    auto start = nowMs();
    auto r = spawn(o);
    CHECK(r.timedOut && r.truncated && seen == 128 && nowMs() - start < 3000);
    o.stopOnLimit = true;
    o.timeoutMs = 3000;
    r = spawn(o);
    CHECK(r.truncated && !r.ok && !r.timedOut);
    return "";
}

TEST(process_Child_Exit_Still_Drains_Descendants) {
    SpawnOpts o;
    o.exe = "/bin/sh";
    o.argv = {"sh", "-c", "(sleep 0.03; echo late) & exit 0"};
    o.timeoutMs = 1000;
    auto r = spawn(o);
    CHECK(r.ok && r.out == "late\n");
    o.argv = {"sh", "-c", "exec 1>&- 2>&-; sleep 0.03"};
    r = spawn(o);
    CHECK(r.ok && r.out.empty());
    return "";
}

TEST(process_Path_Uses_Child_Environment_And_Workdir) {
    std::string dir = makeTempDir("pocket-exe");
    CHECK(atomicWriteFile(dir + "/local-program", "#!/bin/sh\nprintf right", 0755).ok);
    SpawnOpts o;
    o.exe = "local-program";
    o.env = {"PATH=."};
    o.workdir = dir;
    auto r = spawn(o);
    CHECK(r.ok && r.out == "right");
    o.env = {"PATH=/nonexistent"};
    CHECK(!spawn(o).ok);  // no fallback to the workspace when PATH has no match
    rmRf(dir);
    return "";
}

namespace {
std::atomic<bool> nestedCancel{false};
static_assert(std::atomic<bool>::is_always_lock_free);
void cancelNestedFixture(int) { nestedCancel.store(true); }
}

TEST(process_Cancel_Allows_Recursive_Group_Cleanup) {
    std::string dir = makeTempDir("pocket-nested-cancel");
    CHECK(!dir.empty());
    SpawnOpts inner;
    inner.exe = "/bin/sh";
    inner.argv = {"sh", "-c", "trap '' TERM; printf ready; sleep 5; printf changed > '" + dir + "/late'"};
    inner.timeoutMs = 7000;
    inner.cancel = &nestedCancel;
    inner.onChunk = [](std::string_view text, bool err) {
        (void)!write(err ? STDERR_FILENO : STDOUT_FILENO, text.data(), text.size());
    };
    SpawnOpts outer;
    outer.exe = "/bin/false";
    outer.timeoutMs = 4000;
    outer.terminateGraceMs = 1500;  // parent permits the nested 750 ms escalation and receipt
    std::atomic<bool> cancel{false};
    outer.cancel = &cancel;
    outer.onChunk = [&](std::string_view text, bool) {
        if (text.find("ready") != std::string_view::npos) cancel = true;
    };
    // Run a small recursive-harness fixture in the forked process. Its TERM
    // handler mirrors print mode, and its tool owns a separate process group.
    outer.childSetup = [&] {
        struct sigaction action{};
        action.sa_handler = cancelNestedFixture;
        sigemptyset(&action.sa_mask);
        sigaction(SIGTERM, &action, nullptr);
        auto result = spawn(inner);
        if (result.cancelled) (void)!write(STDOUT_FILENO, "cleaned up\n", 11);
        _exit(result.cancelled ? 0 : 1);
    };
    auto start = nowMs();
    auto result = spawn(outer);
    CHECK(result.cancelled && !result.ok && nowMs() - start < 3000);
    CHECK(result.out.find("cleaned up") != std::string::npos);
    CHECK(access((dir + "/late").c_str(), F_OK) != 0);
    rmRf(dir);
    return "";
}

TEST(process_Timeout_Escalates_After_Leader_Exits) {
    SpawnOpts opts;
    opts.exe = "/bin/sh";
    opts.argv = {"sh", "-c", "(trap '' TERM; sleep 30) & exit 0"};
    opts.timeoutMs = 100;
    auto start = nowMs();
    auto result = spawn(opts);
    CHECK(result.timedOut && !result.ok && nowMs() - start < 3000);
    return "";
}

TEST(process_Parent_Death_Terminates_Child) {
    int ready[2];
    CHECK(pipe(ready) == 0);
    pid_t parent = fork();
    CHECK(parent >= 0);
    if (parent == 0) {
        close(ready[0]);
        SpawnOpts opts;
        opts.exe = "/bin/sleep";
        opts.argv = {"sleep", "30"};
        opts.childSetup = [&] {
            pid_t child = getpid();
            (void)!write(ready[1], &child, sizeof(child));
        };
        (void)spawn(opts);
        _exit(0);
    }
    close(ready[1]);
    pollfd pfd{ready[0], POLLIN, 0};
    pid_t child = -1;
    bool started = poll(&pfd, 1, 2000) > 0 && read(ready[0], &child, sizeof(child)) == sizeof(child);
    kill(parent, SIGKILL);
    int status;
    while (waitpid(parent, &status, 0) < 0 && errno == EINTR) {}
    pfd.revents = 0;
    bool closed = started && poll(&pfd, 1, 2000) > 0 && (pfd.revents & POLLHUP);
    if (!closed && child > 0) kill(child, SIGKILL);
    close(ready[0]);
    CHECK(started && closed);
    return "";
}

TEST(process_Linger_Detaches_Background_Holders) {
    SpawnOpts opts;
    opts.exe = "/bin/sh";
    opts.argv = {"sh", "-c", "echo up; sleep 30 & exit 3"};
    opts.timeoutMs = 20000;
    opts.lingerMs = 200;
    auto start = nowMs();
    auto result = spawn(opts);
    CHECK(result.detached && !result.timedOut && result.exitCode == 3 && result.out == "up\n");
    CHECK(nowMs() - start < 3000);
    return "";
}

TEST(process_Session_Exit_Stops_Background_Groups) {
    SpawnOpts opts;
    opts.exe = "/bin/sh";
    opts.argv = {"sh", "-c", "sleep 30 >/dev/null 2>&1 & echo $!"};
    opts.timeoutMs = 20000;
    auto result = spawn(opts);
    pid_t bg = (pid_t)std::atol(result.out.c_str());
    CHECK(result.ok && bg > 1 && kill(bg, 0) == 0);  // outlived its call
    killSessionProcesses(1000);
    bool gone = false;
    for (int i = 0; i < 80 && !gone; ++i) {
        gone = kill(bg, 0) != 0;
        if (!gone) usleep(25000);
    }
    CHECK(gone);
    return "";
}
