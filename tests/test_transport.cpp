// Real curl + confined child + localhost HTTP fixtures. No API keys or model calls.
#include "mini.h"
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include "../src/provider.h"

using namespace pocket;
using namespace pocket::test;

namespace {
struct LocalServer {
    int fd = -1;
    std::string url;
    std::vector<std::string> requests;
    std::thread worker;
    explicit LocalServer(std::vector<std::string> replies) {
        fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t len = sizeof(addr);
        if (fd < 0 || bind(fd, (sockaddr*)&addr, len) || listen(fd, 4) ||
            getsockname(fd, (sockaddr*)&addr, &len)) return;
        url = "http://127.0.0.1:" + std::to_string(ntohs(addr.sin_port)) + "/v1";
        worker = std::thread([this, replies] {
            for (const auto& reply : replies) {
                pollfd wait{fd, POLLIN, 0};
                if (poll(&wait, 1, 7000) <= 0) return;
                int client = accept4(fd, nullptr, nullptr, SOCK_CLOEXEC);
                if (client < 0) return;
                timeval timeout{3, 0};
                setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                std::string request;
                char buf[4096];
                while (true) {
                    ssize_t n = recv(client, buf, sizeof(buf), 0);
                    if (n <= 0) break;
                    request.append(buf, n);
                    size_t end = request.find("\r\n\r\n");
                    if (end == std::string::npos) continue;
                    size_t key = toLower(request).find("content-length:");
                    size_t size = key == std::string::npos ? 0 : strtoul(request.c_str() + key + 15, nullptr, 10);
                    if (request.size() >= end + 4 + size) break;
                }
                requests.push_back(request);
                size_t sent = 0;
                while (sent < reply.size()) {
                    ssize_t n = send(client, reply.data() + sent, reply.size() - sent, MSG_NOSIGNAL);
                    if (n <= 0) break;
                    sent += n;
                }
                close(client);
            }
        });
    }
    void join() { if (worker.joinable()) worker.join(); }
    ~LocalServer() { if (fd >= 0) shutdown(fd, SHUT_RDWR); join(); if (fd >= 0) close(fd); }
};

std::string http(const std::string& body, int status = 200) {
    return "HTTP/1.1 " + std::to_string(status) + " Test\r\nContent-Type: text/event-stream\r\nContent-Length: " +
           std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}

ChatRequest requestFor(const LocalServer& server) {
    ChatRequest req;
    req.model.provider = {"fixture", "openai", server.url, ""};
    req.model.model = "local-model";
    req.system = "stable system";
    req.messages = {{"user", "hello", {}, ""}};
    return req;
}
}  // namespace

TEST(transport_Stream_And_Nonstream_Fallback) {
    std::string dir = makeTempDir("pocket-http");
    HomeGuard hg(dir);
    CHECK(ensureDir(stateDir(), 0700).ok);
    std::string stream = "data: {\"choices\":[{\"delta\":{\"content\":\"hello\"}}]}\r\n\r\n"
                         "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"name\":\"read\",\"arguments\":\"{}\"}}]},\"finish_reason\":\"tool_calls\"}]}\n\n"
                         "data: [DONE]";
    LocalServer server({http(stream), http(R"({"choices":[{"message":{"content":"plain"}}]})")});
    CHECK(!server.url.empty());
    std::string visible;
    ChatCallbacks cb;
    cb.onToken = [&](std::string_view token) { visible += token; };
    auto req = requestFor(server);
    auto r = chatRequest(req, cb);
    if (!r.ok) return r.error;
    CHECK(r.value.text == "hello" && visible == "hello" && r.value.calls.size() == 1);
    CHECK(!r.value.calls[0].id.empty());
    visible.clear();
    r = chatRequest(req, cb);
    CHECK(r.ok && r.value.text == "plain" && visible == "plain");
    server.join();
    CHECK(server.requests[0].find("stable system") != std::string::npos);
    CHECK(server.requests[0].find("Authorization:") == std::string::npos);
    rmRf(dir);
    return "";
}

TEST(transport_Partial_And_Error_Streams_Fail_Closed) {
    std::string dir = makeTempDir("pocket-http-errors");
    HomeGuard hg(dir);
    CHECK(ensureDir(stateDir(), 0700).ok);
    LocalServer server({
        http("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"name\":\"write\",\"arguments\":\"{}\"}}]}}]}\n\n"),
        http("data: {\"error\":{\"message\":\"overloaded\"}}\n\n"),
        http("data: {\"choices\":[{\"delta\":{\"content\":\"partial\"},\"finish_reason\":\"length\"}]}\n\n"),
        http("")});
    CHECK(!server.url.empty());
    auto req = requestFor(server);
    for (int i = 0; i < 4; ++i) {
        auto r = chatRequest(req, {});
        CHECK(!r.ok);
        if (i == 1) CHECK(r.error.find("overloaded") != std::string::npos);
    }
    server.join();
    CHECK_EQ(server.requests.size(), (size_t)4);  // no hidden retries of malformed 200s
    rmRf(dir);
    return "";
}

TEST(transport_Retry_And_Metadata_Use_Correct_Auth_Config) {
    std::string dir = makeTempDir("pocket-http-retry");
    HomeGuard hg(dir);
    EnvGuard key("POCKET_FIXTURE_KEY", "fixture-key");
    CHECK(ensureDir(stateDir(), 0700).ok);
    LocalServer server({http("rate limited", 429), http(R"({"choices":[{"message":{"content":"ok"}}]})"),
                        http(R"({"data":[{"id":"local-model","max_context_length":8192},{"id":"other","context_length":4096}]})")});
    CHECK(!server.url.empty());
    auto req = requestFor(server);
    req.model.provider.keyEnv = "POCKET_FIXTURE_KEY";
    int notices = 0;
    ChatCallbacks cb;
    cb.onNotice = [&](const std::string&) { ++notices; };
    auto r = chatRequest(req, cb);
    CHECK(r.ok && notices == 1);
    CHECK_EQ(fetchModelContext(req.model.provider, "local-model"), 8192L);
    CHECK_EQ(fetchModelContext(req.model.provider, "local-model"), 8192L);
    CHECK_EQ(fetchModelContext(req.model.provider, "other"), 4096L);
    server.join();
    CHECK_EQ(server.requests.size(), (size_t)3);
    CHECK(server.requests[2].find("GET /v1/models ") == 0);
    for (const auto& request : server.requests) CHECK(request.find("Authorization: Bearer fixture-key") != std::string::npos);
    rmRf(dir);
    return "";
}

TEST(transport_NonUtf8_Messages_Are_Repaired_For_Both_Protocols) {
    std::string dir = makeTempDir("pocket-http-unicode");
    HomeGuard hg(dir);
    CHECK(ensureDir(stateDir(), 0700).ok);
    LocalServer server({http(R"({"choices":[{"message":{"content":"ok"}}]})"),
                        http(R"({"content":[{"type":"text","text":"ok"}],"stop_reason":"end_turn"})")});
    CHECK(!server.url.empty());
    auto req = requestFor(server);
    req.messages[0].content = "İ Ç 😀 VER\xddLEN \xf0\x9f\x98";
    CHECK(chatRequest(req, {}).ok);
    req.model.provider.protocol = "anthropic";
    CHECK(chatRequest(req, {}).ok);
    server.join();
    CHECK_EQ(server.requests.size(), (size_t)2);
    for (const auto& request : server.requests) {
        CHECK(request.find("İ Ç 😀 VER\\ufffdLEN \\ufffd\\ufffd\\ufffd") != std::string::npos);
        CHECK(request.find('\xdd') == std::string::npos);
    }
    rmRf(dir);
    return "";
}
