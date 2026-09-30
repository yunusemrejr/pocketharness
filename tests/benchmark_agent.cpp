// Deterministic native-loop fixture. No network or real provider token claims.
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "agent.h"
#include "brain.h"
#include "config.h"
#include "sandbox.h"
#include "../tests/mini.h"

using namespace pocket;

// buildOpenAiBody returns the conversation/tools body; provider.cpp prepends
// the frozen system message afterwards. Include that prefix in this fixture's
// byte/token estimate too, so prompt reductions are actually measurable.
std::string fixtureWire(const ChatRequest& req) {
    auto body = buildOpenAiBody(req);
    if (!req.system.empty()) {
        auto messages = body.at("messages").asArr();
        messages.insert(messages.begin(), json::Object{{"role", "system"}, {"content", req.system}});
        body.asObj()["messages"] = std::move(messages);
    }
    return json::stringify(body);
}

int main() {
    auto home = pocket::test::makeTempDir("pocket-policy-bench");
    pocket::test::HomeGuard isolated(home);
    json::Array results;
    for (bool complex : {false, true}) {
        auto ws = home + (complex ? "/complex" : "/simple");
        (void)ensureDir(ws, 0700);
        for (int i = 0; i < 24; ++i)
            (void)atomicWriteFile(ws + "/evidence" + std::to_string(i),
                                 "Observed unique evidence for implementation " + std::to_string(i));
        auto auth = authorityInit(ws, {}, {}, false);
        if (!auth.ok) return 1;
        ToolEnv env;
        env.workspace = ws;
        env.auth = &auth.value;
        env.unsafe = true;
        env.sessionTmp = ws;
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.tools = &env;
        opts.thinking = "adaptive";
        opts.brief = opts.review = opts.autonomy = true;
        std::atomic<int> requests{0}, planning{0}, reviews{0};
        std::atomic<bool> reviewEvidence{false};
        int rounds = 0, decisions = 0, observers = 0;
        long wireBytes = 0, decisionBytes = 0;
        long systemBytes = 0, schemaBytes = 0;
        std::string firstThinking;
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            ++requests;
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            ChatResponse r;
            // Synthetic accounting = deterministic prompt estimate, 20 output
            // tokens/request. These are fixture numbers, not API measurements.
            auto wire = fixtureWire(req);
            r.inTokens = estTokens(wire);
            r.outTokens = 20;
            if (req.system.find("planning council") != std::string::npos) {
                ++planning;
                r.text = "INTENT: Complete the requested work. ACCEPTANCE: Verify the title file.";
            } else if (req.system.find("strict senior reviewer") != std::string::npos) {
                ++reviews;
                for (const auto& message : req.messages)
                    reviewEvidence = reviewEvidence || (message.content.find("[exit: 0]") != std::string::npos &&
                        message.content.find("\nVerified requested title\n") != std::string::npos);
                r.text = "LGTM";
            } else {
                wireBytes += (long)wire.size();
                if (rounds == 0) {
                    firstThinking = req.thinking;
                    systemBytes = req.system.size();
                    schemaBytes = json::stringify(buildOpenAiBody(req).at("tools")).size();
                }
                int n = rounds++;
                int reads = complex ? 24 : 0;
                if (n < reads)
                    r.calls = {{"read" + std::to_string(n), "read", "{\"path\":\"evidence" + std::to_string(n) + "\"}"}};
                else if (n == reads)
                    r.calls = {{"write", "write", "{\"path\":\"title.txt\",\"content\":\"Pocket Harness\\n\"}"}};
                else if (n == reads + 1)
                    r.calls = {{"check", "bash", json::stringify(json::Object{
                        {"command", "test \"$(cat title.txt)\" = \"Pocket Harness\" && printf 'Verified requested title\\n'"}})}};
                else r.text = "Updated title.txt; the command verified the requested title.";
            }
            return Result<ChatResponse>::Ok(r);
        };
        opts.decide = [&](const json::Value& state, const std::vector<Question>& qs, bool, double*) {
            ++decisions;
            decisionBytes += json::stringify(state).size();
            std::this_thread::sleep_for(std::chrono::milliseconds(12));
            std::map<std::string, double> p;
            for (const auto& q : qs) p[q.id] = q.id == "bad" ? .5 : 0;
            return p;
        };
        opts.progress = [&](const std::string&, double*) {
            ++observers;
            std::this_thread::sleep_for(std::chrono::milliseconds(12));
            return std::string();
        };
        Agent agent(opts);
        int64_t begin = nowMs();
        std::string error = agent.runTurn(complex ?
            "Inspect every evidence file, implement the migration across modules, benchmark correctness and verify delivery." :
            "Please make the project title say Pocket Harness.");
        long elapsed = nowMs() - begin;
        auto final = readFileBounded(ws + "/title.txt", 100);
        bool verifiedCommand = false;
        for (const auto& message : agent.messages())
            if (message.role == "tool" && message.toolCallId == "check")
                verifiedCommand = startsWith(message.content, "[exit: 0]\n") &&
                    message.content.find("\nVerified requested title\n") != std::string::npos;
        bool correct = error.empty() && final.ok && final.value == "Pocket Harness\n" &&
                       verifiedCommand && validateHistory(agent.messages()).empty() &&
                       agent.stats().toolCalls == (complex ? 26 : 2);
        results.push_back(json::Object{{"case", complex ? "complex-steady-progress" : "simple-title-edit"},
            {"correct", correct}, {"error", error}, {"completion_ms", elapsed}, {"model_requests", requests.load()},
            {"planning_requests", planning.load()}, {"review_requests", reviews.load()},
            {"verified_command_result", verifiedCommand},
            {"review_saw_command_result", reviewEvidence.load()},
            {"decision_calls", decisions}, {"decision_input_bytes", decisionBytes}, {"observer_calls", observers},
            {"coordination_calls", planning.load() + reviews.load() + decisions + observers},
            {"tool_calls", agent.stats().toolCalls}, {"fixture_input_tokens", agent.stats().inTokens},
            {"fixture_output_tokens", agent.stats().outTokens}, {"main_wire_bytes", wireBytes},
            {"first_main_system_bytes", systemBytes}, {"tool_schema_bytes", schemaBytes},
            {"initial_thinking", firstThinking}});
        authorityClose(auth.value);
        if (!correct) { std::cerr << error << '\n'; return 1; }
    }
    // Re-reading an unchanged large result must not repeat the relevance
    // judge. Vary the requested limit so this exercises result reuse rather
    // than the guard against an identical tool-call loop.
    {
        auto ws = home + "/repeated";
        (void)ensureDir(ws, 0700);
        std::string evidence;
        for (int i = 0; i < 20; ++i) evidence += std::string(1000, 'a' + i % 20) + "\n";
        (void)atomicWriteFile(ws + "/evidence", evidence);
        auto auth = authorityInit(ws, {}, {}, false);
        if (!auth.ok) return 1;
        ToolEnv env;
        env.workspace = ws;
        env.auth = &auth.value;
        AgentOpts opts;
        opts.model = resolveModel(defaultConfig(), "glm").value;
        opts.tools = &env;
        int requests = 0, decisions = 0;
        long wireBytes = 0, decisionBytes = 0;
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            auto wire = fixtureWire(req);
            wireBytes += wire.size();
            ChatResponse r;
            r.inTokens = estTokens(wire);
            r.outTokens = 20;
            int n = requests++;
            if (n < 8) r.calls = {{"read" + std::to_string(n), "read", json::stringify(json::Object{
                {"path", "evidence"}, {"limit", 100 + n}})}};
            else r.text = "The evidence file has twenty lines; each read returned the same content.";
            return Result<ChatResponse>::Ok(r);
        };
        opts.decide = [&](const json::Value& state, const std::vector<Question>& qs, bool, double*) {
            ++decisions;
            decisionBytes += json::stringify(state).size();
            std::this_thread::sleep_for(std::chrono::milliseconds(12));
            std::map<std::string, double> p;
            for (const auto& q : qs) p[q.id] = 1;
            return p;
        };
        Agent agent(opts);
        int64_t begin = nowMs();
        std::string error = agent.runTurn("Inspect the evidence file and compare repeated observations.");
        long elapsed = nowMs() - begin;
        bool correct = error.empty() && validateHistory(agent.messages()).empty() &&
                       agent.stats().toolCalls == 8 && agent.stats().deduped == 7;
        results.push_back(json::Object{{"case", "repeated-large-result"}, {"correct", correct}, {"error", error},
            {"completion_ms", elapsed}, {"model_requests", requests}, {"planning_requests", 0}, {"review_requests", 0},
            {"decision_calls", decisions}, {"observer_calls", 0}, {"coordination_calls", decisions},
            {"decision_input_bytes", decisionBytes}, {"tool_calls", agent.stats().toolCalls},
            {"fixture_input_tokens", agent.stats().inTokens}, {"fixture_output_tokens", agent.stats().outTokens},
            {"main_wire_bytes", wireBytes}, {"deduplicated_results", agent.stats().deduped}});
        authorityClose(auth.value);
        if (!correct) { std::cerr << error << '\n'; return 1; }
    }
    std::cout << json::stringify(json::Object{{"fixture_latency_ms", 40}, {"decision_latency_ms", 12},
        {"measurement", "offline deterministic fixture; token estimates are not live API usage"}, {"cases", results}}, true) << '\n';
    pocket::test::rmRf(home);
}
