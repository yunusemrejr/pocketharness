// PocketHarness - session implementation: append-only JSONL, crash-safe reads.
#include "session.h"

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>

#include "config.h"

namespace pocket {

namespace {

constexpr size_t kMaxSessionBytes = 64u << 20;
constexpr size_t kMaxMetaBytes = 1u << 20;
constexpr size_t kMaxWorkspaceEvents = 128;
constexpr size_t kMaxWorkspaceText = 2048;
constexpr size_t kMaxWorkspaceBytes = 2u << 20;
std::string coordinationDirOverride;

struct Fd {
    int value;
    explicit Fd(int fd) : value(fd) {}
    ~Fd() { if (value >= 0) close(value); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};

std::string sessionPath(const std::string& id) { return sessionDir() + "/" + id + ".jsonl"; }

bool validId(const std::string& id) {
    if (id.empty() || id.size() > 128) return false;
    for (char c : id)
        if (!(c == '-' || c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z')))
            return false;
    return true;
}

bool regularFd(int fd) {
    struct stat st{};
    return fstat(fd, &st) == 0 && S_ISREG(st.st_mode);
}

int openRegular(const std::string& path, int flags, mode_t mode = 0600) {
    int fd = open(path.c_str(), flags | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, mode);
    if (fd >= 0 && !regularFd(fd)) { close(fd); fd = -1; errno = EINVAL; }
    return fd;
}

bool lockFd(int fd, int operation) {
    int rc;
    do { rc = flock(fd, operation); } while (rc != 0 && errno == EINTR);
    return rc == 0;
}

bool writeAll(int fd, const std::string& text) {
    size_t offset = 0;
    while (offset < text.size()) {
        ssize_t n = write(fd, text.data() + offset, text.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        offset += (size_t)n;
    }
    int rc;
    do { rc = fsync(fd); } while (rc != 0 && errno == EINTR);
    return rc == 0;
}

bool syncDir(const std::string& path) {
    Fd fd(open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (fd.value < 0) return false;
    int rc;
    do { rc = fsync(fd.value); } while (rc != 0 && errno == EINTR);
    return rc == 0;
}

Result<std::string> readRegular(const std::string& path, size_t limit, bool missingOk = false,
                                bool sharedLock = false, bool* missing = nullptr) {
    if (missing) *missing = false;
    Fd fd(openRegular(path, O_RDONLY));
    if (fd.value < 0) {
        if (missingOk && errno == ENOENT) {
            if (missing) *missing = true;
            return Result<std::string>::Ok("");
        }
        return Result<std::string>::Err("cannot open regular file " + path);
    }
    if (sharedLock && !lockFd(fd.value, LOCK_SH))
        return Result<std::string>::Err("cannot lock " + path);
    std::string out;
    char buf[65536];
    for (;;) {
        ssize_t n = read(fd.value, buf, sizeof(buf));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return Result<std::string>::Err("cannot read " + path);
        if (n == 0) return Result<std::string>::Ok(std::move(out));
        if ((size_t)n > limit - out.size())
            return Result<std::string>::Err("file too large: " + path);
        out.append(buf, (size_t)n);
    }
}

// Hold an exclusive log lock while finding and removing a torn final record.
bool repairTail(int fd) {
    off_t end = lseek(fd, 0, SEEK_END);
    if (end < 0) return false;
    if (end == 0) return true;
    char c;
    ssize_t n;
    do { n = pread(fd, &c, 1, end - 1); } while (n < 0 && errno == EINTR);
    if (n != 1) return false;
    if (c == '\n') return true;
    char buf[4096];
    while (end > 0) {
        size_t count = std::min((off_t)sizeof(buf), end);
        end -= count;
        do { n = pread(fd, buf, count, end); } while (n < 0 && errno == EINTR);
        if (n != (ssize_t)count) return false;
        size_t nl = std::string_view(buf, count).rfind('\n');
        if (nl != std::string_view::npos) { end += nl + 1; break; }
    }
    return ftruncate(fd, end) == 0 && lseek(fd, end, SEEK_SET) == end;
}

struct SessionEntry { std::string id; struct timespec modified; };

std::vector<SessionEntry> sessionEntries() {
    std::vector<SessionEntry> entries;
    DIR* dir = opendir(sessionDir().c_str());
    if (!dir) return entries;
    while (dirent* entry = readdir(dir)) {
        std::string name = entry->d_name;
        if (!endsWith(name, ".jsonl")) continue;
        std::string id = name.substr(0, name.size() - 6);
        struct stat st{};
        if (!validId(id) || fstatat(dirfd(dir), name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0 ||
            !S_ISREG(st.st_mode)) continue;
        entries.push_back({std::move(id), st.st_mtim});
    }
    closedir(dir);
    std::sort(entries.begin(), entries.end(), [](const SessionEntry& a, const SessionEntry& b) {
        if (a.modified.tv_sec != b.modified.tv_sec) return a.modified.tv_sec > b.modified.tv_sec;
        if (a.modified.tv_nsec != b.modified.tv_nsec) return a.modified.tv_nsec > b.modified.tv_nsec;
        return a.id > b.id;
    });
    return entries;
}

struct WorkspacePath { std::string workspace, directory, base; };

Result<WorkspacePath> workspacePath(const std::string& workspace, bool create) {
    char canonical[PATH_MAX];
    if (workspace.empty() || workspace.find('\0') != std::string::npos ||
        !realpath(workspace.c_str(), canonical))
        return Result<WorkspacePath>::Err("workspace does not exist");
    struct stat st{};
    if (stat(canonical, &st) != 0 || !S_ISDIR(st.st_mode))
        return Result<WorkspacePath>::Err("workspace is not a directory");
    // Stable compact key. The full path is also checked in each document, so
    // a hash collision fails closed instead of crossing workspace boundaries.
    uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char c : std::string(canonical)) { hash ^= c; hash *= UINT64_C(1099511628211); }
    char key[17];
    snprintf(key, sizeof(key), "%016llx", (unsigned long long)hash);
    std::string dir = sessionWorkspaceCoordinationDir();
    // Default roots contain unrelated workspaces. Grant only this workspace's
    // subdirectory. An inherited override is already the parent's narrow scope.
    if (coordinationDirOverride.empty()) dir += "/" + std::string(key);
    if (create) {
        auto made = ensureDir(dir, 0700);
        if (!made.ok) return Result<WorkspacePath>::Err(made.error);
    }
    return Result<WorkspacePath>::Ok({canonical, dir, dir + "/" + key});
}

Result<json::Value> readWorkspace(const WorkspacePath& path) {
    bool missing = false;
    auto data = readRegular(path.base + ".json", kMaxWorkspaceBytes, true, false, &missing);
    if (!data.ok) return Result<json::Value>::Err(data.error);
    if (missing) {
        json::Object initial;
        initial["workspace"] = path.workspace;
        initial["sequence"] = 0;
        initial["events"] = json::Array{};
        return Result<json::Value>::Ok(json::Value(std::move(initial)));
    }
    auto doc = json::parse(data.value);
    if (!doc.ok || !doc.value.isObj() || doc.value.at("workspace").asStr() != path.workspace ||
        !doc.value.at("events").isArr() || doc.value.at("events").size() > kMaxWorkspaceEvents ||
        !doc.value.at("sequence").isNum() || doc.value.at("sequence").asInt(-1) < 0 ||
        doc.value.at("sequence").asNum() > 9007199254740991.0 ||
        doc.value.at("sequence").asNum() != doc.value.at("sequence").asInt(-1))
        return Result<json::Value>::Err("invalid workspace coordination data");
    long prior = 0;
    for (const auto& event : doc.value.at("events").asArr()) {
        long sequence = event.at("sequence").asInt(-1);
        if (sequence <= prior || (prior && sequence != prior + 1) ||
            event.at("sequence").asNum(-1) != sequence ||
            sequence > doc.value.at("sequence").asInt() ||
            !validId(event.at("session").asStr()) || !validId(event.at("type").asStr()) ||
            !event.at("text").isStr() || event.at("text").asStr().size() > kMaxWorkspaceText)
            return Result<json::Value>::Err("invalid workspace event");
        prior = sequence;
    }
    if (prior != doc.value.at("sequence").asInt())
        return Result<json::Value>::Err("invalid workspace event sequence");
    return doc;
}

}  // namespace

std::string sessionWorkspaceCoordinationDir() {
    return coordinationDirOverride.empty() ? sessionDir() + "/workspaces" : coordinationDirOverride;
}

VoidResult sessionSetWorkspaceCoordinationDir(const std::string& path) {
    if (path.empty()) { coordinationDirOverride.clear(); return VoidResult::Ok(); }
    char canonical[PATH_MAX];
    struct stat st{};
    if (path.find('\0') != std::string::npos || lstat(path.c_str(), &st) != 0 ||
        !S_ISDIR(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 0077) ||
        !realpath(path.c_str(), canonical))
        return VoidResult::Err("coordination directory must be an existing private owned directory");
    coordinationDirOverride = canonical;
    return VoidResult::Ok();
}

Result<std::string> sessionWorkspaceDirectory(const std::string& workspace) {
    auto path = workspacePath(workspace, true);
    if (!path.ok) return Result<std::string>::Err(path.error);
    return Result<std::string>::Ok(path.value.directory);
}

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
    for (int attempt = 0; attempt < 16; ++attempt) {
        std::string id = std::string(buf) + "-" + randHex(6);
        // O_EXCL prevents the rare random collision from merging histories.
        Fd fd(openRegular(sessionPath(id), O_WRONLY | O_CREAT | O_EXCL));
        if (fd.value < 0) {
            if (errno == EEXIST) continue;
            return Result<std::string>::Err("cannot create session file");
        }
        std::string line = json::stringify(sessionEventToJson(
            SessionEvent{"system", "session " + id, "", "", "", true})) + "\n";
        if (!writeAll(fd.value, line) || !syncDir(sessionDir())) {
            unlink(sessionPath(id).c_str());
            return Result<std::string>::Err("cannot persist new session");
        }
        return Result<std::string>::Ok(id);
    }
    return Result<std::string>::Err("cannot allocate a unique session id");
}

VoidResult sessionAppend(const std::string& id, const SessionEvent& ev) {
    if (!validId(id)) return VoidResult::Err("bad session id");
    if (ev.type.empty()) return VoidResult::Err("empty session event type");
    std::string line = json::stringify(sessionEventToJson(ev)) + "\n";
    if (line.size() > kMaxSessionBytes) return VoidResult::Err("session event too large");
    Fd fd(openRegular(sessionPath(id), O_RDWR));
    if (fd.value < 0) return VoidResult::Err("cannot open session " + id);
    // Repair and append use one fd and one lock. Even concurrent readers never
    // see an in-progress record, and concurrent appenders cannot truncate it.
    if (!lockFd(fd.value, LOCK_EX) || !repairTail(fd.value))
        return VoidResult::Err("cannot repair session tail");
    off_t end = lseek(fd.value, 0, SEEK_END);
    if (end < 0 || (uint64_t)end > kMaxSessionBytes - line.size())
        return VoidResult::Err("session reached 64 MiB; start a new session");
    if (!writeAll(fd.value, line)) return VoidResult::Err("cannot persist session event");
    return VoidResult::Ok();
}

Result<int> sessionLock(const std::string& id) {
    if (!validId(id)) return Result<int>::Err("bad session id");
    Fd session(openRegular(sessionPath(id), O_RDONLY));
    if (session.value < 0) return Result<int>::Err("session not found: " + id);
    int fd = openRegular(sessionDir() + "/" + id + ".lock", O_RDWR | O_CREAT);
    if (fd < 0) return Result<int>::Err("cannot lock session " + id);
    if (lockFd(fd, LOCK_EX | LOCK_NB)) return Result<int>::Ok(fd);
    close(fd);
    return Result<int>::Err("session " + id + " is active in another process");
}

Result<int> sessionWorkspaceLock(const std::string& workspace, std::atomic<bool>* cancel) {
    auto path = workspacePath(workspace, true);
    if (!path.ok) return Result<int>::Err(path.error);
    int fd = openRegular(path.value.base + ".write.lock", O_RDWR | O_CREAT);
    if (fd < 0) return Result<int>::Err("cannot open workspace mutation lock");
    for (;;) {
        if (cancel && cancel->load()) { close(fd); return Result<int>::Err("cancelled"); }
        if (lockFd(fd, LOCK_EX | LOCK_NB)) return Result<int>::Ok(fd);
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            close(fd);
            return Result<int>::Err("cannot acquire workspace mutation lock");
        }
        struct timespec pause{0, 20 * 1000 * 1000};
        nanosleep(&pause, nullptr);
    }
}

VoidResult sessionWorkspacePublish(const std::string& workspace, const std::string& sessionId,
                                    const std::string& type, const std::string& text) {
    if (!validId(sessionId) || !validId(type)) return VoidResult::Err("invalid workspace event id/type");
    auto path = workspacePath(workspace, true);
    if (!path.ok) return VoidResult::Err(path.error);
    Fd fd(openRegular(path.value.base + ".events.lock", O_RDWR | O_CREAT));
    if (fd.value < 0 || !lockFd(fd.value, LOCK_EX))
        return VoidResult::Err("cannot lock workspace events");
    auto doc = readWorkspace(path.value);
    if (!doc.ok) return VoidResult::Err(doc.error);
    long sequence = doc.value.at("sequence").asInt();
    if (sequence >= 9007199254740991L) return VoidResult::Err("workspace event sequence exhausted");
    // Normalize malformed UTF-8 through JSON before limiting the encoded text.
    std::string bounded = json::parse(json::stringify(json::Value(text.substr(0, kMaxWorkspaceText)))).value.asStr();
    if (bounded.size() > kMaxWorkspaceText) {
        size_t cut = kMaxWorkspaceText;
        while (cut > 0 && ((unsigned char)bounded[cut] & 0xc0) == 0x80) --cut;
        bounded.resize(cut);
    }
    json::Object event;
    event["sequence"] = ++sequence;
    event["session"] = sessionId;
    event["type"] = type;
    event["text"] = std::move(bounded);
    auto& events = doc.value.asObj()["events"].asArr();
    if (events.size() >= kMaxWorkspaceEvents) events.erase(events.begin());
    events.push_back(json::Value(std::move(event)));
    doc.value.asObj()["sequence"] = sequence;
    auto saved = atomicWriteFile(path.value.base + ".json", json::stringify(doc.value) + "\n", 0600);
    if (!saved.ok) return saved;
    if (!syncDir(path.value.directory))
        return VoidResult::Err("cannot persist workspace events directory");
    return VoidResult::Ok();
}

Result<WorkspaceUpdates> sessionWorkspaceRead(const std::string& workspace, long after,
                                              const std::string& ownSession) {
    if (after < 0 || (!ownSession.empty() && !validId(ownSession)))
        return Result<WorkspaceUpdates>::Err("invalid workspace event cursor/session");
    auto path = workspacePath(workspace, false);
    if (!path.ok) return Result<WorkspaceUpdates>::Err(path.error);
    // Atomic replacement provides a coherent snapshot without blocking a writer.
    auto doc = readWorkspace(path.value);
    if (!doc.ok) return Result<WorkspaceUpdates>::Err(doc.error);
    WorkspaceUpdates out;
    out.lastSequence = doc.value.at("sequence").asInt();
    const auto& events = doc.value.at("events").asArr();
    out.missed = after > out.lastSequence ||
                 (!events.empty() && after < events.front().at("sequence").asInt() - 1);
    for (const auto& event : events) {
        long sequence = event.at("sequence").asInt();
        if ((sequence > after || after > out.lastSequence) && event.at("session").asStr() != ownSession)
            out.events.push_back({sequence, event.at("session").asStr(), event.at("type").asStr(),
                                  event.at("text").asStr()});
    }
    return Result<WorkspaceUpdates>::Ok(std::move(out));
}

Result<SessionLoad> sessionLoad(const std::string& id) {
    if (!validId(id)) return Result<SessionLoad>::Err("bad session id");
    auto t = readRegular(sessionPath(id), kMaxSessionBytes, false, true);
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
        if (!v.ok || !v.value.isObj() || v.value.at("t").asStr().empty()) {
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
    if (max == 0) return out;
    for (const auto& entry : sessionEntries()) {
        if (out.size() >= max) break;
        const auto& id = entry.id;
        auto meta = sessionLoadMeta(id);
        if (!meta.ok || (!workspace.empty() && meta.value.workspace != workspace)) continue;
        SessionInfo si;
        si.id = id;
        si.path = sessionPath(id);
        si.workspace = meta.value.workspace;
        si.goalStatus = meta.value.goalStatus;
        si.goal = meta.value.goal.substr(0, 256);
        si.lastStopReason = meta.value.lastStopReason;
        si.lastStopDetail = meta.value.lastStopDetail;
        si.lastStoppedAtMs = meta.value.lastStoppedAtMs;
        auto lock = sessionLock(id);
        si.active = !lock.ok;
        if (lock.ok) close(lock.value);
        // Listing never parses a whole multi-megabyte conversation. The first
        // 64 KiB is enough for a preview; resume still reads the complete log.
        int fd = openRegular(si.path, O_RDONLY);
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
    bool missing = false;
    auto t = readRegular(p, kMaxMetaBytes, true, false, &missing);
    if (!t.ok) return Result<SessionMeta>::Err(t.error);
    if (missing) return Result<SessionMeta>::Ok(m);
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
    m.sideCost = v.value.at("side_cost").asNum(0);
    m.costEstimated = v.value.at("cost_estimated").asBool(false);
    m.costIncomplete = v.value.at("cost_incomplete").asBool(false);
    m.genTokens = v.value.at("gen_tokens").asInt(m.outTokens);
    m.childSessions = v.value.at("child_sessions").asInt(0);
    m.rolesSet = v.value.at("roles_set").asBool(false);
    m.goal = v.value.at("goal").asStr();
    m.goalStatus = v.value.at("goal_status").asStr();
    m.goalPhase = v.value.at("goal_phase").asStr();
    m.goalBrief = v.value.at("goal_brief").asStr();
    m.goalNext = v.value.at("goal_next").asStr();
    m.goalProgress = v.value.at("goal_progress").asStr();
    m.lastStopReason = v.value.at("last_stop_reason").asStr().substr(0, 64);
    m.lastStopDetail = v.value.at("last_stop_detail").asStr().substr(0, 1024);
    m.lastStoppedAtMs = std::max(0L, v.value.at("last_stopped_at_ms").asInt());
    m.originalRequest = v.value.at("original_request").asStr();
    m.latestRequest = v.value.at("latest_request").asStr();
    m.doubleEnabled = v.value.at("double_enabled").asBool(false);
    m.doubles = v.value.at("doubles").asInt(0);
    for (const auto& [role, model] : v.value.at("roles").asObj())
        if (model.isStr()) m.roles[role] = model.asStr();
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
    o["side_cost"] = json::Value(m.sideCost);
    o["cost_estimated"] = json::Value(m.costEstimated);
    o["cost_incomplete"] = json::Value(m.costIncomplete);
    o["gen_tokens"] = json::Value(m.genTokens);
    o["child_sessions"] = json::Value(m.childSessions);
    o["roles_set"] = json::Value(m.rolesSet);
    o["goal"] = m.goal;
    o["goal_status"] = m.goalStatus;
    o["goal_phase"] = m.goalPhase;
    o["goal_brief"] = m.goalBrief;
    o["goal_next"] = m.goalNext;
    o["goal_progress"] = m.goalProgress;
    o["last_stop_reason"] = m.lastStopReason.substr(0, 64);
    o["last_stop_detail"] = m.lastStopDetail.substr(0, 1024);
    o["last_stopped_at_ms"] = json::Value(std::max<int64_t>(0, m.lastStoppedAtMs));
    o["original_request"] = m.originalRequest;
    o["latest_request"] = m.latestRequest;
    o["double_enabled"] = json::Value(m.doubleEnabled);
    o["doubles"] = json::Value(m.doubles);
    json::Object roles;
    for (const auto& [role, model] : m.roles) roles[role] = model;
    o["roles"] = json::Value(std::move(roles));
    std::string data = json::stringify(json::Value(o), true) + "\n";
    if (data.size() > kMaxMetaBytes) return VoidResult::Err("session metadata exceeds 1 MiB");
    auto saved = atomicWriteFile(sessionDir() + "/" + id + ".meta.json", data, 0600);
    if (!saved.ok) return saved;
    if (!syncDir(sessionDir())) return VoidResult::Err("cannot persist session metadata directory");
    return VoidResult::Ok();
}

Result<std::string> sessionResolve(const std::string& idOrEmpty, const std::string& workspace) {
    if (idOrEmpty.empty() || idOrEmpty == "last") {
        bool found = false;
        for (const auto& entry : sessionEntries()) {
            auto meta = sessionLoadMeta(entry.id);
            if (!meta.ok || (!workspace.empty() && meta.value.workspace != workspace)) continue;
            found = true;
            auto lock = sessionLock(entry.id);
            if (lock.ok) { close(lock.value); return Result<std::string>::Ok(entry.id); }
        }
        return Result<std::string>::Err(found ? "all sessions are active" : "no sessions yet");
    }
    if (!validId(idOrEmpty)) return Result<std::string>::Err("bad session id");
    Fd fd(openRegular(sessionPath(idOrEmpty), O_RDONLY));
    if (fd.value < 0)
        return Result<std::string>::Err("session not found: " + idOrEmpty);
    auto meta = sessionLoadMeta(idOrEmpty);
    if (!meta.ok) return Result<std::string>::Err(meta.error);
    if (!workspace.empty() && !meta.value.workspace.empty() && meta.value.workspace != workspace)
        return Result<std::string>::Err("session belongs to workspace " + meta.value.workspace);
    return Result<std::string>::Ok(idOrEmpty);
}

}  // namespace pocket
