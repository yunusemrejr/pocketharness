// PocketHarness tests - sessions (incl. crash-safe partial lines).
#include "mini.h"
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>

#include "../src/config.h"
#include "../src/session.h"

using namespace pocket;
using namespace pocket::test;

TEST(session_Append_Load_Crash_Safe) {
    std::string home = makeTempDir("pocket-sess");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    CHECK(sessionAppend(id.value, SessionEvent{"user", "hello", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"assistant", "hi", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value,
                        SessionEvent{"tool_call", "", "c1", "read", "{\"path\":\"x\"}", true})
              .ok);
    // Simulate a crash mid-write: partial line without newline.
    {
        FILE* f = fopen((sessionDir() + "/" + id.value + ".jsonl").c_str(), "a");
        CHECK(f);
        fwrite("{\"t\":\"tool_res", 1, 14, f);
        fclose(f);
    }
    auto loaded = sessionLoad(id.value);
    CHECK(loaded.ok);
    CHECK_EQ(loaded.value.events.size(), (size_t)4);  // system + 3
    CHECK_EQ(loaded.value.skipped, 1L);
    CHECK_EQ(loaded.value.events[1].type, std::string("user"));
    CHECK_EQ(loaded.value.events[3].toolName, std::string("read"));
    // No API keys in session files (we never write env there; assert absence).
    auto raw = readFileBounded(sessionDir() + "/" + id.value + ".jsonl", 1 << 20);
    CHECK(raw.ok && raw.value.find("API_KEY") == std::string::npos);
    rmRf(home);
    return "";
}

TEST(session_Meta_Roundtrip) {
    std::string home = makeTempDir("pocket-sessmeta");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    auto empty = sessionLoadMeta(id.value);
    CHECK(empty.ok && empty.value.orSessionId.empty());
    SessionMeta m;
    m.systemPrompt = "SYS";
    m.orSessionId = "OR123";
    m.modelSpec = "p:model";
    m.systemSource = "/tmp/sys.md";
    m.thinking = "max";
    m.turns = 3;
    m.toolCalls = 7;
    m.cacheHit = 100;
    m.cacheSeen = true;
    m.genMs = 42000;
    m.sideCost = 0.125;
    m.childSessions = 2;
    m.costEstimated = true;
    m.costIncomplete = true;
    m.genTokens = 28;
    m.rolesSet = true;
    m.doubleEnabled = true;
    m.roles["decision"] = "local:jev";
    CHECK(sessionSaveMeta(id.value, m).ok);
    auto back = sessionLoadMeta(id.value);
    CHECK(back.ok);
    CHECK_EQ(back.value.systemPrompt, std::string("SYS"));
    CHECK_EQ(back.value.orSessionId, std::string("OR123"));
    CHECK_EQ(back.value.modelSpec, std::string("p:model"));
    CHECK_EQ(back.value.thinking, std::string("max"));
    CHECK_EQ(back.value.turns, 3L);
    CHECK_EQ(back.value.toolCalls, 7L);
    CHECK_EQ(back.value.cacheHit, 100L);
    CHECK_EQ(back.value.genMs, 42000L);
    CHECK(back.value.cacheSeen && !back.value.costSeen);
    CHECK_EQ(back.value.sideCost, 0.125);
    CHECK_EQ(back.value.childSessions, 2L);
    CHECK(back.value.costEstimated && back.value.rolesSet);
    CHECK(back.value.doubleEnabled);
    CHECK(back.value.costIncomplete);
    CHECK_EQ(back.value.genTokens, 28L);
    CHECK_EQ(back.value.roles.at("decision"), std::string("local:jev"));
    m.roles.clear();
    CHECK(sessionSaveMeta(id.value, m).ok);
    CHECK(sessionLoadMeta(id.value).value.rolesSet);
    CHECK(sessionLoadMeta(id.value).value.roles.empty());
    rmRf(home);
    return "";
}

TEST(session_List_Resolve) {
    std::string home = makeTempDir("pocket-sess2");
    CHECK(!home.empty());
    HomeGuard hg(home);
    CHECK(sessionList().empty());
    auto a = sessionCreate();
    auto b = sessionCreate();
    CHECK(a.ok && b.ok);
    CHECK(sessionAppend(b.value, SessionEvent{"user", "second session topic", "", "", "", true})
              .ok);
    auto list = sessionList();
    CHECK_EQ(list.size(), (size_t)2);
    auto last = sessionResolve("");
    CHECK(last.ok);
    auto bad = sessionResolve("../../etc");
    CHECK(!bad.ok);
    auto missing = sessionResolve("nope-nope");
    CHECK(!missing.ok);
    rmRf(home);
    return "";
}

TEST(session_Single_Writer_And_Workspace_Selection) {
    std::string dir = makeTempDir("pocket-lock");
    HomeGuard hg(dir);
    auto a = sessionCreate(), b = sessionCreate();
    CHECK(a.ok && b.ok);
    SessionMeta meta;
    meta.workspace = "/project-a";
    CHECK(sessionSaveMeta(a.value, meta).ok);
    meta.workspace = "/project-b";
    CHECK(sessionSaveMeta(b.value, meta).ok);
    auto lock = sessionLock(a.value);
    CHECK(lock.ok);
    CHECK(!sessionLock(a.value).ok);
    CHECK(sessionList(30, "/project-a")[0].active);
    CHECK(!sessionResolve("last", "/project-a").ok);
    CHECK_EQ(sessionResolve("last", "/project-b").value, b.value);
    close(lock.value);
    CHECK_EQ(sessionResolve("last", "/project-a").value, a.value);
    rmRf(dir);
    return "";
}

TEST(session_Append_After_Torn_Tail) {
    std::string dir = makeTempDir("pocket-torn");
    HomeGuard hg(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    auto path = sessionDir() + "/" + id.value + ".jsonl";
    FILE* f = fopen(path.c_str(), "a");
    CHECK(f);
    fputs("{\"t\":\"assistant\",\"text\":\"torn", f);
    fclose(f);
    CHECK(sessionAppend(id.value, {"user", "kept", "", "", "", true}).ok);
    auto loaded = sessionLoad(id.value);
    CHECK(loaded.ok && loaded.value.events.back().text == "kept");
    struct stat st{};
    CHECK(stat(path.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600);
    rmRf(dir);
    return "";
}

TEST(session_Rejects_Nonregular_Files_And_Invalid_Ids) {
    std::string dir = makeTempDir("pocket-filetypes");
    HomeGuard hg(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    const std::string base = sessionDir() + "/";
    CHECK(mkfifo((base + "pipe.jsonl").c_str(), 0600) == 0);
    CHECK(symlink((base + id.value + ".jsonl").c_str(), (base + "link.jsonl").c_str()) == 0);
    CHECK(!sessionLoad("pipe").ok && !sessionLoad("link").ok);
    CHECK(!sessionResolve("pipe").ok && !sessionResolve("link").ok);
    CHECK(!sessionLock("pipe").ok && !sessionLock("link").ok);
    CHECK(!sessionAppend("pipe", {"user", "no", "", "", "", true}).ok);
    CHECK_EQ(sessionList().size(), (size_t)1);
    CHECK(mkfifo((base + id.value + ".meta.json").c_str(), 0600) == 0);
    CHECK(!sessionLoadMeta(id.value).ok);
    CHECK(unlink((base + id.value + ".meta.json").c_str()) == 0);
    CHECK(unlink((base + id.value + ".lock").c_str()) == 0);
    CHECK(mkfifo((base + id.value + ".lock").c_str(), 0600) == 0);
    CHECK(!sessionLock(id.value).ok);
    CHECK(!sessionLoad("../escape").ok && !sessionLoadMeta("../escape").ok);
    CHECK(!sessionSaveMeta("../escape", {}).ok && !sessionLock("../escape").ok);
    CHECK(!sessionLock("not-created").ok);
    rmRf(dir);
    return "";
}

TEST(session_Newest_Uses_Activity_And_Resolve_Scans_Past_30_Active) {
    std::string dir = makeTempDir("pocket-recency");
    HomeGuard hg(dir);
    auto idle = sessionCreate();
    CHECK(idle.ok);
    struct timespec oldTime[2]{{1, 0}, {1, 0}};
    CHECK(utimensat(AT_FDCWD, (sessionDir() + "/" + idle.value + ".jsonl").c_str(), oldTime, 0) == 0);
    std::vector<int> locks;
    for (int i = 0; i < 31; ++i) {
        auto active = sessionCreate();
        CHECK(active.ok);
        auto lease = sessionLock(active.value);
        CHECK(lease.ok);
        locks.push_back(lease.value);
    }
    auto resolved = sessionResolve("last");
    CHECK(resolved.ok && resolved.value == idle.value);
    CHECK(sessionAppend(idle.value, {"user", "new activity", "", "", "", true}).ok);
    CHECK_EQ(sessionList(1)[0].id, idle.value);
    for (int lock : locks) close(lock);
    SessionMeta meta;
    meta.workspace = "/another-workspace";
    CHECK(sessionSaveMeta(idle.value, meta).ok);
    CHECK(!sessionResolve(idle.value, "/this-workspace").ok);
    rmRf(dir);
    return "";
}

TEST(session_Append_And_Load_Respect_Record_Lock) {
    std::string dir = makeTempDir("pocket-recordlock");
    HomeGuard hg(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    int fd = open((sessionDir() + "/" + id.value + ".jsonl").c_str(), O_RDWR);
    CHECK(fd >= 0 && flock(fd, LOCK_EX) == 0);
    std::atomic<bool> started{false}, appended{false}, loaded{false};
    bool appendOk = false, loadOk = false;
    std::thread writer([&] {
        started = true;
        appendOk = sessionAppend(id.value, {"user", "committed", "", "", "", true}).ok;
        appended = true;
    });
    std::thread reader([&] { loadOk = sessionLoad(id.value).ok; loaded = true; });
    while (!started.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    bool bothBlocked = !appended.load() && !loaded.load();
    close(fd);
    writer.join();
    reader.join();
    CHECK(bothBlocked && appendOk && loadOk);
    CHECK_EQ(sessionLoad(id.value).value.events.back().text, std::string("committed"));
    rmRf(dir);
    return "";
}

TEST(session_Multiprocess_Appends_Preserve_All_Records) {
    std::string dir = makeTempDir("pocket-appenders");
    HomeGuard hg(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    std::vector<pid_t> children;
    for (int child = 0; child < 4; ++child) {
        pid_t pid = fork();
        CHECK(pid >= 0);
        if (!pid) {
            for (int event = 0; event < 12; ++event)
                if (!sessionAppend(id.value, {"user", std::string(16384, 'a' + child), "", "", "", true}).ok)
                    _exit(1);
            _exit(0);
        }
        children.push_back(pid);
    }
    for (pid_t pid : children) {
        int status;
        CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    auto loaded = sessionLoad(id.value);
    CHECK(loaded.ok && loaded.value.skipped == 0);
    CHECK_EQ(loaded.value.events.size(), (size_t)49);
    for (size_t i = 1; i < loaded.value.events.size(); ++i) {
        const std::string& text = loaded.value.events[i].text;
        CHECK(text.size() == 16384 && text == std::string(16384, text[0]));
    }
    rmRf(dir);
    return "";
}

TEST(session_Metadata_Size_Limit_Preserves_Previous_Snapshot) {
    std::string dir = makeTempDir("pocket-metalimit");
    HomeGuard hg(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    SessionMeta meta;
    meta.systemPrompt = "kept";
    CHECK(sessionSaveMeta(id.value, meta).ok);
    meta.systemPrompt.assign(1u << 20, 'x');
    CHECK(!sessionSaveMeta(id.value, meta).ok);
    CHECK_EQ(sessionLoadMeta(id.value).value.systemPrompt, std::string("kept"));
    CHECK(atomicWriteFile(sessionDir() + "/" + id.value + ".meta.json", "", 0600).ok);
    CHECK(!sessionLoadMeta(id.value).ok);
    rmRf(dir);
    return "";
}

TEST(session_Workspace_Notices_Are_Isolated_Bounded_And_Ordered) {
    std::string dir = makeTempDir("pocket-notices");
    HomeGuard hg(dir);
    const std::string work = dir + "/work", other = dir + "/other", alias = dir + "/alias";
    CHECK(ensureDir(work).ok && ensureDir(other).ok && symlink(work.c_str(), alias.c_str()) == 0);
    auto empty = sessionWorkspaceRead(work);
    CHECK(empty.ok && empty.value.events.empty() && empty.value.lastSequence == 0);
    CHECK(sessionWorkspacePublish(work, "a", "write", "first.cpp").ok);
    CHECK(sessionWorkspacePublish(alias, "b", "write", "second.cpp").ok);
    auto notices = sessionWorkspaceRead(work, 0, "a");
    CHECK(notices.ok && notices.value.lastSequence == 2 && !notices.value.missed);
    CHECK_EQ(notices.value.events.size(), (size_t)1);
    CHECK_EQ(notices.value.events[0].sessionId, std::string("b"));
    CHECK_EQ(notices.value.events[0].text, std::string("second.cpp"));
    CHECK(sessionWorkspaceRead(other).value.events.empty());
    CHECK(sessionWorkspaceRead(work, 2).value.events.empty());
    CHECK(!sessionWorkspacePublish(work, "../bad", "write", "bad").ok);
    CHECK(!sessionWorkspaceRead(work, -1).ok);
    for (int i = 0; i < 130; ++i)
        CHECK(sessionWorkspacePublish(work, "a", "edit", std::string(4096, '\xff')).ok);
    notices = sessionWorkspaceRead(alias, 1);
    CHECK(notices.ok && notices.value.missed && notices.value.events.size() == 128);
    CHECK_EQ(notices.value.lastSequence, 132L);
    CHECK_EQ(notices.value.events.front().sequence, 5L);
    for (const auto& event : notices.value.events) CHECK(event.text.size() <= 2048);
    auto reset = sessionWorkspaceRead(work, 999);
    CHECK(reset.ok && reset.value.missed && reset.value.events.size() == 128);
    rmRf(dir);
    return "";
}

TEST(session_Workspace_Lock_Is_Separate_Cancellable_And_Canonical) {
    std::string dir = makeTempDir("pocket-worklock");
    HomeGuard hg(dir);
    const std::string work = dir + "/work", other = dir + "/other", alias = dir + "/alias";
    CHECK(ensureDir(work).ok && ensureDir(other).ok && symlink(work.c_str(), alias.c_str()) == 0);
    auto lease = sessionWorkspaceLock(work), separate = sessionWorkspaceLock(other);
    CHECK(lease.ok && separate.ok);
    CHECK(sessionWorkspacePublish(work, "a", "write", "during mutation").ok);
    std::atomic<bool> cancel{false}, entered{false}, done{false};
    Result<int> waiting;
    std::thread waiter([&] { entered = true; waiting = sessionWorkspaceLock(alias, &cancel); done = true; });
    while (!entered.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    bool blocked = !done.load();
    cancel = true;
    waiter.join();
    close(lease.value);
    close(separate.value);
    CHECK(blocked && !waiting.ok && waiting.error == "cancelled");
    lease = sessionWorkspaceLock(alias);
    CHECK(lease.ok && (fcntl(lease.value, F_GETFD) & FD_CLOEXEC));
    close(lease.value);
    rmRf(dir);
    return "";
}

TEST(session_Workspace_Multiprocess_Publication_Keeps_Every_Event) {
    std::string dir = makeTempDir("pocket-workpub");
    HomeGuard hg(dir);
    std::vector<pid_t> children;
    for (int child = 0; child < 4; ++child) {
        pid_t pid = fork();
        CHECK(pid >= 0);
        if (!pid) {
            for (int event = 0; event < 8; ++event)
                if (!sessionWorkspacePublish(dir, std::to_string(child), "write", std::to_string(event)).ok)
                    _exit(1);
            _exit(0);
        }
        children.push_back(pid);
    }
    for (pid_t pid : children) {
        int status;
        CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    auto notices = sessionWorkspaceRead(dir);
    CHECK(notices.ok && notices.value.lastSequence == 32 && notices.value.events.size() == 32);
    CHECK(!notices.value.missed);
    for (size_t i = 0; i < notices.value.events.size(); ++i)
        CHECK_EQ(notices.value.events[i].sequence, (long)i + 1);
    rmRf(dir);
    return "";
}

TEST(session_Recursive_Coordination_Does_Not_Share_Transcripts) {
    std::string dir = makeTempDir("pocket-recursive");
    HomeGuard hg(dir);
    auto parent = sessionCreate();
    CHECK(parent.ok);
    CHECK(sessionWorkspacePublish(dir, parent.value, "write", "parent.cpp").ok);
    auto scope = sessionWorkspaceDirectory(dir);
    CHECK(scope.ok);
    std::string coordination = scope.value;
    CHECK(ensureDir(dir + "/child", 0700).ok);
    std::string childId;
    {
        HomeGuard childHome(dir + "/child");
        CHECK(sessionSetWorkspaceCoordinationDir(coordination).ok);
        struct Reset { ~Reset() { (void)sessionSetWorkspaceCoordinationDir(""); } } reset;
        auto child = sessionCreate();
        CHECK(child.ok);
        childId = child.value;
        CHECK(!sessionLoad(parent.value).ok);
        CHECK(sessionWorkspacePublish(dir, childId, "write", "child.cpp").ok);
        CHECK_EQ(sessionWorkspaceRead(dir, 0, childId).value.events[0].sessionId, parent.value);
    }
    CHECK(!sessionLoad(childId).ok);
    auto notices = sessionWorkspaceRead(dir, 0, parent.value);
    CHECK(notices.ok && notices.value.events.size() == 1);
    CHECK_EQ(notices.value.events[0].sessionId, childId);
    CHECK(ensureDir(dir + "/public", 0755).ok);
    CHECK(!sessionSetWorkspaceCoordinationDir(dir + "/public").ok);
    CHECK(symlink(coordination.c_str(), (dir + "/coord-link").c_str()) == 0);
    CHECK(!sessionSetWorkspaceCoordinationDir(dir + "/coord-link").ok);
    rmRf(dir);
    return "";
}

TEST(session_Outcome_Summary_Is_Bounded_And_Legacy_Compatible) {
    std::string home = makeTempDir("pocket-outcome-meta");
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    auto legacy = sessionLoadMeta(id.value);
    CHECK(legacy.ok && legacy.value.lastStopReason.empty() && legacy.value.lastStoppedAtMs == 0);
    SessionMeta meta;
    meta.workspace = home;
    meta.goal = std::string(512, 'g');
    meta.goalStatus = "paused";
    meta.lastStopReason = std::string(128, 'r');
    meta.lastStopDetail = std::string(2048, 'd');
    meta.lastStoppedAtMs = 1790450000123LL;
    CHECK(sessionSaveMeta(id.value, meta).ok);
    auto loaded = sessionLoadMeta(id.value);
    CHECK(loaded.ok);
    CHECK_EQ(loaded.value.lastStopReason.size(), size_t(64));
    CHECK_EQ(loaded.value.lastStopDetail.size(), size_t(1024));
    auto listed = sessionList(1, home);
    CHECK_EQ(listed.size(), size_t(1));
    CHECK_EQ(listed[0].goal.size(), size_t(256));
    CHECK_EQ(listed[0].goalStatus, std::string("paused"));
    CHECK_EQ(listed[0].lastStoppedAtMs, meta.lastStoppedAtMs);
    rmRf(home);
    return "";
}
