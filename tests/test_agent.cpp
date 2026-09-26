// PocketHarness tests - agent prompt stability, frozen prefix, resume.
#include "mini.h"

#include <stdexcept>

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

TEST(agent_Resume_Legacy_NonUtf8_Tool_Result) {
    std::string dir = makeTempDir("pocket-legacy-bytes");
    HomeGuard hg(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    CHECK(sessionAppend(id.value, {"user", "inspect file", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, {"assistant", "", "", "", "", true}).ok);
    CHECK(sessionAppend(id.value, {"tool_call", "", "c1", "bash", "{}", true}).ok);
    // Before 0.3.1 the serializer wrote raw command/file bytes to JSONL.
    CHECK(appendLine(sessionDir() + "/" + id.value + ".jsonl",
                     "{\"t\":\"tool_result\",\"id\":\"c1\",\"text\":\"VER\xddLEN \xc3\"}").ok);
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    std::string wire;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        wire = json::stringify(buildOpenAiBody(req));
        ChatResponse response;
        response.text = "recovered";
        return Result<ChatResponse>::Ok(response);
    };
    Agent resumed(opts);
    CHECK(resumed.restore(id.value).ok);
    CHECK(validateHistory(resumed.messages()).empty());
    CHECK(resumed.runTurn("go on").empty());
    CHECK(wire.find("VER\\ufffdLEN \\ufffd") != std::string::npos);
    CHECK(wire.find('\xdd') == std::string::npos);
    CHECK(json::parse(wire).ok);
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

TEST(agent_Side_Costs_Do_Not_Replace_Main_Context_And_Persist) {
    std::string dir = makeTempDir("pocket-costs");
    HomeGuard home(dir);
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.sessionId = id.value;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.hint = [](const std::string&, double* cost) { *cost += .01; return ""; };
    opts.request = [](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        r.text = req.stream ? "finished" : "DONE";
        r.inTokens = req.stream ? 10000 : 10;
        r.cost = req.stream ? .2 : .03;
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runGoal("answer question").empty());
    CHECK(a.contextUsed() >= 10000);
    CHECK(a.stats().cost > .269 && a.stats().cost < .271);
    CHECK(a.stats().sideCost > .069 && a.stats().sideCost < .071);
    // runGoal's final auditor also needs to reach the persisted total.
    Agent b(opts);
    CHECK(b.restore(id.value).ok);
    CHECK_EQ(b.stats().cost, a.stats().cost);
    CHECK_EQ(b.stats().sideCost, a.stats().sideCost);
    rmRf(dir);
    return "";
}

TEST(agent_Subscriptions_Are_Excluded_From_Metered_Cost) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.model.provider.protocol = "codex";
    opts.model.inPrice = opts.model.outPrice = 100;
    opts.request = [](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        r.text = "done";
        r.inTokens = 500;
        r.cost = 123;
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("hello").empty());
    CHECK_EQ(a.stats().cost, 0.0);
    CHECK(!a.stats().costIncomplete);
    CHECK_EQ(a.stats().inTokens, 500L);
    return "";
}

TEST(agent_Duplicate_Tool_IDs_Stop_Before_Side_Effects) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.request = [](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        r.calls = {{"same", "write", "{}"}, {"same", "bash", "{}"}};
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("go").find("invalid tool call batch") != std::string::npos);
    CHECK_EQ(a.stats().toolCalls, 0);
    CHECK(validateHistory(a.messages()).empty());
    return "";
}

TEST(agent_Workspace_Notices_Stay_Out_Of_Frozen_Prefix) {
    std::string dir = makeTempDir("pocket-coordination");
    HomeGuard home(dir);
    auto own = sessionCreate(), peer = sessionCreate();
    CHECK(own.ok && peer.ok);
    CHECK(sessionWorkspacePublish(dir, peer.value, "changed", "src/shared.cpp").ok);
    ToolEnv env;
    env.workspace = dir;
    AgentOpts opts;
    opts.sessionId = own.value;
    opts.tools = &env;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    bool notice = false;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        for (const auto& m : req.messages)
            notice |= m.content.find("src/shared.cpp") != std::string::npos;
        ChatResponse r;
        r.text = "done";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    auto prefix = a.systemPrompt();
    CHECK(a.runTurn("go").empty());
    CHECK(notice);
    CHECK_EQ(a.systemPrompt(), prefix);
    CHECK(a.systemPrompt().find("src/shared.cpp") == std::string::npos);
    rmRf(dir);
    return "";
}

TEST(agent_Compaction_Preserves_Current_Turn_For_Judges) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.model.context = 50000;
    opts.autonomy = true;
    bool currentTurn = false;
    int mainCalls = 0;
    opts.decide = [&](const json::Value& state, const std::vector<Question>&, bool, double*) {
        for (const auto& m : state.at("input").asArr())
            currentTurn |= m.at("content").asStr().find("CURRENT_REQUEST") != std::string::npos;
        return std::map<std::string, double>{{"ask", 0}, {"announce", 0}, {"premature", 0}, {"ignored", 0}};
    };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (!req.stream) { r.text = "summary"; return Result<ChatResponse>::Ok(r); }
        ++mainCalls;
        r.text = mainCalls == 8 ? "I will now inspect the code and make the change." : "Done. All tests passed.";
        r.inTokens = mainCalls == 7 ? 49000 : 100;
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    for (int i = 0; i < 7; ++i) CHECK(a.runTurn("old question").empty());
    CHECK(a.runTurn("CURRENT_REQUEST").empty());
    CHECK_EQ(a.stats().compactions, 1);
    CHECK(currentTurn);
    return "";
}

TEST(agent_Failed_Attempt_Usage_Is_Countable_Without_Double_Count) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int calls = 0;
    opts.request = [&](const ChatRequest&, const ChatCallbacks& cb) {
        ChatResponse r;
        r.cost = .1;
        r.inTokens = 10;
        r.outTokens = 2;
        cb.onUsage(r);
        if (++calls == 1) return Result<ChatResponse>::Err("output limit reached");
        r.text = "done";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("go").empty());
    CHECK_EQ(a.stats().cost, .2);
    CHECK_EQ(a.stats().inTokens, 20L);
    CHECK_EQ(a.stats().genTokens, 4L);
    return "";
}

TEST(agent_Local_Gateway_Uses_Known_Prices_Or_Marks_Unknown) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.model.provider.baseUrl = "http://127.0.0.1:9999/v1";
    opts.model.inPrice = 2;
    opts.model.outPrice = 4;
    opts.request = [](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        r.text = "done";
        r.inTokens = 1000000;
        r.outTokens = 1000000;
        return Result<ChatResponse>::Ok(r);
    };
    Agent priced(opts);
    CHECK(priced.runTurn("hello").empty());
    CHECK_EQ(priced.stats().cost, 6.0);
    CHECK(priced.stats().costEstimated);
    opts.model.inPrice = opts.model.outPrice = -1;
    Agent unknown(opts);
    CHECK(unknown.runTurn("hello").empty());
    CHECK(unknown.stats().costIncomplete);
    return "";
}

TEST(agent_Goal_Cancel_Persist_Resume_Clear) {
    std::string home = makeTempDir("pocket-goal");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::atomic<bool> cancel{false};
    opts.cancel = &cancel;
    int plans = 0, work = 0, audits = 0;
    std::string resumedPrompt;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) {
            ++plans;
            r.text = "INTENT: complete the task";
        } else if (req.system.find("audit") != std::string::npos) {
            ++audits;
            r.text = "DONE";
        } else {
            ++work;
            resumedPrompt = req.messages.back().content;
            if (work == 1) {
                cancel.store(true);
                return Result<ChatResponse>::Err("cancelled");
            }
            r.text = "Completed and verified.";
        }
        return Result<ChatResponse>::Ok(r);
    };
    Agent first(opts);
    CHECK_EQ(first.runGoal("finish the important work"), std::string("cancelled"));
    CHECK(first.goalPaused());
    CHECK_EQ(first.goal(), std::string("finish the important work"));
    CHECK_EQ(sessionLoadMeta(id.value).value.goalStatus, std::string("paused"));
    CHECK_EQ(plans, 1);
    CHECK_EQ(work, 1);
    CHECK_EQ(audits, 0);
    cancel.store(false);
    Agent resumed(opts);
    CHECK(resumed.restore(id.value).ok);
    CHECK(resumed.goalPaused());
    CHECK_EQ(resumed.messageCount(), first.messageCount());
    CHECK(resumed.resumeGoal().empty());
    CHECK(resumedPrompt.find("Do not repeat completed work") != std::string::npos);
    CHECK_EQ(plans, 1);
    CHECK_EQ(work, 2);
    CHECK_EQ(audits, 1);
    CHECK(resumed.goalStatus() == GoalStatus::Completed);
    CHECK_EQ(resumed.goal(), first.goal());
    Agent completed(opts);
    CHECK(completed.restore(id.value).ok);
    CHECK(completed.goalStatus() == GoalStatus::Completed);
    size_t messages = completed.messageCount();
    CHECK(!completed.resumeGoal().empty());
    CHECK(completed.clearGoal().empty());
    CHECK(completed.goal().empty());
    CHECK(completed.goalStatus() == GoalStatus::None);
    CHECK_EQ(completed.messageCount(), messages);
    Agent cleared(opts);
    CHECK(cleared.restore(id.value).ok);
    CHECK(cleared.goal().empty());
    CHECK(cleared.goalStatus() == GoalStatus::None);
    CHECK(!cleared.resumeGoal().empty());
    CHECK_EQ(work, 2);
    rmRf(home);
    return "";
}

TEST(agent_Goal_Audit_Cancellation_Wins_And_Resumes_Audit_Only) {
    // Cancellation wins over a concurrent DONE verdict. Configuring a tiny
    // decision model must not bypass the evidence auditor.
    for (bool decision : {false, true}) {
        std::string home = makeTempDir("pocket-goal-audit");
        CHECK(!home.empty());
        HomeGuard hg(home);
        auto id = sessionCreate();
        CHECK(id.ok);
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.sessionId = id.value;
        std::atomic<bool> cancel{false};
        opts.cancel = &cancel;
        int work = 0, audits = 0, decisions = 0;
        if (decision) opts.decide = [&](const json::Value&, const std::vector<Question>& questions, bool, double*) {
            for (const auto& q : questions) if (q.id == "met") {
                ++decisions;
                return std::map<std::string, double>{{"met", 0.99}};
            }
            return std::map<std::string, double>{};
        };
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            ChatResponse r;
            if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
            else if (req.system.find("audit") != std::string::npos) {
                if (++audits == 1) cancel.store(true);
                r.text = "DONE";
            } else { ++work; r.text = "Completed and verified."; }
            return Result<ChatResponse>::Ok(r);
        };
        Agent first(opts);
        CHECK_EQ(first.runGoal("finish it"), std::string("cancelled"));
        CHECK(first.goalPaused());
        CHECK_EQ(sessionLoadMeta(id.value).value.goalPhase, std::string("audit"));
        CHECK_EQ(work, 1);
        cancel.store(false);
        Agent resumed(opts);
        CHECK(resumed.restore(id.value).ok);
        CHECK(resumed.resumeGoal().empty());
        CHECK(resumed.goalStatus() == GoalStatus::Completed);
        CHECK_EQ(work, 1);
        CHECK_EQ(audits, 2);
        CHECK_EQ(decisions, 0);
        rmRf(home);
    }
    return "";
}

TEST(agent_Goal_Audit_Error_And_Invalid_Verdict_Pause) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int work = 0, audits = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
        else if (req.system.find("audit") != std::string::npos) {
            ++audits;
            if (audits == 1) return Result<ChatResponse>::Err("provider unavailable");
            r.text = audits == 2 ? "maybe done" : "DONE";
        } else { ++work; r.text = "Completed and verified."; }
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runGoal("finish it").find("audit failed") != std::string::npos);
    CHECK(a.goalPaused());
    CHECK(a.resumeGoal().find("no valid") != std::string::npos);
    CHECK(a.goalPaused());
    CHECK(a.resumeGoal().empty());
    CHECK_EQ(work, 1);
    CHECK_EQ(audits, 3);
    return "";
}

TEST(agent_Goal_Limit_And_Explicit_Followup) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int work = 0, plans = 0, audits = 0;
    std::string prompt;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) { ++plans; r.text = "INTENT: finish"; }
        else if (req.system.find("audit") != std::string::npos) {
            r.text = ++audits == 1 ? "CONTINUE: verify integration" : "DONE";
        } else { ++work; prompt = req.messages.back().content; r.text = "Completed and verified."; }
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runGoal("finish it", 1).find("goal paused") != std::string::npos);
    CHECK(a.goalPaused());
    CHECK(a.pauseGoal().empty());
    // An ordinary turn must not restart the audit loop implicitly.
    CHECK(a.runTurn("what changed?").empty());
    CHECK(a.goalPaused());
    CHECK_EQ(audits, 1);
    CHECK(a.resumeGoal("also verify Linux").empty());
    CHECK(prompt.find("USER FOLLOW-UP:\nalso verify Linux") != std::string::npos);
    CHECK_EQ(plans, 1);
    CHECK_EQ(work, 3);
    CHECK(a.goalStatus() == GoalStatus::Completed);
    return "";
}

TEST(agent_Goal_Restore_Active_As_Paused_Without_Requests) {
    std::string home = makeTempDir("pocket-goal-crash");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    int requests = 0;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ++requests;
        return Result<ChatResponse>::Err("unexpected request");
    };
    Agent initial(opts);
    auto meta = sessionLoadMeta(id.value).value;
    meta.goal = "preserve task";
    meta.goalStatus = "active";
    meta.goalPhase = "work";
    CHECK(sessionSaveMeta(id.value, meta).ok);
    Agent restored(opts);
    CHECK(restored.restore(id.value).ok);
    CHECK(restored.goalPaused());
    CHECK_EQ(restored.goal(), meta.goal);
    CHECK_EQ(sessionLoadMeta(id.value).value.goalStatus, std::string("paused"));
    CHECK_EQ(requests, 0);
    meta.goalPhase = "corrupt";
    CHECK(sessionSaveMeta(id.value, meta).ok);
    Agent corrupt(opts);
    CHECK(!corrupt.restore(id.value).ok);
    CHECK_EQ(requests, 0);
    rmRf(home);
    return "";
}

TEST(agent_Goal_PreCancelled_And_Invalid_Input_Do_Not_Run) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    std::atomic<bool> cancel{true};
    opts.cancel = &cancel;
    int requests = 0;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ++requests;
        return Result<ChatResponse>::Err("unexpected request");
    };
    Agent a(opts);
    CHECK_EQ(a.runGoal("preserve this"), std::string("cancelled"));
    CHECK(a.goalPaused());
    CHECK(!a.runGoal("   ").empty());
    CHECK(!a.runGoal("replacement", 0).empty());
    CHECK_EQ(a.goal(), std::string("preserve this"));
    CHECK(!a.resumeGoal(std::string(16385, 'x')).empty());
    CHECK_EQ(a.goal(), std::string("preserve this"));
    CHECK_EQ(requests, 0);
    return "";
}

TEST(agent_Goal_Yields_At_Turn_Boundary_And_Resumes_With_Guidance) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int work = 0, audits = 0, plans = 0;
    std::string prompt;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) { ++plans; r.text = "INTENT: finish"; }
        else if (req.system.find("audit") != std::string::npos) { ++audits; r.text = "DONE"; }
        else { ++work; prompt = req.messages.back().content; r.text = "Completed and verified."; }
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    bool queued = true;
    a.setGoalYield([&] { return queued; });
    CHECK(a.runGoal("finish it").empty());
    CHECK(a.goalPaused());
    CHECK_EQ(work, 1);
    CHECK_EQ(audits, 0);
    queued = false;
    CHECK(a.resumeGoal("also check cancellation").empty());
    CHECK(prompt.find("also check cancellation") != std::string::npos);
    CHECK_EQ(work, 2);
    CHECK_EQ(plans, 1);
    CHECK_EQ(audits, 1);
    CHECK(a.goalStatus() == GoalStatus::Completed);
    return "";
}

TEST(agent_Goal_Exception_Pauses_And_Preserves_Pending_Followup) {
    std::string home = makeTempDir("pocket-goal-exception");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::atomic<bool> cancel{true};
    opts.cancel = &cancel;
    bool throwHint = true;
    opts.hint = [&](const std::string&, double*) -> std::string {
        if (throwHint) throw std::runtime_error("hint interrupted before pushUser");
        return "";
    };
    std::string prompt;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("audit") != std::string::npos) r.text = "DONE";
        else { prompt = req.messages.back().content; r.text = "Completed and verified."; }
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK_EQ(a.runGoal("preserve this goal"), std::string("cancelled"));
    cancel.store(false);
    bool threw = false;
    try { (void)a.resumeGoal("preserve this crucial guidance"); }
    catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    CHECK(a.goalPaused());
    CHECK_EQ(a.messageCount(), (size_t)0);
    auto saved = sessionLoadMeta(id.value);
    CHECK(saved.ok);
    CHECK_EQ(saved.value.goalStatus, std::string("paused"));
    CHECK(saved.value.goalNext.find("preserve this crucial guidance") != std::string::npos);
    throwHint = false;
    Agent resumed(opts);
    CHECK(resumed.restore(id.value).ok);
    CHECK(resumed.resumeGoal().empty());
    CHECK(prompt.find("preserve this crucial guidance") != std::string::npos);
    CHECK(resumed.goalStatus() == GoalStatus::Completed);
    rmRf(home);
    return "";
}

TEST(agent_Goal_Audit_Sees_Earlier_Verified_Work_After_Resume) {
    std::string home = makeTempDir("pocket-goal-evidence");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    int work = 0, audits = 0;
    std::string evidence;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
        else if (req.system.find("audit") != std::string::npos) {
            evidence = req.messages.back().content;
            r.text = ++audits == 1 ? "CONTINUE: verify the second component" : "DONE";
        } else r.text = ++work == 1 ? "FIRST_COMPONENT_VERIFIED" : "SECOND_COMPONENT_VERIFIED";
        return Result<ChatResponse>::Ok(r);
    };
    Agent first(opts);
    CHECK(!first.runGoal("verify both components", 1).empty());
    CHECK(first.goalPaused());
    Agent resumed(opts);
    CHECK(resumed.restore(id.value).ok);
    CHECK(resumed.resumeGoal().empty());
    CHECK(evidence.find("FIRST_COMPONENT_VERIFIED") != std::string::npos);
    CHECK(evidence.find("SECOND_COMPONENT_VERIFIED") != std::string::npos);
    CHECK(evidence.find("verify the second component") != std::string::npos);
    CHECK_EQ(work, 2);
    CHECK_EQ(audits, 2);
    CHECK(sessionLoadMeta(id.value).value.goalProgress.size() <= 40000);
    rmRf(home);
    return "";
}

TEST(agent_Goal_Queued_Guidance_Yields_After_Complete_Tool_Batch) {
    std::string home = makeTempDir("pocket-batch-yield");
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.sessionId = id.value;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int work = 0, audits = 0;
    bool queued = false;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
        else if (req.system.find("audit") != std::string::npos) { ++audits; r.text = "DONE"; }
        else if (++work == 1) {
            queued = true;
            r.calls = {{"one", "read", "{}"}, {"two", "read", "{}"}};
        } else r.text = "Completed and verified.";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    a.setGoalYield([&] { return queued; });
    CHECK(a.runGoal("finish safely").empty());
    CHECK_EQ(work, 1);
    CHECK_EQ(audits, 0);
    CHECK_EQ(a.stats().toolCalls, 2);
    CHECK(validateHistory(a.messages()).empty());
    auto checkpoint = sessionLoadMeta(id.value);
    CHECK(checkpoint.ok);
    CHECK_EQ(checkpoint.value.goalPhase, std::string("work"));
    CHECK_EQ(checkpoint.value.goalStatus, std::string("paused"));
    CHECK_EQ(checkpoint.value.lastStopReason, std::string("yielded"));
    queued = false;
    CHECK(a.resumeGoal("use the new direction").empty());
    CHECK_EQ(work, 2);
    CHECK_EQ(audits, 1);
    rmRf(home);
    return "";
}

TEST(agent_Outcomes_Distinguish_Cancel_Limit_Provider_And_Completion) {
    std::string home = makeTempDir("pocket-outcomes");
    HomeGuard hg(home);
    for (const std::string mode : {"cancelled", "round_limit", "provider_error", "completed"}) {
        auto id = sessionCreate();
        CHECK(id.ok);
        AgentOpts opts;
        opts.sessionId = id.value;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.maxRounds = 2;
        std::atomic<bool> cancel{mode == "cancelled"};
        opts.cancel = &cancel;
        int calls = 0;
        opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
            if (mode == "provider_error") return Result<ChatResponse>::Err("HTTP 400: " + std::string(2000, 'x'));
            ChatResponse r;
            if (mode == "round_limit") r.calls = {{"c" + std::to_string(++calls), "read", "{}"}};
            else r.text = "done";
            return Result<ChatResponse>::Ok(r);
        };
        Agent a(opts);
        auto error = a.runTurn("do the work");
        CHECK_EQ(error.empty(), mode == "completed");
        auto meta = sessionLoadMeta(id.value);
        CHECK(meta.ok && meta.value.lastStopReason == mode);
        CHECK(meta.value.lastStopDetail.size() <= 1024);
        CHECK(meta.value.lastStoppedAtMs > 1700000000000LL);
        auto log = sessionLoad(id.value);
        CHECK(log.ok && log.value.events.back().type == "outcome");
        CHECK_EQ(log.value.events.back().replay.at("reason").asStr(), mode);
        CHECK_EQ(log.value.events.back().replay.at("at_ms").asInt(), meta.value.lastStoppedAtMs);
        auto listed = sessionList(30);
        bool found = false;
        for (const auto& item : listed) if (item.id == id.value) {
            found = true;
            CHECK_EQ(item.lastStopReason, mode);
            CHECK_EQ(item.lastStoppedAtMs, meta.value.lastStoppedAtMs);
        }
        CHECK(found);
        Agent restored(opts);
        CHECK(restored.restore(id.value).ok);
        CHECK_EQ(restored.messageCount(), a.messageCount()); // outcome is diagnostic, never model input
    }
    rmRf(home);
    return "";
}

TEST(agent_Goal_Round_Limit_Checkpoints_Progress_And_Continues) {
    std::string home = makeTempDir("pocket-goal-checkpoint");
    HomeGuard hg(home);
    auto authority = authorityInit(home, {}, {}, false);
    CHECK(authority.ok);
    Config cfg = defaultConfig();
    ToolEnv env;
    env.auth = &authority.value;
    env.cfg = &cfg;
    env.workspace = home;
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.model = resolveModel(cfg, "glm").value;
    opts.sessionId = id.value;
    opts.tools = &env;
    opts.maxRounds = 2;
    for (int i = 1; i <= 5; ++i)
        CHECK(atomicWriteFile(home + "/step" + std::to_string(i), "Evidence observed for completed step " + std::to_string(i)).ok);
    int work = 0, audits = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
        else if (req.system.find("audit") != std::string::npos) { ++audits; r.text = "DONE"; }
        else if (++work <= 5) {
            r.calls = {{"r" + std::to_string(work), "read", json::stringify(json::Object{
                {"path", home + "/step" + std::to_string(work)}})}};
        } else r.text = "Completed and verified.";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runGoal("complete five steps", 5).empty());
    CHECK_EQ(work, 6);
    CHECK_EQ(audits, 1);
    CHECK(a.goalStatus() == GoalStatus::Completed);
    auto log = sessionLoad(id.value);
    CHECK(log.ok);
    int checkpoints = 0;
    for (const auto& event : log.value.events)
        checkpoints += event.type == "outcome" && event.replay.at("reason").asStr() == "progress_checkpoint";
    CHECK_EQ(checkpoints, 2);
    rmRf(home);
    return "";
}

TEST(agent_Goal_Without_New_Progress_Does_Not_Extend_Round_Limit) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.maxRounds = 2;
    int work = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
        else { ++work; r.calls = {{"x" + std::to_string(work), "read", "{}"}}; }
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runGoal("finish safely").find("no new successful observations") != std::string::npos);
    CHECK(a.goalPaused());
    CHECK_EQ(work, 2);
    CHECK(validateHistory(a.messages()).empty());
    return "";
}

TEST(agent_Working_Context_Checkpoint_Preserves_Constraints_And_Thinking) {
    std::string home = makeTempDir("pocket-working-context");
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.model.context = 1000000;
    opts.workingContextTokens = 96000;
    opts.thinking = "high";
    opts.sessionId = id.value;
    int mainCalls = 0, summaries = 0;
    bool anchors = false, thinkingPreserved = true;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (!req.stream) {
            ++summaries;
            r.text = "Work evidence retained; summaries do not restate every constraint.";
        } else {
            ++mainCalls;
            thinkingPreserved &= req.thinking == "high";
            r.inTokens = mainCalls == 7 ? 100000 : 100;
            r.text = "Done. All tests passed.";
            if (mainCalls == 8) {
                for (const auto& message : req.messages)
                    anchors |= message.content.find("ORIGINAL_CONSTRAINT") != std::string::npos &&
                               message.content.find("LATEST_DIRECTION") != std::string::npos;
            }
        }
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("ORIGINAL_CONSTRAINT: preserve the user's data").empty());
    for (int i = 0; i < 6; ++i) CHECK(a.runTurn("intermediate request").empty());
    CHECK(a.runTurn("LATEST_DIRECTION: verify cancellation").empty());
    CHECK_EQ(summaries, 1);
    CHECK(thinkingPreserved && anchors);
    Agent restored(opts);
    CHECK(restored.restore(id.value).ok);
    CHECK_EQ(restored.messages()[0].content, a.messages()[0].content);
    rmRf(home);
    return "";
}

TEST(agent_Failed_Soft_Compaction_Has_Growth_Backoff) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.model.context = 1000000;
    opts.workingContextTokens = 96000;
    int mainCalls = 0, attempts = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        if (!req.stream) { ++attempts; return Result<ChatResponse>::Err("summarizer unavailable"); }
        ChatResponse r;
        r.text = "Done.";
        r.inTokens = ++mainCalls >= 7 ? 100000 : 100;
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    for (int i = 0; i < 12; ++i) CHECK(a.runTurn("continue the existing task").empty());
    CHECK_EQ(attempts, 1);
    CHECK_EQ(a.stats().compactions, 0);
    return "";
}

TEST(agent_Autonomous_Prompt_And_Followup_Cross_Round_Checkpoints) {
    std::string home = makeTempDir("pocket-plain-checkpoints");
    HomeGuard hg(home);
    auto authority = authorityInit(home, {}, {}, false);
    CHECK(authority.ok);
    for (int i = 1; i <= 6; ++i)
        CHECK(atomicWriteFile(home + "/step" + std::to_string(i), "Concrete evidence for completed task step " + std::to_string(i)).ok);
    ToolEnv env;
    env.auth = &authority.value;
    env.workspace = home;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    opts.autonomy = true;
    opts.maxRounds = 2;
    int calls = 0;
    bool followupSeen = false;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        ++calls;
        for (const auto& message : req.messages)
            followupSeen |= message.content == "also inspect the remaining steps";
        if (calls == 4 || calls == 8) r.text = "All requested steps inspected and verified.";
        else {
            int step = calls <= 3 ? calls : calls - 1;
            r.calls = {{"r" + std::to_string(calls), "read", json::stringify(json::Object{
                {"path", home + "/step" + std::to_string(step)}})}};
        }
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("inspect the first three steps").empty());
    CHECK_EQ(calls, 4);
    CHECK(a.runTurn("also inspect the remaining steps").empty());
    CHECK_EQ(calls, 8);
    CHECK(followupSeen);
    CHECK(validateHistory(a.messages()).empty());
    rmRf(home);
    return "";
}

TEST(agent_Autonomous_Checkpoint_Remains_Cancellable) {
    std::string home = makeTempDir("pocket-checkpoint-cancel");
    HomeGuard hg(home);
    auto authority = authorityInit(home, {}, {}, false);
    CHECK(authority.ok);
    CHECK(atomicWriteFile(home + "/evidence", "Concrete evidence verified by the tool").ok);
    ToolEnv env;
    env.auth = &authority.value;
    env.workspace = home;
    std::atomic<bool> cancel{false};
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    opts.cancel = &cancel;
    opts.autonomy = true;
    opts.maxRounds = 1;
    int calls = 0;
    opts.onNotice = [&](const std::string& notice) {
        if (notice.find("progress checkpoint") != std::string::npos) cancel.store(true);
    };
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ++calls;
        ChatResponse r;
        r.calls = {{"r", "read", json::stringify(json::Object{{"path", home + "/evidence"}})}};
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK_EQ(a.runTurn("inspect the evidence"), std::string("cancelled"));
    CHECK_EQ(calls, 1);
    CHECK(validateHistory(a.messages()).empty());
    rmRf(home);
    return "";
}

TEST(agent_Progress_Advisory_Is_Bounded_And_Never_Completion) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.maxRounds = 26;
    int calls = 0, checks = 0;
    size_t traceSize = 0;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        if (++calls == 26) r.text = "Need to inspect the failure before any completion claim.";
        else r.calls = {{"r" + std::to_string(calls), "read", "{\"attempt\":" + std::to_string(calls) + "}"}};
        return Result<ChatResponse>::Ok(r);
    };
    opts.progress = [&](const std::string& trace, double*) {
        ++checks;
        traceSize = std::max(traceSize, trace.size());
        return "Inspect the repeated failure before changing more files.";
    };
    Agent a(opts);
    CHECK(a.runTurn("diagnose the issue").empty());
    CHECK_EQ(calls, 26);
    CHECK_EQ(checks, 2);
    CHECK(traceSize <= 4096);
    CHECK(a.goalStatus() == GoalStatus::None);
    return "";
}

TEST(agent_Repeated_Compaction_Preserves_Intermediate_Requirement_Summary) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.model.context = 1000000;
    opts.workingContextTokens = 96000;
    int calls = 0, summaries = 0;
    bool earlierAmendment = false;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (!req.stream) {
            ++summaries;
            if (summaries > 1)
                earlierAmendment = req.messages[0].content.find("MIDDLE_REQUIREMENT: no dependencies") != std::string::npos;
            r.text = "MIDDLE_REQUIREMENT: no dependencies. Preserve this requirement while continuing.";
        } else {
            ++calls;
            r.inTokens = calls == 7 || calls == 15 ? 100000 : 100;
            r.text = std::string(20000, 'x');
        }
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("ORIGINAL_REQUIREMENT").empty());
    CHECK(a.runTurn("MIDDLE_REQUIREMENT: no dependencies").empty());
    for (int i = 2; i < 16; ++i) CHECK(a.runTurn("latest request " + std::to_string(i)).empty());
    CHECK(summaries >= 2);
    CHECK(earlierAmendment);
    return "";
}

TEST(agent_Old_Progress_Does_Not_Renew_A_Failure_Only_Tail) {
    std::string home = makeTempDir("pocket-progress-recency");
    HomeGuard hg(home);
    auto authority = authorityInit(home, {}, {}, false);
    CHECK(authority.ok);
    CHECK(atomicWriteFile(home + "/initial", "Useful initial discovery, before a long unresolved failure loop").ok);
    ToolEnv env;
    env.auth = &authority.value;
    env.workspace = home;
    for (bool goal : {false, true}) {
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.tools = &env;
        opts.autonomy = true;
        opts.maxRounds = 14;
        int calls = 0;
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            ChatResponse r;
            if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: diagnose";
            else {
                ++calls;
                r.calls = {{"r" + std::to_string(calls), "read", json::stringify(json::Object{
                    {"path", home + (calls == 1 ? "/initial" : "/missing" + std::to_string(calls))}})}};
            }
            return Result<ChatResponse>::Ok(r);
        };
        Agent a(opts);
        std::string error = goal ? a.runGoal("diagnose safely") : a.runTurn("diagnose safely");
        CHECK(error.find("no new successful observations") != std::string::npos);
        CHECK_EQ(calls, 14);
        CHECK(!goal || a.goalPaused());
    }
    rmRf(home);
    return "";
}

TEST(agent_Autonomous_Continuation_Judges_The_Whole_User_Request) {
    std::string home = makeTempDir("pocket-continuation-evidence");
    HomeGuard hg(home);
    auto authority = authorityInit(home, {}, {}, false);
    CHECK(authority.ok);
    CHECK(atomicWriteFile(home + "/first", "FIRST_CHUNK_EVIDENCE: the original requirement was checked").ok);
    CHECK(atomicWriteFile(home + "/second", "SECOND_CHUNK_EVIDENCE: the follow-up requirement was checked").ok);
    ToolEnv env;
    env.auth = &authority.value;
    env.workspace = home;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    opts.autonomy = true;
    opts.maxRounds = 2;
    bool criterionSeen = false, evidenceSeen = false;
    opts.decide = [&](const json::Value& state, const std::vector<Question>&, bool, double*) {
        for (const auto& message : state.at("input").asArr()) {
            auto text = message.at("content").asStr();
            criterionSeen |= text.find("ORIGINAL_ACCEPTANCE_CRITERION") != std::string::npos;
            evidenceSeen |= text.find("FIRST_CHUNK_EVIDENCE") != std::string::npos;
        }
        return std::map<std::string, double>{{"ask", 0}, {"announce", 0}, {"premature", 0}, {"ignored", 0}};
    };
    int calls = 0;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        if (++calls <= 2) r.calls = {{"r" + std::to_string(calls), "read", json::stringify(json::Object{
            {"path", home + (calls == 1 ? "/first" : "/second")}})}};
        else r.text = "All requested criteria checked and verified.";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("ORIGINAL_ACCEPTANCE_CRITERION: inspect both evidence files").empty());
    CHECK_EQ(calls, 3);
    CHECK(criterionSeen && evidenceSeen);
    rmRf(home);
    return "";
}

TEST(agent_Goal_Completion_Notice_Follows_Durable_State) {
    std::string home = makeTempDir("pocket-goal-notice");
    HomeGuard hg(home);
    for (int mode = 0; mode < 3; ++mode) {
        auto id = sessionCreate();
        CHECK(id.ok);
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.sessionId = id.value;
        std::atomic<bool> cancel{false};
        opts.cancel = &cancel;
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            ChatResponse r;
            if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
            else if (req.system.find("You audit an autonomous agent") != std::string::npos) {
                if (mode == 1) (void)atomicWriteFile(sessionDir() + "/" + id.value + ".meta.json", "corrupt fixture");
                if (mode == 2) cancel.store(true);
                r.text = "DONE";
            } else r.text = "Completed and verified.";
            return Result<ChatResponse>::Ok(r);
        };
        int notices = 0;
        bool durableCompleted = false;
        Agent* running = nullptr;
        opts.onNotice = [&](const std::string& notice) {
            if (!startsWith(notice, "goal met")) return;
            ++notices;
            auto meta = sessionLoadMeta(id.value);
            durableCompleted = running && running->goalStatus() == GoalStatus::Completed && meta.ok &&
                               meta.value.goalStatus == "completed" && meta.value.lastStopReason == "completed";
        };
        Agent a(opts);
        running = &a;
        std::string error = a.runGoal("finish fixture goal");
        CHECK_EQ(error.empty(), mode == 0);
        CHECK_EQ(notices, mode == 0 ? 1 : 0);
        CHECK(mode != 0 || durableCompleted);
    }
    rmRf(home);
    return "";
}

TEST(agent_Unknown_Quality_Evidence_Uses_Normal_Council) {
    ToolEnv tools;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &tools;
    opts.review = true;
    int councilRequests = 0, decisionRequests = 0;
    opts.decide = [&](const json::Value&, const std::vector<Question>& questions, bool transcript, double*) {
        if (transcript && questions.size() == 1 && questions[0].id == "bad") ++decisionRequests;
        std::map<std::string, double> lowRisk{{"bad", .01}};
        (void)retainSupportedQualityAnswers(lowRisk, .5); // insufficient companion evidence
        return lowRisk;
    };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse response;
        if (req.system.find("strict senior reviewer") != std::string::npos) {
            ++councilRequests;
            response.text = "LGTM";
        } else {
            // Simulate a completed changed-file batch; the contract under test
            // is whether unknown decision evidence can suppress its reviewer.
            tools.changedFiles.push_back("checked.cpp");
            response.text = "The requested change is implemented and checked.";
        }
        return Result<ChatResponse>::Ok(response);
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Implement and verify the requested change").empty());
    CHECK(decisionRequests == 1 && councilRequests == 1);
    CHECK(agent.stats().reviews == 1);
    return "";
}

TEST(agent_Decision_Transcript_Preserves_Request_And_Final_Tool_Evidence) {
    std::string home = makeTempDir("pocket-judge-evidence");
    HomeGuard hg(home);
    auto authority = authorityInit(home, {}, {}, false);
    CHECK(authority.ok);
    const std::string output = "START_OF_EVIDENCE " + std::string(1600, 'x') + " FINAL_VERIFICATION_ERROR";
    CHECK(atomicWriteFile(home + "/one", output).ok);
    CHECK(atomicWriteFile(home + "/two", output).ok);
    ToolEnv env;
    env.auth = &authority.value;
    env.workspace = home;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    opts.autonomy = true;
    opts.maxRounds = 70;
    int calls = 0;
    bool amended = false;
    bool originalSeen = false, latestSeen = false, failureSeen = false, omissionsSeen = false, bounded = false;
    opts.progress = [](const std::string&, double*) {
        return "Synthetic progress nudge: inspect the next observation.";
    };
    opts.decide = [&](const json::Value& state, const std::vector<Question>&, bool, double*) {
        std::string text = json::stringify(state);
        originalSeen = text.find("ORIGINAL_ACCEPTANCE_CRITERION") != std::string::npos;
        latestSeen = text.find("AMENDED_REQUIREMENT") != std::string::npos &&
                     text.find("LATEST_USER_DIRECTION") != std::string::npos;
        failureSeen = text.find("FINAL_VERIFICATION_ERROR") != std::string::npos;
        omissionsSeen = text.find("bytes omitted") != std::string::npos &&
                        text.find("older evidence events omitted") != std::string::npos;
        bounded = state.at("input").size() <= 60 && json::stringify(state.at("input")).size() <= 24000;
        return std::map<std::string, double>{{"ask", 0}, {"announce", 0}, {"premature", 0}, {"ignored", 0}};
    };
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse response;
        if (!amended) {
            response.text = "Original criterion recorded.";
            return Result<ChatResponse>::Ok(response);
        }
        if (++calls <= 65)
            response.calls = {{"read" + std::to_string(calls), "read", json::stringify(json::Object{
                {"path", home + (calls % 2 ? "/one" : "/two")}})}};
        else response.text = "Verification still reports the observed error.";
        return Result<ChatResponse>::Ok(response);
    };
    Agent agent(opts);
    CHECK(agent.runTurn("ORIGINAL_ACCEPTANCE_CRITERION").empty());
    amended = true;
    CHECK(agent.runTurn("AMENDED_REQUIREMENT " + std::string(6000, 'y') + " LATEST_USER_DIRECTION").empty());
    CHECK(calls == 66 && originalSeen && latestSeen && failureSeen && omissionsSeen && bounded);
    authorityClose(authority.value);
    rmRf(home);
    return "";
}

TEST(agent_Decision_Transcript_Bounds_Json_Escaped_Anchors_And_Answer) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.autonomy = true;
    int calls = 0, decisions = 0;
    bool bounded = true, finalEndsSeen = true, anchorsSeen = false;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse response;
        if (++calls % 2) response.calls = {{"read" + std::to_string(calls), "read", "{}"}};
        else response.text = "FINAL_HEAD " + std::string(6000, '\1') + " FINAL_TAIL Would you like me to continue?";
        return Result<ChatResponse>::Ok(response);
    };
    opts.decide = [&](const json::Value& state, const std::vector<Question>& questions, bool, double*) {
        ++decisions;
        auto wire = json::stringify(decisionRequest("fixture", state, questions));
        bounded &= json::stringify(state.at("input")).size() <= 24000 && wire.size() <= 60000;
        std::string answer = state.at("output").at("content").asStr();
        finalEndsSeen &= answer.find("FINAL_HEAD") != std::string::npos && answer.find("FINAL_TAIL") != std::string::npos;
        if (decisions == 2) {
            auto input = json::stringify(state.at("input"));
            anchorsSeen = input.find("ORIGINAL_HEAD") != std::string::npos && input.find("ORIGINAL_TAIL") != std::string::npos &&
                          input.find("LATEST_HEAD") != std::string::npos && input.find("LATEST_TAIL") != std::string::npos;
        }
        return std::map<std::string, double>{{"ask", 0}, {"announce", 0}, {"premature", 0}, {"ignored", 0}};
    };
    Agent agent(opts);
    CHECK(agent.runTurn("ORIGINAL_HEAD " + std::string(6000, '\2') + " ORIGINAL_TAIL").empty());
    CHECK(agent.runTurn("LATEST_HEAD " + std::string(6000, '\"') + " LATEST_TAIL").empty());
    CHECK(decisions == 2 && bounded && finalEndsSeen && anchorsSeen);
    return "";
}
