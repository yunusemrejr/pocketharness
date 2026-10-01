// PocketHarness tests - agent prompt stability, frozen prefix, resume.
#include "mini.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>

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

TEST(agent_Exact_Large_Results_Are_Reused_Before_Distillation) {
    std::string ws = makeTempDir("pocket-result-reuse");
    auto auth = authorityInit(ws, {}, {}, false);
    CHECK(auth.ok);
    const std::string evidence(20000, 'x');
    CHECK(atomicWriteFile(ws + "/evidence", evidence).ok);
    ToolEnv env;
    env.workspace = ws;
    env.auth = &auth.value;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    int requests = 0, decisions = 0;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        int n = requests++;
        if (n < 4) r.calls = {{"c" + std::to_string(n), "read", json::stringify(json::Object{
            {"path", "evidence"}, {"limit", n + 1}})}};
        else r.text = "The evidence is unchanged.";
        return Result<ChatResponse>::Ok(r);
    };
    opts.decide = [&](const json::Value&, const std::vector<Question>& qs, bool, double*) {
        ++decisions;
        std::map<std::string, double> p;
        for (const auto& q : qs) p[q.id] = 1;
        return p;
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Inspect repeated observations.").empty());
    CHECK_EQ(decisions, 1);
    CHECK_EQ(agent.stats().deduped, 3);
    CHECK(validateHistory(agent.messages()).empty());
    for (const auto& m : agent.messages())
        if (m.role == "tool" && m.toolCallId != "c0")
            CHECK(m.content.find("earlier result of call c0") != std::string::npos);
    authorityClose(auth.value);
    rmRf(ws);
    return "";
}

TEST(agent_Different_Raw_Results_With_Identical_Caps_Are_Not_Duplicates) {
    std::string ws = makeTempDir("pocket-result-collision");
    auto auth = authorityInit(ws, {}, {}, false);
    CHECK(auth.ok);
    std::string evidence(20000, 'x');
    CHECK(atomicWriteFile(ws + "/evidence", evidence).ok);
    ToolEnv env;
    env.workspace = ws;
    env.auth = &auth.value;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    int requests = 0;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        int n = requests++;
        if (n == 1) {
            evidence[10000] = 'y'; // the cap omits this byte, but the result changed
            (void)atomicWriteFile(ws + "/evidence", evidence);
        }
        if (n < 2) r.calls = {{"c" + std::to_string(n), "read", "{\"path\":\"evidence\"}"}};
        else r.text = "Inspected both observations.";
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Inspect the evidence.").empty());
    CHECK_EQ(agent.stats().deduped, 0);
    for (const auto& m : agent.messages())
        if (m.role == "tool") CHECK(m.content.find("identical to the earlier") == std::string::npos);
    authorityClose(auth.value);
    rmRf(ws);
    return "";
}

TEST(agent_Result_Reuse_Never_References_Evidence_Evicted_By_Compaction) {
    std::string ws = makeTempDir("pocket-result-compact");
    auto auth = authorityInit(ws, {}, {}, false);
    CHECK(auth.ok);
    const std::string evidence(20000, 'x');
    CHECK(atomicWriteFile(ws + "/evidence", evidence).ok);
    for (int i = 0; i < 6; ++i) CHECK(atomicWriteFile(ws + "/small" + std::to_string(i), "unique evidence " + std::to_string(i)).ok);
    ToolEnv env;
    env.workspace = ws;
    env.auth = &auth.value;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    int requests = 0, decisions = 0;
    Agent* current = nullptr;
    std::string compactError;
    bool staleReference = false;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (!req.stream) { r.text = "Inspected the earlier observations."; return Result<ChatResponse>::Ok(r); }
        int n = requests++;
        if (n == 7) {
            compactError = current->compactNow();
            auto changed = evidence;
            changed[10000] = 'y'; // same wire cap; a reused ID must not alias the evicted result
            (void)atomicWriteFile(ws + "/evidence", changed);
        }
        if (n == 8) (void)atomicWriteFile(ws + "/evidence", evidence);
        if (n == 0 || n == 7 || n == 8)
            r.calls = {{n == 7 ? "c0" : "c" + std::to_string(n), "read", "{\"path\":\"evidence\"}"}};
        else if (n < 7) r.calls = {{"c" + std::to_string(n), "read", json::stringify(json::Object{
            {"path", "small" + std::to_string(n - 1)}})}};
        else {
            for (const auto& m : req.messages)
                if (m.role == "tool" && m.toolCallId == "c8") staleReference = m.content.find("earlier result") != std::string::npos;
            r.text = "Inspected the repeated evidence after the checkpoint.";
        }
        return Result<ChatResponse>::Ok(r);
    };
    opts.decide = [&](const json::Value&, const std::vector<Question>& qs, bool, double*) {
        ++decisions;
        std::map<std::string, double> p;
        for (const auto& q : qs) p[q.id] = 1;
        return p;
    };
    Agent agent(opts);
    current = &agent;
    CHECK(agent.runTurn("Inspect all observations.").empty());
    CHECK(compactError.empty());
    CHECK_EQ(agent.stats().compactions, 1);
    CHECK_EQ(decisions, 3);
    CHECK(!staleReference);
    CHECK(validateHistory(agent.messages()).empty());
    authorityClose(auth.value);
    rmRf(ws);
    return "";
}

TEST(agent_Result_Reuse_Preserves_New_Failure_Guidance) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int requests = 0, decisions = 0;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        r.calls = {{"c" + std::to_string(requests++), "read", "{}", std::string(20000, 'x')}};
        return Result<ChatResponse>::Ok(r);
    };
    opts.decide = [&](const json::Value&, const std::vector<Question>& qs, bool, double*) {
        ++decisions;
        std::map<std::string, double> p;
        for (const auto& q : qs) p[q.id] = 1;
        return p;
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Inspect the evidence.").find("three identical tool batches") != std::string::npos);
    CHECK_EQ(decisions, 2); // the third failure carries a fresh strategy warning
    CHECK_EQ(agent.stats().deduped, 1);
    CHECK(agent.messages().back().content.find("failed 3 times") != std::string::npos);
    CHECK(validateHistory(agent.messages()).empty());
    return "";
}

TEST(agent_NoOp_Writes_Do_Not_Create_Unverified_Work_Or_New_Goal_Progress) {
    std::string ws = makeTempDir("pocket-agent-noop");
    auto auth = authorityInit(ws, {}, {}, false);
    CHECK(auth.ok);
    CHECK(atomicWriteFile(ws + "/ready", "already correct\n").ok);
    ToolEnv env;
    env.workspace = ws;
    env.auth = &auth.value;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    opts.maxRounds = 1;
    int requests = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) {
            r.text = "INTENT: Verify the ready file. ACCEPTANCE: It contains already correct.";
            return Result<ChatResponse>::Ok(r);
        }
        ++requests;
        r.calls = {{"noop" + std::to_string(requests), "write", "{\"path\":\"ready\",\"content\":\"already correct\\n\"}"}};
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    CHECK(agent.runGoal("Ensure ready contains already correct.", 2).find("no new successful observations") != std::string::npos);
    CHECK_EQ(requests, 1); // a no-op cannot renew the chunk budget
    CHECK(env.changedFiles.empty());
    CHECK_EQ(agent.stats().nudges, 0);
    authorityClose(auth.value);
    rmRf(ws);
    return "";
}

TEST(agent_Quality_State_And_Task_Intent_Reset_At_New_Turns) {
    ToolEnv env;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    bool cleared = true;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        cleared = cleared && env.qualitySeen.empty() && env.semanticChecks.empty();
        ChatResponse r;
        r.text = "4";
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    env.qualitySeen["a"] = {"prior finding"};
    env.semanticChecks["a"].calls = 2;
    CHECK(agent.runTurn("What is 2+2?").empty());
    CHECK_EQ(env.taskIntent, std::string("What is 2+2?"));
    env.qualitySeen["a"] = {"prior finding"};
    env.semanticChecks["a"].calls = 2;
    CHECK(agent.runTurn("What is 4+4?").empty());
    CHECK_EQ(env.taskIntent, std::string("What is 4+4?"));
    CHECK(cleared);
    return "";
}

TEST(agent_Decision_Transcript_Does_Not_Repeat_Pinned_Request_And_Keeps_Attachments) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.autonomy = true;
    opts.hint = [](const std::string&, double*) { return "[skill hint] NATIVE_GUIDE_REQUIREMENT"; };
    int decisions = 0;
    bool once = false, hintRetained = false;
    opts.decide = [&](const json::Value& state, const std::vector<Question>&, bool, double*) {
        ++decisions;
        auto input = json::stringify(state.at("input"));
        auto pos = input.find("NATIVE_REQUEST_UNIQUE");
        once = pos != std::string::npos && input.find("NATIVE_REQUEST_UNIQUE", pos + 1) == std::string::npos;
        hintRetained = input.find("NATIVE_GUIDE_REQUIREMENT") != std::string::npos;
        return std::map<std::string, double>{{"ask", 0}, {"announce", 0}, {"premature", 0}, {"ignored", 0}};
    };
    opts.request = [](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        r.text = "The requested implementation is complete.";
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Implement NATIVE_REQUEST_UNIQUE in the existing project.").empty());
    CHECK_EQ(decisions, 1);
    CHECK(once && hintRetained);
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

TEST(agent_Goal_Audit_Error_And_Unclear_Verdict_Keep_Working) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int work = 0, audits = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
        else if (req.system.find("audit") != std::string::npos) {
            ++audits;
            if (audits == 1) return Result<ChatResponse>::Err("provider output limit reached");
            r.text = audits == 2 ? "maybe done" : audits == 3 ? "The goal is not done yet.\n- tests" : "**Verdict:** DONE";
        } else { ++work; r.text = "Completed and verified."; }
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runGoal("finish it").empty());
    CHECK(a.goalStatus() == GoalStatus::Completed);
    CHECK_EQ(work, 3);    // the output-limit error is retried once with a larger budget
    CHECK_EQ(audits, 4);
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
    CHECK(agent.runTurn("Fix and verify checked.cpp").empty());
    CHECK(decisionRequests == 1 && councilRequests == 1);
    CHECK(agent.stats().reviews == 1);
    return "";
}

TEST(agent_Review_Digest_Includes_Observed_Command_Results_And_Trailing_Status) {
    std::vector<ChatMessage> messages = {
        {"user", "Fix and verify the library", {}, ""},
        {"assistant", "", {{"test", "bash", "{\"command\":\"python3 -m unittest -v\"}"}}, ""},
        {"tool", "[exit: 0]\n[stderr]\ntest_zero ... ok\nRan 14 tests in 0.003s\n\nOK\n", {}, "test"}};
    auto digest = workDigest(messages, 0, messages.size(), 10000);
    CHECK(digest.find("Ran 14 tests") != std::string::npos);
    CHECK(digest.find("[exit: 0]") != std::string::npos);
    messages.back().content = "test_first ... ok\n" + std::string(12000, 'x') + "\nFAIL: zero weight\n[exit: 1]\n";
    digest = workDigest(messages, 0, messages.size(), 10000);
    CHECK(digest.size() < 2500);
    CHECK(digest.find("test_first ... ok") != std::string::npos);
    CHECK(digest.find("FAIL: zero weight") != std::string::npos);
    CHECK(digest.find("[exit: 1]") != std::string::npos);
    return "";
}

TEST(agent_Complex_Review_Uses_Real_Verification_Without_Redundant_Prefilter) {
    std::string ws = makeTempDir("pocket-review-evidence");
    auto auth = authorityInit(ws, {}, {}, false);
    CHECK(auth.ok);
    ToolEnv env;
    env.workspace = ws;
    env.auth = &auth.value;
    env.unsafe = true;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    opts.review = opts.autonomy = true;
    int main = 0, reviews = 0, decisions = 0;
    bool sawEvidence = false;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("strict senior reviewer") != std::string::npos) {
            ++reviews;
            auto log = req.messages.back().content;
            sawEvidence = log.find("verified ready") != std::string::npos && log.find("[exit: 0]") != std::string::npos;
            r.text = sawEvidence ? "LGTM" : "Verification output is missing; run the command again.";
        } else {
            int n = main++;
            if (n == 0) r.calls = {{"write", "write", "{\"path\":\"ready\",\"content\":\"ready\\n\"}"}};
            else if (n == 1) r.calls = {{"test", "bash", json::stringify(json::Object{
                {"command", "test \"$(cat ready)\" = ready && printf 'verified ready\\n'"}})}};
            else r.text = "Implemented and verified the requested change.";
        }
        return Result<ChatResponse>::Ok(r);
    };
    opts.decide = [&](const json::Value&, const std::vector<Question>& qs, bool, double*) {
        ++decisions;
        // Unknown quality still requires the independent reviewer.
        std::map<std::string, double> p;
        for (const auto& q : qs) if (q.id == "ask" || q.id == "announce") p[q.id] = 0;
        return p;
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Implement the migration across modules and verify correctness").empty());
    CHECK(sawEvidence);
    CHECK_EQ(reviews, 1);
    CHECK_EQ(main, 3);
    CHECK_EQ(decisions, 1);
    CHECK(validateHistory(agent.messages()).empty());
    authorityClose(auth.value);
    rmRf(ws);
    return "";
}

TEST(agent_Conflicting_Generated_Brief_Cannot_Amend_Human_Contract) {
    std::string home = makeTempDir("pocket-advisory-contract");
    HomeGuard isolated(home);
    const std::string request = "Implement CSV export across modules and verify correctness. "
        "For empty records, return the header row name followed by a newline; do not return an empty string.";
    const std::string invented = "Empty records return an empty string.";
    for (bool goal : {false, true}) {
        std::string ws = home + (goal ? "/goal" : "/turn");
        CHECK(ensureDir(ws, 0700).ok);
        auto auth = authorityInit(ws, {}, {}, false);
        CHECK(auth.ok);
        ToolEnv env;
        env.workspace = ws;
        env.auth = &auth.value;
        env.unsafe = true;
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.tools = &env;
        opts.brief = opts.review = true;
        int plans = 0, main = 0, reviews = 0, audits = 0;
        bool plannerContract = false, followedHuman = false, reviewAccepted = false, auditAccepted = false;
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            ChatResponse r;
            if (req.system.find("planning council") != std::string::npos) {
                ++plans;
                plannerContract = req.system.find("Preserve explicit behavior and edge cases verbatim") != std::string::npos &&
                    req.system.find("OPTIONAL") != std::string::npos && req.messages.back().content.find(request) != std::string::npos;
                // A generated plan can still be wrong despite its instructions.
                r.text = "INTENT: Export records.\nDECISIONS: OPTIONAL: omit the header for empty input.\nACCEPTANCE: " + invented;
            } else if (req.system.find("strict senior reviewer") != std::string::npos) {
                ++reviews;
                const auto& log = req.messages.back().content;
                reviewAccepted = req.system.find("User requirements are authoritative") != std::string::npos &&
                    log.find(request) != std::string::npos && log.find(invented) != std::string::npos &&
                    log.find("\nVerified empty-record header\n") != std::string::npos;
                // Without precedence, the invented acceptance criterion rejects
                // the correct human-requested behavior and demands another edit.
                r.text = reviewAccepted ? "LGTM" : "The generated acceptance requires empty output; remove the header.";
            } else if (req.system.find("You audit an autonomous agent") != std::string::npos) {
                ++audits;
                const auto& log = req.messages.back().content;
                auditAccepted = req.system.find("User requirements are authoritative") != std::string::npos &&
                    log.find(request) != std::string::npos && log.find(invented) != std::string::npos;
                r.text = auditAccepted ? "DONE" : "CONTINUE\nReturn empty output as required by the generated acceptance.";
            } else {
                int n = main++;
                if (n == 0) {
                    const auto& input = req.messages.back().content;
                    followedHuman = input.find(request) != std::string::npos && input.find(invented) != std::string::npos &&
                        input.find("[ADVISORY GENERATED BRIEF]") != std::string::npos &&
                        input.find("follow the human request on every conflict") != std::string::npos;
                    r.calls = {{"write", "write", json::stringify(json::Object{
                        {"path", "empty.csv"}, {"content", followedHuman ? "name\n" : ""}})}};
                } else if (n == 1) {
                    r.calls = {{"verify", "bash", json::stringify(json::Object{
                        {"command", "test \"$(cat empty.csv)\" = name && printf 'Verified empty-record header\\n'"}})}};
                } else r.text = "CSV export preserves the requested header for empty records and the check passed.";
            }
            return Result<ChatResponse>::Ok(r);
        };
        Agent agent(opts);
        CHECK((goal ? agent.runGoal(request, 1) : agent.runTurn(request)).empty());
        auto output = readFileBounded(ws + "/empty.csv", 100);
        CHECK(output.ok && output.value == "name\n");
        CHECK(plannerContract && followedHuman && reviewAccepted);
        CHECK_EQ(plans, 1);
        CHECK_EQ(main, 3);
        CHECK_EQ(reviews, 1);
        CHECK_EQ(audits, goal ? 1 : 0);
        CHECK(!goal || (auditAccepted && agent.goalStatus() == GoalStatus::Completed));
        CHECK(validateHistory(agent.messages()).empty());
        authorityClose(auth.value);
    }
    rmRf(home);
    return "";
}

TEST(agent_Small_Review_Reuses_Unknown_Completion_Assessment) {
    ToolEnv tools;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &tools;
    opts.review = opts.autonomy = true;
    int reviews = 0, decisions = 0;
    bool combined = false;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("strict senior reviewer") != std::string::npos) { ++reviews; r.text = "LGTM"; }
        else { tools.changedFiles.push_back("checked.cpp"); r.text = "The requested change is implemented and checked."; }
        return Result<ChatResponse>::Ok(r);
    };
    opts.decide = [&](const json::Value&, const std::vector<Question>& qs, bool, double*) {
        ++decisions;
        bool bad = false, premature = false;
        for (const auto& q : qs) { bad |= q.id == "bad"; premature |= q.id == "premature"; }
        combined = bad && premature;
        return std::map<std::string, double>{{"ask", 0}, {"announce", 0}};
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Fix and verify checked.cpp").empty());
    CHECK(combined && decisions == 1 && reviews == 1);
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

// --- Double mode (/double): two-stream first pass + reconcile ---

namespace {
bool isDoubleA(const ChatRequest& req) {
    // Any position: evidence follow-ups append tool messages after the tail.
    for (const auto& m : req.messages)
        if (m.content.find("You are instance A of two") != std::string::npos) return true;
    return false;
}
bool isDoubleB(const ChatRequest& req) {
    for (const auto& m : req.messages)
        if (m.content.find("You are instance B of two") != std::string::npos) return true;
    return false;
}
bool isDoubleReconcile(const ChatRequest& req) {
    return req.messages.size() == 1 &&
           startsWith(req.messages[0].content, "Instance A analysis");
}
struct DoubleProbe {
    std::mutex mu;
    std::vector<ChatRequest> dualA, dualB, reconciles, mains;
    std::string failA, failB, failReconcile;
    bool streamCalls = false;  // dual responses carry tool calls (must be ignored)
    // Extended knobs for the hardened Double paths (mu-guarded unless noted).
    std::string failAOnce;  // transient: fail only the first A request (with usage, like a real 503)
    std::string textA, textB;  // override analysis texts ("" = defaults)
    std::string servedA, servedB, servedReco;  // reported model ids ("" = silent)
    bool skipUsage = false;  // usage only in the returned value (fallback billing)
    bool blockA = false, blockB = false, blockReco = false;  // block until cb.cancel
    int slowMs = 0;  // artificial latency per dual request (concurrency overlap)
    int toolRoundsA = 0, toolRoundsB = 0;  // leading read/bash rounds before the final text
    bool evilCalls = false;  // duals emit mutation calls (must never execute)
    bool sawEvidenceA = false, sawEvidenceB = false;  // follow-up carried a real tool result
    std::atomic<int> live{0};  // dual requests currently inside the mock
    std::atomic<int> maxLive{0};  // high-water mark: 2 proves real concurrency
};
// RAII overlap tracker for the dual branch (every return path decrements).
struct LiveGuard {
    DoubleProbe& p;
    explicit LiveGuard(DoubleProbe& q) : p(q) {
        int cur = p.live.fetch_add(1) + 1;
        int mx = p.maxLive.load();
        while (cur > mx && !p.maxLive.compare_exchange_weak(mx, cur)) {}
    }
    ~LiveGuard() { p.live.fetch_sub(1); }
};
bool waitCancelled(const ChatCallbacks& cb, int maxMs = 8000) {
    for (int k = 0; k < maxMs / 10; ++k) {
        if (cb.cancel && cb.cancel->load()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}
bool historyCarries(const ChatRequest& req, const std::string& needle) {
    for (const auto& m : req.messages)
        if (m.role == "tool" && m.content.find(needle) != std::string::npos) return true;
    return false;
}
Result<ChatResponse> doubleMock(DoubleProbe& p, const ChatRequest& req, const ChatCallbacks& cb) {
    auto usage = [&](ChatResponse& r, long in, long out, double cost, const std::string& served) {
        r.inTokens = in;
        r.outTokens = out;
        r.cost = cost;
        r.servedModel = served;
        if (!p.skipUsage && cb.onUsage) cb.onUsage(r);
    };
    if (isDoubleA(req) || isDoubleB(req)) {
        bool a = isDoubleA(req);
        LiveGuard g(p);
        if (p.slowMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(p.slowMs));
        size_t nth;
        {
            std::lock_guard<std::mutex> lk(p.mu);
            auto& v = a ? p.dualA : p.dualB;
            v.push_back(req);
            nth = v.size();
            if (a && historyCarries(req, "hello")) p.sawEvidenceA = true;
            if (!a && historyCarries(req, "note.txt")) p.sawEvidenceB = true;
        }
        if ((a && p.blockA) || (!a && p.blockB)) {
            if (waitCancelled(cb)) return Result<ChatResponse>::Err("cancelled");
            return Result<ChatResponse>::Err("mock: cancel never arrived");
        }
        if (a && nth == 1 && !p.failAOnce.empty()) {
            ChatResponse billed;  // failed attempts still bill, like a real 503
            usage(billed, 50, 0, 0.0005, p.servedA);
            return Result<ChatResponse>::Err(p.failAOnce);
        }
        const std::string& fail = a ? p.failA : p.failB;
        if (!fail.empty()) return Result<ChatResponse>::Err(fail);
        const std::string& served = a ? p.servedA : p.servedB;
        int rounds = a ? p.toolRoundsA : p.toolRoundsB;
        if (rounds > 0 && (int)nth <= rounds) {
            ChatResponse r;  // one evidence round, then the final text
            if (a) r.calls.push_back({"rd1", "read", R"({"path":"note.txt"})"});
            else r.calls.push_back({"ls1", "bash", R"({"command":"ls"})"});
            usage(r, 100, 5, 0.001, served);
            return Result<ChatResponse>::Ok(r);
        }
        if (p.evilCalls) {
            ChatResponse r;  // mutations the read-only pass must refuse to run
            r.calls = {{"evil1", "write", R"({"path":"evil.txt","content":"pwned"})"},
                       {"evil2", "bash", R"({"command":"rm -f note.txt"})"}};
            if (cb.onToken) cb.onToken(a ? "ANALYSIS-A" : "ANALYSIS-B");
            r.text = a ? "ANALYSIS-A" : "ANALYSIS-B";
            usage(r, 100, 20, 0.001, served);
            return Result<ChatResponse>::Ok(r);
        }
        std::string text = a ? (p.textA.empty() ? "ANALYSIS-A understanding plan risks assumptions" : p.textA)
                             : (p.textB.empty() ? "ANALYSIS-B skeptical alternative risks" : p.textB);
        if (cb.onToken) cb.onToken(text);
        ChatResponse r;
        r.text = text;
        if (p.streamCalls) r.calls = {{a ? "ax" : "bx", "bash", "{}"}};
        usage(r, 100, 20, 0.001, served);
        return Result<ChatResponse>::Ok(r);
    }
    if (isDoubleReconcile(req)) {
        {
            std::lock_guard<std::mutex> lk(p.mu);
            p.reconciles.push_back(req);
        }  // released before any blocking wait below
        if (p.blockReco) {
            if (waitCancelled(cb)) return Result<ChatResponse>::Err("cancelled");
            return Result<ChatResponse>::Err("mock: cancel never arrived");
        }
        if (!p.failReconcile.empty()) return Result<ChatResponse>::Err(p.failReconcile);
        if (cb.onToken) cb.onToken("UNIFIED-BRIEF plan actions risks");
        ChatResponse r;
        r.text = "UNIFIED-BRIEF plan actions risks";
        usage(r, 200, 30, 0.002, p.servedReco);
        return Result<ChatResponse>::Ok(r);
    }
    size_t n;
    {
        std::lock_guard<std::mutex> lk(p.mu);
        p.mains.push_back(req);
        n = p.mains.size();
    }
    ChatResponse r;
    if (n == 1) {
        r.calls = {{"m1", "read", "{}"}};  // main-loop tools still run under double
    } else {
        r.text = "FINAL-ANSWER";
        if (cb.onToken) cb.onToken(r.text);
    }
    usage(r, 150, 10, 0.001, "");
    return Result<ChatResponse>::Ok(r);
}
bool historyHas(const Agent& agent, const std::string& role, const std::string& needle) {
    for (const auto& m : agent.messages())
        if (m.role == role && m.content.find(needle) != std::string::npos) return true;
    return false;
}
bool historyCallHas(const Agent& agent, const std::string& id) {
    for (const auto& m : agent.messages())
        for (const auto& tc : m.toolCalls)
            if (tc.id == id) return true;
    return false;
}
}  // namespace

TEST(agent_Double_Two_Streams_Reconcile_Into_One_Brief) {
    std::string home = makeTempDir("pocket-double");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    DoubleProbe probe;
    probe.streamCalls = true;  // dual calls must never reach the tool loop
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.doubleEnabled());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK_EQ(probe.dualA.size(), (size_t)1);
    CHECK_EQ(probe.dualB.size(), (size_t)1);
    CHECK_EQ(probe.reconciles.size(), (size_t)1);
    CHECK_EQ(probe.mains.size(), (size_t)2);
    // Same model, provider, and config on both streams: never substituted.
    CHECK_EQ(probe.dualA[0].model.spec, opts.model.spec);
    CHECK_EQ(probe.dualB[0].model.spec, opts.model.spec);
    CHECK_EQ(probe.dualA[0].model.provider.name, opts.model.provider.name);
    CHECK_EQ(probe.dualB[0].model.provider.name, opts.model.provider.name);
    CHECK_EQ(probe.dualA[0].thinking, std::string("off"));
    CHECK_EQ(probe.dualB[0].thinking, std::string("off"));
    CHECK_EQ(probe.reconciles[0].model.spec, opts.model.spec);
    // Proposals only: no tools offered to the first pass, full tools after.
    CHECK(probe.dualA[0].tools.empty() && probe.dualB[0].tools.empty());
    CHECK(!probe.mains[0].tools.empty());
    // Independence: identical shared prefix, divergent tails, no cross-feed.
    CHECK_EQ(probe.dualA[0].system, probe.dualB[0].system);
    CHECK_EQ(probe.dualA[0].messages.size(), probe.dualB[0].messages.size());
    for (size_t i = 0; i + 1 < probe.dualA[0].messages.size(); ++i) {
        CHECK_EQ(probe.dualA[0].messages[i].role, probe.dualB[0].messages[i].role);
        CHECK_EQ(probe.dualA[0].messages[i].content, probe.dualB[0].messages[i].content);
    }
    CHECK(probe.dualA[0].messages.back().content != probe.dualB[0].messages.back().content);
    std::string wireB = json::stringify(buildOpenAiBody(probe.dualB[0]));
    CHECK(wireB.find("ANALYSIS-A") == std::string::npos);
    // One authoritative result: the brief seeds history, the turn answers once.
    CHECK(historyHas(agent, "user", "[double] Unified plan"));
    CHECK(historyHas(agent, "user", "UNIFIED-BRIEF"));
    CHECK(historyHas(agent, "assistant", "FINAL-ANSWER"));
    CHECK_EQ(agent.stats().turns, 1);
    // The dual tool calls never executed: only the main-loop call counted.
    CHECK_EQ(agent.stats().toolCalls, 1);
    CHECK(!historyCallHas(agent, "ax") && !historyCallHas(agent, "bx"));
    CHECK(historyCallHas(agent, "m1"));
    // Exact accounting across both streams plus reconcile plus main loop.
    CHECK_EQ(agent.stats().doubles, 1);
    CHECK_EQ(agent.stats().inTokens, 700L);
    CHECK_EQ(agent.stats().outTokens, 90L);
    CHECK(agent.stats().costSeen && agent.stats().cost > 0.005 && agent.stats().cost < 0.007);
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    CHECK(notes.find("Double: 2×") != std::string::npos);
    CHECK(notes.find("Double A + B ready") != std::string::npos);
    CHECK(notes.find("reconciling into one plan") != std::string::npos);
    CHECK(notes.find("Double: unified plan ready") != std::string::npos);
    rmRf(home);
    return "";
}

TEST(agent_Double_Survivor_Continues_Without_Reconcile) {
    std::string home = makeTempDir("pocket-double-surv");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    DoubleProbe probe;
    probe.failB = "boom";  // deterministic: no retry, straight to the survivor path
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK_EQ(probe.dualA.size(), (size_t)1);
    CHECK_EQ(probe.dualB.size(), (size_t)1);
    CHECK(probe.reconciles.empty());  // no reconcile call for a lone survivor
    CHECK(historyHas(agent, "user", "[double] Plan from an uncorroborated single analysis"));
    CHECK(historyHas(agent, "user", "ANALYSIS-A"));
    CHECK(historyHas(agent, "user", "no cross-check"));
    CHECK(historyHas(agent, "assistant", "FINAL-ANSWER"));
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    CHECK(notes.find("continuing with uncorroborated A alone") != std::string::npos);
    CHECK_EQ(agent.stats().doubles, 1);
    rmRf(home);
    return "";
}

TEST(agent_Double_Both_Fail_Falls_Back_To_Single) {
    std::string home = makeTempDir("pocket-double-fail");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    DoubleProbe probe;
    probe.failA = "boom";
    probe.failB = "bust";
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK(probe.reconciles.empty());
    CHECK(!historyHas(agent, "user", "[double]"));  // no brief, plain single turn
    CHECK(historyHas(agent, "assistant", "FINAL-ANSWER"));
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    CHECK(notes.find("continuing as a single stream") != std::string::npos);
    rmRf(home);
    return "";
}

TEST(agent_Double_Reconcile_Failure_Preserves_Both_Analyses) {
    std::string home = makeTempDir("pocket-double-rec");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    DoubleProbe probe;
    probe.failReconcile = "HTTP 429 limited";
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK_EQ(probe.reconciles.size(), (size_t)1);
    // No "longer response wins": BOTH bounded analyses seed the parent with
    // an explicit instruction to compare them and commit to one path.
    CHECK(historyHas(agent, "user", "Reconciliation produced no unified plan"));
    CHECK(historyHas(agent, "user", "ANALYSIS-A"));
    CHECK(historyHas(agent, "user", "ANALYSIS-B"));
    CHECK(historyHas(agent, "user", "commit to ONE plan"));
    CHECK(historyHas(agent, "assistant", "FINAL-ANSWER"));
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    CHECK(notes.find("both analyses preserved") != std::string::npos);
    CHECK(notes.find("fuller analysis") == std::string::npos);
    rmRf(home);
    return "";
}

TEST(agent_Double_Cancel_Abandons_Pass) {
    std::string home = makeTempDir("pocket-double-cancel");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    DoubleProbe probe;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::atomic<bool> cancel{true};  // pre-cancelled: the pass never starts
    opts.cancel = &cancel;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK_EQ(agent.runTurn("fix the bug"), std::string("cancelled"));
    CHECK_EQ(agent.stats().doubles, 0);
    CHECK(probe.dualA.empty() && probe.dualB.empty());
    // A stray "cancelled" string with a clear atomic is an ordinary failure.
    cancel.store(false);
    DoubleProbe probe2;
    probe2.failB = "cancelled";
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe2, req, cb); };
    Agent agent2(opts);
    CHECK(agent2.setDouble(true).empty());
    CHECK(agent2.runTurn("fix the bug").empty());
    CHECK(historyHas(agent2, "user", "ANALYSIS-A"));
    rmRf(home);
    return "";
}

TEST(agent_Double_Toggle_Persists_Across_Resume) {
    std::string home = makeTempDir("pocket-double-meta");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    Agent a1(opts);
    CHECK(!a1.doubleEnabled());
    CHECK(a1.setDouble(true).empty());
    CHECK(sessionLoadMeta(id.value).value.doubleEnabled);
    Agent a2(opts);
    CHECK(a2.restore(id.value).ok);
    CHECK(a2.doubleEnabled());
    CHECK(a2.setDouble(false).empty());
    Agent a3(opts);
    CHECK(a3.restore(id.value).ok);
    CHECK(!a3.doubleEnabled());
    CHECK(!sessionLoadMeta(id.value).value.doubleEnabled);
    rmRf(home);
    return "";
}

TEST(agent_Double_Skipped_For_Goal_Turns) {
    std::string home = makeTempDir("pocket-double-goal");
    CHECK(!home.empty());
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    int duals = 0, work = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (!req.messages.empty() && req.messages.back().content.find("You are instance") != std::string::npos) {
            ++duals;
            r.text = "unexpected twin";
            return Result<ChatResponse>::Ok(r);
        }
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish it";
        else if (req.system.find("audit") != std::string::npos) r.text = "DONE";
        else {
            ++work;
            r.text = "Completed and verified.";
        }
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runGoal("finish it").empty());
    CHECK(agent.goalStatus() == GoalStatus::Completed);
    CHECK(work > 0 && duals == 0);
    CHECK_EQ(agent.stats().doubles, 0);
    rmRf(home);
    return "";
}

namespace {
// Real tool environment for the Double evidence tests: the workers must
// execute read-only tools for real (proving the sandbox path) while the
// repository proves nothing was mutated.
struct DoubleTools {
    std::string base, ws, tmp;
    Authority auth;
    Config cfg;
    ToolEnv env;
    bool ok = false;
    DoubleTools() {
        base = makeTempDir("pocket-double-tools");
        if (base.empty()) return;
        ws = base + "/ws";
        tmp = base + "/tmp";
        if (!ensureDir(ws, 0755).ok || !ensureDir(tmp + "/home", 0700).ok) return;
        auto a = authorityInit(ws, {}, {}, false);
        if (!a.ok) return;
        auth = a.value;
        cfg = defaultConfig();
        env.auth = &auth;
        env.cfg = &cfg;
        env.workspace = auth.workspace;
        env.sessionTmp = tmp;
        env.sandboxHome = tmp + "/home";
        ok = true;
    }
    ~DoubleTools() {
        authorityClose(auth);
        if (!base.empty()) rmRf(base);
    }
};
}  // namespace

TEST(agent_Double_Streams_Run_Concurrently) {
    std::string home = makeTempDir("pocket-double-conc");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.slowMs = 200;  // both streams must overlap inside the mock
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    int64_t t0 = nowMs();
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK_EQ(probe.maxLive.load(), 2);  // A and B truly overlapped
    CHECK(nowMs() - t0 < 5000);  // overlapped, not serialized-plus-hang
    rmRf(home);
    return "";
}

TEST(agent_Double_Tails_Diverge_Without_Crossfeed) {
    std::string home = makeTempDir("pocket-double-tails");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK_EQ(probe.dualA.size(), (size_t)1);
    CHECK_EQ(probe.dualB.size(), (size_t)1);
    const std::string& tailA = probe.dualA[0].messages.back().content;
    const std::string& tailB = probe.dualB[0].messages.back().content;
    CHECK(tailA.find("instance A of two") != std::string::npos);
    CHECK(tailA.find("instance B of two") == std::string::npos);
    CHECK(tailB.find("instance B of two") != std::string::npos);
    CHECK(tailB.find("instance A of two") == std::string::npos);
    std::string wireA = json::stringify(buildOpenAiBody(probe.dualA[0]));
    std::string wireB = json::stringify(buildOpenAiBody(probe.dualB[0]));
    CHECK(wireA.find("instance B of two") == std::string::npos);
    CHECK(wireB.find("instance A of two") == std::string::npos);
    CHECK(wireA.find("ANALYSIS-B") == std::string::npos);
    CHECK(wireB.find("ANALYSIS-A") == std::string::npos);
    rmRf(home);
    return "";
}

TEST(agent_Double_Reconcile_Thinking_Derived) {
    struct Case { const char* parent; const char* reco; };
    Case cases[] = {{"off", "low"}, {"medium", "medium"}, {"max", "medium"}};
    for (const auto& c : cases) {
        std::string home = makeTempDir("pocket-double-think");
        HomeGuard hg(home);
        auto id = sessionCreate();
        DoubleProbe probe;
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.thinking = c.parent;
        opts.sessionId = id.value;
        opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
        Agent agent(opts);
        CHECK(agent.setDouble(true).empty());
        CHECK(agent.runTurn("fix the bug").empty());
        CHECK_EQ(probe.dualA[0].thinking, std::string(c.parent));  // streams keep the parent level
        CHECK_EQ(probe.dualB[0].thinking, std::string(c.parent));
        CHECK_EQ(probe.reconciles.size(), (size_t)1);
        if (probe.reconciles[0].thinking != c.reco)
            return std::string("parent ") + c.parent + " reconciled at " + probe.reconciles[0].thinking;
        rmRf(home);
    }
    return "";
}

TEST(agent_Double_Route_Substitution_Marks_Degraded) {
    std::string home = makeTempDir("pocket-double-route");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    probe.servedA = opts.model.model;  // A genuinely served
    probe.servedB = "sneaky-model";  // B silently substituted by the router
    probe.servedReco = opts.model.model;
    opts.sessionId = id.value;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());  // substituted stays usable
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    CHECK(notes.find("route substituted") != std::string::npos);
    CHECK(notes.find("sneaky-model") != std::string::npos);
    CHECK(probe.reconciles[0].messages[0].content.find("route substituted by sneaky-model") !=
          std::string::npos);
    CHECK(probe.reconciles[0].messages[0].content.find("route verified") != std::string::npos);
    CHECK(historyHas(agent, "user", "UNIFIED-BRIEF"));
    rmRf(home);
    return "";
}

TEST(agent_Double_Route_Unverified_When_Silent) {
    std::string home = makeTempDir("pocket-double-unver");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;  // no served ids anywhere: the provider stayed silent
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    CHECK(notes.find("route unverified") != std::string::npos);
    CHECK(notes.find("substituted") == std::string::npos);
    CHECK(probe.reconciles[0].messages[0].content.find("route unverified") != std::string::npos);
    CHECK(historyHas(agent, "user", "route unverified"));  // claimed honestly in the seed
    rmRf(home);
    return "";
}

TEST(agent_Double_ReadOnly_Evidence_Tools) {
    DoubleTools tools;
    CHECK(tools.ok);
    CHECK(boxWrite(tools.auth, "note.txt", "hello-evidence", 0644).ok);
    std::string home = makeTempDir("pocket-double-ev");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.toolRoundsA = 1;  // A reads the file, B lists the dir, then both conclude
    probe.toolRoundsB = 1;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.tools = &tools.env;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK_EQ(probe.dualA.size(), (size_t)2);  // evidence round + conclusion
    CHECK_EQ(probe.dualB.size(), (size_t)2);
    CHECK(!probe.dualA[0].tools.empty());  // read-only defs offered, full tools never
    CHECK_EQ(probe.dualA[0].tools.size(), (size_t)2);
    CHECK(probe.sawEvidenceA);  // the follow-up carried the real file bytes
    CHECK(probe.sawEvidenceB);  // ... and the real directory listing
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    CHECK(notes.find("Double A: evidence (1 reads, 0 commands)") != std::string::npos);
    CHECK(notes.find("Double B: evidence (0 reads, 1 commands)") != std::string::npos);
    CHECK(historyHas(agent, "user", "UNIFIED-BRIEF"));
    CHECK_EQ(boxRead(tools.auth, "note.txt", 100).value, std::string("hello-evidence"));  // untouched
    CHECK(historyHas(agent, "user", "Already inspected by the analyses"));  // digest spares re-exploration
    CHECK(historyHas(agent, "user", "- read note.txt"));
    CHECK_EQ(agent.stats().toolCalls, 1);  // parent's own call only; evidence never counted
    rmRf(home);
    return "";
}

TEST(agent_Double_Proposed_Mutations_Never_Execute) {
    DoubleTools tools;
    CHECK(tools.ok);
    CHECK(boxWrite(tools.auth, "note.txt", "hello", 0644).ok);
    std::string home = makeTempDir("pocket-double-evil");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.evilCalls = true;  // both streams propose write + rm every round
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.tools = &tools.env;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK(!boxExists(tools.auth, "evil.txt").value);  // the write never ran
    CHECK_EQ(boxRead(tools.auth, "note.txt", 100).value, std::string("hello"));  // the rm never ran
    CHECK(historyHas(agent, "user", "UNIFIED-BRIEF"));  // analyses still usable
    CHECK(!historyCallHas(agent, "evil1") && !historyCallHas(agent, "evil2"));
    CHECK_EQ(agent.stats().toolCalls, 1);  // only the parent's main-loop call
    rmRf(home);
    return "";
}

TEST(agent_Double_Transient_Retry_Recovers) {
    std::string home = makeTempDir("pocket-double-retry");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.failAOnce = "HTTP 503 blown";
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK_EQ(probe.dualA.size(), (size_t)2);  // initial + exactly one retry
    CHECK_EQ(probe.dualB.size(), (size_t)1);
    CHECK(historyHas(agent, "user", "UNIFIED-BRIEF"));  // recovered, reconciled
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    CHECK(notes.find("retrying once after") != std::string::npos);
    // Exact accounting: the failed attempt bills (50/0) plus every success.
    CHECK_EQ(agent.stats().inTokens, 750L);
    CHECK_EQ(agent.stats().outTokens, 90L);
    rmRf(home);
    return "";
}

TEST(agent_Double_Retry_Exhaustion_Survives) {
    std::string home = makeTempDir("pocket-double-exh");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.failA = "HTTP 503 down";  // transient but persistent: one retry, then survivor
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK_EQ(probe.dualA.size(), (size_t)2);  // never a third attempt
    CHECK(probe.reconciles.empty());  // no reconcile for a lone survivor
    CHECK(historyHas(agent, "user", "uncorroborated single analysis"));
    CHECK(historyHas(agent, "user", "ANALYSIS-B"));
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    CHECK(notes.find("retrying once after") != std::string::npos);
    CHECK(notes.find("uncorroborated B alone") != std::string::npos);
    rmRf(home);
    return "";
}

TEST(agent_Double_Deadline_Cancels_Slow_Stream) {
    std::string home = makeTempDir("pocket-double-dead");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.blockB = true;  // B never answers unless cancelled
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.doubleDeadlineMs = 150;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    int64_t t0 = nowMs();
    CHECK(agent.runTurn("fix the bug").empty());  // completes: no indefinite join
    CHECK(nowMs() - t0 < 5000);
    CHECK_EQ(probe.dualB.size(), (size_t)1);  // cancelled, never retried
    CHECK_EQ(agent.stats().doubles, 1);
    CHECK(historyHas(agent, "user", "uncorroborated single analysis"));
    CHECK(historyHas(agent, "user", "ANALYSIS-A"));
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    // A finished long before the deadline: B is cut off as a straggler.
    CHECK(notes.find("straggling") != std::string::npos);
    rmRf(home);
    return "";
}

TEST(agent_Double_Cancel_During_Streams) {
    std::string home = makeTempDir("pocket-double-cc");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.blockA = true;
    probe.blockB = true;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::atomic<bool> cancel{false};
    opts.cancel = &cancel;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    std::thread canceller([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cancel.store(true);
    });
    std::string err = agent.runTurn("fix the bug");
    canceller.join();
    CHECK_EQ(err, std::string("cancelled"));
    CHECK_EQ(agent.stats().doubles, 0);
    CHECK(!probe.dualA.empty() && !probe.dualB.empty());  // launched, then abandoned
    CHECK(!historyHas(agent, "user", "[double]"));  // no half-finished plan persisted
    rmRf(home);
    return "";
}

TEST(agent_Double_Cancel_During_Reconcile) {
    std::string home = makeTempDir("pocket-double-cr");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.blockReco = true;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    std::atomic<bool> cancel{false};
    opts.cancel = &cancel;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    std::thread canceller([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cancel.store(true);
    });
    std::string err = agent.runTurn("fix the bug");
    canceller.join();
    CHECK_EQ(err, std::string("cancelled"));
    CHECK_EQ(probe.reconciles.size(), (size_t)1);
    CHECK(!historyHas(agent, "user", "[double] Unified plan"));  // streams done, plan never seeded
    CHECK(!historyHas(agent, "user", "Reconciliation produced no unified plan"));
    rmRf(home);
    return "";
}

TEST(agent_Double_Shorter_Analysis_Survives_Reconcile_Failure) {
    std::string home = makeTempDir("pocket-double-short");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.textA = "TINY-A";
    probe.textB = std::string(5000, 'L');  // far longer, must not win by length
    probe.failReconcile = "boom";
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK(historyHas(agent, "user", "TINY-A"));  // the short analysis is preserved
    CHECK(historyHas(agent, "user", "[... truncated"));  // the long one is bounded, not crowned
    CHECK(historyHas(agent, "user", "commit to ONE plan"));
    rmRf(home);
    return "";
}

TEST(agent_Double_Persistence_Restores_Doubles) {
    std::string home = makeTempDir("pocket-double-persist");
    HomeGuard hg(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    DoubleProbe probe;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent a1(opts);
    CHECK(a1.setDouble(true).empty());
    CHECK(a1.runTurn("fix the bug").empty());
    CHECK_EQ(a1.stats().doubles, 1);
    CHECK(sessionLoadMeta(id.value).value.doubles == 1);
    Agent a2(opts);
    CHECK(a2.restore(id.value).ok);
    CHECK_EQ(a2.stats().doubles, 1);  // the counter survives the resume
    CHECK(a2.doubleEnabled());
    CHECK(historyHas(a2, "user", "UNIFIED-BRIEF"));  // the seed replays as history
    for (const auto& m : a2.messages()) {
        CHECK(m.content.find("[double A]") == std::string::npos);  // raw analyses stay transcript-only
        CHECK(m.content.find("[double B]") == std::string::npos);
    }
    CHECK(a2.runTurn("again").empty());  // resumed sessions double again cleanly
    CHECK_EQ(a2.stats().doubles, 2);
    rmRf(home);
    return "";
}

TEST(agent_Double_Fallback_Billing_Exact_Once) {
    std::string home = makeTempDir("pocket-double-bill");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.skipUsage = true;  // usage only in returned values: the fallback must bill once
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK(historyHas(agent, "user", "UNIFIED-BRIEF"));
    CHECK_EQ(agent.stats().inTokens, 700L);  // neither zero (missed) nor doubled
    CHECK_EQ(agent.stats().outTokens, 90L);
    rmRf(home);
    return "";
}

TEST(agent_Double_Parent_Executes_Own_Write_Once) {
    DoubleTools tools;
    CHECK(tools.ok);
    std::string home = makeTempDir("pocket-double-parent");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.evilCalls = true;  // streams propose mutations; the parent does its own write
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.tools = &tools.env;
    int mains = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) -> Result<ChatResponse> {
        if (!isDoubleReconcile(req) && !isDoubleA(req) && !isDoubleB(req)) {
            std::lock_guard<std::mutex> lk(probe.mu);
            probe.mains.push_back(req);
            ++mains;
            ChatResponse r;
            if (mains == 1) r.calls = {{"m1", "write", R"({"path":"note.txt","content":"parent"})"}};
            else {
                r.text = "FINAL-ANSWER";
                if (cb.onToken) cb.onToken(r.text);
            }
            r.inTokens = 150;
            r.outTokens = 10;
            r.cost = 0.001;
            if (cb.onUsage) cb.onUsage(r);
            return Result<ChatResponse>::Ok(r);
        }
        return doubleMock(probe, req, cb);
    };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK_EQ(boxRead(tools.auth, "note.txt", 100).value, std::string("parent"));  // parent wrote, once
    CHECK(!boxExists(tools.auth, "evil.txt").value);  // streams wrote nothing
    CHECK(historyCallHas(agent, "m1"));
    CHECK(!historyCallHas(agent, "evil1") && !historyCallHas(agent, "evil2"));
    CHECK_EQ(agent.stats().toolCalls, 1);
    rmRf(home);
    return "";
}

TEST(agent_Old_Images_Drop_In_Batches) {
    std::string home = makeTempDir("pocket-aimgprune");
    CHECK(!home.empty());
    HomeGuard hg(home);
    std::string ws = home + "/ws";
    CHECK(ensureDir(ws, 0755).ok);
    CHECK(atomicWriteFile(ws + "/s.png", std::string("\x89PNG\r\n\x1a\nPAYLOAD", 15), 0644).ok);
    ToolEnv env;
    env.workspace = ws;
    AgentOpts ao;
    ao.model = resolveModel(defaultConfig(), "glm").value;
    ao.tools = &env;
    size_t lastImages = 0;
    ao.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        lastImages = 0;
        for (const auto& m : req.messages) lastImages += m.images.size();
        ChatResponse r;
        r.text = "seen";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(ao);
    for (int turn = 0; turn < 4; ++turn) {
        for (int i = 0; i < 3; ++i) CHECK(a.attachImage(ws + "/s.png").empty());
        CHECK(a.runTurn("look").empty());
    }
    CHECK_EQ(lastImages, (size_t)12);  // at the threshold: nothing dropped yet
    for (int i = 0; i < 3; ++i) CHECK(a.attachImage(ws + "/s.png").empty());
    CHECK(a.runTurn("look").empty());
    CHECK_EQ(lastImages, (size_t)4);  // one batch down to the newest four
    CHECK(a.messages()[0].content.find("older image(s) dropped") != std::string::npos);
    rmRf(home);
    return "";
}

TEST(agent_Provider_Error_Recovers_On_Same_Model) {
    AgentOpts ao;
    ao.model = resolveModel(defaultConfig(), "glm").value;
    ao.recoverDelayMs = 1;
    int calls = 0;
    std::vector<std::string> notices;
    ao.onNotice = [&](const std::string& n) { notices.push_back(n); };
    ao.request = [&](const ChatRequest&, const ChatCallbacks&) {
        if (++calls < 3) return Result<ChatResponse>::Err("Provider returned error");
        ChatResponse r;
        r.text = "recovered";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(ao);
    CHECK(a.runTurn("hi").empty());
    CHECK_EQ(calls, 3);
    CHECK(!notices.empty() && notices[0].find("recovering") != std::string::npos);
    // Deterministic payload errors are never retried.
    calls = 0;
    ao.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ++calls;
        return Result<ChatResponse>::Err("HTTP 400: invalid tool schema");
    };
    Agent b(ao);
    CHECK(!b.runTurn("hi").empty());
    CHECK_EQ(calls, 1);
    // ...but a payload the main model still rejects goes to the fallback model.
    calls = 0;
    ao.fallback = {resolveModel(defaultConfig(), "deepseek").value};
    ao.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ++calls;
        if (req.model.spec == ao.model.spec) return Result<ChatResponse>::Err("HTTP 400: unsupported replay field");
        ChatResponse r;
        r.text = "via fallback";
        return Result<ChatResponse>::Ok(r);
    };
    Agent c(ao);
    CHECK(c.runTurn("hi").empty());
    CHECK_EQ(calls, 2);
    return "";
}

TEST(agent_Request_Budget_Bounds_Escalation_Fallback_And_Recovery) {
    AgentOpts ao;
    ao.model = resolveModel(defaultConfig(), "glm").value;
    ao.fallback = {resolveModel(defaultConfig(), "deepseek").value};
    ao.recoverDelayMs = 1;
    int calls = 0;
    bool sawDeadline = true;
    ao.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ++calls;
        sawDeadline = sawDeadline && req.deadlineAtMs > 0;  // every layer sees the one shared deadline
        return Result<ChatResponse>::Err("Provider returned error");
    };
    // A spent budget stops fallback and same-model recovery after the first attempt.
    ao.requestBudgetMs = 1500;  // below the 2 s attempt margin: nothing further may start
    Agent spent(ao);
    std::string err = spent.runTurn("hi");
    CHECK(err.find("request time budget spent") != std::string::npos);
    CHECK_EQ(calls, 1);
    // With ample budget the same outage still gets the existing bounded
    // escalation: fallback once, then two recoveries.
    calls = 0;
    ao.requestBudgetMs = 600000;
    Agent roomy(ao);
    CHECK(!roomy.runTurn("hi").empty());
    CHECK_EQ(calls, 4);
    CHECK(sawDeadline);
    return "";
}

TEST(agent_Double_Evidence_Budget_Forces_Conclusion) {
    DoubleTools tools;
    CHECK(tools.ok);
    CHECK(boxWrite(tools.auth, "note.txt", "hello", 0644).ok);
    std::string home = makeTempDir("pocket-double-budget");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.toolRoundsA = 99;  // A would search forever; the conclusion round stops it
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.tools = &tools.env;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK_EQ(probe.dualA.size(), (size_t)5);  // 4 evidence rounds + 1 conclusion round
    const auto& last = probe.dualA.back().messages.back();
    CHECK(last.role == "user" && last.content.find("Evidence budget spent") != std::string::npos);
    rmRf(home);
    return "";
}

TEST(agent_Double_Deadline_Fires_When_Both_Slow) {
    std::string home = makeTempDir("pocket-double-dead2");
    HomeGuard hg(home);
    auto id = sessionCreate();
    DoubleProbe probe;
    probe.blockA = probe.blockB = true;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.sessionId = id.value;
    opts.doubleDeadlineMs = 150;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) { return doubleMock(probe, req, cb); };
    Agent agent(opts);
    CHECK(agent.setDouble(true).empty());
    int64_t t0 = nowMs();
    CHECK(agent.runTurn("fix the bug").empty());
    CHECK(nowMs() - t0 < 5000);
    std::string notes;
    for (const auto& n : notices) notes += n + "\n";
    CHECK(notes.find("deadline exceeded") != std::string::npos);
    CHECK(notes.find("continuing as a single stream") != std::string::npos);
    rmRf(home);
    return "";
}

TEST(agent_Council_Reviewers_Run_Concurrently) {
    ToolEnv tools;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.reviewers = {opts.model, opts.model, opts.model};
    opts.tools = &tools;
    opts.review = true;
    opts.decide = [&](const json::Value&, const std::vector<Question>&, bool, double*) {
        return std::map<std::string, double>{{"bad", .5}};
    };
    std::atomic<int> live{0}, maxLive{0}, votes{0};
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse response;
        if (req.system.find("strict senior reviewer") != std::string::npos) {
            int cur = ++live;
            for (int mx = maxLive.load(); cur > mx && !maxLive.compare_exchange_weak(mx, cur);) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            --live;
            response.text = ++votes == 1 ? "LGTM." : "**lgtm!**";
        } else {
            tools.changedFiles.push_back("checked.cpp");
            response.text = "The requested change is implemented and checked.";
        }
        return Result<ChatResponse>::Ok(response);
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Implement and verify the migration across production modules").empty());
    CHECK_EQ(votes.load(), 3);
    CHECK(maxLive.load() >= 2);  // reviewers overlapped instead of queueing
    CHECK_EQ(agent.stats().reviews, 1);
    CHECK(agent.messages().back().content.find("review council") == std::string::npos);  // approvals held
    return "";
}

TEST(agent_Council_Does_Not_Wait_For_Stalled_Reviewer) {
    ToolEnv tools;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.reviewers = {opts.model, opts.model};
    opts.tools = &tools;
    opts.review = true;
    opts.councilGraceMs = 100;
    opts.decide = [&](const json::Value&, const std::vector<Question>&, bool, double*) {
        return std::map<std::string, double>{{"bad", .5}};
    };
    std::atomic<int> reviews{0};
    std::atomic<bool> stalledCancelled{false};
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) {
        ChatResponse response;
        if (req.system.find("strict senior reviewer") != std::string::npos) {
            if (++reviews == 2) {  // a provider that never answers until cancelled
                for (int i = 0; i < 1000 && !(cb.cancel && cb.cancel->load()); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                stalledCancelled = cb.cancel && cb.cancel->load();
                return Result<ChatResponse>::Err("cancelled");
            }
            response.text = "LGTM";
        } else {
            tools.changedFiles.push_back("checked.cpp");
            response.text = "The requested change is implemented and checked.";
        }
        return Result<ChatResponse>::Ok(response);
    };
    Agent agent(opts);
    int64_t t0 = nowMs();
    CHECK(agent.runTurn("Implement and verify the migration across production modules").empty());
    CHECK(nowMs() - t0 < 3000);
    CHECK(stalledCancelled.load());
    bool approved = false;
    for (const auto& n : notices) approved |= n.find("review: 1/1 approve; no verdict: ") == 0 && n.find("timed out") != std::string::npos;
    CHECK(approved);
    return "";
}

TEST(agent_Cancelled_Final_Does_Not_Start_Reviewers) {
    // Escape may race the main reply or arrive while the review prefilter is
    // answering. Neither boundary may start a new council with fresh flags.
    for (bool cancelInPrefilter : {false, true}) {
        ToolEnv tools;
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.tools = &tools;
        opts.review = true;
        opts.reviewers = {opts.model, opts.model};
        std::atomic<bool> cancel{false};
        std::atomic<int> reviewers{0};
        int decisions = 0;
        opts.cancel = &cancel;
        opts.decide = [&](const json::Value&, const std::vector<Question>&, bool, double*) {
            ++decisions;
            cancel = true;
            return std::map<std::string, double>{{"bad", .5}};
        };
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            ChatResponse response;
            if (req.system.find("strict senior reviewer") != std::string::npos) {
                ++reviewers;
                response.text = "LGTM";
            } else {
                tools.changedFiles.push_back("checked.cpp");
                response.text = "The requested change is implemented and checked.";
                if (!cancelInPrefilter) cancel = true;
            }
            return Result<ChatResponse>::Ok(response);
        };
        Agent agent(opts);
        CHECK_EQ(agent.runTurn("Implement and verify the requested change"), std::string("cancelled"));
        CHECK_EQ(reviewers.load(), 0);
        CHECK_EQ(decisions, cancelInPrefilter ? 1 : 0);
        CHECK_EQ(agent.stats().reviews, 0);
    }
    return "";
}

TEST(agent_Cancelled_Council_Records_Usage_Without_A_Verdict) {
    ToolEnv tools;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &tools;
    opts.review = true;
    opts.reviewers = {opts.model};
    std::atomic<bool> cancel{false};
    opts.cancel = &cancel;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) {
        ChatResponse response;
        if (req.system.find("strict senior reviewer") != std::string::npos) {
            response.cost = .025;
            cb.onUsage(response);
            cancel = true;
            response.text = "LGTM";
        } else {
            tools.changedFiles.push_back("checked.cpp");
            response.text = "The requested change is implemented and checked.";
        }
        return Result<ChatResponse>::Ok(response);
    };
    Agent agent(opts);
    CHECK_EQ(agent.runTurn("Implement and verify the requested change"), std::string("cancelled"));
    CHECK_EQ(agent.stats().reviews, 0);
    CHECK(agent.stats().sideCost > .024 && agent.stats().sideCost < .026);
    for (const auto& n : notices) CHECK(!startsWith(n, "review:"));
    return "";
}

TEST(agent_Slow_Brief_Is_Skipped_At_Deadline) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.brief = true;
    opts.briefDeadlineMs = 100;
    std::atomic<bool> briefCancelled{false};
    std::string workPrompt;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks& cb) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) {
            for (int i = 0; i < 1000 && !(cb.cancel && cb.cancel->load()); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            briefCancelled = cb.cancel && cb.cancel->load();
            r.text = "INTENT: late";
            return Result<ChatResponse>::Ok(r);
        }
        workPrompt = req.messages.back().content;
        r.text = "Done.";
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    int64_t t0 = nowMs();
    CHECK(agent.runTurn("fix the parser migration across modules and benchmark the complete pipeline please").empty());
    CHECK(nowMs() - t0 < 3000);
    CHECK(briefCancelled.load());
    CHECK(workPrompt.find("[brief") == std::string::npos);  // a late brief is never injected
    bool noted = false;
    for (const auto& n : notices) noted |= n.find("brief: skipped") == 0;
    CHECK(noted);
    return "";
}

TEST(agent_Adaptive_Simple_Work_Skips_Planning_And_Uses_Fast_Until_Failure) {
    for (bool explicitThinking : {false, true}) {
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        auto fast = opts.model;
        fast.spec = "fixture:fast";
        fast.model = "fixture-fast";
        opts.fast = {fast};
        opts.thinking = explicitThinking ? "max" : "adaptive";
        opts.adaptiveModel = opts.brief = true;
        std::vector<std::string> models, thinking;
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            models.push_back(req.model.spec);
            thinking.push_back(req.thinking);
            ChatResponse response;
            // No tool authority: a failed tool escalates the next request.
            if (models.size() == 1) response.calls = {{"read1", "read", "{\"path\":\"title.txt\"}"}};
            else response.text = "The title file is unavailable in this workspace.";
            return Result<ChatResponse>::Ok(response);
        };
        Agent agent(opts);
        CHECK(agent.runTurn("Please make the project title say Pocket Harness.").empty());
        CHECK_EQ(models.size(), (size_t)2); // no planning request for a precise edit
        CHECK_EQ(models[0], explicitThinking ? opts.model.spec : fast.spec);
        CHECK_EQ(models[1], opts.model.spec);
        CHECK_EQ(thinking[0], std::string(explicitThinking ? "max" : "low"));
        CHECK_EQ(thinking[1], std::string(explicitThinking ? "max" : "high"));
    }
    return "";
}

TEST(agent_Adaptive_Failed_Fast_Request_Escalates_To_Main) {
    for (const auto& error : {"HTTP 503: fixture unavailable", "HTTP 401: fixture unauthorized"}) {
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        auto fast = opts.model;
        fast.spec = "fixture:fast";
        opts.fast = {fast};
        opts.thinking = "adaptive";
        opts.adaptiveModel = true;
        std::vector<std::string> models, levels;
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            models.push_back(req.model.spec);
            levels.push_back(req.thinking);
            if (req.model.spec == fast.spec) return Result<ChatResponse>::Err(error);
            ChatResponse r;
            r.text = "42";
            return Result<ChatResponse>::Ok(r);
        };
        Agent agent(opts);
        CHECK(agent.runTurn("What is six times seven?").empty());
        CHECK_EQ(models.size(), (size_t)2);
        CHECK_EQ(models[0], fast.spec);
        CHECK_EQ(models[1], opts.model.spec);
        CHECK_EQ(levels[1], std::string("high"));
        CHECK_EQ(agent.stats().fallbacks, 1);
    }
    return "";
}

TEST(agent_Adaptive_Planning_Avoids_Known_Slow_Or_Unhealthy_Advisory_Routes) {
    std::string home = makeTempDir("pocket-planning-health");
    HomeGuard isolated(home);
    const std::string model = resolveModel(defaultConfig(), "glm").value.model;
    auto key = [&](const char* provider) { return brainRouteKey(provider, model); };
    for (int i = 0; i < 8; ++i) {
        brainNoteHealth(key("fixture-main"), true, 1000);
        brainNoteHealth(key("fixture-healthy"), true, 400);
        brainNoteHealth(key("fixture-slow"), true, 9000);
        brainNoteHealth(key("fixture-unhealthy"), false, 100);
    }
    struct Case { const char* provider; bool implicit; const char* thinking; bool main; };
    const Case cases[] = {
        {"fixture-healthy", true, "adaptive", false},
        {"fixture-slow", true, "adaptive", true},
        {"fixture-unhealthy", true, "adaptive", true},
        {"fixture-slow", false, "adaptive", false}, // explicit model intent
        {"fixture-slow", true, "high", false},      // explicit thinking
    };
    for (const auto& c : cases) {
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.model.provider.name = "fixture-main";
        opts.model.spec = "fixture-main:main";
        auto fast = opts.model;
        fast.provider.name = c.provider;
        fast.spec = std::string(c.provider) + ":fast";
        opts.fast = {fast};
        opts.adaptiveModel = c.implicit;
        opts.thinking = c.thinking;
        opts.brief = true;
        std::string planner;
        std::vector<std::string> notices;
        opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            ChatResponse r;
            if (req.system.find("planning council") != std::string::npos) {
                planner = req.model.spec;
                r.text = "INTENT: Implement the requested migration. ACCEPTANCE: Verify correctness.";
            } else r.text = "The migration is implemented and verified.";
            return Result<ChatResponse>::Ok(r);
        };
        Agent agent(opts);
        CHECK(agent.runTurn("Implement the migration across modules and benchmark correctness").empty());
        auto expected = c.main ? opts.model.spec : fast.spec;
        CHECK_EQ(planner, expected);
        CHECK(std::find(notices.begin(), notices.end(), "brief: request interpreted by " + expected) != notices.end());
    }
    rmRf(home);
    return "";
}

TEST(agent_Adaptive_Numeric_Answer_Skips_Judge_But_Broad_Completion_Does_Not) {
    for (bool complex : {false, true}) {
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.autonomy = true;
        int decisions = 0;
        opts.decide = [&](const json::Value&, const std::vector<Question>&, bool, double*) {
            ++decisions;
            return std::map<std::string, double>{{"ask", 0}, {"announce", 0}, {"premature", 0}, {"ignored", 0}};
        };
        opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
            ChatResponse r;
            r.text = complex ? "All tests pass and the build is clean." : "4";
            return Result<ChatResponse>::Ok(r);
        };
        Agent agent(opts);
        CHECK(agent.runTurn(complex ? "Implement the migration across production modules and verify release" : "What is 2+2?").empty());
        CHECK_EQ(decisions, complex ? 1 : 0);
    }
    return "";
}

TEST(agent_Adaptive_Simple_Action_Does_Not_Accept_Unevidenced_Completion) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.autonomy = true;
    int decisions = 0, requests = 0;
    opts.decide = [&](const json::Value&, const std::vector<Question>&, bool, double*) {
        return std::map<std::string, double>{{"ask", 0}, {"announce", 0},
            {"premature", ++decisions == 1 ? .99 : 0}, {"ignored", 0}};
    };
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ++requests;
        ChatResponse r;
        r.text = "Done";
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Fix the save bug").empty());
    CHECK_EQ(requests, 2);
    CHECK_EQ(decisions, 2);
    CHECK_EQ(agent.stats().nudges, 1);
    return "";
}

TEST(agent_Adaptive_Explicit_Failure_Output_Escalates_Despite_Exit_Zero) {
    std::string ws = makeTempDir("pocket-explicit-failure");
    auto auth = authorityInit(ws, {}, {}, false);
    CHECK(auth.ok);
    ToolEnv tools;
    tools.workspace = tools.sessionTmp = ws;
    tools.auth = &auth.value;
    tools.unsafe = true;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.thinking = "adaptive";
    opts.tools = &tools;
    std::vector<std::string> levels;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        levels.push_back(req.thinking);
        ChatResponse r;
        if (levels.size() == 1) r.calls = {{"check", "bash",
            "{\"command\":\"printf 'Traceback (most recent call last):\\\\nfixture error\\\\n'\"}"}};
        else r.text = "The command reports a traceback; the requested work remains unverified.";
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Check the command").empty());
    CHECK_EQ(levels.size(), (size_t)2);
    CHECK_EQ(levels[1], std::string("high"));
    authorityClose(auth.value);
    rmRf(ws);
    return "";
}

TEST(agent_Quoted_Failure_Markers_Are_Successful_Read_Evidence) {
    std::string ws = makeTempDir("pocket-quoted-failure");
    auto auth = authorityInit(ws, {}, {}, false);
    CHECK(auth.ok);
    for (int i = 0; i < 8; ++i)
        CHECK(atomicWriteFile(ws + "/example" + std::to_string(i),
            "Source documents a literal marker: Traceback (most recent call last):\n"
            "FAIL is another quoted output example, not an actual read failure.\n").ok);
    ToolEnv tools;
    tools.workspace = ws;
    tools.auth = &auth.value;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.thinking = "adaptive";
    opts.tools = &tools;
    std::vector<std::string> levels;
    int observers = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        int n = (int)levels.size();
        levels.push_back(req.thinking);
        ChatResponse r;
        if (n < 8) r.calls = {{"read" + std::to_string(n), "read", json::stringify(
            json::Object{{"path", "example" + std::to_string(n)}})}};
        else r.text = "The source examples were successfully inspected.";
        return Result<ChatResponse>::Ok(r);
    };
    opts.progress = [&](const std::string&, double*) { ++observers; return std::string(); };
    Agent agent(opts);
    CHECK(agent.runTurn("Inspect the source examples").empty());
    CHECK_EQ(levels.size(), (size_t)9);
    for (const auto& level : levels) CHECK_EQ(level, std::string("low"));
    CHECK_EQ(observers, 0);
    CHECK_EQ(agent.stats().toolCalls, 8);
    CHECK(validateHistory(agent.messages()).empty());
    authorityClose(auth.value);
    rmRf(ws);
    return "";
}

TEST(agent_Review_Fixes_Rerun_Changed_Work_Gates_Without_Repeating_Passed_Hooks) {
    for (bool repairFile : {false, true}) {
        std::string ws = makeTempDir("pocket-review-revision");
        auto auth = authorityInit(ws, {}, {}, false);
        CHECK(auth.ok);
        ToolEnv tools;
        tools.workspace = tools.sessionTmp = ws;
        tools.auth = &auth.value;
        tools.unsafe = true;
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.tools = &tools;
        opts.review = true;
        opts.stopHooks = {"printf 'checked\\n' >> hook.log"};
        int main = 0, reviews = 0;
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            ChatResponse r;
            if (req.system.find("strict senior reviewer") != std::string::npos) {
                r.text = ++reviews == 1 ? "The title or completion description needs the requested correction." : "LGTM";
            } else {
                int n = ++main;
                if (n == 1 || (repairFile && n == 4))
                    r.calls = {{"write" + std::to_string(n), "write", json::stringify(json::Object{
                        {"path", "title.txt"}, {"content", n == 1 ? "First title\n" : "Corrected title\n"}})}};
                else if (n == 2 || (repairFile && n == 5))
                    r.calls = {{"verify" + std::to_string(n), "bash", "{\"command\":\"test -s title.txt\"}"}};
                else r.text = "The title change is implemented and the command verified it.";
            }
            return Result<ChatResponse>::Ok(r);
        };
        Agent agent(opts);
        CHECK(agent.runTurn("Correct the title in this text file").empty());
        CHECK_EQ(reviews, repairFile ? 2 : 1);
        auto hook = readFileBounded(ws + "/hook.log", 100);
        CHECK(hook.ok);
        CHECK_EQ(hook.value, std::string(repairFile ? "checked\nchecked\n" : "checked\n"));
        CHECK(validateHistory(agent.messages()).empty());
        authorityClose(auth.value);
        rmRf(ws);
    }
    return "";
}

TEST(agent_Progress_Observer_Skips_Successful_Discovery) {
    std::string ws = makeTempDir("pocket-progress-success");
    auto auth = authorityInit(ws, {}, {}, false);
    CHECK(auth.ok);
    for (int i = 0; i < 20; ++i)
        CHECK(atomicWriteFile(ws + "/e" + std::to_string(i), "Useful unique discovery with sufficient evidence " + std::to_string(i)).ok);
    ToolEnv tools;
    tools.workspace = ws;
    tools.auth = &auth.value;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &tools;
    int requests = 0, observers = 0;
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        int n = requests++;
        if (n < 20) r.calls = {{"e" + std::to_string(n), "read", json::stringify(json::Object{{"path", "e" + std::to_string(n)}})}};
        else r.text = "All twenty evidence files were inspected.";
        return Result<ChatResponse>::Ok(r);
    };
    opts.progress = [&](const std::string&, double*) { ++observers; return std::string(); };
    Agent agent(opts);
    CHECK(agent.runTurn("Inspect every evidence file").empty());
    CHECK_EQ(requests, 21);
    CHECK_EQ(observers, 0);
    authorityClose(auth.value);
    rmRf(ws);
    return "";
}

TEST(agent_Restore_Remembers_Loaded_Skills_Across_Compaction) {
    std::string home = makeTempDir("pocket-skill-restore");
    HomeGuard isolated(home);
    auto id = sessionCreate();
    CHECK(id.ok);
    SessionEvent assistant{"assistant", "", "", "", "", true};
    assistant.replay = json::Object{{"calls", json::Array{json::Object{
        {"id", "load-ui"}, {"name", "skill"}, {"args", "{\"action\":\"load\",\"name\":\" ai-design-slop \"}"}}}}};
    CHECK(sessionAppend(id.value, assistant).ok);
    CHECK(sessionAppend(id.value, {"tool_result", "The design doctrine", "load-ui", "skill", "", true}).ok);
    SessionEvent compact{"compact", "Completed skill discovery; preserve project identity", "", "", "", true};
    compact.replay = json::Object{{"cut", 2}, {"summary", "The doctrine was loaded"}};
    CHECK(sessionAppend(id.value, compact).ok);
    ToolEnv tools;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &tools;
    Agent agent(opts);
    CHECK(agent.restore(id.value).ok);
    CHECK_EQ(tools.loadedSkills.size(), (size_t)1);
    CHECK_EQ(tools.loadedSkills[0], std::string(kUiDocSkill));
    CHECK(tools.uiDocLoaded);
    rmRf(home);
    return "";
}

TEST(agent_Text_Only_Model_Gets_Images_Stripped) {
    std::string home = makeTempDir("pocket-ablind");
    CHECK(!home.empty());
    HomeGuard hg(home);
    CHECK(atomicWriteFile(home + "/s.png", std::string("\x89PNG\r\n\x1a\nPAYLOAD", 15), 0644).ok);
    ToolEnv env;
    env.workspace = home;
    AgentOpts ao;
    ao.model = resolveModel(defaultConfig(), "glm").value;
    ao.tools = &env;
    int calls = 0, rejected = 0;
    ao.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ++calls;
        for (const auto& m : req.messages)
            if (!m.images.empty()) {
                ++rejected;
                return Result<ChatResponse>::Err(
                    "HTTP 400: The provided messages contain images, but qwen does not support image inputs.");
            }
        ChatResponse r;
        r.text = "fine";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(ao);
    CHECK(a.attachImage(home + "/s.png").empty());
    CHECK(a.runTurn("look").empty());
    CHECK_EQ(calls, 2);  // one rejection, one text-only resend
    CHECK(a.attachImage(home + "/s.png").empty());
    CHECK(a.runTurn("again").empty());
    CHECK_EQ(calls, 3);  // the model is remembered as blind: no second rejection
    CHECK_EQ(rejected, 1);
    rmRf(home);
    return "";
}

TEST(agent_Compaction_Rejects_Tool_Markup_Summaries) {
    CHECK(cleanSummary("<tool_call><function=bash><parameter=command>ls</parameter></function></tool_call>").empty());
    CHECK(cleanSummary("<function=read><parameter=path>a</parameter></function>").empty());
    CHECK_EQ(cleanSummary("short summary"), std::string("short summary"));
    std::string prose(300, 'x');
    CHECK_EQ(cleanSummary(prose + "<tool_call>junk</tool_call>"), prose);
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int summaries = 0;
    bool strictRetry = false, framed = false;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.stream) { r.text = "answer"; return Result<ChatResponse>::Ok(r); }
        ++summaries;
        const std::string& ask = req.messages[0].content;
        framed = startsWith(ask, "<work_log>") && ask.find("Do not call tools") != std::string::npos;
        strictRetry = ask.find("previous reply was tool-call markup") != std::string::npos;
        r.text = "<tool_call><function=bash><parameter=command>ls</parameter></function></tool_call>";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    for (int i = 0; i < 7; ++i) CHECK(a.runTurn("question " + std::to_string(i)).empty());
    CHECK(a.compactNow().empty());
    CHECK_EQ(summaries, 2);
    CHECK(framed && strictRetry);
    const std::string& kept = a.messages().front().content;
    CHECK(kept.find("<tool_call>") == std::string::npos);
    CHECK(kept.find("native digest") != std::string::npos);
    CHECK(kept.find("question 0") != std::string::npos);
    return "";
}

TEST(agent_Missing_Deliverables_Checks_Named_Format) {
    std::string ws = makeTempDir("pocket-dl");
    CHECK(!ws.empty());
    CHECK(missingDeliverables("fix the parser bug", ws).empty());               // no format named
    CHECK(missingDeliverables("the mp4 player crashes", ws).empty());           // names a format, makes nothing
    CHECK(missingDeliverables("save a short video as mp4", ws).find("no .mp4") != std::string::npos);
    CHECK(atomicWriteFile(ws + "/out.mp4", std::string(64, 'x'), 0644).ok);     // right name, wrong bytes
    CHECK(missingDeliverables("Render it as MP4.", ws).find("not a valid .mp4") != std::string::npos);
    CHECK(ensureDir(ws + "/build", 0755).ok);
    CHECK(atomicWriteFile(ws + "/build/real.mp4", std::string("\0\0\0\x18" "ftypisom", 12) + std::string(40, 'x'), 0644).ok);
    CHECK(missingDeliverables("save as mp4", ws).empty());
    // A format named incidentally is not a request to produce one. The gate
    // must not forge artifacts (and extra workspace files) the goal never asked for.
    CHECK(missingDeliverables("write a script that converts webm to mp4", ws).empty());
    CHECK(missingDeliverables("build a landing page with a png logo", ws).empty());
    CHECK(missingDeliverables("add an mp4 export button to the app", ws).empty());
    // Output positions still fire, with or without an explicit preposition.
    CHECK(missingDeliverables("export the chart to svg", ws).find("no .svg") != std::string::npos);
    CHECK(missingDeliverables("produce a pdf report", ws).find("no .pdf") != std::string::npos);
    rmRf(ws);
    return "";
}

TEST(agent_Goal_Deliverable_Gate_Rechecks_And_Blocks_Certification) {
    std::string home = makeTempDir("pocket-dg");
    CHECK(!home.empty());
    HomeGuard hg(home);
    Config cfg = defaultConfig();
    ToolEnv env;
    env.cfg = &cfg;
    env.workspace = home;
    auto id = sessionCreate();
    CHECK(id.ok);
    AgentOpts opts;
    opts.model = resolveModel(cfg, "glm").value;
    opts.sessionId = id.value;
    opts.tools = &env;
    opts.maxRounds = 2;
    int audits = 0, gapNotices = 0;
    opts.onNotice = [&](const std::string& s) {
        if (s.find("named deliverable missing") != std::string::npos) ++gapNotices;
    };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
        else if (req.system.find("audit") != std::string::npos) { ++audits; r.text = "DONE"; }
        else r.text = "Still working on it.";
        return Result<ChatResponse>::Ok(r);
    };
    // The named .mp4 never appears. An auditor that answers DONE must not
    // certify it: the native gate rechecks the file every cycle, chases it
    // twice, then pauses the goal as explicitly blocked (never Completed).
    Agent a(opts);
    std::string stopped = a.runGoal("save a short video as mp4", 5);
    CHECK(stopped.find("required deliverable is still missing") != std::string::npos);
    CHECK(a.goalStatus() != GoalStatus::Completed);
    CHECK_EQ(gapNotices, 2);
    CHECK_EQ(audits, 0);  // a model verdict was never even consulted

    // A code goal that merely names a format must never trigger the gate, and
    // must be certifiable immediately by the auditor.
    gapNotices = 0;
    audits = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
        else if (req.system.find("audit") != std::string::npos) { ++audits; r.text = "DONE"; }
        else r.text = "Implemented the converter and its tests.";
        return Result<ChatResponse>::Ok(r);
    };
    Agent b(opts);
    CHECK(b.runGoal("write a script that converts webm to mp4", 3).empty());
    CHECK_EQ(gapNotices, 0);
    CHECK_EQ(audits, 1);
    rmRf(home);
    return "";
}

TEST(agent_Goal_Deleted_Deliverable_Is_Not_Certified) {
    std::string home = makeTempDir("pocket-dg2");
    HomeGuard hg(home);
    Config cfg = defaultConfig();
    ToolEnv env;
    env.cfg = &cfg;
    env.workspace = home;
    auto id = sessionCreate();
    CHECK(id.ok);
    CHECK(atomicWriteFile(home + "/out.mp4", std::string("\0\0\0\x18" "ftypisom", 12) + std::string(40, 'x'), 0644).ok);
    AgentOpts opts;
    opts.model = resolveModel(cfg, "glm").value;
    opts.sessionId = id.value;
    opts.tools = &env;
    opts.maxRounds = 2;
    int audits = 0, turns = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: finish";
        else if (req.system.find("audit") != std::string::npos) { ++audits; r.text = "DONE"; }
        else { if (++turns == 1) unlink((home + "/out.mp4").c_str()); r.text = "Rendered it."; }
        return Result<ChatResponse>::Ok(r);
    };
    // The file exists at the start but is gone by the first audit.
    Agent a(opts);
    CHECK(!a.runGoal("render the clip as mp4", 4).empty());
    CHECK(a.goalStatus() != GoalStatus::Completed);
    CHECK_EQ(audits, 0);
    rmRf(home);
    return "";
}

TEST(agent_Verification_Command_Classifier_Separates_Inspection_From_Checks) {
    for (const char* inert : {"pwd", "true", "echo ok", "ls -la", "cat file.py", "git status", "git diff", "pwd && true",
                              "cd src && ls", "grep -rn foo .", "echo done > marker.txt", "cp a b", "sed -i s/a/b/ f.py",
                              "FOO=1 true", "touch x; mkdir y", "find . -name '*.py' | wc -l", "git add -A && git commit -m x"})
        CHECK(!isVerificationCommand(inert));
    for (const char* check : {"make test", "pytest -q", "python3 -m unittest", "python3 solver.py", "npm test", "cargo build",
                              "./run_tests.sh", "cd build && ctest", "timeout 30 node app.js", "ffprobe out.mp4",
                              "g++ -o t t.cpp && ./t", "echo go; make", "FOO=1 pytest", "ls | xargs pytest", "bash check.sh"})
        CHECK(isVerificationCommand(check));
    return "";
}

TEST(agent_Unrelated_Success_Does_Not_Clear_Verification_And_Reedit_Invalidates) {
    std::string ws = makeTempDir("pocket-verify");
    auto auth = authorityInit(ws, {}, {}, false);
    CHECK(auth.ok);
    ToolEnv env;
    env.workspace = ws;
    env.auth = &auth.value;
    env.sessionTmp = ws;
    env.unsafe = true;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    int verifyNudges = 0, step = 0;
    opts.onNotice = [&](const std::string& n) { if (n.find("asking for verification") != std::string::npos) ++verifyNudges; };
    opts.request = [&](const ChatRequest&, const ChatCallbacks&) {
        ChatResponse r;
        auto call = [&](const char* name, const std::string& args) {
            r.calls.push_back({"c" + std::to_string(step), name, args});
        };
        switch (step++) {
        case 0: call("write", "{\"path\":\"broken.py\",\"content\":\"def f(:\\n\"}"); break;
        case 1: call("bash", "{\"command\":\"pwd\"}"); break;       // succeeds, verifies nothing
        case 2: r.text = "Done."; break;                            // gate must still ask for verification
        case 3: call("bash", "{\"command\":\"python3 broken.py\"}"); break;
        default: r.text = "Done."; break;
        }
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    agent.runTurn("write broken.py");
    CHECK_EQ(verifyNudges, 1);  // pwd did not satisfy the verification gate
    rmRf(ws);
    return "";
}

TEST(agent_Review_Digest_Labels_Failed_And_Interrupted_Mutations) {
    std::vector<ChatMessage> messages = {
        {"user", "Update the config", {}, ""},
        {"assistant", "", {{"w1", "write", "{\"path\":\"a.txt\",\"content\":\"NEW-A\"}"},
                           {"e1", "edit", "{\"path\":\"b.txt\",\"old_text\":\"x\",\"new_text\":\"NEW-B\"}"},
                           {"w2", "write", "{\"path\":\"c.txt\",\"content\":\"NEW-C\"}"}}, ""},
        {"tool", "wrote a.txt", {}, "w1"},
        {"tool", "TOOL FAILED: edit: found 0 occurrence(s), expected 1; file untouched.", {}, "e1"}};
    auto digest = workDigest(messages, 0, messages.size(), 10000);
    CHECK(digest.find("succeeded write a.txt") != std::string::npos);
    CHECK(digest.find("FAILED, NOT APPLIED edit b.txt") != std::string::npos);
    CHECK(digest.find("found 0 occurrence") != std::string::npos);
    CHECK(digest.find("NO RESULT RECORDED") != std::string::npos);
    CHECK(digest.find("c.txt was NOT confirmed applied") != std::string::npos);
    // A defect past the old 4000-byte cut stays visible.
    std::string big = std::string(9000, 'a') + "TAIL-DEFECT";
    messages.push_back({"assistant", "", {{"w3", "write", "{\"path\":\"big.txt\",\"content\":\"" + big + "\"}"}}, ""});
    messages.push_back({"tool", "wrote big.txt", {}, "w3"});
    digest = workDigest(messages, 0, messages.size(), 20000);
    CHECK(digest.find("TAIL-DEFECT") != std::string::npos);
    CHECK(digest.find("bytes omitted") != std::string::npos);
    return "";
}

TEST(agent_Review_Approval_Parse_And_Quorum_Are_Explicit) {
    CHECK(isReviewApproval("LGTM") && isReviewApproval(" lgtm. ") && isReviewApproval("**LGTM**") && isReviewApproval("LGTM!"));
    CHECK(!isReviewApproval("LGTM except data loss"));
    CHECK(!isReviewApproval("LGTM - but the parser drops the last row"));
    CHECK(!isReviewApproval("") && !isReviewApproval("1. LGTM"));
    CHECK(reviewQuorum(0, 0) == ReviewVerdict::Unavailable);
    CHECK(reviewQuorum(1, 0) == ReviewVerdict::Approved && reviewQuorum(1, 1) == ReviewVerdict::Rejected);
    CHECK(reviewQuorum(2, 1) == ReviewVerdict::Rejected);  // a tie rejects
    CHECK(reviewQuorum(3, 1) == ReviewVerdict::Approved && reviewQuorum(3, 2) == ReviewVerdict::Rejected);
    return "";
}

TEST(agent_Unavailable_Council_Is_Incomplete_Not_Approved) {
    ToolEnv tools;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.reviewers = {opts.model, opts.model};
    opts.tools = &tools;
    opts.review = true;
    std::vector<std::string> notices;
    opts.onNotice = [&](const std::string& n) { notices.push_back(n); };
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        if (req.system.find("strict senior reviewer") != std::string::npos)
            return Result<ChatResponse>::Err("provider down");
        tools.changedFiles.push_back("checked.cpp");
        ChatResponse r;
        r.text = "The change is implemented.";
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Implement and verify the migration across production modules").empty());
    bool incomplete = false, noReview = false;
    for (const auto& n : notices) {
        incomplete = incomplete || n.find("INCOMPLETE: review unavailable") != std::string::npos;
        noReview = noReview || n.find("WITHOUT independent review") != std::string::npos;
    }
    CHECK(incomplete && noReview);
    return "";
}

TEST(agent_Caveated_Lgtm_Is_An_Objection) {
    ToolEnv tools;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.reviewers = {opts.model};
    opts.tools = &tools;
    opts.review = true;
    int reviews = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("strict senior reviewer") != std::string::npos) {
            r.text = ++reviews == 1 ? "LGTM except data loss on empty input" : "LGTM";
        } else {
            tools.changedFiles.push_back("checked.cpp");
            r.text = "The change is implemented.";
        }
        return Result<ChatResponse>::Ok(r);
    };
    Agent agent(opts);
    CHECK(agent.runTurn("Implement and verify the migration across production modules").empty());
    bool relayed = false;
    for (const auto& m : agent.messages()) relayed = relayed || m.content.find("review council found problems") != std::string::npos;
    CHECK(relayed);
    return "";
}
