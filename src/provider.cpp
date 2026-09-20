// PocketHarness - provider implementation, part 1: bodies + stream parsing.
#include "provider.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <time.h>

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <optional>

#include "process.h"
#include "sandbox.h"

namespace pocket {

namespace {

json::Value toolCallToOpenAi(const ToolCall& tc) {
    json::Object fn;
    fn["name"] = json::Value(tc.name);
    fn["arguments"] = json::Value(tc.argsJson.empty() ? "{}" : tc.argsJson);
    json::Object o;
    o["id"] = json::Value(tc.id);
    o["type"] = json::Value("function");
    o["function"] = json::Value(fn);
    return json::Value(o);
}

bool isOpenRouter(const ResolvedModel& m) {
    return m.provider.name == "openrouter";
}

// Shared usage-block reader (streaming deltas and full responses alike).
// Reads exactly what the provider reported; absent fields stay -1.
void readUsage(const json::Value& u, long& in, long& out, long& hit, long& miss, double& cost) {
    if (!u.isObj()) return;
    auto count = [](const json::Value& v, long fallback) {
        double n = v.asNum(-1);
        return n >= 0 && n <= 1000000000 && n == std::floor(n) ? (long)n : fallback;
    };
    if (u.has("prompt_tokens")) in = count(u.at("prompt_tokens"), in);
    if (u.has("completion_tokens")) out = count(u.at("completion_tokens"), out);
    if (u.has("prompt_cache_hit_tokens")) hit = count(u.at("prompt_cache_hit_tokens"), hit);
    if (u.has("prompt_cache_miss_tokens")) miss = count(u.at("prompt_cache_miss_tokens"), miss);
    const auto& det = u.at("prompt_tokens_details");
    if (det.isObj() && det.has("cached_tokens"))
        hit = count(det.at("cached_tokens"), hit);
    if (u.has("input_tokens")) {
        long fresh = count(u.at("input_tokens"), -1);
        long created = count(u.at("cache_creation_input_tokens"), u.has("cache_creation_input_tokens") ? -1 : 0);
        long cached = count(u.at("cache_read_input_tokens"), u.has("cache_read_input_tokens") ? -1 : 0);
        if (fresh >= 0 && created >= 0 && cached >= 0) in = fresh + created + cached;
        if (u.has("cache_read_input_tokens")) hit = cached;
    }
    if (u.has("output_tokens")) out = count(u.at("output_tokens"), out);
    if (hit >= 0 && in >= hit && !u.has("prompt_cache_miss_tokens")) miss = in - hit;
    if (u.has("cost") && u.at("cost").isNum()) cost = u.at("cost").asNum(cost);
}

bool replayMatches(const ChatMessage& m, const ResolvedModel& model) {
    return m.replay.at("model").asStr() == model.provider.name + ":" + model.model;
}

long streamIndex(const json::Value& v, long fallback = 0) {
    if (v.isNull()) return fallback;
    double n = v.asNum(-1);
    return n >= 0 && n < 64 && n == std::floor(n) ? (long)n : -1;
}

std::vector<std::string> providerEnv(const std::string& stage) {
    std::vector<std::string> env = {"PATH=/usr/bin:/bin", "HOME=" + stage};
    for (const char* name : {"HTTPS_PROXY", "https_proxy", "HTTP_PROXY", "http_proxy",
                             "ALL_PROXY", "all_proxy", "NO_PROXY", "no_proxy",
                             "SSL_CERT_FILE", "SSL_CERT_DIR", "CURL_CA_BUNDLE"})
        if (const char* v = getenv(name)) env.push_back(std::string(name) + "=" + v);
    return env;
}

}  // namespace

json::Value buildOpenAiBody(const ChatRequest& req) {
    json::Object b;
    b["model"] = json::Value(req.model.model);
    json::Array msgs;
    for (const auto& m : req.messages) {
        json::Object o;
        if (m.role == "tool") {
            o["role"] = json::Value("tool");
            o["tool_call_id"] = json::Value(m.toolCallId);
            o["content"] = json::Value(m.content);
        } else if (m.role == "assistant" && !m.toolCalls.empty()) {
            o["role"] = json::Value("assistant");
            o["content"] = json::Value(m.content);
            json::Array tcs;
            for (const auto& tc : m.toolCalls) tcs.push_back(toolCallToOpenAi(tc));
            o["tool_calls"] = json::Value(tcs);
        } else if (m.role == "user" && !m.images.empty()) {
            o["role"] = json::Value("user");
            json::Array parts;
            json::Object t;
            t["type"] = json::Value("text");
            t["text"] = json::Value(m.content);
            parts.push_back(json::Value(t));
            for (const auto& img : m.images) {
                json::Object u;
                u["url"] = json::Value("data:" + img.mime + ";base64," + img.b64);
                json::Object p;
                p["type"] = json::Value("image_url");
                p["image_url"] = json::Value(u);
                parts.push_back(json::Value(p));
            }
            o["content"] = json::Value(parts);
        } else {
            o["role"] = json::Value(m.role);
            o["content"] = json::Value(m.content);
        }
        if (m.role == "assistant" && replayMatches(m, req.model)) {
            for (const char* key : {"reasoning_content", "reasoning_details"})
                if (m.replay.has(key)) o[key] = m.replay.at(key);
        }
        msgs.push_back(json::Value(o));
    }
    b["messages"] = json::Value(msgs);
    if (!req.tools.empty()) {
        json::Array tools;
        for (const auto& t : req.tools) {
            json::Object fn;
            fn["name"] = json::Value(t.name);
            fn["description"] = json::Value(t.description);
            auto schema = json::parse(t.paramsJson);
            fn["parameters"] =
                json::Value(schema.ok ? schema.value : json::Value(json::obj()));
            json::Object o;
            o["type"] = json::Value("function");
            o["function"] = json::Value(fn);
            tools.push_back(json::Value(o));
        }
        b["tools"] = json::Value(tools);
    }
    b["stream"] = json::Value(req.stream);
    if (req.stream && req.model.options.streamUsage) {
        json::Object so;
        so["include_usage"] = json::Value(true);
        b["stream_options"] = json::Value(so);
    }
    b[req.model.options.tokenParameter] = json::Value(req.maxTokens);
    // Reasoning knob only when explicitly requested (compat with strict servers).
    if (req.model.options.reasoning != "none" && req.thinking != "off" && req.thinking != "auto") {
        std::string effort = req.thinking;
        if (isOpenRouter(req.model))
            b["reasoning"] = json::Object{{"effort", effort}};
        else b["reasoning_effort"] = effort;
    }
    if (req.model.provider.name == "deepseek" && req.model.options.reasoning != "none" && req.thinking != "auto")
        b["thinking"] = json::Object{{"type", req.thinking == "off" || req.thinking == "none" ? "disabled" : "enabled"}};
    if (req.model.provider.name == "openai" && req.model.options.promptCache && !req.sessionTag.empty())
        b["prompt_cache_key"] = req.sessionTag;
    // OpenRouter provider routing, e.g. model "hy4-preview@deepinfra".
    if (!req.model.routing.empty() && req.model.routing != "auto" && isOpenRouter(req.model)) {
        json::Array order;
        order.push_back(json::Value(req.model.routing));
        json::Object prov;
        prov["order"] = json::Value(order);
        prov["allow_fallbacks"] = json::Value(false);
        b["provider"] = json::Value(prov);
    }
    // OpenRouter sticky session: same ID every request in this Pocket session
    // so follow-ups route to the endpoint holding the warm prompt cache.
    if (!req.sessionTag.empty() && isOpenRouter(req.model))
        b["session_id"] = json::Value(req.sessionTag);
    return json::Value(b);
}

json::Value buildAnthropicBody(const ChatRequest& req) {
    json::Object b;
    b["model"] = json::Value(req.model.model);
    b["max_tokens"] = json::Value((double)req.maxTokens);
    if (!req.system.empty()) b["system"] = json::Value(req.system);
    if (req.model.options.promptCache) b["cache_control"] = json::Object{{"type", "ephemeral"}};
    json::Array msgs;
    for (const auto& m : req.messages) {
        json::Object o;
        if (m.role == "tool") {
            o["role"] = json::Value("user");
            json::Array blocks;
            json::Object tr;
            tr["type"] = json::Value("tool_result");
            tr["tool_use_id"] = json::Value(m.toolCallId);
            tr["content"] = json::Value(m.content);
            blocks.push_back(json::Value(tr));
            o["content"] = json::Value(blocks);
        } else if (m.role == "assistant") {
            o["role"] = json::Value("assistant");
            json::Array blocks;
            if (replayMatches(m, req.model))
                for (const auto& block : m.replay.at("thinking").asArr()) blocks.push_back(block);
            if (!m.content.empty()) {
                json::Object t;
                t["type"] = json::Value("text");
                t["text"] = json::Value(m.content);
                blocks.push_back(json::Value(t));
            }
            for (const auto& tc : m.toolCalls) {
                json::Object tu;
                tu["type"] = json::Value("tool_use");
                tu["id"] = json::Value(tc.id);
                tu["name"] = json::Value(tc.name);
                auto args = json::parse(tc.argsJson.empty() ? "{}" : tc.argsJson);
                tu["input"] = args.ok ? args.value : json::Value(json::obj());
                blocks.push_back(json::Value(tu));
            }
            o["content"] = json::Value(blocks);
        } else if (m.role == "user" && !m.images.empty()) {
            o["role"] = json::Value("user");
            json::Array blocks;
            json::Object t;
            t["type"] = json::Value("text");
            t["text"] = json::Value(m.content);
            blocks.push_back(json::Value(t));
            for (const auto& img : m.images) {
                json::Object src;
                src["type"] = json::Value("base64");
                src["media_type"] = json::Value(img.mime);
                src["data"] = json::Value(img.b64);
                json::Object b;
                b["type"] = json::Value("image");
                b["source"] = json::Value(src);
                blocks.push_back(json::Value(b));
            }
            o["content"] = json::Value(blocks);
        } else {
            o["role"] = json::Value(m.role == "system" ? "user" : m.role);
            o["content"] = json::Value(m.content);
        }
        // All results from one tool batch belong in the immediately following
        // user message, not a series of separate user messages.
        if (m.role == "tool" && !msgs.empty() && msgs.back().at("role").asStr() == "user" &&
            msgs.back().at("content").isArr() &&
            msgs.back().at("content").at(0).at("type").asStr() == "tool_result") {
            auto& dst = msgs.back().asObj()["content"].asArr();
            for (const auto& block : o["content"].asArr()) dst.push_back(block);
        } else msgs.push_back(json::Value(o));
    }
    b["messages"] = json::Value(msgs);
    if (!req.tools.empty()) {
        json::Array tools;
        for (const auto& t : req.tools) {
            json::Object o;
            o["name"] = json::Value(t.name);
            o["description"] = json::Value(t.description);
            auto schema = json::parse(t.paramsJson);
            o["input_schema"] =
                schema.ok ? schema.value : json::Value(json::obj());
            tools.push_back(json::Value(o));
        }
        b["tools"] = json::Value(tools);
    }
    b["stream"] = json::Value(req.stream);
    if (req.model.options.reasoning != "none" && req.thinking != "off" &&
        req.thinking != "none" && req.thinking != "auto") {
        if (req.model.options.reasoning == "adaptive") {
            b["thinking"] = json::Object{{"type", "adaptive"}};
            std::string effort = req.thinking == "minimal" ? "low" : req.thinking;
            if (effort == "xhigh") effort = "high";
            b["output_config"] = json::Object{{"effort", effort}};
        } else if (req.maxTokens > 1024) {
            long budget = req.thinking == "minimal" ? 1024 : req.thinking == "low" ? 2048 :
                          req.thinking == "medium" ? 8000 : req.thinking == "high" ? 16000 : 32000;
            budget = std::min(budget, std::max(1024L, req.maxTokens * 3 / 4));
            b["thinking"] = json::Object{{"type", "enabled"}, {"budget_tokens", budget}};
        }
    }
    return json::Value(b);
}

std::vector<std::string> sseSplit(std::string_view chunk, std::string& carry) {
    carry.append(chunk);
    std::vector<std::string> out;
    size_t consumed = 0, lineStart = 0;
    std::string data;
    while (true) {
        size_t end = carry.find('\n', lineStart);
        if (end == std::string::npos) break;
        std::string_view line(carry.data() + lineStart, end - lineStart);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) {
            if (!data.empty()) { data.pop_back(); out.push_back(std::move(data)); data.clear(); }
            consumed = end + 1;
        } else if (startsWith(line, "data:")) {
            line.remove_prefix(5);
            if (!line.empty() && line.front() == ' ') line.remove_prefix(1);
            data.append(line);
            data += '\n';
        }
        lineStart = end + 1;
    }
    carry.erase(0, consumed);  // retain the entire unfinished event, including multiline data
    return out;
}

void OpenAiStreamAcc::feed(const json::Value& p) {
    if (p.has("error")) {
        error = p.at("error").at("message").asStr();
        if (error.empty()) error = "provider stream error";
        return;
    }
    const auto& choices = p.at("choices");
    if (choices.isArr() && choices.size() > 0) {
        if (choices.at(0).at("finish_reason").isStr()) {
            stopReason = choices.at(0).at("finish_reason").asStr();
            done = true;
        }
        const auto& delta = choices.at(0).at("delta");
        if (delta.has("content") && delta.at("content").isStr())
            text += delta.at("content").asStr();
        // Thinking preview shapes (DeepSeek reasoning_content, OpenRouter
        // reasoning / reasoning_details). Replay intact for tool continuation.
        if (delta.has("reasoning_content") && delta.at("reasoning_content").isStr())
            reasoning += delta.at("reasoning_content").asStr();
        if (delta.has("reasoning")) {
            const auto& r = delta.at("reasoning");
            if (r.isStr()) reasoning += r.asStr();
        }
        const auto& rds = delta.at("reasoning_details");
        if (rds.isArr()) {
            for (const auto& rd : rds.asArr()) {
                if (rd.has("text") && rd.at("text").isStr()) reasoning += rd.at("text").asStr();
                long idx = streamIndex(rd.at("index"));
                if (idx < 0 || idx >= 64) { error = "invalid reasoning index"; return; }
                if ((size_t)idx >= details.size()) details.resize(idx + 1, json::Object{});
                for (const auto& [key, val] : rd.asObj()) {
                    auto& dst = details[idx].asObj()[key];
                    if (key == "text" || key == "data" || key == "signature")
                        dst = dst.asStr() + val.asStr();
                    else dst = val;
                }
            }
        }
        const auto& tcs = delta.at("tool_calls");
        if (tcs.isArr()) {
            for (const auto& tc : tcs.asArr()) {
                long idx = streamIndex(tc.at("index"));
                if (idx < 0 || idx >= 64) { error = "invalid tool call index"; return; }
                if ((size_t)idx >= pend.size()) pend.resize((size_t)idx + 1);
                Pending& pe = pend[(size_t)idx];
                if (tc.has("id") && tc.at("id").isStr()) pe.id += tc.at("id").asStr();
                const auto& fn = tc.at("function");
                if (fn.has("name") && fn.at("name").isStr()) pe.name += fn.at("name").asStr();
                if (fn.has("arguments") && fn.at("arguments").isStr())
                    pe.args += fn.at("arguments").asStr();
            }
        }
    }
    readUsage(p.at("usage"), inTokens, outTokens, cacheHit, cacheMiss, cost);
}

ChatResponse OpenAiStreamAcc::finish() {
    ChatResponse r;
    r.text = text;
    r.reasoning = reasoning;
    r.inTokens = inTokens;
    r.outTokens = outTokens;
    r.cacheHit = cacheHit;
    r.cacheMiss = cacheMiss;
    r.cost = cost;
    r.error = error;
    r.stopReason = stopReason;
    r.replay = json::Object{};
    if (!details.empty()) r.replay.asObj()["reasoning_details"] = details;
    else if (!reasoning.empty()) r.replay.asObj()["reasoning_content"] = reasoning;
    for (auto& pe : pend) {
        if (pe.name.empty() && pe.id.empty()) continue;
        ToolCall tc;
        tc.id = pe.id;
        tc.name = pe.name;
        tc.argsJson = pe.args;
        r.calls.push_back(std::move(tc));
    }
    return r;
}

void AnthropicStreamAcc::feed(const json::Value& p) {
    std::string type = p.at("type").asStr();
    if (type == "error") {
        error = p.at("error").at("message").asStr();
        if (error.empty()) error = "provider stream error";
        return;
    }
    if (type == "message_stop") done = true;
    if (type == "content_block_start") {
        long idx = streamIndex(p.at("index"), -1);
        if (idx < 0 || idx >= 64) { error = "invalid content block index"; return; }
        if ((size_t)idx >= blocks.size()) blocks.resize((size_t)idx + 1);
        const auto& cb = p.at("content_block");
        blocks[idx].value = cb;
        if (cb.at("type").asStr() == "text") text += cb.at("text").asStr();
        if (cb.at("type").asStr() == "thinking") reasoning += cb.at("thinking").asStr();
        if (cb.at("type").asStr() == "tool_use") {
            blocks[(size_t)idx].isTool = true;
            blocks[(size_t)idx].id = cb.at("id").asStr();
            blocks[(size_t)idx].name = cb.at("name").asStr();
        }
    } else if (type == "content_block_delta") {
        long idx = streamIndex(p.at("index"), -1);
        if (idx < 0 || idx >= 64) { error = "invalid content block index"; return; }
        if ((size_t)idx >= blocks.size()) blocks.resize((size_t)idx + 1);
        const auto& d = p.at("delta");
        std::string dt = d.at("type").asStr();
        if (dt == "text_delta")
            text += d.at("text").asStr();
        else if (dt == "input_json_delta")
            blocks[(size_t)idx].input += d.at("partial_json").asStr();
        else if (dt == "thinking_delta") {
            if (d.has("thinking") && d.at("thinking").isStr())
                reasoning += d.at("thinking").asStr();
            auto& value = blocks[idx].value;
            if (value.isNull()) value = json::Object{{"type", "thinking"}};
            value.asObj()["thinking"] = value.at("thinking").asStr() + d.at("thinking").asStr();
        } else if (dt == "signature_delta") {
            auto& value = blocks[idx].value;
            if (value.isNull()) value = json::Object{{"type", "thinking"}};
            value.asObj()["signature"] = value.at("signature").asStr() + d.at("signature").asStr();
        }
    } else if (type == "message_start" || type == "message_delta") {
        const auto& u = type == "message_start" ? p.at("message").at("usage") : p.at("usage");
        for (const auto& [key, val] : u.asObj()) usage.asObj()[key] = val;
        readUsage(usage, inTokens, outTokens, cacheHit, cacheMiss, cost);
        if (p.at("delta").has("stop_reason")) stopReason = p.at("delta").at("stop_reason").asStr();
    }
}

ChatResponse AnthropicStreamAcc::finish() {
    ChatResponse r;
    r.text = text;
    r.reasoning = reasoning;
    r.inTokens = inTokens;
    r.outTokens = outTokens;
    r.cacheHit = cacheHit;
    r.cacheMiss = cacheMiss;
    r.cost = cost;
    r.error = error;
    r.stopReason = stopReason;
    json::Array thinking;
    for (auto& b : blocks) {
        if (b.value.at("type").asStr() == "thinking" || b.value.at("type").asStr() == "redacted_thinking")
            thinking.push_back(b.value);
        if (!b.isTool) continue;
        ToolCall tc;
        tc.id = b.id;
        tc.name = b.name;
        tc.argsJson = b.input.empty() ? json::stringify(b.value.at("input")) : b.input;
        if (tc.argsJson == "null") tc.argsJson = "{}";
        r.calls.push_back(std::move(tc));
    }
    r.replay = json::Object{{"thinking", thinking}};
    return r;
}

Result<ChatResponse> parseOpenAiResponse(const json::Value& v) {
    if (v.has("error")) {
        std::string msg = v.at("error").at("message").asStr();
        if (msg.empty()) msg = json::stringify(v.at("error"));
        return Result<ChatResponse>::Err("provider error: " + msg);
    }
    const auto& choices = v.at("choices");
    if (!choices.isArr() || choices.size() == 0)
        return Result<ChatResponse>::Err("provider error: no choices in response");
    const auto& msg = choices.at(0).at("message");
    ChatResponse r;
    r.text = msg.at("content").asStr();
    r.stopReason = choices.at(0).at("finish_reason").asStr();
    r.replay = json::Object{};
    for (const char* key : {"reasoning_content", "reasoning_details"})
        if (msg.has(key)) r.replay.asObj()[key] = msg.at(key);
    const auto& tcs = msg.at("tool_calls");
    if (tcs.isArr()) {
        for (const auto& tc : tcs.asArr()) {
            ToolCall c;
            c.id = tc.at("id").asStr();
            c.name = tc.at("function").at("name").asStr();
            const auto& args = tc.at("function").at("arguments");
            c.argsJson = args.isObj() ? json::stringify(args) : args.asStr();
            if (!c.name.empty()) r.calls.push_back(std::move(c));
        }
    }
    readUsage(v.at("usage"), r.inTokens, r.outTokens, r.cacheHit, r.cacheMiss, r.cost);
    return Result<ChatResponse>::Ok(std::move(r));
}

Result<ChatResponse> parseAnthropicResponse(const json::Value& v) {
    if (v.at("type").asStr() == "error" || v.has("error")) {
        std::string msg = v.at("error").at("message").asStr();
        if (msg.empty()) msg = json::stringify(v);
        return Result<ChatResponse>::Err("provider error: " + msg);
    }
    ChatResponse r;
    r.stopReason = v.at("stop_reason").asStr();
    json::Array thinking;
    for (const auto& b : v.at("content").asArr()) {
        std::string t = b.at("type").asStr();
        if (t == "text")
            r.text += b.at("text").asStr();
        else if (t == "thinking" || t == "redacted_thinking") thinking.push_back(b);
        else if (t == "tool_use") {
            ToolCall c;
            c.id = b.at("id").asStr();
            c.name = b.at("name").asStr();
            c.argsJson = json::stringify(b.at("input"));
            r.calls.push_back(std::move(c));
        }
    }
    const auto& u = v.at("usage");
    readUsage(u, r.inTokens, r.outTokens, r.cacheHit, r.cacheMiss, r.cost);
    r.replay = json::Object{{"thinking", thinking}};
    return Result<ChatResponse>::Ok(std::move(r));
}

Result<std::string> providerApiKey(const ProviderCfg& prov) {
    // $keyEnv ONLY. No keyfile fallback: a recursive `pocket` inherits keys
    // solely through explicit expose_env passthrough in the user config, so
    // key flow is always a deliberate user decision, never ambient magic.
    if (!prov.keyEnv.empty()) {
        if (const char* v = getenv(prov.keyEnv.c_str())) {
            if (v[0] != '\0') {
                for (const unsigned char* p = (const unsigned char*)v; *p; ++p)
                    if (*p <= 32 || *p >= 127 || *p == '"' || *p == '\\')
                        return Result<std::string>::Err("invalid characters in API key environment variable");
                return Result<std::string>::Ok(v);
            }
        }
    }
    // Local daemons usually run without auth; an empty key means "send no
    // Authorization header at all" (never an empty bearer token).
    if (isLoopbackHttp(prov.baseUrl))
        return Result<std::string>::Ok("");
    return Result<std::string>::Err("missing API key: export " + prov.keyEnv +
                                    " in your shell (recursive pocket: expose_env it)");
}

bool shouldRetryRequest(int httpCode, bool curlFailed, bool timedOut, bool emitted) {
    if (emitted || timedOut) return false;  // visible output, or hung past max-time
    if (curlFailed) return true;            // connect/DNS/reset: nothing answered
    return httpCode == 429 || httpCode == 500 || httpCode == 502 || httpCode == 503 ||
           httpCode == 504;
}

long retryDelayMs(int attempt) {
    if (attempt < 1) attempt = 1;
    if (attempt >= 6) return 30000;
    long ms = 1000L << (attempt - 1);  // 1s, 2s, 4s, ...
    return ms > 30000 ? 30000 : ms;
}

long retryAfterMs(const std::string& headers) {
    long delay = 0;
    for (const auto& line : splitLines(headers)) {
        if (startsWith(line, "HTTP/")) delay = 0;
        if (!startsWith(toLower(line), "retry-after:")) continue;
        std::string value = trim(line.substr(12));
        if (value.empty() || value.size() > 9 || value.find_first_not_of("0123456789") != std::string::npos)
            continue;
        delay = std::min(60000L, std::stol(value) * 1000);
    }
    return delay;
}

// ---------------------------------------------------------------------------
// Part 2: curl subprocess execution (argv-based; key via -K config file).
// ---------------------------------------------------------------------------
namespace {

std::string joinUrl(const std::string& base, const std::string& path) {
    std::string b = base;
    while (!b.empty() && b.back() == '/') b.pop_back();
    return b + path;
}

// Per-request secret staging: stateDir()/curl-<pid>-<tag>, mode 0700.
// The state dir is granted to NO child profile, so only the provider-curl
// child (whose profile grants exactly this dir) can read the header file;
// tool children can never reach it. Unlinked + rmdir'd after each request.
std::string provStaging(const std::string& tag) {
    std::string d = stateDir() + "/curl-" + std::to_string((long)getpid()) + "-" + tag;
    if (mkdir(d.c_str(), 0700) != 0) return "";
    return d;
}

// curl transport locks: HTTPS-only protocol set + modern TLS, but only for
// https URLs. Plain http is rejected at config validation except loopback
// (local Ollama-style daemons), which needs neither flag.
void lockTransport(std::vector<std::string>& argv, const std::string& url) {
    if (isLoopbackHttp(url)) {
        argv.insert(argv.end() - 1, {"--noproxy", "*", "--proto", "=http"});
        return;
    }
    argv.insert(argv.end() - 1, "--proto");
    argv.insert(argv.end() - 1, "=https");
    argv.insert(argv.end() - 1, "--proto-redir");
    argv.insert(argv.end() - 1, "=https");
    argv.insert(argv.end() - 1, "--tlsv1.2");
}

int readHttpStatus(const std::string& hdrPath) {
    auto t = readFileBounded(hdrPath, 1 << 20);
    if (!t.ok) return -1;
    // Last "HTTP/x ..." line wins (redirects).
    int code = -1;
    for (const std::string& line : splitLines(t.value)) {
        if (startsWith(line, "HTTP/")) {
            size_t sp = line.find(' ');
            if (sp != std::string::npos) code = atoi(line.c_str() + sp + 1);
        }
    }
    return code;
}

// Sleep ms in short slices so Ctrl-C also cancels a retry cooldown.
bool sleepCancellable(long ms, std::atomic<bool>* cancel) {
    int64_t end = nowMs() + ms;
    struct timespec ts{0, 50 * 1000 * 1000};
    for (;;) {
        if (cancel && cancel->load()) return false;
        if (nowMs() >= end) return true;
        nanosleep(&ts, nullptr);
    }
}

}  // namespace

bool curlAvailable() {
    SpawnOpts o;
    o.exe = "curl";
    o.env = {"PATH=/usr/bin:/bin"};
    o.argv = {"curl", "--disable", "--version"};
    o.timeoutMs = 5000;
    o.outLimit = 4096;
    SpawnResult r = spawn(o);
    return r.ok && r.exitCode == 0;
}

long parseModelsContext(const std::string& body, const std::string& modelId) {
    auto v = json::parse(body);
    if (!v.ok) return -1;
    const json::Value* arr = nullptr;
    if (v.value.isObj() && v.value.at("data").isArr()) arr = &v.value.at("data");
    // Gemini: {"models":[{"name":"models/gemini-...","inputTokenLimit":N}]}
    else if (v.value.isObj() && v.value.at("models").isArr())
        arr = &v.value.at("models");
    else if (v.value.isArr()) arr = &v.value;
    if (!arr) return -1;
    for (const auto& e : arr->asArr()) {
        if (!e.isObj()) continue;
        std::string id = e.at("id").asStr();
        if (id.empty()) {
            id = e.at("name").asStr();
            if (startsWith(id, "models/")) id = id.substr(7);
        }
        if (id != modelId) continue;
        for (const char* f :
             {"context_length", "context_window", "max_context", "max_context_length",
              "max_model_len", "context", "inputTokenLimit"}) {
            long n = e.at(f).asInt(-1);
            if (n >= 512 && n <= 10000000) return n;
        }
        return -1;  // id matched but unpublished
    }
    return -1;
}

long fetchModelContext(const ProviderCfg& prov, const std::string& modelId) {
    static std::map<std::string, std::string> cache;  // one catalog per endpoint, process-lifetime
    static std::mutex mu;
    std::lock_guard<std::mutex> lk(mu);
    std::string key = prov.baseUrl + "\n" + prov.protocol + "\n" + prov.keyEnv;
    auto it = cache.find(key);
    if (it != cache.end()) return parseModelsContext(it->second, modelId);
    std::string catalog;
    auto k = providerApiKey(prov);
    if (k.ok) {
        std::string tag = randHex(4);
        std::string stage = provStaging(tag);
        if (!stage.empty()) {
            std::string cfgPath = stage + "/curl.conf";
            bool useKey = !k.value.empty();  // loopback daemons may run without auth
            std::string hdr =
                prov.protocol == "anthropic" ? "x-api-key: " : "Authorization: Bearer ";
            bool staged =
                !useKey ||
                atomicWriteFile(cfgPath, "header = \"" + hdr + k.value + "\"\n", 0600).ok;
            if (staged) {
                std::string url = joinUrl(prov.baseUrl, "/models");
                SpawnOpts o;
                o.exe = "curl";
                o.env = providerEnv(stage);
                o.argv = {"curl", "--disable", "-fsS", "--no-progress-meter", "--connect-timeout", "2",
                          "--max-time", "5", url};
                if (useKey) {
                    o.argv.insert(o.argv.end() - 1, {"-K", cfgPath});
                }
                if (prov.protocol == "anthropic") {
                    o.argv.insert(o.argv.end() - 1, "-H");
                    o.argv.insert(o.argv.end() - 1, "anthropic-version: 2023-06-01");
                }
                lockTransport(o.argv, url);
                o.timeoutMs = 6000;
                o.outLimit = 2 << 20;
                ChildSpec cs;
                cs.providerCurl = true;
                cs.providerTmp = stage;
                o.childSetup = [cs]() { childEnterSandbox(cs); };
                SpawnResult r = spawn(o);
                if (r.ok && r.exitCode == 0 && !r.truncated)
                    catalog = std::move(r.out);
            }
            unlink(cfgPath.c_str());
            rmdir(stage.c_str());
        }
    }
    auto& stored = cache[key] = std::move(catalog);
    return parseModelsContext(stored, modelId);
}

Result<ChatResponse> chatRequest(const ChatRequest& req, const ChatCallbacks& cb) {
    if (req.model.provider.protocol != "openai" && req.model.provider.protocol != "anthropic")
        return Result<ChatResponse>::Err("unsupported protocol");
    auto key = providerApiKey(req.model.provider);
    if (!key.ok) return Result<ChatResponse>::Err(key.error);

    bool isOpenAi = req.model.provider.protocol == "openai";
    json::Value body = isOpenAi ? buildOpenAiBody(req) : buildAnthropicBody(req);
    if (!isOpenAi) {
        // Anthropic carries system at top level; OpenAI prepends a message.
    } else if (!req.system.empty()) {
        // Prepend system message for OpenAI wire format.
        json::Object sys;
        sys["role"] = json::Value("system");
        sys["content"] = json::Value(req.system);
        json::Array msgs;
        msgs.push_back(json::Value(sys));
        for (const auto& m : body.at("messages").asArr()) msgs.push_back(m);
        body.asObj()["messages"] = json::Value(msgs);
    }
    std::string url = isOpenAi ? joinUrl(req.model.provider.baseUrl, "/chat/completions")
                               : joinUrl(req.model.provider.baseUrl, "/messages");
    std::string bodyJson = json::stringify(body);
    bool useKey = !key.value.empty();  // loopback daemons may run without auth
    auto finish = [&](Result<ChatResponse> r) {
        if (!r.ok) return r;
        if (!r.value.error.empty()) return Result<ChatResponse>::Err(r.value.error);
        if (r.value.stopReason == "length" || r.value.stopReason == "max_tokens")
            return Result<ChatResponse>::Err("provider output limit reached; increase model max_tokens (no tools executed)");
        if (r.value.text.empty() && r.value.calls.empty())
            return Result<ChatResponse>::Err("empty response from provider");
        if (r.value.calls.size() > 64) return Result<ChatResponse>::Err("too many tool calls");
        std::vector<std::string> ids;
        for (auto& call : r.value.calls) {
            if (call.id.empty()) call.id = "call-" + randHex(8);  // keyless local adapters
            if (call.name.empty() || std::find(ids.begin(), ids.end(), call.id) != ids.end())
                return Result<ChatResponse>::Err("invalid/duplicate tool call");
            ids.push_back(call.id);
            if (call.argsJson.empty()) call.argsJson = "{}";
            auto args = json::parse(call.argsJson);
            if (!args.ok || !args.value.isObj())
                return Result<ChatResponse>::Err("invalid tool arguments (no tools executed)");
        }
        if (r.value.replay.isObj())
            r.value.replay.asObj()["model"] = req.model.provider.name + ":" + req.model.model;
        return r;
    };

    for (int attempt = 0;; ++attempt) {
        if (cb.cancel && cb.cancel->load()) return Result<ChatResponse>::Err("cancelled");
        // All staging lives in a per-attempt parent-only dir: the confined
        // provider curl is granted exactly this dir, nothing else. (The
        // child-visible session tmp is deliberately unreachable to it.)
        std::string tag = randHex(4);
        std::string stage = provStaging(tag);
        if (stage.empty()) return Result<ChatResponse>::Err("cannot stage request");
        std::string bodyPath = stage + "/req.json";
        std::string hdrPath = stage + "/resp.hdr";
        std::string cfgPath = stage + "/curl.conf";
        {
            auto w = atomicWriteFile(bodyPath, bodyJson, 0600);
            if (!w.ok) {
                rmdir(stage.c_str());
                return Result<ChatResponse>::Err("cannot stage request: " + w.error);
            }
            // curl -K config carries the secret header so the key never
            // appears in argv (visible via /proc) or shell history.
            if (useKey) {
                std::string conf;
                if (isOpenAi)
                    conf = "header = \"Authorization: Bearer " + key.value + "\"\n";
                else
                    conf = "header = \"x-api-key: " + key.value + "\"\n";
                auto w2 = atomicWriteFile(cfgPath, conf, 0600);
                if (!w2.ok) {
                    unlink(bodyPath.c_str());
                    rmdir(stage.c_str());
                    return Result<ChatResponse>::Err("cannot stage request: " + w2.error);
                }
            }
        }

        SpawnOpts o;
        o.exe = "curl";
        o.env = providerEnv(stage);
        o.argv = {"curl",
                  "--disable",  // never read ambient ~/.curlrc
                  "-sS",
                  "-N",  // no buffering: SSE chunks must reach us as generated
                  "--no-progress-meter",
                  "--connect-timeout",
                  "25",
                  "--max-time",
                  "600",
                  "-X",
                  "POST",
                  "-H",
                  "Content-Type: application/json",
                  "-H",
                  req.stream ? "Accept: text/event-stream" : "Accept: application/json",
                  "--data-binary",
                  "@" + bodyPath,
                  "-D",
                  hdrPath,
                  url};
        if (useKey) {
            o.argv.insert(o.argv.begin() + 2, {"-K", cfgPath});
        }
        if (isOpenAi) {
            o.argv.insert(o.argv.end() - 1, "-H");
            o.argv.insert(o.argv.end() - 1,
                          "HTTP-Referer: https://github.com/yunusemrejr/pocketharness");
        } else {
            o.argv.insert(o.argv.end() - 1, "-H");
            o.argv.insert(o.argv.end() - 1, "anthropic-version: 2023-06-01");
        }
        lockTransport(o.argv, url);
        o.timeoutMs = 620000;
        o.outLimit = 8 << 20;
        o.stopOnLimit = true;
        o.cancel = cb.cancel;
        ChildSpec cs;
        cs.providerCurl = true;
        cs.providerTmp = stage;
        o.childSetup = [cs]() { childEnterSandbox(cs); };

        std::string sseCarry;
        OpenAiStreamAcc oacc;
        AnthropicStreamAcc aacc;
        bool emitted = false;  // any token reached the UI: retries would duplicate it
        o.onChunk = [&](std::string_view chunk, bool isErr) {
            if (isErr) return;
            if (!req.stream) return;
            for (const std::string& payload : sseSplit(chunk, sseCarry)) {
                if (payload == "[DONE]") { oacc.done = true; continue; }
                if (payload.empty()) continue;
                auto v = json::parse(payload);
                if (!v.ok) { oacc.error = aacc.error = "invalid JSON in provider stream"; continue; }
                size_t before = isOpenAi ? oacc.text.size() : aacc.text.size();
                size_t rBefore = isOpenAi ? oacc.reasoning.size() : aacc.reasoning.size();
                if (isOpenAi)
                    oacc.feed(v.value);
                else
                    aacc.feed(v.value);
                size_t after = isOpenAi ? oacc.text.size() : aacc.text.size();
                if (after > before) emitted = true;
                if (cb.onToken && after > before) {
                    const std::string& t = isOpenAi ? oacc.text : aacc.text;
                    cb.onToken(std::string_view(t.data() + before, after - before));
                    emitted = true;
                }
                size_t rAfter = isOpenAi ? oacc.reasoning.size() : aacc.reasoning.size();
                if (rAfter > rBefore) emitted = true;
                if (cb.onReasoning && rAfter > rBefore) {
                    const std::string& t = isOpenAi ? oacc.reasoning : aacc.reasoning;
                    cb.onReasoning(std::string_view(t.data() + rBefore, rAfter - rBefore));
                    emitted = true;
                }
            }
        };

        SpawnResult r = spawn(o);
        unlink(bodyPath.c_str());
        unlink(cfgPath.c_str());
        int http = readHttpStatus(hdrPath);
        auto headers = readFileBounded(hdrPath, 1 << 20);
        long retryAfter = headers.ok ? retryAfterMs(headers.value) : 0;
        unlink(hdrPath.c_str());
        rmdir(stage.c_str());  // last: only succeeds once the dir is empty

        if (r.cancelled) return Result<ChatResponse>::Err("cancelled");
        if (r.truncated) return Result<ChatResponse>::Err("provider response exceeds 8 MiB limit");
        bool timedOut = r.timedOut || r.exitCode == 28;
        bool transportFailed = !r.ok || r.exitCode != 0;
        std::string failMsg;
        if (timedOut) {
            failMsg = "provider request timed out";
        } else if (transportFailed) {
            failMsg = trim(r.err);
            if (failMsg.size() > 500) failMsg = failMsg.substr(0, 500);
            if (failMsg.empty())
                failMsg = "curl failed (exit " + std::to_string(r.exitCode) + ")";
        } else if (http != -1 && (http < 200 || http >= 300)) {
            failMsg = "HTTP " + std::to_string(http);
            // Error bodies may be JSON or SSE; extract a message cheaply.
            std::string blob = req.stream ? std::string() : r.out;
            if (req.stream) {
                // Re-scan stdout for an error payload.
                std::string carry2;
                for (const std::string& p : sseSplit(r.out, carry2)) {
                    if (p.empty() || p == "[DONE]") continue;
                    blob = p;
                    break;
                }
                if (blob.empty()) blob = r.out.substr(0, 2000);
            }
            auto v = json::parse(blob);
            if (v.ok) {
                std::string em = v.value.at("error").at("message").asStr();
                if (em.empty()) em = v.value.at("message").asStr();
                if (!em.empty()) failMsg += ": " + em.substr(0, 500);
            } else if (!blob.empty()) {
                failMsg += ": " + trim(blob).substr(0, 300);
            }
        }
        if (!failMsg.empty()) {
            bool retry = attempt + 1 < kChatMaxAttempts &&
                         shouldRetryRequest(http, transportFailed, timedOut, emitted);
            if (!retry) return Result<ChatResponse>::Err(failMsg);
            long ms = std::max(retryDelayMs(attempt + 1), retryAfter);
            if (cb.onNotice)
                cb.onNotice("request failed (" + failMsg.substr(0, 120) + "); retry " +
                            std::to_string(attempt + 2) + "/" +
                            std::to_string(kChatMaxAttempts) + " in " +
                            std::to_string(ms / 1000) + "s");
            if (!sleepCancellable(ms, cb.cancel))
                return Result<ChatResponse>::Err("cancelled");
            continue;
        }

        if (req.stream) {
            // Some local gateways omit the final blank line. Use the same
            // dispatch path so the final delta also reaches the UI.
            if (!sseCarry.empty()) o.onChunk("\n\n", false);
            ChatResponse resp = isOpenAi ? oacc.finish() : aacc.finish();
            if (!resp.error.empty()) return Result<ChatResponse>::Err(resp.error);
            if (resp.text.empty() && resp.calls.empty() && !r.out.empty()) {
                // Some gateways ignore stream:true and return plain JSON.
                auto v = json::parse(r.out);
                if (v.ok)
                    {
                        auto parsed = finish(isOpenAi ? parseOpenAiResponse(v.value)
                                                      : parseAnthropicResponse(v.value));
                        if (parsed.ok && cb.onToken) cb.onToken(parsed.value.text);
                        return parsed;
                    }
                return Result<ChatResponse>::Err("empty response from provider");
            }
            if (!(isOpenAi ? oacc.done : aacc.done))
                return Result<ChatResponse>::Err("provider stream ended before completion (no tools executed)");
            return finish(Result<ChatResponse>::Ok(std::move(resp)));
        }
        auto v = json::parse(r.out);
        if (!v.ok) return Result<ChatResponse>::Err("invalid JSON from provider: " + v.error);
        return finish(isOpenAi ? parseOpenAiResponse(v.value) : parseAnthropicResponse(v.value));
    }
}

}  // namespace pocket
