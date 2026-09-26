// End-to-end recursive CLI with a real confined curl transport and local fixture.
#include "mini.h"
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <cmath>
#include <filesystem>
#include "../src/config.h"
#include "../src/process.h"
#include "../src/session.h"
#include "../src/skills.h"

using namespace pocket;
using namespace pocket::test;

namespace {
enum class CliFixture { Recursive, Judge, Goal };
struct RecursiveServer {
    int fd = -1;
    std::string url;
    std::thread worker;
    std::vector<std::string> models;
    bool sawChild = false;
    CliFixture fixture;
    std::atomic<bool> completeAudit{false};
    int audits = 0, work = 0;
    bool sawFollowup = false;
    int completions = 0;
    explicit RecursiveServer(CliFixture mode = CliFixture::Recursive) : fixture(mode) {
        fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t len = sizeof(address);
        if (fd < 0 || bind(fd, (sockaddr*)&address, len) || listen(fd, 8) || getsockname(fd, (sockaddr*)&address, &len)) return;
        url = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port)) + "/v1";
        worker = std::thread([this] {
            for (;;) {
                pollfd wait{fd, POLLIN, 0};
                if (poll(&wait, 1, 20000) <= 0) return;
                int client = accept4(fd, nullptr, nullptr, SOCK_CLOEXEC);
                if (client < 0) return;
                timeval timeout{5, 0};
                setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                std::string request;
                char buf[4096];
                size_t boundary = std::string::npos;
                while (request.size() < (1u << 20)) {
                    auto n = recv(client, buf, sizeof(buf), 0);
                    if (n <= 0) break;
                    request.append(buf, n);
                    boundary = request.find("\r\n\r\n");
                    if (boundary == std::string::npos) continue;
                    auto k = toLower(request).find("content-length:");
                    size_t bytes = k == std::string::npos ? 0 : strtoul(request.c_str() + k + 15, nullptr, 10);
                    if (request.size() >= boundary + 4 + bytes) break;
                }
                std::string reply = R"({"data":[]})";
                if (startsWith(request, "POST /completion ")) {
                    ++completions;
                    reply = R"({"completion_probabilities":[{"top_probs":[{"token":"yes","prob":0.95},{"token":"no","prob":0.05}]}]})";
                } else if (startsWith(request, "POST ") && boundary != std::string::npos) {
                    auto body = json::parse(request.substr(boundary + 4));
                    std::string model = body.value.at("model").asStr();
                    models.push_back(model);
                    if (fixture == CliFixture::Goal) {
                        const auto& messages = body.value.at("messages").asArr();
                        const std::string system = messages.empty() ? "" : messages.front().at("content").asStr();
                        std::string answer;
                        if (system.find("You audit an autonomous agent") != std::string::npos) {
                            ++audits;
                            answer = completeAudit.load() ? "DONE" : "invalid audit response";
                        } else if (system.find("planning council") != std::string::npos) answer = "INTENT: fixture goal";
                        else {
                            ++work;
                            answer = "work completed";
                            for (const auto& message : messages)
                                sawFollowup |= message.at("content").asStr().find("verify the final requirement") != std::string::npos;
                        }
                        reply = json::stringify(json::Object{{"choices", json::Array{json::Object{
                            {"message", json::Object{{"content", answer}}}, {"finish_reason", "stop"}}}}});
                    } else if (model == "child-model") {
                        sawChild = true;
                        reply = R"({"choices":[{"message":{"content":"child completed"},"finish_reason":"stop"}],"usage":{"prompt_tokens":20,"completion_tokens":5,"cost":0.2}})";
                    } else if (models.size() == 1 && fixture == CliFixture::Recursive) {
                        reply = R"({"choices":[{"message":{"content":"","tool_calls":[{"id":"child","type":"function","function":{"name":"bash","arguments":"{\"command\":\"pocket --allow-root -p child\"}"}}]},"finish_reason":"tool_calls"}],"usage":{"prompt_tokens":20,"completion_tokens":5,"cost":0.1}})";
                    } else {
                        reply = R"({"choices":[{"message":{"content":"parent completed"},"finish_reason":"stop"}],"usage":{"prompt_tokens":20,"completion_tokens":5,"cost":0.1}})";
                    }
                }
                std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                    std::to_string(reply.size()) + "\r\nConnection: close\r\n\r\n" + reply;
                size_t offset = 0;
                while (offset < response.size()) {
                    auto n = send(client, response.data() + offset, response.size() - offset, MSG_NOSIGNAL);
                    if (n <= 0) break;
                    offset += n;
                }
                close(client);
            }
        });
    }
    void stop() { if (fd >= 0) shutdown(fd, SHUT_RDWR); if (worker.joinable()) worker.join(); }
    ~RecursiveServer() { stop(); if (fd >= 0) close(fd); }
};
}

TEST(cli_Recursive_Child_Uses_Selected_Model_And_Reports_Combined_Cost) {
    std::string dir = makeTempDir("pocket-recursive-cli");
    HomeGuard hg(dir);
    CHECK(ensureDir(dir + "/workspace", 0700).ok);
    CHECK(ensureDir(userConfigDir(), 0700).ok);
    RecursiveServer server;
    CHECK(!server.url.empty());
    auto config = json::Object{{"providers", json::Object{{"fixture", json::Object{{"base_url", server.url}}}}},
        {"default_model", "fixture:main-model"}, {"roles", json::Object{{"subagent", "fixture:child-model"}}},
        {"autonomy", false}, {"review", false}, {"jev", false}, {"max_rounds", 4}};
    CHECK(atomicWriteFile(userConfigPath(), json::stringify(config)).ok);
    CHECK(ensureDir(dir + "/workspace/.pocket", 0700).ok);
    CHECK(atomicWriteFile(dir + "/workspace/.pocket/config.json",
                          R"({"roles":{"main":"fixture:main-model"}})").ok);
    char cwd[4096];
    CHECK(getcwd(cwd, sizeof(cwd)) != nullptr);
    // Debug/sanitizer binaries may exceed the old 64 MiB staging limit. An
    // inert ELF trailer makes the same boundary testable in release builds.
    const std::string executable = dir + "/pocket";
    std::error_code copyError;
    CHECK(std::filesystem::copy_file(std::string(cwd) + "/pocket", executable, copyError));
    if (std::filesystem::file_size(executable) < (65u << 20))
        CHECK(truncate(executable.c_str(), 65u << 20) == 0);
    SpawnOpts opts;
    opts.exe = executable;
    opts.argv = {opts.exe, "--allow-root", dir + "/workspace", "-p", "delegate"};
    opts.timeoutMs = 20000;
    auto result = spawn(opts);
    server.stop();
    if (!result.ok || result.exitCode != 0 || !server.sawChild) {
        std::string diagnostic = "recursive CLI: child=" + std::to_string(server.sawChild) +
            " exit=" + std::to_string(result.exitCode) + " " + result.error + "\n" + result.err;
        for (const auto& s : sessionList(10, dir + "/workspace")) {
            auto transcript = sessionLoad(s.id);
            if (transcript.ok) for (const auto& event : transcript.value.events)
                if (event.type == "tool_result") diagnostic += "\ntool result: " + event.text;
        }
        return diagnostic;
    }
    CHECK_EQ(server.models.size(), size_t(3));
    CHECK(result.out.find("parent completed") != std::string::npos);
    auto sessions = sessionList(10, dir + "/workspace");
    CHECK_EQ(sessions.size(), size_t(1));
    auto meta = sessionLoadMeta(sessions[0].id);
    CHECK(meta.ok);
    CHECK(std::fabs(meta.value.cost - .4) < 1e-9);
    CHECK_EQ(meta.value.childSessions, 1L);
    auto notices = sessionWorkspaceRead(dir + "/workspace");
    CHECK(notices.ok);
    bool peer = false;
    for (const auto& event : notices.value.events) peer |= event.sessionId != sessions[0].id;
    CHECK(peer);
    rmRf(dir);
    return "";
}

TEST(cli_Empty_And_Conflicting_Prompt_Modes_Fail_Fast) {
    for (const auto& args : std::vector<std::vector<std::string>>{{"./pocket", "-p", ""},
            {"./pocket", "-g", "   "}, {"./pocket", "-g", "goal", "-p", "prompt"}}) {
        SpawnOpts opts;
        opts.exe = "./pocket";
        opts.argv = args;
        opts.timeoutMs = 2000;
        auto r = spawn(opts);
        CHECK_EQ(r.exitCode, 2);
        CHECK(!r.timedOut);
    }
    return "";
}

TEST(cli_Local_Judge_Activity_Reaches_The_User) {
    std::string dir = makeTempDir("pocket-judge-cli");
    HomeGuard hg(dir);
    const std::string workspace = dir + "/workspace";
    CHECK(ensureDir(workspace, 0700).ok);
    CHECK(ensureDir(userConfigDir(), 0700).ok);
    CHECK(ensureDir(projectSkillDir(workspace) + "/parser", 0700).ok);
    CHECK(atomicWriteFile(projectSkillDir(workspace) + "/parser/SKILL.md",
                         "# Parser tests\n\nFix parser bugs and test invalid inputs.\n").ok);
    RecursiveServer server(CliFixture::Judge);
    CHECK(!server.url.empty());
    const int port = std::stoi(server.url.substr(server.url.find_last_of(':') + 1));
    auto config = json::Object{{"providers", json::Object{{"fixture", json::Object{{"base_url", server.url}}}}},
        {"default_model", "fixture:main-model"}, {"autonomy", false}, {"review", false}, {"jev", false},
        {"local_lm", json::Object{{"port", port}}}};
    CHECK(atomicWriteFile(userConfigPath(), json::stringify(config)).ok);
    SpawnOpts opts;
    opts.exe = "./pocket";
    opts.argv = {opts.exe, "--allow-root", workspace, "-p", "Fix parser bugs and test invalid inputs carefully."};
    opts.timeoutMs = 10000;
    auto result = spawn(opts);
    server.stop();
    if (!result.ok || result.exitCode != 0) return "judge CLI: " + result.error + "\n" + result.err;
    CHECK_EQ(server.completions, 1);
    CHECK(result.err.find("local LM") != std::string::npos);
    CHECK(result.err.find("successful") != std::string::npos);
    CHECK(result.out.find("parent completed") != std::string::npos);
    CHECK(result.out.find("judge:") == std::string::npos);  // keep -p stdout composable
    rmRf(dir);
    return "";
}

TEST(cli_Resume_Followup_Continues_Paused_Goal) {
    const std::string dir = makeTempDir("pocket-goal-cli"), workspace = dir + "/workspace";
    HomeGuard hg(dir);
    CHECK(ensureDir(workspace, 0700).ok);
    CHECK(ensureDir(userConfigDir(), 0700).ok);
    RecursiveServer server(CliFixture::Goal);
    CHECK(!server.url.empty());
    const int port = std::stoi(server.url.substr(server.url.find_last_of(':') + 1));
    auto config = json::Object{{"providers", json::Object{{"fixture", json::Object{{"base_url", server.url}}}}},
        {"default_model", "fixture:main-model"}, {"autonomy", false}, {"review", false}, {"jev", false},
        {"local_lm", json::Object{{"port", port}}}};
    CHECK(atomicWriteFile(userConfigPath(), json::stringify(config)).ok);
    SpawnOpts opts;
    opts.exe = "./pocket";
    opts.argv = {opts.exe, "--allow-root", workspace, "-g", "Complete the fixture goal"};
    opts.timeoutMs = 15000;
    auto first = spawn(opts);
    if (!first.ok || first.exitCode != 1) return "goal should pause on invalid audit:\n" + first.err;
    auto sessions = sessionList(10, workspace);
    CHECK_EQ(sessions.size(), size_t(1));
    auto paused = sessionLoadMeta(sessions[0].id);
    CHECK(paused.ok && paused.value.goalStatus == "paused");
    server.completeAudit.store(true);
    opts.argv = {opts.exe, "--allow-root", workspace, "--resume", sessions[0].id, "-p", "verify the final requirement"};
    auto second = spawn(opts);
    server.stop();
    if (!second.ok || second.exitCode != 0) return "goal follow-up did not resume:\n" + second.err;
    auto completed = sessionLoadMeta(sessions[0].id);
    CHECK(completed.ok && completed.value.goalStatus == "completed");
    CHECK_EQ(completed.value.goal, std::string("Complete the fixture goal"));
    CHECK(server.sawFollowup);
    CHECK_EQ(server.work, 2);
    CHECK_EQ(server.audits, 2);
    rmRf(dir);
    return "";
}
