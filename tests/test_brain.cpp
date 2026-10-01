// PocketHarness tests - native intelligence, catalog, overseer, codex wire.
#include "mini.h"

#include <cmath>
#include <thread>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

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

TEST(brain_Task_Policy_Scales_With_Risk_Uncertainty_And_Observed_Work) {
    auto simple = taskPolicy("Please make the project title say Pocket Harness.");
    CHECK(simple.scale == TaskScale::Simple && simple.fastModel && !simple.planningBrief && simple.thinking == "low");
    auto uncertain = taskPolicy("Diagnose the intermittent parser regression");
    CHECK(uncertain.scale == TaskScale::Standard && !uncertain.fastModel && uncertain.thinking == "medium");
    auto complex = taskPolicy("Implement the migration across modules and benchmark correctness");
    CHECK(complex.scale == TaskScale::Complex && complex.planningBrief && complex.reviewers == 3);
    auto risky = taskPolicy("Deploy the production authentication change");
    CHECK(risky.scale == TaskScale::Critical && risky.thinking == "high" && !risky.fastModel);
    auto failed = taskPolicy("Read the title", {2, 0, 2, 2});
    CHECK(failed.scale == TaskScale::Standard && failed.thinking == "high" && !failed.fastModel);
    auto broad = taskPolicy("Fix the title", {0, 4, 6, 0});
    CHECK(broad.scale == TaskScale::Complex && broad.reviewers == 3);
    TaskObservation riskyAction;
    riskyAction.actionScale = TaskScale::Critical;
    CHECK(taskPolicy("Read the title", riskyAction).scale == TaskScale::Critical);
    CHECK(taskPolicy("Fine-tune a language model with QLoRA in Google Colab").scale == TaskScale::Complex);
    CHECK(taskPolicy("Create an animation with music and a voiceover").scale == TaskScale::Complex);
    CHECK(taskPolicy("Fix the save bug").workspaceWork);
    CHECK(!taskPolicy("How do I fix the save bug?").workspaceWork);
    CHECK(!taskPolicy("Write a short poem about rain").workspaceWork);
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
    CHECK(quirkFromError("HTTP 400: The `reasoning_content` in the thinking mode must be passed back to the API.", true,
                         "max_tokens") == "thinking_off_once");
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

TEST(brain_Implicit_Fast_Route_Avoids_Well_Observed_Unhealthy_Or_Slow_Providers) {
    std::string home = makeTempDir("pocket-route-health");
    HomeGuard isolated(home);
    CHECK(brainPreferFast("main", "unknown")); // unknown is not fabricated as bad
    brainNoteHealth("one-failure", false, 100);
    CHECK(brainPreferFast("main", "one-failure")); // one sample is not a trend
    for (int i = 0; i < 8; ++i) {
        brainNoteHealth("main", true, 1000);
        brainNoteHealth("unhealthy", false, 100);
        brainNoteHealth("slow", true, 9000);
        brainNoteHealth("fast", true, 400);
    }
    CHECK(!brainPreferFast("main", "unhealthy"));
    CHECK(!brainPreferFast("main", "slow"));
    CHECK(brainPreferFast("main", "fast"));
    CHECK(brainPreferFast("unknown", "slow")); // no main latency to compare
    CHECK(brainPreferFast("unhealthy", "unhealthy")); // the very same route is never compared with itself
    // Two models behind one provider/gateway are judged separately.
    std::string slowModel = brainRouteKey("gateway", "fast-model"), mainModel = brainRouteKey("gateway", "main-model");
    for (int i = 0; i < 8; ++i) {
        brainNoteHealth(mainModel, true, 1000);
        brainNoteHealth(slowModel, true, 9000);
    }
    CHECK(!brainPreferFast(mainModel, slowModel));
    CHECK(brainPreferFast(mainModel, brainRouteKey("gateway", "untried-model")));  // sparse data stays eligible
    CHECK(brainRouteKey("gateway", "m", "anthropic") != brainRouteKey("gateway", "m"));  // explicit routing is its own route
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

TEST(catalog_Records_Mandatory_Reasoning_Floor) {
    auto c = parseCatalog("openrouter", R"({"data":[
        {"id":"z/must","supported_parameters":["reasoning"],
         "reasoning":{"mandatory":true,"supported_efforts":["max","high","low"],"default_effort":"max"}},
        {"id":"z/opt","supported_parameters":["reasoning"],"reasoning":{"mandatory":false}}]})");
    CHECK(c.size() == 2);
    CHECK_EQ(c[0].floor, std::string("low"));
    CHECK(c[1].floor.empty() && c[1].reasoning == 1);
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
    CHECK(atomicWriteFile(d + "/Makefile", "test :\n\t./t\n").ok);
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

TEST(agent_Planning_Brief_Only_For_Substantial_Work) {
    auto brief = [](const std::string& request) {
        auto policy = taskPolicy(request);
        return policy.planningBrief && policy.workspaceWork;
    };
    CHECK(brief("Fine-tune a language model with QLoRA in Google Colab"));
    CHECK(brief("Implement the migration across modules and benchmark correctness"));
    CHECK(!brief("How do I fine-tune a language model with QLoRA?"));
    CHECK(!brief("Please make the project title say Pocket Harness."));
    CHECK(!brief("Write a short poem about rain"));
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
    CHECK(work == 2 && audits == 2 && a.goal() == "ship it");
    CHECK(a.goalStatus() == GoalStatus::Completed);
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

TEST(brain_Unseen_Vocabulary_Does_Not_Bias_Stop_Class) {
    std::string text;
    for (int i = 0; i < 30; ++i) text += "unseenwordxyz ";
    auto result = classifyStop(text);
    CHECK(result.kind == StopKind::Done);
    CHECK(std::fabs(result.p - 0.6) < 1e-9);
    return "";
}

TEST(brain_State_Isolated_Across_Homes_And_Merged_Across_Processes) {
    std::string dir = makeTempDir("pocket-brain-isolation");
    {
        HomeGuard hg(dir);
        brainNoteQuirk("first:model", "no_reasoning");
        // The parent has already read state. Children must reload under their
        // write lock instead of writing forked copies of that stale snapshot.
        std::vector<pid_t> children;
        for (int i = 0; i < 4; ++i) {
            pid_t pid = fork();
            CHECK(pid >= 0);
            if (!pid) {
                brainNoteQuirk("child:" + std::to_string(i), "no_stream_usage");
                for (int j = 0; j < 4; ++j) brainNoteHealth("shared", true, 10);
                _exit(0);
            }
            children.push_back(pid);
        }
        for (pid_t pid : children) {
            int status = 0;
            CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        }
        for (int i = 0; i < 4; ++i) CHECK(brainQuirks("child:" + std::to_string(i)).size() == 1);
        auto saved = readFileBounded(stateDir() + "/brain.json", 1 << 20);
        CHECK(saved.ok);
        auto parsed = json::parse(saved.value);
        CHECK(parsed.ok && parsed.value.at("health").at("shared").at("n").asInt() == 16);
        {
            HomeGuard other(dir + "/second");
            CHECK(brainQuirks("first:model").empty());
            brainNoteQuirk("second:model", "no_reasoning");
        }
        CHECK(brainQuirks("first:model").size() == 1);
        CHECK(brainQuirks("second:model").empty());
        brainNoteQuirk("tokens:model", "max_tokens");
        brainNoteQuirk("tokens:model", "max_completion_tokens");
        CHECK(brainQuirks("tokens:model") == std::vector<std::string>{"max_completion_tokens"});
    }
    rmRf(dir);
    return "";
}

TEST(oversee_Parses_Whitespace_Prefix_And_Rejects_Invalid_Probabilities) {
    CHECK(parseYesProbability(R"({"completion_probabilities":[
        {"token":"\n","top_probs":[{"token":"\n","prob":1}]},
        {"token":"yes","probs":[{"tok_str":"yes","prob":0.7},{"tok_str":"no","prob":0.3}]}]})") == 0.7);
    CHECK(parseYesProbability(R"({"completion_probabilities":[{"top_probs":[
        {"token":"yes","prob":-1},{"token":"no","prob":2}]}]})") < 0);
    CHECK(parseYesProbability(R"({"completion_probabilities":[{"token":"<think>"},
        {"token":"yes","probs":[{"tok_str":"yes","prob":1}]}]})") < 0);
    return "";
}

TEST(oversee_Local_Server_Stops_Only_When_Last_Session_Leaves) {
    const char* oldHome = getenv("HOME");
    std::string saved = oldHome ? oldHome : "";
    char tmpl[] = "/tmp/pocket-judge-XXXXXX";
    std::string home = mkdtemp(tmpl);
    // Restores HOME and never leaks the fake server, whichever CHECK fails.
    struct Cleanup {
        std::string home, saved;
        bool hadHome;
        pid_t server = -1;
        ~Cleanup() {
            if (server > 0 && kill(server, SIGKILL) == 0) waitpid(server, nullptr, 0);
            if (hadHome) setenv("HOME", saved.c_str(), 1); else unsetenv("HOME");
            (void)!std::system(("rm -rf " + home).c_str());
        }
    } cleanup{home, saved, oldHome != nullptr};
    setenv("HOME", home.c_str(), 1);
    Config cfg;
    cfg.jev = false;  // stay local: no remote judge, no network
    cfg.localLm.model = home + "/none.gguf";  // configured but absent: never spawns
    cfg.localLm.port = 18999;
    CHECK(ensureDir(stateDir(), 0700).ok);
    // A pocket-started server (by some session) recorded in the pid file.
    pid_t server = fork();
    if (server == 0) {
        execl("/bin/sh", "sh", "-c", "while :; do sleep 1; done", "--port", "18999", (char*)nullptr);
        _exit(127);
    }
    cleanup.server = server;
    std::string pidFile = stateDir() + "/judge-18999.pid";
    CHECK(atomicWriteFile(pidFile, std::to_string(server) + "\n").ok);
    usleep(100000);  // let exec land so /proc/PID/cmdline names the port
    judgeWarm(cfg);  // this session registers as a user
    // Another live session holds its own share of the same server.
    int other = open((stateDir() + "/judge-18999.users").c_str(), O_RDWR | O_CLOEXEC);
    CHECK(other >= 0);
    if (flock(other, LOCK_SH | LOCK_NB) != 0) { close(other); CHECK(false); }
    judgeShutdown();
    CHECK(waitpid(server, nullptr, WNOHANG) == 0);  // still serving the other session
    judgeWarm(cfg);   // a fresh session in this process
    close(other);     // the other session exits
    judgeShutdown();  // last one out stops it
    int status = 0;
    bool stopped = false;
    for (int i = 0; i < 80 && !stopped; ++i) {
        pid_t r = waitpid(server, &status, WNOHANG);  // -1: already reaped by the release
        stopped = r == server || (r < 0 && errno == ECHILD);
        if (!stopped) usleep(25000);
    }
    CHECK(stopped);
    CHECK(access(pidFile.c_str(), F_OK) != 0);
    if (stopped) cleanup.server = -1;
    return "";
}

// Concurrent registration used to race on the shared users-file bookkeeping:
// two judges could both open the file, and the loser overwrote g_usersFd and
// leaked its descriptor. assertNoLeakedUsersFd turns that into a hard check.
static int openFdCount() {
    DIR* d = opendir("/proc/self/fd");
    if (!d) return -1;
    int n = 0;
    while (readdir(d)) ++n;
    closedir(d);
    return n;
}

TEST(oversee_Concurrent_Judge_Warm_Registers_Once_And_Leaks_Nothing) {
    std::string home = makeTempDir("pocket-warm");
    CHECK(!home.empty());
    HomeGuard hg(home);
    Config cfg;
    cfg.jev = false;  // stay local: no remote judge, no network
    cfg.localLm.model = home + "/none.gguf";  // configured but absent: never spawns
    cfg.localLm.port = 18997;
    CHECK(ensureDir(stateDir(), 0700).ok);
    int before = openFdCount();
    CHECK(before >= 0);
    std::vector<std::thread> warmers;
    for (int i = 0; i < 8; ++i) warmers.emplace_back([&] { judgeWarm(cfg); });
    for (auto& t : warmers) t.join();
    judgeShutdown();  // drop the share so the accounting is back to zero
    int after = openFdCount();
    // One users-file descriptor is the most a correct registration can hold.
    CHECK(after <= before + 1);
    return "";
}
