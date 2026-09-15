// PocketHarness - provider client: wire protocols, not brands.
// HTTPS is delegated to the system `curl` binary (argv-based, streamed).
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "common.h"
#include "config.h"
#include "json.h"

namespace pocket {

struct ToolCall {
    std::string id;
    std::string name;
    std::string argsJson;  // raw JSON object string (may be "")
};

struct ToolDef {
    std::string name;
    std::string description;
    std::string paramsJson;  // JSON Schema object (serialized)
};

struct ChatMessage {
    std::string role;  // user|assistant|tool
    std::string content;
    std::vector<ToolCall> toolCalls;  // assistant messages only
    std::string toolCallId;           // tool messages only
};

struct ChatRequest {
    ResolvedModel model;
    std::string system;
    std::vector<ChatMessage> messages;
    std::vector<ToolDef> tools;
    std::string thinking = "off";  // off|low|medium|high|max
    bool stream = true;
    long maxTokens = 8192;
    // Stable per-PocketHarness-session tag. Sent as OpenRouter `session_id`
    // for sticky routing to the warm-cache endpoint; ignored elsewhere.
    std::string sessionTag;
};

struct ChatResponse {
    std::string text;
    std::string reasoning;  // model thinking, when the provider streams it
    std::vector<ToolCall> calls;
    long inTokens = -1;
    long outTokens = -1;
    // Cache accounting, exactly as the provider reported it (-1 = unreported).
    // Never inferred: a miss/expiry is simply what the provider says.
    long cacheHit = -1;   // cached/reused input tokens
    long cacheMiss = -1;  // uncached input tokens (DeepSeek prompt_cache_miss)
    double cost = -1;     // reported request cost, if any (OpenRouter)
};

struct ChatCallbacks {
    std::function<void(std::string_view token)> onToken;
    std::function<void(std::string_view chunk)> onReasoning;  // thinking preview
    std::atomic<bool>* cancel = nullptr;
};

// Request-body builders (pure, unit-tested).
json::Value buildOpenAiBody(const ChatRequest& req);
json::Value buildAnthropicBody(const ChatRequest& req);

// Incremental SSE payload splitter: feed raw bytes, get "data:" payloads.
// Lines not starting with "data:" are ignored (event:/comments/id:).
// Returns payloads; "[DONE]" arrives as a payload and means end-of-stream.
std::vector<std::string> sseSplit(std::string_view chunk, std::string& carry);

// Streaming accumulators (pure, unit-tested). Feed parsed data payloads.
struct OpenAiStreamAcc {
    std::string text;
    std::string reasoning;
    struct Pending {
        std::string id, name, args;
    };
    std::vector<Pending> pend;  // indexed by "index"
    long inTokens = -1, outTokens = -1;
    long cacheHit = -1, cacheMiss = -1;
    double cost = -1;
    void feed(const json::Value& payload);
    ChatResponse finish();
};
struct AnthropicStreamAcc {
    std::string text;
    std::string reasoning;
    struct Block {
        std::string id, name, input;
        bool isTool = false;
    };
    std::vector<Block> blocks;  // indexed by content_block "index"
    long inTokens = -1, outTokens = -1;
    long cacheHit = -1, cacheMiss = -1;
    double cost = -1;
    void feed(const json::Value& payload);
    ChatResponse finish();
};

// Non-streaming response parsers (pure, unit-tested).
Result<ChatResponse> parseOpenAiResponse(const json::Value& v);
Result<ChatResponse> parseAnthropicResponse(const json::Value& v);

// API key lookup: $keyEnv only (recursive children: explicit expose_env).
// The returned key must never be logged or placed in argv.
Result<std::string> providerApiKey(const ProviderCfg& prov);

// Full request over `curl`. All staging (body, headers, secrets) lives in
// a per-request parent-only dir under the state dir; nothing caller-visible.
Result<ChatResponse> chatRequest(const ChatRequest& req, const ChatCallbacks& cb);

// True when the system curl binary is runnable.
bool curlAvailable();

// Context window for a model id from a /models listing body (exact id
// match; reads context_length|context_window|max_context|context|
// inputTokenLimit). Handles {"data":[...]}, Gemini {"models":[...]} and
// bare [...] shapes. -1 when unpublished.
long parseModelsContext(const std::string& body, const std::string& modelId);

// Live context window via GET {base}/models, cached per process. Secret
// staging is internal (parent-only dir); needs no caller tmp dir.
// -1 on any failure (offline, auth, unpublished id): callers keep static.
long fetchModelContext(const ProviderCfg& prov, const std::string& modelId);

}  // namespace pocket
