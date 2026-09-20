// PocketHarness tests - process capture/timeout/cancel.
#include "mini.h"

#include <signal.h>

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
