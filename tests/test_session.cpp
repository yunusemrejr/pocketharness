// PocketHarness tests - sessions (incl. crash-safe partial lines).
#include "mini.h"

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
    m.turns = 3;
    m.toolCalls = 7;
    m.cacheHit = 100;
    m.cacheSeen = true;
    CHECK(sessionSaveMeta(id.value, m).ok);
    auto back = sessionLoadMeta(id.value);
    CHECK(back.ok);
    CHECK_EQ(back.value.systemPrompt, std::string("SYS"));
    CHECK_EQ(back.value.orSessionId, std::string("OR123"));
    CHECK_EQ(back.value.turns, 3L);
    CHECK_EQ(back.value.toolCalls, 7L);
    CHECK_EQ(back.value.cacheHit, 100L);
    CHECK(back.value.cacheSeen && !back.value.costSeen);
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
