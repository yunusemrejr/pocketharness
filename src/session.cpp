// PocketHarness - session implementation: append-only JSONL, crash-safe reads.
#include "session.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
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
    if (!ev.replay.isNull()) o["replay"] = ev.replay;
    if (!ev.text.empty()) o["text"] = json::Value(ev.text);
    if (!ev.toolId.empty()) o["id"] = json::Value(ev.toolId);
    if (!ev.toolName.empty()) o["name"] = json::Value(ev.toolName);
    if (!ev.toolArgs.empty()) o["args"] = json::Value(ev.toolArgs);
    if (ev.type == "tool_result") o["ok"] = json::Value(ev.toolOk);
    if (ev.type == "image") {
        o["file"] = json::Value(ev.imgFile);
        o["mime"] = json::Value(ev.imgMime);
    }
    return json::Value(o);
}

SessionEvent sessionEventFromJson(const json::Value& v) {
    SessionEvent ev;
    ev.type = v.at("t").asStr();
    ev.replay = v.at("replay");
    ev.text = v.at("text").asStr();
    ev.toolId = v.at("id").asStr();
    ev.toolName = v.at("name").asStr();
    ev.toolArgs = v.at("args").asStr();
    ev.toolOk = v.at("ok").asBool(true);
    ev.imgFile = v.at("file").asStr();
    ev.imgMime = v.at("mime").asStr();
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
    // Remove an uncommitted tail before appending the next durable event.
    int fd = open(sessionPath(id).c_str(), O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return VoidResult::Err("cannot open session " + id);
    off_t end = lseek(fd, 0, SEEK_END);
    char c = '\n';
    bool ok = end >= 0 && (end == 0 || pread(fd, &c, 1, end - 1) == 1);
    if (ok && c != '\n') {
        char buf[4096];
        while (ok && end > 0) {
            size_t n = std::min((off_t)sizeof(buf), end);
            end -= n;
            ok = pread(fd, buf, n, end) == (ssize_t)n;
            if (!ok) break;
            std::string_view chunk(buf, n);
            size_t nl = chunk.rfind('\n');
            if (nl != std::string_view::npos) { end += nl + 1; break; }
        }
        if (ok) ok = ftruncate(fd, end) == 0;
    }
    close(fd);
    if (!ok) return VoidResult::Err("cannot repair session tail");
    return appendLine(sessionPath(id), json::stringify(sessionEventToJson(ev)));
}

Result<int> sessionLock(const std::string& id) {
    if (!validId(id)) return Result<int>::Err("bad session id");
    int fd = open((sessionDir() + "/" + id + ".lock").c_str(),
                  O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return Result<int>::Err("cannot lock session " + id);
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) return Result<int>::Ok(fd);
    close(fd);
    return Result<int>::Err("session " + id + " is active in another process");
}

Result<SessionLoad> sessionLoad(const std::string& id) {
    if (!validId(id)) return Result<SessionLoad>::Err("bad session id");
    auto t = readFileBounded(sessionPath(id), 64 << 20);
    if (!t.ok) return Result<SessionLoad>::Err(t.error);
    SessionLoad out;
    // Even valid JSON without a newline is an uncommitted/torn event.
    if (!t.value.empty() && t.value.back() != '\n') {
        size_t nl = t.value.rfind('\n');
        t.value.resize(nl == std::string::npos ? 0 : nl + 1);
        ++out.skipped;
    }
    for (const std::string& line : splitLines(t.value)) {
        if (trim(line).empty()) continue;
        auto v = json::parse(line);
        if (!v.ok || !v.value.isObj() || !v.value.at("t").isStr()) {
            // Partial final line from a crash, or corruption: skip, keep going.
            out.skipped++;
            continue;
        }
        out.events.push_back(sessionEventFromJson(v.value));
    }
    return Result<SessionLoad>::Ok(std::move(out));
}

std::vector<SessionInfo> sessionList(size_t max, const std::string& workspace) {
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
        auto meta = sessionLoadMeta(id);
        if (!meta.ok || (!workspace.empty() && meta.value.workspace != workspace)) continue;
        SessionInfo si;
        si.id = id;
        si.path = sessionPath(id);
        si.workspace = meta.value.workspace;
        auto lock = sessionLock(id);
        si.active = !lock.ok;
        if (lock.ok) close(lock.value);
        // Listing never parses a whole multi-megabyte conversation. The first
        // 64 KiB is enough for a preview; resume still reads the complete log.
        int fd = open(si.path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        char prefix[65536];
        ssize_t n = read(fd, prefix, sizeof(prefix));
        close(fd);
        if (n < 0) continue;
        for (const auto& line : splitLines(std::string(prefix, n))) {
            auto parsed = json::parse(line);
            if (!parsed.ok) continue;
            auto ev = sessionEventFromJson(parsed.value);
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
    if (access(p.c_str(), F_OK) != 0 && errno == ENOENT) return Result<SessionMeta>::Ok(m);
    auto t = readFileBounded(p, 1 << 20);
    if (!t.ok) return Result<SessionMeta>::Err(t.error);
    auto v = json::parse(t.value);
    if (!v.ok || !v.value.isObj()) return Result<SessionMeta>::Err("invalid session metadata: " + id);
    m.systemPrompt = v.value.at("system_prompt").asStr();
    m.orSessionId = v.value.at("or_session_id").asStr();
    m.modelSpec = v.value.at("model").asStr();
    m.systemSource = v.value.at("system_source").asStr();
    m.thinking = v.value.at("thinking").asStr();
    m.workspace = v.value.at("workspace").asStr();
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
    o["workspace"] = json::Value(m.workspace);
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

Result<std::string> sessionResolve(const std::string& idOrEmpty, const std::string& workspace) {
    if (idOrEmpty.empty() || idOrEmpty == "last") {
        auto list = sessionList(30, workspace);
        if (list.empty()) return Result<std::string>::Err("no sessions yet");
        for (const auto& si : list)
            if (!si.active) return Result<std::string>::Ok(si.id);
        return Result<std::string>::Err("all recent sessions are active");
    }
    if (!validId(idOrEmpty)) return Result<std::string>::Err("bad session id");
    if (access(sessionPath(idOrEmpty).c_str(), R_OK) != 0)
        return Result<std::string>::Err("session not found: " + idOrEmpty);
    return Result<std::string>::Ok(idOrEmpty);
}

}  // namespace pocket
