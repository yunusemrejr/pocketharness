// PocketHarness tests - native intelligence, catalog, overseer, codex wire.
#include "mini.h"

#include <cmath>

#include "../src/agent.h"
#include "../src/brain.h"
#include "../src/catalog.h"
#include "../src/config.h"
#include "../src/oversee.h"
#include "../src/sandbox.h"
#include "../src/tools.h"
#include "../src/tui.h"
#include "../src/wisdom.h"

using namespace pocket;
using namespace pocket::test;

TEST(brain_Fuzzy_Ranks_Substring_Then_Typos) {
    CHECK(fuzzyScore("glm", "orcarouter:z-ai/glm-5.3-flash") == 1);
    CHECK(fuzzyScore("glm flsh", "orcarouter:z-ai/glm-5.3-flash") > 0.5);  // typo still lands
    CHECK(fuzzyScore("deep", "orcarouter:z-ai/glm-5.3-flash") < 0.35);
    std::vector<std::string> labels = {"anthropic:claude", "deepseek:deepseek-flash", "openrouter:z-ai/glm-5.3-flash"};
    auto r = fuzzyRank(labels, "dpsk flash");
    CHECK(!r.empty() && r[0] == 1);
    CHECK(fuzzyRank(labels, "").size() == 3);
    return "";
}

TEST(brain_Bm25_Prefers_Rare_Matching_Terms) {
    auto s = bm25({"react ui components and css layout", "postgres database migrations", "css css css"},
                  "database migration postgres");
    CHECK(s[1] > s[0] && s[1] > s[2] && s[0] == 0);
    return "";
}

TEST(brain_Stop_Classifier_Separates_Done_Announce_Permission) {
    CHECK(classifyStop("All tests pass and the build is clean.").kind == StopKind::Done);
    CHECK(classifyStop("Now let me run the test suite to check.").kind == StopKind::Announce);
    CHECK(classifyStop("Would you like me to go ahead and apply these changes?").kind == StopKind::Permission);
    auto g = classifyStop("Fixed the parser bug and added a regression test; all 12 tests pass.");
    CHECK(g.kind == StopKind::Done && g.p > 0.8);
    return "";
}

TEST(brain_Quirks_Learned_From_400s_Persist) {
    std::string home = makeTempDir("pocket-brain");
    HomeGuard hg(home);
    CHECK(quirkFromError("HTTP 400: reasoning_effort is not supported", true, "max_tokens") == "no_reasoning");
    CHECK(quirkFromError("HTTP 400: reasoning_effort is not supported", false, "max_tokens") == "");
    CHECK(quirkFromError("HTTP 400: Unrecognized field stream_options", false, "max_tokens") == "no_stream_usage");
    CHECK(quirkFromError("HTTP 400: use max_completion_tokens instead", false, "max_tokens") == "max_completion_tokens");
    CHECK(quirkFromError("HTTP 500: reasoning exploded", true, "max_tokens") == "");
    brainNoteQuirk("p:m", "no_reasoning");
    brainNoteQuirk("p:m", "no_reasoning");  // idempotent
    CHECK(brainQuirks("p:m").size() == 1);
    ModelOptions o;
    applyQuirk(o, "no_reasoning");
    applyQuirk(o, "no_stream_usage");
    CHECK(o.reasoning == "none" && !o.streamUsage);
    brainNoteHealth("prov", false, 100);
    CHECK(brainHealth("prov") < 1 && brainHealth("unknown") == 1);
    rmRf(home);
    return "";
}

TEST(catalog_Parses_OpenRouter_And_Bare_Shapes) {
    auto or_ = parseCatalog("openrouter", R"({"data":[
        {"id":"a/think","context_length":131072,"supported_parameters":["tools","reasoning"],
         "architecture":{"input_modalities":["text","image"]},"pricing":{"prompt":"0.0000003","completion":"0.0000012"}},
        {"id":"a/plain","context_length":8192,"supported_parameters":["tools"]},
        {"id":"text-embedding-3","context_length":8192}]})");
    CHECK(or_.size() == 2);
    CHECK(or_[0].reasoning == 1 && or_[0].vision && std::fabs(or_[0].inPrice - 0.3) < 1e-9);
    CHECK(or_[1].reasoning == 0 && or_[1].context == 8192);
    CHECK(catalogLabel(or_[0]).find("think") != std::string::npos);
    auto bare = parseCatalog("together", R"([{"id":"m1","type":"chat","context_length":32768,"pricing":{"input":0.2,"output":0.6}},{"id":"e","type":"embedding"}])");
    CHECK(bare.size() == 1 && bare[0].outPrice == 0.6 && bare[0].reasoning == -1);
    return "";
}

TEST(config_Env_File_And_Roles) {
    std::string home = makeTempDir("pocket-env");
    HomeGuard hg(home);
    CHECK(ensureDir(userConfigDir(), 0700).ok);
    CHECK(atomicWriteFile(userEnvPath(), "# c\nexport POCKET_T1=abc\nPOCKET_T2='q v'\nbad-name=x\nPATH=/nope\n", 0644).ok);
    CHECK(loadEnvFile(userEnvPath()) == 0);  // group/world readable: refused
    chmod(userEnvPath().c_str(), 0600);
    CHECK(loadEnvFile(userEnvPath()) == 2);
    CHECK(std::string(getenv("POCKET_T1")) == "abc" && std::string(getenv("POCKET_T2")) == "q v");
    CHECK(std::string(getenv("PATH")) != "/nope");  // never overrides the shell
    unsetenv("POCKET_T1");
    unsetenv("POCKET_T2");
    CHECK(saveRole("fast", "deepseek:deepseek-flash").ok);
    CHECK(saveRole("main", "glm").ok);
    auto cfg = loadConfig(home);
    CHECK(cfg.ok && cfg.value.roles["fast"] == "deepseek:deepseek-flash" && cfg.value.defaultModel == "glm");
    CHECK(resolveModel(cfg.value, "codex").ok && resolveModel(cfg.value, "nvidia:x/y").ok);
    rmRf(home);
    return "";
}

TEST(oversee_Parses_Local_Yes_Probability) {
    CHECK(std::fabs(parseYesProbability(R"({"completion_probabilities":[{"top_logprobs":[
        {"token":" yes","logprob":-0.105},{"token":" no","logprob":-2.302},{"token":"\n","logprob":-3}]}]})") - 0.9) < 0.01);
    CHECK(parseYesProbability(R"({"completion_probabilities":[{"probs":[{"tok_str":"No","prob":0.8}]}]})") == 0);
    CHECK(parseYesProbability(R"({"content":"x"})") < 0);
    return "";
}

TEST(oversee_Verify_Command_Follows_Project_Type) {
    std::string d = makeTempDir("pocket-verify");
    CHECK(verifyCommand(d).empty());
    CHECK(atomicWriteFile(d + "/Makefile", "all:\n\ntest:\n\t./t\n").ok);
    CHECK(verifyCommand(d) == "make test");
    rmRf(d);
    return "";
}

TEST(wisdom_Retrieves_Relevant_Sections) {
    CHECK(wisdomDoctrine().find("top priority") != std::string::npos);
    std::string w = wisdomFor("make the landing page ui elegant, fix fonts and colors", 3000);
    CHECK(w.find("## interface") != std::string::npos || w.find("## brand") != std::string::npos);
    CHECK(w.size() <= 3000);
    return "";
}

TEST(agent_Needs_Brief_Only_For_Real_Work) {
    CHECK(needsBrief("please fix the user interface and make it elegant, not boilerplate"));
    CHECK(!needsBrief("what does this function do?"));
    CHECK(!needsBrief("fix it"));
    CHECK(!needsBrief("[overseer] fix the remaining issues in the parser module now"));
    return "";
}

TEST(agent_Overseer_Nudges_Announced_Work_Then_Finishes) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.autonomy = true;
    int n = 0;
    std::vector<std::string> seen;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        seen.push_back(req.messages.back().content);
        ChatResponse r;
        r.text = n++ == 0 ? "Now let me run the tests." : "All tests pass.";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("run the suite").empty());
    CHECK(n == 2 && seen[1].find("[overseer]") != std::string::npos);
    CHECK(a.stats().nudges == 1);
    return "";
}

TEST(agent_Goal_Runs_Until_Audit_Says_Done) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int work = 0, audits = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        ChatResponse r;
        if (req.system.find("planning council") != std::string::npos) r.text = "INTENT: ship";
        else if (req.system.find("audit") != std::string::npos) r.text = ++audits < 2 ? "CONTINUE: tests missing" : "DONE";
        else r.text = "Implemented and verified (" + std::to_string(++work) + ").";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runGoal("ship it", 5).empty());
    CHECK(work == 2 && audits == 2 && a.goal().empty());
    return "";
}

TEST(agent_Fallback_Role_Takes_Over_Transient_Failures) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.fallback = {resolveModel(defaultConfig(), "deepseek").value};
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        if (req.model.provider.name == "orcarouter") return Result<ChatResponse>::Err("HTTP 503: overloaded");
        ChatResponse r;
        r.text = "ok";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("hi").empty() && a.stats().fallbacks == 1);
    return "";
}

TEST(provider_Codex_Body_And_Stream) {
    ChatRequest req;
    req.model = resolveModel(defaultConfig(), "codex").value;
    req.system = "sys";
    req.thinking = "high";
    req.sessionTag = "tag";
    req.messages = {{"user", "hi", {}, ""}, {"assistant", "", {{"c1", "bash", "{\"command\":\"ls\"}"}}, ""},
                    {"tool", "out", {}, "c1"}};
    req.tools = nativeToolDefs();
    auto b = buildCodexBody(req);
    CHECK(b.at("instructions").asStr() == "sys" && !b.at("store").asBool() && b.at("stream").asBool());
    CHECK(b.at("input").size() == 3 && b.at("input").at((size_t)1).at("type").asStr() == "function_call");
    CHECK(b.at("input").at((size_t)2).at("output").asStr() == "out");
    CHECK(b.at("reasoning").at("effort").asStr() == "high" && b.at("prompt_cache_key").asStr() == "tag");
    CodexStreamAcc acc;
    for (const char* ev : {R"({"type":"response.output_text.delta","delta":"Hel"})",
                           R"({"type":"response.output_text.delta","delta":"lo"})",
                           R"({"type":"response.output_item.done","item":{"type":"function_call","call_id":"x","name":"read","arguments":"{}"}})",
                           R"({"type":"response.output_item.done","item":{"type":"reasoning","encrypted_content":"E"}})",
                           R"({"type":"response.completed","response":{"usage":{"input_tokens":100,"output_tokens":5,"input_tokens_details":{"cached_tokens":60}}}})"})
        acc.feed(json::parse(ev).value);
    auto r = acc.finish();
    CHECK(acc.done && r.text == "Hello" && r.calls.size() == 1 && r.calls[0].id == "x");
    CHECK(r.cacheHit == 60 && r.cacheMiss == 40 && r.replay.at("items").size() == 1);
    return "";
}

TEST(tui_Pick_Filter_Adds_Fuzzy_After_Substring) {
    std::vector<std::string> labels = {"glm-5.3-flash", "deepseek-flash", "claude"};
    auto f = pickFilter(labels, "flash");
    CHECK(f.size() == 2 && f[0] == 0);
    auto t = pickFilter(labels, "depsek");
    CHECK(!t.empty() && t[0] == 1);
    return "";
}

TEST(agent_Recovers_From_Empty_Reasoning_Only_Replies) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    int n = 0;
    opts.thinking = "high";
    std::vector<std::string> levels;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        levels.push_back(req.thinking);
        if (n++ < 2) return Result<ChatResponse>::Err("empty response from provider");
        ChatResponse r;
        r.text = "done";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    std::string err = a.runTurn("go");
    if (!err.empty()) return "runTurn: " + err + " after " + std::to_string(n);
    CHECK(n == 3 && levels[0] == "high" && levels[1] == "off" && levels[2] == "off");
    CHECK(validateHistory(a.messages()).empty());
    return "";
}

TEST(agent_Output_Cap_Raises_Budget_And_Retries) {
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    std::vector<long> budgets;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        budgets.push_back(req.maxTokens);
        if (budgets.size() == 1)
            return Result<ChatResponse>::Err("provider output limit reached; increase model max_tokens (no tools executed)");
        ChatResponse r;
        r.text = "done";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("write a big file").empty());
    CHECK(budgets.size() == 2 && budgets[1] == budgets[0] * 2);
    CHECK(validateHistory(a.messages()).empty());
    return "";
}

TEST(agent_Read_Image_Attaches_Pixels_To_Next_Message) {
    std::string ws = makeTempDir("pocket-view");
    CHECK(atomicWriteFile(ws + "/shot.png", std::string("\x89PNG\r\n\x1a\n", 8) + std::string(64, 'p')).ok);
    auto auth = authorityInit(ws, {}, {}, false);
    CHECK(auth.ok);
    ToolEnv env;
    env.auth = &auth.value;
    env.workspace = ws;
    AgentOpts opts;
    opts.model = resolveModel(defaultConfig(), "glm").value;
    opts.tools = &env;
    int n = 0;
    size_t imagesSeen = 0;
    opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
        for (const auto& m : req.messages) imagesSeen = std::max(imagesSeen, m.images.size());
        ChatResponse r;
        if (n++ == 0) r.calls = {{"c1", "read", "{\"path\":\"shot.png\"}"}};
        else r.text = "looks right";
        return Result<ChatResponse>::Ok(r);
    };
    Agent a(opts);
    CHECK(a.runTurn("check the screenshot").empty());
    CHECK(imagesSeen == 1 && env.viewImages.empty());
    CHECK(validateHistory(a.messages()).empty());
    authorityClose(auth.value);
    rmRf(ws);
    return "";
}
