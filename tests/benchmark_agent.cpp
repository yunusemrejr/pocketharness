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
        int rounds = 0, decisions = 0, observers = 0;
        long wireBytes = 0;
        std::string firstThinking;
        opts.request = [&](const ChatRequest& req, const ChatCallbacks&) {
            ++requests;
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            ChatResponse r;
            // Synthetic accounting = deterministic prompt estimate, 20 output
            // tokens/request. These are fixture numbers, not API measurements.
            auto wire = json::stringify(buildOpenAiBody(req));
            r.inTokens = estTokens(wire);
            r.outTokens = 20;
            if (req.system.find("planning council") != std::string::npos) {
                ++planning;
                r.text = "INTENT: Complete the requested work. ACCEPTANCE: Verify the title file.";
            } else if (req.system.find("strict senior reviewer") != std::string::npos) {
                ++reviews;
                r.text = "LGTM";
            } else {
                wireBytes += (long)wire.size();
                if (rounds == 0) firstThinking = req.thinking;
                int n = rounds++;
                int reads = complex ? 24 : 0;
                if (n < reads)
                    r.calls = {{"read" + std::to_string(n), "read", "{\"path\":\"evidence" + std::to_string(n) + "\"}"}};
                else if (n == reads)
                    r.calls = {{"write", "write", "{\"path\":\"title.txt\",\"content\":\"Pocket Harness\\n\"}"}};
                else if (n == reads + 1)
                    r.calls = {{"check", "bash", "{\"command\":\"test \\\"$(cat title.txt)\\\" = \\\"Pocket Harness\\\"\"}"}};
                else r.text = "Updated title.txt; the command verified the requested title.";
            }
            return Result<ChatResponse>::Ok(r);
        };
        opts.decide = [&](const json::Value&, const std::vector<Question>& qs, bool, double*) {
            ++decisions;
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
        bool correct = error.empty() && final.ok && final.value == "Pocket Harness\n" &&
                       validateHistory(agent.messages()).empty() && agent.stats().toolCalls == (complex ? 26 : 2);
        results.push_back(json::Object{{"case", complex ? "complex-steady-progress" : "simple-title-edit"},
            {"correct", correct}, {"error", error}, {"completion_ms", elapsed}, {"model_requests", requests.load()},
            {"planning_requests", planning.load()}, {"review_requests", reviews.load()},
            {"decision_calls", decisions}, {"observer_calls", observers},
            {"coordination_calls", planning.load() + reviews.load() + decisions + observers},
            {"tool_calls", agent.stats().toolCalls}, {"fixture_input_tokens", agent.stats().inTokens},
            {"fixture_output_tokens", agent.stats().outTokens}, {"main_wire_bytes", wireBytes},
            {"initial_thinking", firstThinking}});
        authorityClose(auth.value);
        if (!correct) { std::cerr << error << '\n'; return 1; }
    }
    std::cout << json::stringify(json::Object{{"fixture_latency_ms", 40}, {"decision_latency_ms", 12},
        {"measurement", "offline deterministic fixture; token estimates are not live API usage"}, {"cases", results}}, true) << '\n';
    pocket::test::rmRf(home);
}
