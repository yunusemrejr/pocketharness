// PocketHarness - session implementation: append-only JSONL, crash-safe reads.
#include "session.h"

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>

#include "config.h"

namespace pocket {

namespace {

std::string sessionPath(const std::string& id) { return sessionDir() + "/" + id + ".jsonl"; }

bool validId(const std::string& id) {
    if (id.empty() || id.size() > 128) return false;
    for (char c : id)
        if (!(c == '-' || c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z')))
            return false;
    return true;
}

}  // namespace

json::Value sessionEventToJson(const SessionEvent& ev) {
    json::Object o;
    o["t"] = json::Value(ev.type);
    if (!ev.text.empty()) o["text"] = json::Value(ev.text);
    if (!ev.toolId.empty()) o["id"] = json::Value(ev.toolId);
    if (!ev.toolName.empty()) o["name"] = json::Value(ev.toolName);
    if (!ev.toolArgs.empty()) o["args"] = json::Value(ev.toolArgs);
    if (ev.type == "tool_result") o["ok"] = json::Value(ev.toolOk);
    return json::Value(o);
}

SessionEvent sessionEventFromJson(const json::Value& v) {
    SessionEvent ev;
    ev.type = v.at("t").asStr();
    ev.text = v.at("text").asStr();
    ev.toolId = v.at("id").asStr();
    ev.toolName = v.at("name").asStr();
    ev.toolArgs = v.at("args").asStr();
    ev.toolOk = v.at("ok").asBool(true);
    return ev;
}

Result<std::string> sessionCreate() {
    auto r = ensureDir(sessionDir(), 0700);
    if (!r.ok) return Result<std::string>::Err(r.error);
    time_t now = time(nullptr);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tmv);
    std::string id = std::string(buf) + "-" + randHex(3);
    // Create the file so resume can find it even if no events yet.
    auto w = appendLine(sessionPath(id), json::stringify(sessionEventToJson(
                                               SessionEvent{"system", "session " + id, "", "", "", true})));
    if (!w.ok) return Result<std::string>::Err(w.error);
    return Result<std::string>::Ok(id);
}

VoidResult sessionAppend(const std::string& id, const SessionEvent& ev) {
    if (!validId(id)) return VoidResult::Err("bad session id");
    return appendLine(sessionPath(id), json::stringify(sessionEventToJson(ev)));
}

Result<SessionLoad> sessionLoad(const std::string& id) {
    if (!validId(id)) return Result<SessionLoad>::Err("bad session id");
    auto t = readFileBounded(sessionPath(id), 64 << 20);
    if (!t.ok) return Result<SessionLoad>::Err(t.error);
    SessionLoad out;
    for (const std::string& line : splitLines(t.value)) {
        if (trim(line).empty()) continue;
        auto v = json::parse(line);
        if (!v.ok) {
            // Partial final line from a crash, or corruption: skip, keep going.
            out.skipped++;
            continue;
        }
        out.events.push_back(sessionEventFromJson(v.value));
    }
    return Result<SessionLoad>::Ok(std::move(out));
}

std::vector<SessionInfo> sessionList(size_t max) {
    std::vector<SessionInfo> out;
    DIR* d = opendir(sessionDir().c_str());
    if (!d) return out;
    struct DirCloser {
        DIR* d;
        ~DirCloser() { closedir(d); }
    } closer{d};
    std::vector<std::string> names;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (endsWith(n, ".jsonl")) names.push_back(n.substr(0, n.size() - 6));
    }
    std::sort(names.begin(), names.end(), std::greater<std::string>());
    for (const auto& id : names) {
        if (out.size() >= max) break;
        SessionInfo si;
        si.id = id;
        si.path = sessionPath(id);
        auto loaded = sessionLoad(id);
        if (!loaded.ok) continue;
        si.events = (long)loaded.value.events.size();
        for (const auto& ev : loaded.value.events) {
            if (ev.type == "user" && !ev.text.empty()) {
                si.firstLine = ev.text.substr(0, 80);
                for (char& c : si.firstLine)
                    if (c == '\n' || c == '\r') c = ' ';
                break;
            }
        }
        out.push_back(std::move(si));
    }
    return out;
}

Result<SessionMeta> sessionLoadMeta(const std::string& id) {
    SessionMeta m;
    if (!validId(id)) return Result<SessionMeta>::Err("bad session id");
    std::string p = sessionDir() + "/" + id + ".meta.json";
    if (access(p.c_str(), R_OK) != 0) return Result<SessionMeta>::Ok(m);  // none yet
    auto t = readFileBounded(p, 1 << 20);
    if (!t.ok) return Result<SessionMeta>::Ok(m);
    auto v = json::parse(t.value);
    if (!v.ok) return Result<SessionMeta>::Ok(m);
    m.systemPrompt = v.value.at("system_prompt").asStr();
    m.orSessionId = v.value.at("or_session_id").asStr();
    m.modelSpec = v.value.at("model").asStr();
    m.systemSource = v.value.at("system_source").asStr();
    m.thinking = v.value.at("thinking").asStr();
    m.turns = v.value.at("turns").asInt(0);
    m.toolCalls = v.value.at("tool_calls").asInt(0);
    m.compactions = v.value.at("compactions").asInt(0);
    m.inTokens = v.value.at("in_tokens").asInt(0);
    m.outTokens = v.value.at("out_tokens").asInt(0);
    m.cacheHit = v.value.at("cache_hit").asInt(0);
    m.cacheMiss = v.value.at("cache_miss").asInt(0);
    m.genMs = v.value.at("gen_ms").asInt(0);
    m.lastPrompt = v.value.at("last_prompt").asInt(-1);
    m.cost = v.value.at("cost").asNum(0);
    m.cacheSeen = v.value.at("cache_seen").asBool(false);
    m.costSeen = v.value.at("cost_seen").asBool(false);
    return Result<SessionMeta>::Ok(m);
}

VoidResult sessionSaveMeta(const std::string& id, const SessionMeta& m) {
    if (!validId(id)) return VoidResult::Err("bad session id");
    auto r = ensureDir(sessionDir(), 0700);
    if (!r.ok) return r;
    json::Object o;
    o["system_prompt"] = json::Value(m.systemPrompt);
    o["or_session_id"] = json::Value(m.orSessionId);
    o["model"] = json::Value(m.modelSpec);
    o["system_source"] = json::Value(m.systemSource);
    o["thinking"] = json::Value(m.thinking);
    o["turns"] = json::Value((double)m.turns);
    o["tool_calls"] = json::Value((double)m.toolCalls);
    o["compactions"] = json::Value((double)m.compactions);
    o["in_tokens"] = json::Value((double)m.inTokens);
    o["out_tokens"] = json::Value((double)m.outTokens);
    o["cache_hit"] = json::Value((double)m.cacheHit);
    o["cache_miss"] = json::Value((double)m.cacheMiss);
    o["gen_ms"] = json::Value((double)m.genMs);
    o["last_prompt"] = json::Value((double)m.lastPrompt);
    o["cost"] = json::Value(m.cost);
    o["cache_seen"] = json::Value(m.cacheSeen);
    o["cost_seen"] = json::Value(m.costSeen);
    return atomicWriteFile(sessionDir() + "/" + id + ".meta.json",
                           json::stringify(json::Value(o), true) + "\n", 0600);
}

Result<std::string> sessionResolve(const std::string& idOrEmpty) {
    if (idOrEmpty.empty() || idOrEmpty == "last") {
        auto list = sessionList(1);
        if (list.empty()) return Result<std::string>::Err("no sessions yet");
        return Result<std::string>::Ok(list[0].id);
    }
    if (!validId(idOrEmpty)) return Result<std::string>::Err("bad session id");
    if (access(sessionPath(idOrEmpty).c_str(), R_OK) != 0)
        return Result<std::string>::Err("session not found: " + idOrEmpty);
    return Result<std::string>::Ok(idOrEmpty);
}

}  // namespace pocket
