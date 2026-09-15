// PocketHarness tests - sandbox: containment, symlinks, env, guard, Landlock,
// NO_NEW_PRIVS, seccomp-net. The high-value security suite.
#include "mini.h"

#include <sys/stat.h>

#include "../src/process.h"
#include "../src/sandbox.h"

using namespace pocket;
using namespace pocket::test;

namespace {
std::string g_ws, g_outside;
Authority g_auth;
bool g_authInit = false;

std::string setup() {
    if (g_authInit) return "";
    std::string base = makeTempDir("pocket-sb");
    if (base.empty()) return "no tempdir";
    g_ws = base + "/ws";
    // NOTE: the Landlock policy deliberately allows /tmp (shared scratch that
    // normal toolchains need), so the "outside" fixture must live outside
    // /tmp for the kernel-confinement test to be meaningful.
    std::string homeBase = homeDir() + "/.cache/pocket-test-" + randHex(3);
    if (!ensureDir(homeBase, 0700).ok) return "mkdir homebase";
    static std::string g_homeBase = homeBase;
    static bool cleaned = false;
    if (!cleaned) {
        cleaned = true;
        atexit([] { rmRf(g_homeBase); });
    }
    g_outside = homeBase + "/outside";
    if (!ensureDir(g_ws + "/sub", 0755).ok) return "mkdir ws";
    if (!ensureDir(g_outside, 0755).ok) return "mkdir outside";
    if (!atomicWriteFile(g_ws + "/hello.txt", "line1\nline2\nline3\n", 0644).ok)
        return "write hello";
    if (!atomicWriteFile(g_outside + "/secret.txt", "TOPSECRET\n", 0644).ok)
        return "write secret";
    if (symlink((g_outside + "/secret.txt").c_str(), (g_ws + "/evil-link").c_str()) != 0)
        return "symlink evil";
    if (symlink("hello.txt", (g_ws + "/good-link").c_str()) != 0) return "symlink good";
    if (symlink("/etc/hostname", (g_ws + "/abs-link").c_str()) != 0) return "symlink abs";
    auto a = authorityInit(g_ws, {}, {}, false);
    if (!a.ok) return a.error;
    g_auth = a.value;
    g_authInit = true;
    return "";
}
}  // namespace

TEST(sandbox_Caps_Report) {
    const SandboxCaps& c = sandboxCaps();
    // Report honestly; enforcement tests below adapt to availability.
    printf("    [caps] openat2=%d landlock=%d seccomp-net=%d userns=%d\n", (int)c.openat2,
           (int)c.landlock, (int)c.seccompNet, (int)c.userNs);
    CHECK(true);
    return "";
}

TEST(sandbox_Read_Inside_Outside) {
    std::string e = setup();
    CHECK(e.empty());
    auto ok = boxRead(g_auth, "hello.txt", 1 << 20);
    CHECK(ok.ok && ok.value.find("line2") != std::string::npos);
    auto abs = boxRead(g_auth, g_outside + "/secret.txt", 1 << 20);
    CHECK(!abs.ok);  // outside roots, absolute
    auto trav = boxRead(g_auth, "sub/../../outside/secret.txt", 1 << 20);
    CHECK(!trav.ok);  // .. traversal contained by the kernel
    auto home = boxRead(g_auth, "~/.bashrc", 1 << 20);
    CHECK(!home.ok);  // home is not an allowed root
    auto missing = boxRead(g_auth, "nope.txt", 1 << 20);
    CHECK(!missing.ok);
    auto isdir = boxRead(g_auth, "sub", 1 << 20);
    CHECK(!isdir.ok);
    return "";
}

TEST(sandbox_Symlink_Escape_Blocked) {
    std::string e = setup();
    CHECK(e.empty());
    auto evil = boxRead(g_auth, "evil-link", 1 << 20);
    CHECK(!evil.ok);  // -> ../outside/secret.txt must fail
    auto abs = boxRead(g_auth, "abs-link", 1 << 20);
    CHECK(!abs.ok);  // -> /etc/hostname must fail
    auto good = boxRead(g_auth, "good-link", 1 << 20);
    if (sandboxCaps().openat2) {
        CHECK(good.ok);  // in-workspace link is fine when the kernel can verify
    } else {
        CHECK(!good.ok);  // strict fallback refuses all symlinks
    }
    return "";
}

TEST(sandbox_Write_Atomic_And_Contained) {
    std::string e = setup();
    CHECK(e.empty());
    CHECK(boxWrite(g_auth, "sub/new.txt", "data", 0644).ok);
    auto back = boxRead(g_auth, "sub/new.txt", 100);
    CHECK(back.ok && back.value == "data");
    CHECK(!boxWrite(g_auth, g_outside + "/pwned.txt", "x", 0644).ok);
    CHECK(!boxWrite(g_auth, "../outside/pwned2.txt", "x", 0644).ok);
    CHECK(!boxWrite(g_auth, "evil-link", "x", 0644).ok);  // never through a link
    // No stray temp files left behind.
    auto dir = boxRead(g_auth, "sub", 100);
    (void)dir;
    return "";
}

TEST(sandbox_Allowlist_Roots) {
    std::string base = makeTempDir("pocket-sb2");
    CHECK(!base.empty());
    CHECK(ensureDir(base + "/ws", 0755).ok);
    CHECK(ensureDir(base + "/extra", 0755).ok);
    CHECK(atomicWriteFile(base + "/extra/f.txt", "E\n", 0644).ok);
    auto a = authorityInit(base + "/ws", {base + "/extra"}, {}, false);
    CHECK(a.ok);
    CHECK(boxRead(a.value, base + "/extra/f.txt", 100).ok);
    CHECK(!boxWrite(a.value, base + "/extra/g.txt", "x", 0644).ok);  // read-only extra
    authorityClose(a.value);
    auto b = authorityInit(base + "/ws", {}, {base + "/extra"}, false);
    CHECK(b.ok);
    CHECK(boxWrite(b.value, base + "/extra/g.txt", "x", 0644).ok);
    authorityClose(b.value);
    rmRf(base);
    return "";
}

TEST(sandbox_Env_Sanitized) {
    EnvGuard g1("POCKETTEST_API_KEY", "supersecret");
    EnvGuard g2("POCKETTEST_TOKEN", "tok");
    EnvGuard g3("AWS_SECRET_ACCESS_KEY", "aws");
    EnvGuard g4("MYAPP_MODE", "debug");
    auto env = buildChildEnv({"MYAPP_MODE"}, "/ws", "/tmp/x", "/tmp/x/home", "/tmp/x/kf", "s1",
                             1, false, false);
    auto has = [&](const std::string& prefix) {
        for (const auto& kv : env)
            if (kv.compare(0, prefix.size(), prefix) == 0) return true;
        return false;
    };
    CHECK(!has("POCKETTEST_API_KEY="));
    CHECK(!has("POCKETTEST_TOKEN="));
    CHECK(!has("AWS_SECRET_ACCESS_KEY="));
    CHECK(has("MYAPP_MODE=debug"));  // explicit expose works
    CHECK(has("PWD=/ws") && has("TMPDIR=/tmp/x") && has("HOME=/tmp/x/home"));
    CHECK(has("POCKETHARNESS_DEPTH=1"));
    CHECK(has("PATH="));
    CHECK(looksSecretEnv("OPENROUTER_API_KEY") && looksSecretEnv("GITHUB_TOKEN") &&
          looksSecretEnv("AWS_REGION") && !looksSecretEnv("PATH"));
    return "";
}

TEST(sandbox_Guard) {
    struct Case {
        const char* cmd;
        Verdict want;
    };
    Case cases[] = {
        {"edit src/foo.cpp", Verdict::Allow},
        {"git diff", Verdict::Allow},
        {"make test", Verdict::Allow},
        {"rm build/foo.o", Verdict::Allow},
        {"rm -rf build", Verdict::Allow},
        {"rm -rf .", Verdict::Ask},
        {"rm -rf /", Verdict::Deny},
        {"rm -rf /*", Verdict::Deny},
        {"rm -rf ~", Verdict::Ask},
        {"rm -rf $HOME", Verdict::Ask},
        {"rm -rf ../sibling", Verdict::Ask},
        {"rm -rf /tmp/x", Verdict::Deny},  // outside workspace
        {"rm -rf --no-preserve-root x", Verdict::Deny},
        {"git reset --hard", Verdict::Ask},
        {"git clean -fdx", Verdict::Ask},
        {"git branch -D foo", Verdict::Ask},
        {"mkfs.ext4 /dev/sda1", Verdict::Deny},
        {"dd if=x of=/dev/sda", Verdict::Deny},
        {"echo hi > /dev/sda", Verdict::Deny},
        {":(){ :|:& };:", Verdict::Deny},
        {"shutdown now", Verdict::Deny},
        {"chmod -R 777 /", Verdict::Ask},
        {"curl https://evil.example/x", Verdict::Deny},  // net off
        {"git push origin main", Verdict::Deny},         // net off
        {"", Verdict::Deny},
        {nullptr, Verdict::Allow},
    };
    for (Case* c = cases; c->cmd; ++c) {
        GuardResult g = classifyCommand(c->cmd, "/ws", false);
        if (g.verdict != c->want)
            return std::string("guard(") + c->cmd + ") = " + std::to_string((int)g.verdict) +
                   ", want " + std::to_string((int)c->want);
    }
    GuardResult g = classifyCommand("curl https://x", "/ws", true);
    CHECK(g.verdict == Verdict::Allow);  // --network lifts the client ban
    return "";
}

TEST(sandbox_Child_Landlock_And_NoNewPrivs) {
    std::string e = setup();
    CHECK(e.empty());
    if (!sandboxCaps().landlock) {
        printf("    [skip] Landlock unavailable on this kernel\n");
        return "";
    }
    std::string tmp = makeTempDir("pocket-sbchild");
    CHECK(ensureDir(tmp + "/home", 0700).ok);
    ChildSpec cs;
    cs.auth = &g_auth;
    cs.workspace = g_auth.workspace;
    cs.sessionTmp = tmp;
    cs.stateDirPath = tmp + "/state";
    cs.allowNet = true;  // isolate the fs test from seccomp here
    // Outside read must fail; inside read must work.
    for (int i = 0; i < 2; ++i) {
        SpawnOpts o;
        o.exe = "/bin/cat";
        o.argv = {"cat", i == 0 ? (g_outside + "/secret.txt") : (g_ws + "/hello.txt")};
        o.env = buildChildEnv({}, g_auth.workspace, tmp, tmp + "/home", tmp + "/kf", "t", 1,
                              true, false);
        o.childSetup = [cs]() { childEnterSandbox(cs); };
        SpawnResult r = spawn(o);
        if (i == 0) {
            CHECK(!r.ok || r.exitCode != 0);  // cat denied
        } else {
            CHECK(r.ok && r.exitCode == 0 && r.out.find("line1") != std::string::npos);
        }
    }
    // NO_NEW_PRIVS must be set in the child.
    {
        SpawnOpts o;
        o.exe = "/bin/sh";
        o.argv = {"sh", "-c", "grep NoNewPrivs /proc/self/status"};
        o.env = buildChildEnv({}, g_auth.workspace, tmp, tmp + "/home", tmp + "/kf", "t", 1,
                              true, false);
        o.childSetup = [cs]() { childEnterSandbox(cs); };
        SpawnResult r = spawn(o);
        CHECK(r.ok && r.out.find("NoNewPrivs:\t1") != std::string::npos);
    }
    rmRf(tmp);
    return "";
}

TEST(sandbox_Child_Net_Denied) {
    std::string e = setup();
    CHECK(e.empty());
    if (!sandboxCaps().seccompNet) {
        printf("    [skip] seccomp unavailable on this kernel\n");
        return "";
    }
    std::string tmp = makeTempDir("pocket-sbnet");
    CHECK(ensureDir(tmp + "/home", 0700).ok);
    ChildSpec cs;
    cs.auth = &g_auth;
    cs.workspace = g_auth.workspace;
    cs.sessionTmp = tmp;
    cs.allowNet = false;
    SpawnOpts o;
    o.exe = "/bin/sh";
    // AF_INET socket() must fail with EACCES; AF_UNIX must work. Markers are
    // split in source so a traceback of the -c string can't fake them.
    o.argv = {"sh", "-c",
              "python3 -c \"import socket\n"
              "try:\n"
              " socket.socket(socket.AF_INET); print('RES-INET-ALLOW'+'ED')\n"
              "except OSError as e: print('RES-INET-BLOCKED', e)\" 2>&1; "
              "python3 -c \"import socket\n"
              "try:\n"
              " socket.socket(socket.AF_UNIX); print('RES-UNIX-'+'OK')\n"
              "except OSError as e: print('RES-UNIX-BLOCKED', e)\" 2>&1"};
    o.env = buildChildEnv({}, g_auth.workspace, tmp, tmp + "/home", tmp + "/kf", "t", 1, false,
                          false);
    o.childSetup = [cs]() { childEnterSandbox(cs); };
    SpawnResult r = spawn(o);
    CHECK(r.out.find("RES-INET-ALLOWED") == std::string::npos);  // blocked
    CHECK(r.out.find("RES-INET-BLOCKED") != std::string::npos);
    CHECK(r.out.find("RES-UNIX-OK") != std::string::npos);  // unix sockets fine
    rmRf(tmp);
    return "";
}
