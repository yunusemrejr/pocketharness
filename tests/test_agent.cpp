// PocketHarness tests - agent prompt stability, frozen prefix, resume.
#include "mini.h"

#include "../src/agent.h"
#include "../src/config.h"
#include "../src/session.h"

using namespace pocket;
using namespace pocket::test;

TEST(agent_System_Prompt_Deterministic) {
    std::string ws = makeTempDir("pocket-ap");
    CHECK(!ws.empty());
    CHECK(ensureDir(ws + "/proj", 0755).ok);
    CHECK(atomicWriteFile(ws + "/proj/POCKET.md", "Use tabs.\n", 0644).ok);
    CHECK(atomicWriteFile(ws + "/proj/AGENTS.md", "Use spaces.\n", 0644).ok);
    std::string a = buildSystemPrompt(ws + "/proj");
    std::string b = buildSystemPrompt(ws + "/proj");
    CHECK_EQ(a, b);  // byte-stable across builds
    CHECK(a.find("Use tabs.") != std::string::npos);
    CHECK(a.find("Use spaces.") == std::string::npos);  // POCKET.md wins
    CHECK(a.find("Active workspace: " + ws + "/proj") != std::string::npos);
    CHECK(a.find("skill(action=list)") != std::string::npos);  // static skills line
    rmRf(ws);
    return "";
}

TEST(agent_Prefix_Stable_Across_Turns) {
    // Ordinary turns APPEND messages; the serialized prefix of earlier
    // requests must be byte-identical in later ones (cache reuse).
    Config cfg = defaultConfig();
    ResolvedModel m = resolveModel(cfg, "glm").value;
    auto bodyFor = [&](const std::vector<ChatMessage>& msgs) {
        ChatRequest req;
        req.model = m;
        req.system = "FROZEN-SYSTEM";
        req.messages = msgs;
        req.tools = nativeToolDefs();
        return buildOpenAiBody(req);
    };
    std::vector<ChatMessage> turn1 = {{"user", "q1", {}, ""}, {"assistant", "a1", {}, ""}};
    std::vector<ChatMessage> turn2 = turn1;
    turn2.push_back({"user", "load skill", {}, ""});
    turn2.push_back({"assistant", "", {{"c1", "skill", "{\"action\":\"load\"}"}}, ""});
    turn2.push_back({"tool", "# skill content...", {}, "c1"});  // skill arrives at the tail
    turn2.push_back({"assistant", "a2", {}, ""});
    json::Value b1 = bodyFor(turn1), b2 = bodyFor(turn2);
    CHECK_EQ(b1.at("messages").size(), (size_t)2);
    for (size_t i = 0; i < 2; ++i)
        CHECK_EQ(json::stringify(b1.at("messages").at(i)), json::stringify(b2.at("messages").at(i)));
    CHECK_EQ(json::stringify(b1.at("tools")), json::stringify(b2.at("tools")));
    return "";
}

TEST(agent_Frozen_Prefix_And_Sticky_Tag_Survive_Resume) {
    std::string home = makeTempDir("pocket-ameta");
    CHECK(!home.empty());
    HomeGuard hg(home);
    std::string ws = home + "/ws";
    CHECK(ensureDir(ws, 0755).ok);
    CHECK(atomicWriteFile(ws + "/POCKET.md", "v1 instructions\n", 0644).ok);
    auto id = sessionCreate();
    CHECK(id.ok);
    Config cfg = defaultConfig();
    ToolEnv env;
    env.workspace = ws;
    AgentOpts ao;
    ao.model = resolveModel(cfg, "glm").value;
    ao.tools = &env;
    ao.sessionId = id.value;
    Agent a1(ao);
    CHECK(!a1.systemPrompt().empty() && !a1.sessionTag().empty());
    CHECK(a1.systemPrompt().find("v1 instructions") != std::string::npos);
    // Project instructions change mid-life: the live session keeps its prefix.
    CHECK(atomicWriteFile(ws + "/POCKET.md", "v2 instructions\n", 0644).ok);
    Agent a2(ao);
    CHECK(a2.restore(id.value).ok);
    CHECK_EQ(a2.systemPrompt(), a1.systemPrompt());
    CHECK_EQ(a2.sessionTag(), a1.sessionTag());
    CHECK(a2.systemPrompt().find("v2 instructions") == std::string::npos);
    rmRf(home);
    return "";
}

TEST(agent_System_Prompt_Override) {
    std::string home = makeTempDir("pocket-asys");
    CHECK(!home.empty());
    HomeGuard hg(home);
    std::string ws = home + "/ws";
    CHECK(ensureDir(ws + "/.pocket", 0755).ok);
    CHECK_EQ(systemPromptSource(ws), std::string(""));  // built-in by default
    std::string builtin = buildSystemPrompt(ws);
    CHECK(builtin.find("engineering principles always apply") != std::string::npos);
    CHECK(builtin.find("Capabilities:") != std::string::npos);
    // Global override replaces the base but keeps instructions/workspace tail.
    CHECK(ensureDir(userConfigDir(), 0755).ok);
    CHECK(atomicWriteFile(userSystemPath(), "GLOBAL-BASE\n", 0644).ok);
    CHECK_EQ(systemPromptSource(ws), userSystemPath());
    std::string g = buildSystemPrompt(ws);
    CHECK(g.find("GLOBAL-BASE") != std::string::npos);
    CHECK(g.find("Active workspace: " + ws) != std::string::npos);
    // Project override wins; empty project file falls back to global.
    CHECK(atomicWriteFile(projectSystemPath(ws), "PROJECT-BASE\n", 0644).ok);
    CHECK_EQ(systemPromptSource(ws), projectSystemPath(ws));
    CHECK(buildSystemPrompt(ws).find("PROJECT-BASE") != std::string::npos);
    CHECK(atomicWriteFile(projectSystemPath(ws), "  \n", 0644).ok);
    CHECK_EQ(systemPromptSource(ws), userSystemPath());
    rmRf(home);
    return "";
}

TEST(agent_Compact_Cut_Points) {
    auto msg = [](const std::string& role) { return ChatMessage{role, "x", {}, ""}; };
    CHECK_EQ(compactCutPoint({msg("user"), msg("assistant")}), (size_t)2);  // too short
    // Short history starting at a user message: nothing worth dropping.
    CHECK_EQ(compactCutPoint({msg("user"), msg("assistant"), msg("user"), msg("assistant"),
                              msg("user")}),
             (size_t)5);
    // 12 alternating turns: keep last 8, cut lands on a user boundary.
    std::vector<ChatMessage> twelve;
    for (int i = 0; i < 12; ++i) twelve.push_back(msg(i % 2 == 0 ? "user" : "assistant"));
    CHECK_EQ(compactCutPoint(twelve), (size_t)4);
    CHECK_EQ(twelve[compactCutPoint(twelve)].role, std::string("user"));
    // Cut must skip past a tool exchange, never split a call from its result.
    std::vector<ChatMessage> chain = {msg("user"), msg("assistant")};
    ChatMessage acall{"assistant", "", {{"c1", "bash", "{}"}}, ""};
    chain.push_back(msg("user"));   // 2
    chain.push_back(acall);         // 3 <- naive keepLast=8 cut (size 11)
    chain.push_back(msg("tool"));   // 4
    chain.push_back(msg("user"));   // 5
    chain.push_back(msg("assistant"));
    chain.push_back(msg("user"));
    chain.push_back(msg("assistant"));
    chain.push_back(msg("user"));
    chain.push_back(msg("assistant"));  // size 11
    size_t cut = compactCutPoint(chain);
    CHECK_EQ(chain[cut].role, std::string("assistant"));
    CHECK(cut == 3);  // skipped assistant@3 + tool@4, kept pair intact in summary zone
    return "";
}

TEST(agent_History_Invariant) {
    CHECK(validateHistory({{"user", "q", {}, ""}}).empty());
    ChatMessage a{"assistant", "", {{"c1", "bash", "{}"}, {"c2", "read", "{}"}}, ""};
    CHECK(validateHistory({{"user", "q", {}, ""}, a, {"tool", "o1", {}, "c1"},
                           {"tool", "o2", {}, "c2"}})
              .empty());
    CHECK(!validateHistory({{"user", "q", {}, ""}, a, {"tool", "o1", {}, "c1"},
                            {"tool", "oX", {}, "cX"}})
              .empty());  // orphan result
    CHECK(!validateHistory({{"tool", "early", {}, "c1"}, a}).empty());  // result first
    return "";
}

TEST(agent_Restore_Multi_Call_Round) {
    // Regression: one assistant message with TWO calls must restore both
    // (interleaved call/result/call/result events).
    std::string home = makeTempDir("pocket-amulti");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    CHECK(sessionAppend(id.value, SessionEvent{"user", "q", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"assistant", "a", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"tool_call", "", "c1", "bash", "{}", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"tool_result", "o1", "c1", "", "", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"tool_call", "", "c2", "read", "{}", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"tool_result", "o2", "c2", "", "", true}).ok);
    ToolEnv env;
    env.workspace = home;
    AgentOpts ao;
    ao.model = resolveModel(defaultConfig(), "glm").value;
    ao.tools = &env;
    Agent agent(ao);
    CHECK(agent.restore(id.value).ok);
    CHECK_EQ(agent.messageCount(), (size_t)4);  // user, assistant+2calls, tool, tool
    CHECK_EQ(agent.messages()[1].toolCalls.size(), (size_t)2);  // both calls kept
    CHECK(validateHistory(agent.messages()).empty());  // provider-acceptable
    rmRf(home);
    return "";
}

TEST(agent_Restore_Pairs_Tools) {
    std::string home = makeTempDir("pocket-arestore");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    CHECK(sessionAppend(id.value, SessionEvent{"user", "q", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"assistant", "a", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"tool_call", "", "c1", "bash", "{}", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"tool_result", "out", "c1", "", "", true}).ok);
    ToolEnv env;
    env.workspace = home;
    AgentOpts ao;
    ao.model = resolveModel(defaultConfig(), "glm").value;
    ao.tools = &env;
    ao.sessionId = "";
    Agent a(ao);
    CHECK(a.restore(id.value).ok);
    CHECK_EQ(a.messageCount(), (size_t)3);  // user, assistant+calls, tool
    CHECK(a.contextUsed() > 0);
    rmRf(home);
    return "";
}

TEST(agent_Wire_Trim_Caps) {
    std::string big(20000, 'x');
    big.replace(big.size() - 10, 10, "last-error");
    std::vector<ChatMessage> msgs = {{"tool", big, {}, "c1"}};
    auto before = trimWireHistory(msgs);
    CHECK(before[0].content.size() <= kWireToolCap);
    CHECK(before[0].content.find("last-error") != std::string::npos);
    for (int i = 0; i < 10; ++i) msgs.push_back({"tool", big, {}, "next"});
    auto after = trimWireHistory(msgs);
    CHECK_EQ(before[0].content, after[0].content);  // aging never invalidates the prefix
    CHECK_EQ(trimWireHistory(after)[0].content, after[0].content);  // idempotent
    CHECK_EQ(msgs[0].content, big);  // raw history preserved
    return "";
}

TEST(agent_Cache_Window) {
    AgentStats st;
    noteCacheSample(st, -1, -1);  // unreported: not a sample
    CHECK(st.cacheWindow.empty() && st.recentHit == 0 && st.recentMiss == 0);
    noteCacheSample(st, 90, 10);
    noteCacheSample(st, 99, 1);
    CHECK_EQ(st.cacheWindow.size(), (size_t)2);
    CHECK_EQ(st.recentHit, 189L);
    CHECK_EQ(st.recentMiss, 11L);
    // Hit-only reporting (no miss counter) still samples.
    noteCacheSample(st, 50, -1);
    CHECK_EQ(st.recentMiss, 11L);
    // Window evicts the oldest past 20.
    for (int i = 0; i < 25; ++i) noteCacheSample(st, 100, 0);
    CHECK_EQ(st.cacheWindow.size(), kCacheWindow);
    CHECK_EQ(st.recentHit, 2000L);  // 20 x 100, the early samples evicted
    CHECK_EQ(st.recentMiss, 0L);
    return "";
}

TEST(agent_Image_Sniff_And_Load) {
    CHECK_EQ(sniffImageMime("\x89PNG\r\n\x1a\n...."), std::string("image/png"));
    CHECK_EQ(sniffImageMime("\xff\xd8\xff...."), std::string("image/jpeg"));
    CHECK_EQ(sniffImageMime("GIF89a...."), std::string("image/gif"));
    CHECK_EQ(sniffImageMime("GIF87a...."), std::string("image/gif"));
    CHECK_EQ(sniffImageMime("RIFF....WEBP"), std::string("image/webp"));
    CHECK(sniffImageMime("hello world, not an image").empty());
    CHECK(sniffImageMime("").empty());
    CHECK(sniffImageMime("\x89PNG").empty());  // truncated magic
    std::string ws = makeTempDir("pocket-aimg");
    CHECK(!ws.empty());
    std::string png("\x89PNG\r\n\x1a\nPAYLOAD", 15);
    CHECK(atomicWriteFile(ws + "/a.png", png, 0644).ok);
    CHECK(atomicWriteFile(ws + "/t.txt", "just text", 0644).ok);
    auto img = loadImageFile(ws + "/a.png");
    CHECK(img.ok);
    CHECK_EQ(img.value.mime, std::string("image/png"));
    CHECK_EQ(img.value.b64, base64Encode(png));
    CHECK(!loadImageFile(ws + "/t.txt").ok);    // not an image
    CHECK(!loadImageFile(ws + "/missing").ok);  // not a file
    rmRf(ws);
    return "";
}

TEST(agent_Collect_Image_Tokens) {
    std::string ws = makeTempDir("pocket-atok");
    CHECK(!ws.empty());
    CHECK(ensureDir(ws + "/proj", 0755).ok);
    std::string png("\x89PNG\r\n\x1a\nPAYLOAD", 15);
    CHECK(atomicWriteFile(ws + "/proj/shot.png", png, 0644).ok);
    CHECK(atomicWriteFile(ws + "/proj/notes.txt", "hi", 0644).ok);
    // Absolute, quoted-with-spaces, file:// and relative spellings.
    CHECK(atomicWriteFile(ws + "/proj/my pic.png", png, 0644).ok);
    std::string text = "look at " + ws + "/proj/shot.png and '" + ws + "/proj/my pic.png' plus " +
                       "file://" + ws + "/proj/shot.png and notes.txt";
    auto found = collectImageTokens(text, ws + "/proj");
    CHECK_EQ(found.size(), (size_t)2);  // shot.png deduped (bare + file://)
    CHECK_EQ(found[0].path, ws + "/proj/shot.png");
    CHECK_EQ(found[1].path, ws + "/proj/my pic.png");
    // Plain prose collects nothing; missing files are ignored.
    CHECK(collectImageTokens("hello world, no paths here", ws + "/proj").empty());
    CHECK(collectImageTokens("see /tmp/does-not-exist.png thanks", ws + "/proj").empty());
    rmRf(ws);
    return "";
}

TEST(agent_Attach_Restore_Images) {
    std::string home = makeTempDir("pocket-aattach");
    CHECK(!home.empty());
    HomeGuard hg(home);
    std::string ws = home + "/ws";
    CHECK(ensureDir(ws, 0755).ok);
    std::string png("\x89PNG\r\n\x1a\nPAYLOAD", 15);
    CHECK(atomicWriteFile(ws + "/s.png", png, 0644).ok);
    auto id = sessionCreate();
    CHECK(id.ok);
    ToolEnv env;
    env.workspace = ws;
    env.sessionTmp = home + "/tmp";
    CHECK(ensureDir(env.sessionTmp, 0700).ok);
    AgentOpts ao;
    ao.model = resolveModel(defaultConfig(), "glm").value;
    ao.tools = &env;
    ao.sessionId = id.value;
    Agent a(ao);
    CHECK(a.attachImage(ws + "/s.png").empty());
    CHECK_EQ(a.pendingImages(), (size_t)1);
    CHECK(a.attachImage(ws + "/s.png").empty());  // second attach still fine
    CHECK_EQ(a.pendingImages(), (size_t)2);
    // Simulate the turn's user event, then resume elsewhere: images reload.
    CHECK(sessionAppend(id.value, SessionEvent{"user", "see [attached image: s.png]", "", "", "",
                                              true})
              .ok);
    Agent b(ao);
    CHECK(b.restore(id.value).ok);
    CHECK_EQ(b.messageCount(), (size_t)1);
    CHECK_EQ(b.messages()[0].images.size(), (size_t)2);
    CHECK_EQ(b.messages()[0].images[0].mime, std::string("image/png"));
    CHECK_EQ(b.messages()[0].images[0].b64, base64Encode(png));
    rmRf(home);
    return "";
}

TEST(agent_Restore_Compact_Boundary) {
    std::string home = makeTempDir("pocket-acompact");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    // Pre-compact history, a compaction, then post-compact turns.
    CHECK(sessionAppend(id.value, SessionEvent{"user", "old q", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"assistant", "old a", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"compact", "SUMMARY", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"user", "new q", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, SessionEvent{"assistant", "new a", "", "", "", true}).ok);
    Config cfg = defaultConfig();
    ToolEnv env;
    env.workspace = home;
    AgentOpts ao;
    ao.model = resolveModel(cfg, "glm").value;
    ao.tools = &env;
    ao.sessionId = id.value;
    Agent a(ao);
    CHECK(a.restore(id.value).ok);
    // Parity with live compaction: the pre-compact raw events are dropped
    // (their content lives in the summary), the summary + tail are kept.
    CHECK_EQ(a.messageCount(), (size_t)3);
    CHECK(startsWith(a.messages()[0].content, "[Summary of earlier work]"));
    CHECK(a.messages()[0].content.find("SUMMARY") != std::string::npos);
    CHECK_EQ(a.messages()[1].content, std::string("new q"));
    CHECK_EQ(a.messages()[2].content, std::string("new a"));
    rmRf(home);
    return "";
}

TEST(agent_Cancelled_Batch_And_Resume_Are_Paired) {
    std::string dir = makeTempDir("pocket-cancelbatch");
    HomeGuard hg(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    std::atomic<bool> cancel{false};
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.cancel = &cancel;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        r.calls = {{"one", "bash", "{}"}, {"two", "write", "{}"}};
        cancel.store(true);
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    CHECK_EQ(agent.runTurn("work"), std::string("cancelled"));
    CHECK(validateHistory(agent.messages()).empty());
    CHECK_EQ(agent.stats().toolCalls, 0);
    Agent resumed(opts);
    CHECK(resumed.restore(id.value).ok);
    CHECK_EQ(resumed.messages()[1].toolCalls.size(), (size_t)2);
    CHECK(validateHistory(resumed.messages()).empty());
    rmRf(dir);
    return "";
}

TEST(agent_Crash_Closes_Unknown_Tool_Outcome) {
    std::string dir = makeTempDir("pocket-crashbatch");
    HomeGuard hg(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    CHECK(sessionAppend(id.value, {"user", "work", "", "", "", true}).ok);
    SessionEvent event{"assistant", "", "", "", "", true};
    event.replay = json::Object{{"calls", json::Array{
        json::Object{{"id", "c1"}, {"name", "bash"}, {"args", "{}"}},
        json::Object{{"id", "c2"}, {"name", "edit"}, {"args", "{}"}}}}};
    CHECK(sessionAppend(id.value, event).ok);
    CHECK(sessionAppend(id.value, {"tool_result", "done", "c1", "bash", "", true}).ok);
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    Agent a(opts);
    CHECK(a.restore(id.value).ok);
    CHECK(validateHistory(a.messages()).empty());
    CHECK(a.messages().back().content.find("outcome unknown") != std::string::npos);
    Agent b(opts);
    CHECK(b.restore(id.value).ok);
    CHECK_EQ(a.messageCount(), b.messageCount());  // repair once, never execute a call
    rmRf(dir);
    return "";
}

TEST(agent_Compaction_Retains_Tail_On_Every_Resume) {
    std::string dir = makeTempDir("pocket-compact-replay");
    HomeGuard hg(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.sessionId = id.value;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.request = [](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        r.text = req.stream ? "answer" : "summary";
        return Result<ChatResponse>::Ok(r);
    };
    Agent live(opts);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 7; ++i) CHECK(live.runTurn("question " + std::to_string(i)).empty());
        CHECK(live.compactNow().empty());
        Agent resumed(opts);
        CHECK(resumed.restore(id.value).ok);
        CHECK_EQ(live.messageCount(), resumed.messageCount());
        for (size_t i = 0; i < live.messageCount(); ++i) {
            CHECK_EQ(live.messages()[i].content, resumed.messages()[i].content);
            CHECK_EQ(live.messages()[i].role, resumed.messages()[i].role);
        }
        CHECK(validateHistory(resumed.messages()).empty());
    }
    rmRf(dir);
    return "";
}

TEST(agent_New_Tokens_And_Model_Change_Invalidate_Usage) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.request = [](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        r.text = std::string(4000, 'x');
        r.inTokens = 5000;
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("hi").empty());
    CHECK(a.contextUsed() > 5000);  // includes the response added after prompt_tokens
    a.setModel(opts.model, "none");
    CHECK(a.stats().lastPrompt == -1);
    CHECK(a.contextUsed() < 5000);
    return "";
}

TEST(agent_No_Progress_And_Persistence_Stop_Before_More_Work) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int requests = 0;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ++requests;
        ChatResponse r;
        r.calls = {{"c", "unknown", "{}"}};
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("do it").find("no progress") != std::string::npos);
    CHECK_EQ(requests, 3);
    CHECK(validateHistory(a.messages()).empty());
    std::string dir = makeTempDir("pocket-writefail");
    HomeGuard hg(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    opts.sessionId = id.value;
    Agent b(opts);
    CHECK(unlink((sessionDir() + "/" + id.value + ".jsonl").c_str()) == 0);
    CHECK(b.runTurn("do it").find("persistence failed") != std::string::npos);
    CHECK_EQ(requests, 3);
    rmRf(dir);
    return "";
}

TEST(agent_Image_Detection_Reads_Only_The_Header) {
    std::string dir = makeTempDir("pocket-image-header");
    CHECK(atomicWriteFile(dir + "/picture.png", std::string("\x89PNG\r\n\x1a\n", 8) + std::string(10000, 'p')).ok);
    auto images = collectImageTokens("picture.png", dir);
    CHECK(images.size() == 1 && images[0].path == dir + "/picture.png");
    rmRf(dir);
    return "";
}
