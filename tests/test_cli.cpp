// End-to-end recursive CLI with a real confined curl transport and local fixture.
#include "mini.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <cmath>
#include <filesystem>
#include <locale.h>
#include <langinfo.h>
#include "../src/config.h"
#include "../src/process.h"
#include "../src/session.h"
#include "../src/skills.h"

using namespace pocket;
using namespace pocket::test;

TEST(cli_Decimal_Arguments_And_Css_Are_Independent_Of_Desktop_Locale) {
    std::string commaLocale;
    for (const char* name : {"en_DK.utf8", "de_DE.UTF-8", "fr_FR.UTF-8", "tr_TR.UTF-8"}) {
        locale_t locale = newlocale(LC_NUMERIC_MASK, name, nullptr);
        if (!locale) continue;
        bool comma = std::string(nl_langinfo_l(RADIXCHAR, locale)) == ",";
        freelocale(locale);
        if (comma) { commaLocale = name; break; }
    }
    // Minimal CI images may only ship C/en_US. The actual desktop probe uses
    // en_DK; this remains a real subprocess regression wherever available.
    if (commaLocale.empty()) return "";
    EnvGuard locale("LC_ALL", commaLocale);
    SpawnOpts opts;
    opts.exe = "./pocket";
    opts.argv = {"./pocket", "kit", "spring", "170", "26.5", "1"};
    opts.timeoutMs = 5000;
    auto spring = spawn(opts);
    CHECK(spring.ok && spring.exitCode == 0);
    CHECK(spring.out.find("linear(0, 0.") != std::string::npos);
    opts.argv = {"./pocket", "kit", "frame", "/nonexistent-pocket-scene.html", "out.png", "--time", "1.5"};
    auto frame = spawn(opts);
    CHECK(frame.ok && frame.exitCode != 0);
    CHECK(frame.err.find("existing local HTML") != std::string::npos);
    return "";
}

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
    bool sawRestoredHistory = false;
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
                    for (const auto& message : body.value.at("messages").asArr())
                        sawRestoredHistory |= message.at("content").asStr() == "remember the target session marker";
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
    if (!first.ok || first.exitCode != 1) return "goal should pause after unconfirmed audits:\n" + first.err;
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
    CHECK_EQ(server.work, 13);  // 12 unconfirmed cycles, then the follow-up
    CHECK_EQ(server.audits, 13);
    rmRf(dir);
    return "";
}

TEST(cli_Interactive_Resume_Handoff_Restores_Target_And_Releases_Leases) {
    const std::string dir = makeTempDir("pocket-session-handoff"), workspace = dir + "/workspace";
    struct Cleanup { std::string path; ~Cleanup() { rmRf(path); } } cleanup{dir};
    HomeGuard hg(dir);
    CHECK(ensureDir(workspace, 0700).ok);
    CHECK(ensureDir(userConfigDir(), 0700).ok);
    const std::string scratchRoot = dir + "/scratch";
    CHECK(ensureDir(scratchRoot, 0700).ok);
    EnvGuard tmpdir("TMPDIR", scratchRoot);
    RecursiveServer server(CliFixture::Judge);
    CHECK(!server.url.empty());
    CHECK(atomicWriteFile(userConfigPath(), json::stringify(json::Object{
        {"providers", json::Object{{"fixture", json::Object{{"base_url", server.url}}}}},
        {"default_model", "fixture:source-model"}, {"autonomy", false}, {"review", false}, {"jev", false}})).ok);
    auto source = sessionCreate(), target = sessionCreate();
    CHECK(source.ok && target.ok);
    SessionMeta old;
    old.workspace = workspace; old.modelSpec = "fixture:source-model";
    old.thinking = "off"; old.cost = 1.25; old.costSeen = true; old.turns = 2;
    CHECK(sessionSaveMeta(source.value, old).ok);
    SessionMeta wanted;
    wanted.workspace = workspace; wanted.modelSpec = "fixture:target-model";
    wanted.thinking = "none"; wanted.cost = .25; wanted.costSeen = true; wanted.turns = 7;
    // Restoring an active goal writes a paused checkpoint during preflight.
    // That write must never collect receipts from the currently open session.
    wanted.goal = "saved target goal"; wanted.goalStatus = "active"; wanted.goalPhase = "work";
    wanted.rolesSet = true; wanted.roles["subagent"] = "fixture:target-child";
    CHECK(sessionSaveMeta(target.value, wanted).ok);
    CHECK(sessionAppend(target.value, {"user", "remember the target session marker", "", "", "", true}).ok);
    CHECK(sessionAppend(target.value, {"assistant", "remembered", "", "", "", true}).ok);
    SpawnOpts opts;
    opts.exe = "./pocket";
    opts.argv = {opts.exe, "--allow-root", workspace, "--resume", source.value,
                 "--model", "fixture:temporary-cli-override", "--thinking", "high"};
    struct InputPipe {
        int fds[2]{-1, -1};
        ~InputPipe() { for (int fd : fds) if (fd >= 0) close(fd); }
    } input;
    CHECK(pipe2(input.fds, O_CLOEXEC) == 0);
    const std::string sourceInfo = "/session\n";
    CHECK(write(input.fds[1], sourceInfo.data(), sourceInfo.size()) == (ssize_t)sourceInfo.size());
    opts.childSetup = [&] {
        if (dup2(input.fds[0], STDIN_FILENO) < 0) _exit(125);
        close(input.fds[0]);
        close(input.fds[1]);
    };
    std::string observed, preparationError;
    bool injected = false;
    opts.onChunk = [&](std::string_view chunk, bool isError) {
        if (isError || injected || !preparationError.empty()) return;
        observed.append(chunk);
        if (observed.find("session: " + source.value) == std::string::npos) return;
        // The source UI is waiting for its next command, so no turn can race
        // receipt collection before the target's preflight restore.
        std::string scratch;
        for (const auto& entry : std::filesystem::directory_iterator(scratchRoot))
            if (startsWith(entry.path().filename().string(), "pocket-" + source.value + "-"))
                scratch = entry.path().string();
        if (scratch.empty()) { preparationError = "source scratch not found"; return; }
        auto receipt = atomicWriteFile(scratch + "/pocket-child-pending.json", json::stringify(json::Object{
            {"cost", 7.0}, {"side_cost", 2.0}, {"children", 0}, {"seen", true}}), 0600);
        if (!receipt.ok) { preparationError = receipt.error; return; }
        injected = true;
        std::string followup = "/resume " + target.value +
            "\n/session\n/goal clear\ncontinue with target history\n/quit\n";
        if (write(input.fds[1], followup.data(), followup.size()) != (ssize_t)followup.size())
            preparationError = "cannot send handoff commands";
        close(input.fds[1]);
        input.fds[1] = -1;
    };
    opts.timeoutMs = 15000;
    auto result = spawn(opts);
    server.stop();
    CHECK(preparationError.empty() && injected);
    if (!result.ok || result.exitCode != 0) return "session handoff failed: " + result.error + "\n" + result.out + result.err;
    CHECK(result.out.find("continuing session: " + target.value) != std::string::npos);
    CHECK(result.out.find("model: target-model") != std::string::npos);
    CHECK(result.out.find("thinking: none") != std::string::npos);
    CHECK(server.models == std::vector<std::string>{"target-model"});
    CHECK(server.sawRestoredHistory);
    auto saved = sessionLoadMeta(target.value), previous = sessionLoadMeta(source.value);
    CHECK(saved.ok && previous.ok);
    CHECK_EQ(saved.value.turns, 8);
    if (std::fabs(saved.value.cost - .35) >= 1e-9)
        return "target cost should be $0.35 without source child usage; got $" + std::to_string(saved.value.cost) +
               " (side $" + std::to_string(saved.value.sideCost) + ", children " + std::to_string(saved.value.childSessions) + ")";
    CHECK(saved.value.sideCost == 0 && saved.value.childSessions == 0);
    CHECK(saved.value.roles.at("subagent") == "fixture:target-child");
    // Leaving the source must collect its outstanding receipt exactly once,
    // without inventing a turn or transferring usage to the target.
    CHECK(previous.value.turns == 2 && previous.value.cost == 8.25);
    CHECK(previous.value.sideCost == 2 && previous.value.childSessions == 1);
    for (const auto& id : {source.value, target.value}) {
        auto lease = sessionLock(id);
        CHECK(lease.ok);
        close(lease.value);
    }
    CHECK(sessionList(10, workspace).size() == 2);
    return "";
}
