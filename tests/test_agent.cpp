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
    CHECK_EQ(chain[cut].role, std::string("user"));
    CHECK(cut == 5);  // skipped assistant@3 + tool@4, kept pair intact in summary zone
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
