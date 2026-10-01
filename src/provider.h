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
    // Set when the model sent unusable arguments: the call is answered with
    // this error instead of running, so one malformed call never ends a turn.
    std::string invalid{};
};

struct ToolDef {
    std::string name;
    std::string description;
    std::string paramsJson;  // JSON Schema object (serialized)
};

// One vision payload: raw bytes are base64'd once at attach time and the
// data URL is rebuilt byte-identically on every request (cache-stable).
struct ChatImage {
    std::string mime;  // image/png|image/jpeg|image/gif|image/webp
    std::string b64;   // base64-encoded file bytes
};

struct ChatMessage {
    std::string role;  // user|assistant|tool
    std::string content;
    std::vector<ToolCall> toolCalls;  // assistant messages only
    std::string toolCallId;           // tool messages only
    std::vector<ChatImage> images{};  // user messages only (empty by default)
    json::Value replay{};  // signed/opaque reasoning, scoped to the originating model
};

struct ChatRequest {
    ResolvedModel model;
    std::string system;
    std::vector<ChatMessage> messages;
    std::vector<ToolDef> tools;
    std::string thinking = "off";  // auto|off|none|minimal|low|medium|high|xhigh|max
    bool stream = true;
    long maxTokens = 8192;
    // Stable per-PocketHarness-session tag. Sent as OpenRouter `session_id`
    // for sticky routing to the warm-cache endpoint; ignored elsewhere.
    std::string sessionTag;
    // Absolute end-to-end deadline for this logical request (monotonic nowMs;
    // 0 = none). Each transport attempt's timeout is clamped to what remains,
    // and no retry is started whose backoff would cross it.
    int64_t deadlineAtMs = 0;
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
    std::string servedModel;  // actual model id the provider reports ("" = unreported)
    json::Value replay{};
    std::string error, stopReason;
};

struct ChatCallbacks {
    std::function<void(std::string_view token)> onToken;
    std::function<void(std::string_view chunk)> onReasoning;  // thinking preview
    std::function<void(const std::string& note)> onNotice;    // retry cooldowns
    std::atomic<bool>* cancel = nullptr;
    // Exactly once per HTTP attempt, before validation/retry, including failed
    // or cancelled attempts. Only usage fields are populated; -1 is unknown.
    // This includes billed responses rejected for length/invalid tool calls.
    std::function<void(const ChatResponse& usage)> onUsage;
};

// Retry policy (pure, unit-tested). A failed request is retried only while
// the answer has not started (emitted=false): once tokens reached the UI,
// retrying would duplicate visible output. Retried: curl transport failures
// and HTTP 429/5xx. Never retried: HTTP 4xx (the payload is at fault),
// our own timeout, cancellation, and empty/unparseable 200s.
inline constexpr int kChatMaxAttempts = 4;  // 1 initial + 3 retries
bool shouldRetryRequest(int httpCode, bool curlFailed, bool timedOut, bool emitted);
// Gateway/upstream failure wording (OpenRouter "Provider returned error",
// "overloaded", "upstream ...") that is worth another attempt even when the
// HTTP status is 200 (in-stream error) or a gateway-specific code.
bool isTransientProviderMessage(const std::string& msg);
// Sleeps in short slices; false when cancel fired first.
bool sleepCancellable(long ms, std::atomic<bool>* cancel);
// Cooldown before attempt N (N>=1): 1s, 2s, 4s, ... capped at 30s.
long retryDelayMs(int attempt);
long retryAfterMs(const std::string& headers);  // numeric Retry-After, bounded to 60s

// Request-body builders (pure, unit-tested).
json::Value buildOpenAiBody(const ChatRequest& req);
json::Value buildAnthropicBody(const ChatRequest& req);

// Best-effort fix of model-sent tool arguments: code fences, a double-encoded
// string, raw control characters inside strings, trailing junk. Returns a
// JSON object text, or "" (with why) when the call cannot be trusted.
std::string repairToolArgs(const std::string& raw, std::string* why = nullptr);

// Incremental SSE event splitter: joins multiline data at blank-line boundaries.
// Lines not starting with "data:" are ignored (event:/comments/id:).
// Returns payloads; "[DONE]" arrives as a payload and means end-of-stream.
std::vector<std::string> sseSplit(std::string_view chunk, std::string& carry);

// Streaming accumulators (pure, unit-tested). Feed parsed data payloads.
struct OpenAiStreamAcc {
    std::string text;
    std::string reasoning;
    std::string error, stopReason;
    bool done = false;
    json::Array details;
    struct Pending {
        std::string id, name, args;
    };
    std::vector<Pending> pend;  // arrival order
    std::vector<size_t> slot;   // stream "index" -> pend position
    long inTokens = -1, outTokens = -1;
    long cacheHit = -1, cacheMiss = -1;
    double cost = -1;
    std::string model;  // first reported chunk model id ("" = unreported)
    void feed(const json::Value& payload);
    ChatResponse finish();
};
struct AnthropicStreamAcc {
    std::string text;
    std::string reasoning;
    std::string error, stopReason;
    bool done = false;
    json::Value usage{json::obj()};
    struct Block {
        std::string id, name, input;
        bool isTool = false;
        json::Value value{};
    };
    std::vector<Block> blocks;  // indexed by content_block "index"
    long inTokens = -1, outTokens = -1;
    long cacheHit = -1, cacheMiss = -1;
    double cost = -1;
    std::string model;  // message_start model id ("" = unreported)
    void feed(const json::Value& payload);
    ChatResponse finish();
};

// OpenAI Responses stream (ChatGPT Codex backend). Reasoning items are kept
// encrypted for replay to the same model, like Anthropic signed thinking.
struct CodexStreamAcc {
    std::string text, reasoning, error, stopReason;
    bool done = false;
    std::vector<ToolCall> calls;
    json::Array items;  // reasoning items for replay
    long inTokens = -1, outTokens = -1, cacheHit = -1, cacheMiss = -1;
    double cost = -1;
    std::string model;  // completed-response model id ("" = unreported)
    void feed(const json::Value& payload);
    ChatResponse finish();
};
json::Value buildCodexBody(const ChatRequest& req);
// Codex credentials from the Codex CLI or Pi login (read-only; the refresh
// token is never rotated here). Err explains how to log in again.
struct CodexAuth {
    std::string token, accountId;
};
Result<CodexAuth> codexAuth();

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

// Bounded GET (body "") or JSON POST through the confined provider curl.
// secretHeader (e.g. "Authorization: Bearer k") is staged in a 0600 -K
// config, never argv. Returns the 2xx body, else "HTTP n: ..." / transport.
Result<std::string> httpRequest(const std::string& url, const std::string& secretHeader,
                                const std::string& body, long timeoutMs,
                                const std::vector<std::string>& headers = {},
                                std::atomic<bool>* cancel = nullptr);
std::string providerAuthHeader(const ProviderCfg& prov, const std::string& key);
// Raw GET {base}/models body, cached per endpoint/auth ("" on failure).
// Successes expire after 10 minutes; transient failures after 5 seconds.
// `cancel` aborts the in-flight probe so a blocked UI stays interruptible.
std::string fetchModelsBody(const ProviderCfg& prov, long timeoutMs,
                            std::atomic<bool>* cancel = nullptr);
// Apply one learned quirk (see brain.h) to model options.
void applyQuirk(ModelOptions& o, const std::string& quirk);

// Live context window via GET {base}/models, cached per process. Secret
// staging is internal (parent-only dir); needs no caller tmp dir.
// -1 on any failure (offline, auth, unpublished id): callers keep static.
long fetchModelContext(const ProviderCfg& prov, const std::string& modelId,
                       std::atomic<bool>* cancel = nullptr);

}  // namespace pocket
