// PocketHarness - provider implementation, part 1: bodies + stream parsing.
#include "provider.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <cstdio>

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
    return m.provider.name == "openrouter" ||
           m.provider.baseUrl.find("openrouter") != std::string::npos;
}

// Shared usage-block reader (streaming deltas and full responses alike).
// Reads exactly what the provider reported; absent fields stay -1.
void readUsage(const json::Value& u, long& in, long& out, long& hit, long& miss, double& cost) {
    if (!u.isObj()) return;
    if (u.has("prompt_tokens")) in = u.at("prompt_tokens").asInt(in);
    if (u.has("completion_tokens")) out = u.at("completion_tokens").asInt(out);
    if (u.has("prompt_cache_hit_tokens")) hit = u.at("prompt_cache_hit_tokens").asInt(hit);
    if (u.has("prompt_cache_miss_tokens")) miss = u.at("prompt_cache_miss_tokens").asInt(miss);
    const auto& det = u.at("prompt_tokens_details");
    if (det.isObj() && det.has("cached_tokens") && hit < 0)
        hit = det.at("cached_tokens").asInt(hit);
    // Anthropic cache shape (cache_creation = write, cache_read = reuse).
    if (u.has("cache_read_input_tokens")) hit = u.at("cache_read_input_tokens").asInt(hit);
    if (u.has("cost") && u.at("cost").isNum()) cost = u.at("cost").asNum(cost);
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
        } else {
            o["role"] = json::Value(m.role);
            o["content"] = json::Value(m.content);
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
    if (req.stream) {
        json::Object so;
        so["include_usage"] = json::Value(true);
        b["stream_options"] = json::Value(so);
    }
    b["max_tokens"] = json::Value((double)req.maxTokens);
    // Reasoning knob only when explicitly requested (compat with strict servers).
    if (req.thinking == "low" || req.thinking == "medium" || req.thinking == "high")
        b["reasoning_effort"] = json::Value(req.thinking);
    else if (req.thinking == "max")
        b["reasoning_effort"] = json::Value("xhigh");
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
        } else if (m.role == "assistant" && !m.toolCalls.empty()) {
            o["role"] = json::Value("assistant");
            json::Array blocks;
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
        } else {
            o["role"] = json::Value(m.role == "system" ? "user" : m.role);
            o["content"] = json::Value(m.content);
        }
        msgs.push_back(json::Value(o));
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
    if (req.thinking == "low") {
        json::Object th;
        th["type"] = json::Value("enabled");
        th["budget_tokens"] = json::Value(2048.0);
        b["thinking"] = json::Value(th);
    } else if (req.thinking == "medium") {
        json::Object th;
        th["type"] = json::Value("enabled");
        th["budget_tokens"] = json::Value(8000.0);
        b["thinking"] = json::Value(th);
    } else if (req.thinking == "high") {
        json::Object th;
        th["type"] = json::Value("enabled");
        th["budget_tokens"] = json::Value(16000.0);
        b["thinking"] = json::Value(th);
    } else if (req.thinking == "max") {
        json::Object th;
        th["type"] = json::Value("enabled");
        th["budget_tokens"] = json::Value(32000.0);
        b["thinking"] = json::Value(th);
    }
    return json::Value(b);
}

std::vector<std::string> sseSplit(std::string_view chunk, std::string& carry) {
    carry.append(chunk.data(), chunk.size());
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        size_t nl = carry.find('\n', start);
        if (nl == std::string::npos) break;
        std::string line = carry.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        start = nl + 1;
        if (startsWith(line, "data:")) {
            std::string payload = trim(line.substr(5));
            out.push_back(std::move(payload));
        }
        // ignore "event:", ":", "id:", "" (dispatch by data payload shape)
    }
    carry.erase(0, start);
    return out;
}

void OpenAiStreamAcc::feed(const json::Value& p) {
    const auto& choices = p.at("choices");
    if (choices.isArr() && choices.size() > 0) {
        const auto& delta = choices.at(0).at("delta");
        if (delta.has("content") && delta.at("content").isStr())
            text += delta.at("content").asStr();
        const auto& tcs = delta.at("tool_calls");
        if (tcs.isArr()) {
            for (const auto& tc : tcs.asArr()) {
                long idx = tc.at("index").asInt(0);
                if (idx < 0) continue;
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
    r.inTokens = inTokens;
    r.outTokens = outTokens;
    r.cacheHit = cacheHit;
    r.cacheMiss = cacheMiss;
    r.cost = cost;
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
    if (type == "content_block_start") {
        long idx = p.at("index").asInt(-1);
        if (idx < 0) return;
        if ((size_t)idx >= blocks.size()) blocks.resize((size_t)idx + 1);
        const auto& cb = p.at("content_block");
        if (cb.at("type").asStr() == "tool_use") {
            blocks[(size_t)idx].isTool = true;
            blocks[(size_t)idx].id = cb.at("id").asStr();
            blocks[(size_t)idx].name = cb.at("name").asStr();
        }
    } else if (type == "content_block_delta") {
        long idx = p.at("index").asInt(-1);
        if (idx < 0) return;
        if ((size_t)idx >= blocks.size()) blocks.resize((size_t)idx + 1);
        const auto& d = p.at("delta");
        std::string dt = d.at("type").asStr();
        if (dt == "text_delta")
            text += d.at("text").asStr();
        else if (dt == "input_json_delta")
            blocks[(size_t)idx].input += d.at("partial_json").asStr();
        else if (dt == "thinking_delta") {
            // Thinking content is not shown; models still act on it.
        }
    } else if (type == "message_start") {
        const auto& u = p.at("message").at("usage");
        if (u.has("input_tokens")) inTokens = u.at("input_tokens").asInt(inTokens);
        readUsage(u, inTokens, outTokens, cacheHit, cacheMiss, cost);
    } else if (type == "message_delta") {
        const auto& u = p.at("usage");
        if (u.has("input_tokens")) inTokens = u.at("input_tokens").asInt(inTokens);
        if (u.has("output_tokens")) outTokens = u.at("output_tokens").asInt(outTokens);
        readUsage(u, inTokens, outTokens, cacheHit, cacheMiss, cost);
    }
}

ChatResponse AnthropicStreamAcc::finish() {
    ChatResponse r;
    r.text = text;
    r.inTokens = inTokens;
    r.outTokens = outTokens;
    r.cacheHit = cacheHit;
    r.cacheMiss = cacheMiss;
    r.cost = cost;
    for (auto& b : blocks) {
        if (!b.isTool) continue;
        ToolCall tc;
        tc.id = b.id;
        tc.name = b.name;
        tc.argsJson = b.input;
        r.calls.push_back(std::move(tc));
    }
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
    const auto& tcs = msg.at("tool_calls");
    if (tcs.isArr()) {
        for (const auto& tc : tcs.asArr()) {
            ToolCall c;
            c.id = tc.at("id").asStr();
            c.name = tc.at("function").at("name").asStr();
            c.argsJson = tc.at("function").at("arguments").asStr();
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
    for (const auto& b : v.at("content").asArr()) {
        std::string t = b.at("type").asStr();
        if (t == "text")
            r.text += b.at("text").asStr();
        else if (t == "tool_use") {
            ToolCall c;
            c.id = b.at("id").asStr();
            c.name = b.at("name").asStr();
            c.argsJson = json::stringify(b.at("input"));
            r.calls.push_back(std::move(c));
        }
    }
    const auto& u = v.at("usage");
    if (u.has("input_tokens")) r.inTokens = u.at("input_tokens").asInt(-1);
    if (u.has("output_tokens")) r.outTokens = u.at("output_tokens").asInt(-1);
    readUsage(u, r.inTokens, r.outTokens, r.cacheHit, r.cacheMiss, r.cost);
    return Result<ChatResponse>::Ok(std::move(r));
}

Result<std::string> providerApiKey(const ProviderCfg& prov) {
    if (const char* v = getenv(prov.keyEnv.c_str())) {
        if (v[0] != '\0') return Result<std::string>::Ok(v);
    }
    // Recursive `pocket` children receive a key file instead of env secrets.
    if (const char* kf = getenv("POCKETHARNESS_KEYFILE")) {
        if (kf[0] != '\0' && access(kf, R_OK) == 0) {
            auto t = readFileBounded(kf, 65536);
            if (t.ok) {
                for (const std::string& line : splitLines(t.value)) {
                    size_t eq = line.find('=');
                    if (eq == std::string::npos) continue;
                    if (line.substr(0, eq) == prov.keyEnv) {
                        std::string val = line.substr(eq + 1);
                        if (!val.empty()) return Result<std::string>::Ok(val);
                    }
                }
            }
        }
    }
    return Result<std::string>::Err("missing API key: set $" + prov.keyEnv + " in your shell");
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

}  // namespace

bool curlAvailable() {
    SpawnOpts o;
    o.exe = "curl";
    o.argv = {"curl", "--version"};
    o.timeoutMs = 5000;
    o.outLimit = 4096;
    SpawnResult r = spawn(o);
    return r.ok && r.exitCode == 0;
}

Result<ChatResponse> chatRequest(const ChatRequest& req, const ChatCallbacks& cb,
                                 const std::string& tmpDir) {
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

    std::string tag = randHex(4);
    std::string bodyPath = tmpDir + "/req-" + tag + ".json";
    std::string hdrPath = tmpDir + "/resp-" + tag + ".hdr";
    std::string cfgPath = tmpDir + "/curl-" + tag + ".conf";
    {
        auto w = atomicWriteFile(bodyPath, json::stringify(body), 0600);
        if (!w.ok) return Result<ChatResponse>::Err("cannot stage request: " + w.error);
        // curl -K config carries the secret header so the key never appears
        // in argv (visible via /proc) or shell history.
        std::string conf;
        if (isOpenAi)
            conf = "header = \"Authorization: Bearer " + key.value + "\"\n";
        else
            conf = "header = \"x-api-key: " + key.value + "\"\n";
        auto w2 = atomicWriteFile(cfgPath, conf, 0600);
        if (!w2.ok) return Result<ChatResponse>::Err("cannot stage request: " + w2.error);
    }

    SpawnOpts o;
    o.exe = "curl";
    o.argv = {"curl",
              "-sS",
              "-N",  // no buffering: SSE chunks must reach us as generated
              "--no-progress-meter",
              "--connect-timeout",
              "25",
              "--max-time",
              "600",
              "-X",
              "POST",
              "-K",
              cfgPath,
              "-H",
              "Content-Type: application/json",
              "-H",
              "Accept: application/json",
              "--data-binary",
              "@" + bodyPath,
              "-D",
              hdrPath,
              url};
    if (isOpenAi) {
        o.argv.insert(o.argv.end() - 1, "-H");
        o.argv.insert(o.argv.end() - 1, "HTTP-Referer: https://github.com/yunusemrejr/pocketharness");
    } else {
        o.argv.insert(o.argv.end() - 1, "-H");
        o.argv.insert(o.argv.end() - 1, "anthropic-version: 2023-06-01");
    }
    o.timeoutMs = 620000;
    o.outLimit = 32 << 20;
    o.cancel = cb.cancel;
    ChildSpec cs;
    cs.providerCurl = true;  // trusted harness networking: no confinement
    o.childSetup = [cs]() { childEnterSandbox(cs); };

    std::string sseCarry;
    OpenAiStreamAcc oacc;
    AnthropicStreamAcc aacc;
    std::string rawBody;  // non-streaming accumulator
    o.onChunk = [&](std::string_view chunk, bool isErr) {
        if (isErr) return;
        if (!req.stream) {
            rawBody.append(chunk.data(), chunk.size());
            return;
        }
        for (const std::string& payload : sseSplit(chunk, sseCarry)) {
            if (payload.empty() || payload == "[DONE]") continue;
            auto v = json::parse(payload);
            if (!v.ok) continue;  // keep-alive / partial: ignore
            size_t before = isOpenAi ? oacc.text.size() : aacc.text.size();
            if (isOpenAi)
                oacc.feed(v.value);
            else
                aacc.feed(v.value);
            size_t after = isOpenAi ? oacc.text.size() : aacc.text.size();
            if (cb.onToken && after > before) {
                const std::string& t = isOpenAi ? oacc.text : aacc.text;
                cb.onToken(std::string_view(t.data() + before, after - before));
            }
        }
    };

    SpawnResult r = spawn(o);
    unlink(bodyPath.c_str());
    unlink(cfgPath.c_str());
    int http = readHttpStatus(hdrPath);
    unlink(hdrPath.c_str());

    if (r.cancelled) return Result<ChatResponse>::Err("cancelled");
    if (r.timedOut) return Result<ChatResponse>::Err("provider request timed out");
    if (!r.ok || r.exitCode != 0) {
        std::string msg = trim(r.err);
        if (msg.size() > 500) msg = msg.substr(0, 500);
        if (msg.empty()) msg = "curl failed (exit " + std::to_string(r.exitCode) + ")";
        return Result<ChatResponse>::Err(msg);
    }
    if (http != -1 && (http < 200 || http >= 300)) {
        std::string msg = "HTTP " + std::to_string(http);
        // Error bodies may be JSON or SSE; extract a message cheaply.
        std::string blob = req.stream ? std::string() : rawBody;
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
            if (!em.empty()) msg += ": " + em.substr(0, 500);
        } else if (!blob.empty()) {
            msg += ": " + trim(blob).substr(0, 300);
        }
        return Result<ChatResponse>::Err(msg);
    }

    if (req.stream) {
        // Flush any trailing payload without a newline.
        if (!sseCarry.empty()) {
            std::string line = trim(sseCarry);
            if (startsWith(line, "data:")) {
                auto v = json::parse(trim(line.substr(5)));
                if (v.ok) {
                    if (isOpenAi)
                        oacc.feed(v.value);
                    else
                        aacc.feed(v.value);
                }
            }
            sseCarry.clear();
        }
        ChatResponse resp = isOpenAi ? oacc.finish() : aacc.finish();
        if (resp.text.empty() && resp.calls.empty() && !r.out.empty()) {
            // Some gateways ignore stream:true and return plain JSON.
            auto v = json::parse(r.out);
            if (v.ok)
                return isOpenAi ? parseOpenAiResponse(v.value) : parseAnthropicResponse(v.value);
            return Result<ChatResponse>::Err("empty response from provider");
        }
        return Result<ChatResponse>::Ok(std::move(resp));
    }
    auto v = json::parse(rawBody.empty() ? r.out : rawBody);
    if (!v.ok) return Result<ChatResponse>::Err("invalid JSON from provider: " + v.error);
    return isOpenAi ? parseOpenAiResponse(v.value) : parseAnthropicResponse(v.value);
}

}  // namespace pocket
