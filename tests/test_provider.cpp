// PocketHarness tests - provider bodies, SSE, streaming, cache usage.
#include "mini.h"

#include "../src/provider.h"

using namespace pocket;
using namespace pocket::test;

namespace {
ResolvedModel mkModel(const std::string& pname = "orcarouter") {
    ResolvedModel m;
    m.provider = ProviderCfg{pname, "openai", "https://api.orcarouter.ai/v1", "ORCAROUTER_API_KEY"};
    if (pname == "openrouter") m.provider.baseUrl = "https://openrouter.ai/api/v1";
    m.model = "glm-5.3-flash";
    m.context = 200000;
    m.spec = pname + ":glm-5.3-flash";
    return m;
}
}  // namespace

TEST(provider_OpenAi_Body) {
    ChatRequest req;
    req.model = mkModel();
    req.system = "sys";
    req.messages = {{"user", "hi", {}, ""}};
    req.tools = {{"read", "reads", R"({"type":"object"})"}};
    req.thinking = "off";
    json::Value b = buildOpenAiBody(req);
    CHECK_EQ(b.at("model").asStr(), std::string("glm-5.3-flash"));
    CHECK_EQ(b.at("messages").size(), (size_t)1);  // system prepended at send time
    CHECK(!b.has("system") && !b.has("reasoning_effort") && !b.has("session_id"));
    CHECK(b.at("stream").asBool(false));
    CHECK(b.at("stream_options").at("include_usage").asBool(false));
    CHECK_EQ(b.at("tools").at(0).at("function").at("name").asStr(), std::string("read"));
    // Assistant tool-call round-trip shape.
    req.messages.push_back({"assistant", "", {{"c1", "read", "{\"path\":\"x\"}"}}, ""});
    req.messages.push_back({"tool", "content", {}, "c1"});
    b = buildOpenAiBody(req);
    CHECK_EQ(b.at("messages").at(1).at("tool_calls").at(0).at("id").asStr(), std::string("c1"));
    CHECK_EQ(b.at("messages").at(2).at("tool_call_id").asStr(), std::string("c1"));
    // Thinking knob only when explicitly set.
    req.thinking = "high";
    CHECK_EQ(buildOpenAiBody(req).at("reasoning_effort").asStr(), std::string("high"));
    req.thinking = "max";
    CHECK_EQ(buildOpenAiBody(req).at("reasoning_effort").asStr(), std::string("xhigh"));
    return "";
}

TEST(provider_OpenRouter_Session_And_Routing) {
    ChatRequest req;
    req.model = mkModel("openrouter");
    req.model.routing = "deepinfra";
    req.sessionTag = "sess-abc";
    json::Value b = buildOpenAiBody(req);
    CHECK_EQ(b.at("session_id").asStr(), std::string("sess-abc"));
    CHECK_EQ(b.at("provider").at("order").at(0).asStr(), std::string("deepinfra"));
    // Other providers: no session_id leakage (strict servers must not see it).
    req.model = mkModel("deepseek");
    b = buildOpenAiBody(req);
    CHECK(!b.has("session_id") && !b.has("provider"));
    return "";
}

TEST(provider_Anthropic_Body) {
    ChatRequest req;
    req.model = mkModel();
    req.model.provider.protocol = "anthropic";
    req.system = "sys";
    req.messages = {{"user", "hi", {}, ""},
                    {"assistant", "", {{"t1", "read", "{\"path\":\"x\"}"}}, ""},
                    {"tool", "content", {}, "t1"}};
    req.tools = {{"read", "reads", R"({"type":"object"})"}};
    req.thinking = "low";
    json::Value b = buildAnthropicBody(req);
    CHECK_EQ(b.at("system").asStr(), std::string("sys"));
    CHECK_EQ(b.at("messages").at(1).at("content").at(0).at("type").asStr(),
             std::string("tool_use"));
    CHECK_EQ(b.at("messages").at(2).at("content").at(0).at("type").asStr(),
             std::string("tool_result"));
    CHECK(b.at("thinking").at("budget_tokens").asInt(0) > 0);
    return "";
}

TEST(provider_Sse_Split) {
    std::string carry;
    auto p = sseSplit("event: message\ndata: {\"a\":1}\n: keepalive\n\ndata: [DONE]\n", carry);
    CHECK_EQ(p.size(), (size_t)2);
    CHECK_EQ(p[0], std::string("{\"a\":1}"));
    p = sseSplit("data: {\"b\":", carry);
    CHECK(p.empty());  // partial line held in carry
    p = sseSplit("2}\r\n\r\n", carry);
    CHECK_EQ(p.size(), (size_t)1);
    CHECK_EQ(p[0], std::string("{\"b\":2}"));
    return "";
}

TEST(provider_OpenAi_Stream_Tool_Assembly) {
    OpenAiStreamAcc acc;
    const char* chunks[] = {
        R"({"choices":[{"delta":{"content":"Hi"}}]})",
        R"({"choices":[{"delta":{"tool_calls":[{"index":0,"id":"c1","function":{"name":"rea"}}]}}]})",
        R"({"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"{\"pa"}}]}}]})",
        R"({"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"th\":\"x\"}"}}]}}]})",
        R"({"choices":[],"usage":{"prompt_tokens":100,"completion_tokens":20,"prompt_cache_hit_tokens":80,"prompt_cache_miss_tokens":20,"cost":0.001}})",
        nullptr,
    };
    for (const char** c = chunks; *c; ++c) {
        auto v = json::parse(*c);
        CHECK(v.ok);
        acc.feed(v.value);
    }
    ChatResponse r = acc.finish();
    CHECK_EQ(r.text, std::string("Hi"));
    CHECK_EQ(r.calls.size(), (size_t)1);
    CHECK_EQ(r.calls[0].name, std::string("rea"));
    CHECK_EQ(r.calls[0].argsJson, std::string("{\"path\":\"x\"}"));
    CHECK_EQ(r.inTokens, 100L);
    CHECK_EQ(r.cacheHit, 80L);
    CHECK_EQ(r.cacheMiss, 20L);
    CHECK(r.cost > 0);
    return "";
}

TEST(provider_Anthropic_Stream) {
    AnthropicStreamAcc acc;
    const char* chunks[] = {
        R"({"type":"message_start","message":{"usage":{"input_tokens":50}}})",
        R"({"type":"content_block_start","index":0,"content_block":{"type":"tool_use","id":"t1","name":"bash"}})",
        R"({"type":"content_block_delta","index":0,"delta":{"type":"input_json_delta","partial_json":"{\"com"}})",
        R"({"type":"content_block_delta","index":0,"delta":{"type":"input_json_delta","partial_json":"mand\":\"x\"}"}})",
        R"({"type":"message_delta","usage":{"output_tokens":12,"cache_read_input_tokens":40}})",
        nullptr,
    };
    for (const char** c = chunks; *c; ++c) {
        auto v = json::parse(*c);
        CHECK(v.ok);
        acc.feed(v.value);
    }
    ChatResponse r = acc.finish();
    CHECK_EQ(r.calls.size(), (size_t)1);
    CHECK_EQ(r.calls[0].argsJson, std::string("{\"command\":\"x\"}"));
    CHECK_EQ(r.inTokens, 50L);
    CHECK_EQ(r.cacheHit, 40L);
    return "";
}

TEST(provider_Response_Parsers) {
    auto v = json::parse(
        R"({"choices":[{"message":{"content":"done","tool_calls":[{"id":"c9","function":{"name":"bash","arguments":"{}"}}]}}],"usage":{"prompt_tokens":10,"completion_tokens":3}})");
    CHECK(v.ok);
    auto r = parseOpenAiResponse(v.value);
    CHECK(r.ok && r.value.calls.size() == 1 && r.value.calls[0].id == "c9");
    v = json::parse(R"({"error":{"message":"bad key"}})");
    CHECK(!parseOpenAiResponse(v.value).ok);
    v = json::parse(
        R"({"content":[{"type":"text","text":"A"},{"type":"tool_use","id":"t","name":"read","input":{"path":"p"}}],"usage":{"input_tokens":5,"output_tokens":2}})");
    auto a = parseAnthropicResponse(v.value);
    CHECK(a.ok && a.value.text == "A" && a.value.calls.size() == 1);
    return "";
}

TEST(provider_ApiKey_Lookup) {
    ProviderCfg p{"t", "openai", "https://x", "POCKETTEST_PROV_KEY"};
    EnvGuard g("POCKETTEST_PROV_KEY", "env-key");
    auto k = providerApiKey(p);
    CHECK(k.ok && k.value == "env-key");
    return "";
}

TEST(provider_Models_Gemini_Shape) {
    std::string body =
        R"({"models":[{"name":"models/gemini-flash","inputTokenLimit":1000000},)"
        R"({"name":"models/other","inputTokenLimit":32000}]})";
    CHECK_EQ(parseModelsContext(body, "gemini-flash"), 1000000L);
    CHECK_EQ(parseModelsContext(body, "missing"), -1L);
    CHECK_EQ(parseModelsContext(R"({"data":[{"id":"m","context_length":64000}]})", "m"), 64000L);
    CHECK_EQ(parseModelsContext(R"({"data":[{"id":"m"}]})", "m"), -1L);  // unpublished
    return "";
}

TEST(provider_Retry_Policy) {
    CHECK_EQ(kChatMaxAttempts, 4);
    // Transport failures retry while the answer has not started.
    CHECK(shouldRetryRequest(-1, true, false, false));
    CHECK(shouldRetryRequest(0, true, false, false));
    // Retryable HTTP statuses (rate limit / gateway trouble).
    for (int c : {429, 500, 502, 503, 504}) CHECK(shouldRetryRequest(c, false, false, false));
    // Client errors never retry: the payload is at fault, not the network.
    for (int c : {200, 400, 401, 403, 404, 422}) CHECK(!shouldRetryRequest(c, false, false, false));
    // No retry once tokens reached the UI (would duplicate output)...
    CHECK(!shouldRetryRequest(503, false, false, true));
    CHECK(!shouldRetryRequest(-1, true, false, true));
    // ...or when curl hung past our own max-time (repeating a 10-minute
    // hang is not a cooldown, it is a stall).
    CHECK(!shouldRetryRequest(-1, true, true, false));
    CHECK_EQ(retryDelayMs(1), 1000L);
    CHECK_EQ(retryDelayMs(2), 2000L);
    CHECK_EQ(retryDelayMs(3), 4000L);
    CHECK(retryDelayMs(99) <= 30000L);  // capped
    return "";
}

TEST(provider_OpenAi_Image_Body) {
    ChatRequest req;
    req.model = mkModel();
    ChatMessage m{"user", "what is this?", {}, ""};
    m.images = {{"image/png", "aGVsbG8="}};
    req.messages = {m};
    json::Value b = buildOpenAiBody(req);
    const json::Value& c = b.at("messages").at(0).at("content");
    CHECK(c.isArr() && c.size() == (size_t)2);
    CHECK_EQ(c.at(0).at("type").asStr(), std::string("text"));
    CHECK_EQ(c.at(0).at("text").asStr(), std::string("what is this?"));
    CHECK_EQ(c.at(1).at("type").asStr(), std::string("image_url"));
    CHECK_EQ(c.at(1).at("image_url").at("url").asStr(),
             std::string("data:image/png;base64,aGVsbG8="));
    // Plain messages keep the string form (no gratuitous reshaping).
    req.messages = {{"user", "hi", {}, ""}};
    CHECK(buildOpenAiBody(req).at("messages").at(0).at("content").isStr());
    return "";
}

TEST(provider_Anthropic_Image_Body) {
    ChatRequest req;
    req.model = mkModel();
    req.model.provider.protocol = "anthropic";
    ChatMessage m{"user", "describe", {}, ""};
    m.images = {{"image/jpeg", "QUJD"}};
    req.messages = {m};
    json::Value b = buildAnthropicBody(req);
    const json::Value& c = b.at("messages").at(0).at("content");
    CHECK(c.isArr() && c.size() == (size_t)2);
    CHECK_EQ(c.at(1).at("type").asStr(), std::string("image"));
    CHECK_EQ(c.at(1).at("source").at("media_type").asStr(), std::string("image/jpeg"));
    CHECK_EQ(c.at(1).at("source").at("data").asStr(), std::string("QUJD"));
    return "";
}

TEST(provider_ApiKey_Loopback_Keyless) {
    // Loopback daemons run fine without auth: empty key, no error.
    ProviderCfg local{"lm", "openai", "http://127.0.0.1:1234/v1", "POCKETTEST_MISSING_KEY"};
    unsetenv("POCKETTEST_MISSING_KEY");
    auto k = providerApiKey(local);
    CHECK(k.ok && k.value.empty());
    // Remote endpoints still demand a key.
    ProviderCfg remote{"r", "openai", "https://api.example.com/v1", "POCKETTEST_MISSING_KEY"};
    CHECK(!providerApiKey(remote).ok);
    // ...unless one is actually configured.
    EnvGuard g("POCKETTEST_MISSING_KEY", "k");
    auto k2 = providerApiKey(remote);
    CHECK(k2.ok && k2.value == "k");
    return "";
}

TEST(provider_Stream_Reasoning_Captured) {
    OpenAiStreamAcc a;
    auto v = json::parse(R"({"choices":[{"delta":{"reasoning_content":"hmm"}}]})");
    CHECK(v.ok);
    a.feed(v.value);
    CHECK_EQ(a.reasoning, std::string("hmm"));
    CHECK(a.text.empty());
    AnthropicStreamAcc b;
    auto w = json::parse(
        R"({"type":"content_block_delta","index":0,)"
        R"("delta":{"type":"thinking_delta","thinking":"deep"}})");
    CHECK(w.ok);
    b.feed(w.value);
    CHECK_EQ(b.reasoning, std::string("deep"));
    CHECK(b.text.empty());
    return "";
}
