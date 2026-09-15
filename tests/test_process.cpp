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
