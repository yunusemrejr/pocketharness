// End-to-end recursive CLI with a real confined curl transport and local fixture.
#include "mini.h"
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <cmath>
#include "../src/config.h"
#include "../src/process.h"
#include "../src/session.h"

using namespace pocket;
using namespace pocket::test;

namespace {
struct RecursiveServer {
    int fd = -1;
    std::string url;
    std::thread worker;
    std::vector<std::string> models;
    bool sawChild = false;
    RecursiveServer() {
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
                if (startsWith(request, "POST ") && boundary != std::string::npos) {
                    auto body = json::parse(request.substr(boundary + 4));
                    std::string model = body.value.at("model").asStr();
                    models.push_back(model);
                    if (model == "child-model") {
                        sawChild = true;
                        reply = R"({"choices":[{"message":{"content":"child completed"},"finish_reason":"stop"}],"usage":{"prompt_tokens":20,"completion_tokens":5,"cost":0.2}})";
                    } else if (models.size() == 1) {
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
    SpawnOpts opts;
    opts.exe = std::string(cwd) + "/pocket";
    opts.argv = {opts.exe, "--allow-root", dir + "/workspace", "-p", "delegate"};
    opts.timeoutMs = 20000;
    auto result = spawn(opts);
    server.stop();
    if (!result.ok || result.exitCode != 0) return "recursive CLI: " + result.error + "\n" + result.err;
    CHECK(server.sawChild);
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
