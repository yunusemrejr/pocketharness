// PocketHarness - native tool implementations.
#include "tools.h"

#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>
#include <dirent.h>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

#include "agent.h"
#include "brain.h"
#include "kit.h"
#include "kit_lint.h"
#include "oversee.h"
#include "process.h"
#include "skills.h"

namespace pocket {

std::vector<ToolDef> nativeToolDefs() {
    return {
        {"read",
         "Read a file (lines are 1-based, numbered). Paths relative to the workspace "
         "or absolute inside allowed roots (/tmp is readable too). $TMPDIR/ means private session scratch. Bounded output.",
         R"({"type":"object","properties":{"path":{"type":"string"},"offset":{"type":"integer"},"limit":{"type":"integer"}},"required":["path"]})"},
        {"write",
         "Create or replace a file atomically. Parent directories are created inside "
         "allowed roots. Use $TMPDIR/name for private scratch files, not arbitrary /tmp paths. Refuses to follow symlinks.",
         R"({"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}},"required":["path","content"]})"},
        {"edit",
         "Atomic exact replacement(s). Supply old_text/new_text OR edits array. Each old_text must occur exactly expected_matches times "
         "(default 1), else the edit fails without touching the file. $TMPDIR/ means private session scratch.",
         R"({"type":"object","properties":{"path":{"type":"string"},"old_text":{"type":"string"},"new_text":{"type":"string"},"expected_matches":{"type":"integer"},"edits":{"type":"array","items":{"type":"object","properties":{"old_text":{"type":"string"},"new_text":{"type":"string"},"expected_matches":{"type":"integer"}},"required":["old_text","new_text"]}}},"required":["path"]})"},
        {"bash",
         "Run a Linux command (bash) with captured stdout/stderr, timeout, "
         "filesystem sandboxing and network access. The workspace is already the "
         "working directory: never cd there first. Use normal programs (git, grep, "
         "make, ssh, ...) through this tool. When the session is offline, network "
         "commands are blocked for the whole session: do not retry them. Chain dependent steps with &&; "
         "a failed command can leave earlier side effects, so inspect state before retrying. "
         "Background servers: cmd >$TMPDIR/cmd.log 2>&1 &",
         R"({"type":"object","properties":{"command":{"type":"string"},"timeout":{"type":"integer"}},"required":["command"]})"},
        {"skill",
         "Discover and load Markdown skills. Check the catalog (list) before domain "
         "tasks. Actions: list (compact catalog), search {query}, load {name} "
         "(full instructions).",
         R"({"type":"object","properties":{"action":{"type":"string"},"query":{"type":"string"},"name":{"type":"string"}},"required":["action"]})"},
    };
}

std::vector<ToolDef> readOnlyToolDefs() {
    std::vector<ToolDef> out;
    for (const auto& d : nativeToolDefs()) {
        if (d.name == "read") out.push_back(d);
        else if (d.name == "bash")
            out.push_back({"bash",
                           "Run ONE simple read-only command (no pipes, redirection, lists, or "
                           "substitution) to gather evidence: ls, find, grep, rg, git status/diff/log/show, "
                           "cat, head, tail, wc, file, diff, tree, fd, stat, du. Writes, edits, installs, "
                           "network use, and anything outside the allowlist are blocked.",
                           d.paramsJson});
    }
    return out;
}

namespace {

void emit(ToolEnv& env, const std::string& line) {
    if (env.onEvent) env.onEvent(line);
}

long countOccurrences(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return 0;
    long n = 0;
    size_t pos = 0;
    while ((pos = hay.find(needle, pos)) != std::string::npos) {
        ++n;
        pos += needle.size();
    }
    return n;
}

std::string replaceAll(const std::string& hay, const std::string& needle,
                       const std::string& repl) {
    if (needle.empty()) return hay;
    std::string out;
    size_t pos = 0, prev = 0;
    while ((pos = hay.find(needle, prev)) != std::string::npos) {
        out.append(hay, prev, pos - prev);
        out += repl;
        prev = pos + needle.size();
    }
    out.append(hay, prev, std::string::npos);
    return out;
}

void rememberUndo(ToolEnv& env, const std::string& path, const Result<std::string>& old,
                  bool existed, const std::string& written) {
    if ((!old.ok && existed) || written.size() > (4u << 20)) return;
    env.undo.push_back({path, old.ok, old.ok ? old.value : "", written});
    size_t bytes = 0;
    for (const auto& entry : env.undo) bytes += entry.content.size() + entry.written.size();
    while (env.undo.size() > kMaxUndo || bytes > (16u << 20)) {
        bytes -= env.undo.front().content.size() + env.undo.front().written.size();
        env.undo.erase(env.undo.begin());
    }
}

// Guardian pass after every successful change: built-in anti-slop checks
// plus post_edit hooks. Findings go straight back to the model.
std::string afterChange(ToolEnv& env, const std::string& path, const std::string& content) {
    // Staging a file is not a project edit; don't run paid style judges or
    // broadcast scratch paths to peer sessions for temporary working pieces.
    if (!env.sessionTmp.empty() && startsWith(path, env.sessionTmp + "/")) return "";
    bool firstTouch = std::find(env.changedFiles.begin(), env.changedFiles.end(), path) == env.changedFiles.end();
    if (firstTouch) env.changedFiles.push_back(path);
    if (!env.sessionId.empty())
        (void)sessionWorkspacePublish(env.workspace, env.sessionId, "changed", path);
    std::string out;
    // Stubs first, then the rule engine (security, perf, DRY, UI, SVG); later edits of a
    // file only repeat what is High so a legacy file is not re-reported every time.
    auto findings = slopScan(path, content);
    for (auto& f : lintScan(path, content, firstTouch ? 'M' : 'H')) findings.push_back(std::move(f));
    if (!findings.empty()) {
        out += "\n[quality] fix before finishing:";
        for (size_t i = 0; i < findings.size() && i < 10; ++i) out += "\n  " + findings[i];
        if (findings.size() > 10) out += "\n  (+" + std::to_string(findings.size() - 10) + " more: pocket kit lint " + path + ")";
    }
    // Taste check: Jev spots template-grade UI and copy that no regex can.
    std::string lp = toLower(path);
    bool visual = false;
    for (const char* ext : {".html", ".css", ".jsx", ".tsx", ".vue", ".svelte", ".md", ".astro", ".scss"})
        visual = visual || endsWith(lp, ext);
    // Once per file per turn: repeated small edits to one page cost a paid
    // judge call each and rarely change the verdict.
    // Deterministic backstop for the Jev prompt-time verdict: writing a UI file
    // without having read the design doctrine gets one firm demand.
    if (firstTouch && !env.uiDocLoaded)
        for (const char* ext : {".html", ".css", ".jsx", ".tsx", ".vue", ".svelte", ".astro", ".scss"})
            if (endsWith(lp, ext)) {
                out += "\n[harness] UI work: you have not read the design doctrine. Before continuing, run "
                       "skill(action=load, name=\"" + std::string(kUiDocSkill) + "\") and fix what it flags here.";
                break;
            }
    if (visual && firstTouch && env.cfg && content.size() >= 300) {
        auto v = decide(*env.cfg, json::Object{{"file", path}, {"content", content.substr(0, 20000)}},
                        {{"generic", "Is this generic AI-template work (stock purple/blue gradients, emoji decoration, "
                                     "pulsing dots, buzzword hero copy, glassmorphism everywhere, lorem-style filler)?"},
                         {"fake", "Does it contain placeholder, fake or made-up content presented as real?"}},
                        false, &env.sideCost, env.cancel, env.onEvent);
        if (v.count("generic") && v["generic"] >= 0.8)
            out += "\n[quality:jev] reads as generic AI-template design/copy (p=" + std::to_string(v["generic"]).substr(0, 4) +
                   "): give it a deliberate identity.";
        if (v.count("fake") && v["fake"] >= 0.85)
            out += "\n[quality:jev] contains placeholder or fake content (p=" + std::to_string(v["fake"]).substr(0, 4) + ").";
    }
    if (env.cfg && env.cfg->hooks.count("post_edit"))
        for (const auto& cmd : env.cfg->hooks.at("post_edit")) {
            ToolResult h = runHook(env, cmd, "file", path);
            if (!h.ok) out += "\n[hook post_edit failed] " + cmd + "\n" + h.output.substr(0, 2000);
        }
    return out;
}

ToolResult toolRead(ToolEnv& env, const json::Value& args) {
    std::string path = args.at("path").asStr();
    long offset = args.at("offset").asInt(1), limit = args.at("limit").asInt(200);
    if (offset < 1 || limit < 1 || limit > 20000) return {false, "read: invalid offset/limit"};
    auto fd = boxOpenRead(*env.auth, path);
    if (!fd.ok) return {false, fd.error};
    struct Close { int fd; ~Close() { close(fd); } } closer{fd.value};
    // Images are for eyes, not line numbers: the agent attaches the pixels
    // to its next message so vision models can inspect renders/screenshots.
    char head[16];
    ssize_t hn = pread(fd.value, head, sizeof head, 0);
    std::string mime = hn > 0 ? sniffImageMime(std::string_view(head, (size_t)hn)) : "";
    if (!mime.empty()) {
        auto bytes = boxRead(*env.auth, path, kMaxImageBytes);
        if (!bytes.ok) return {false, "read: image over 5 MiB or unreadable: " + path +
                                  " (capture smaller: pocket kit shot URL out.png 1280x800; for video use pocket kit vsheet)"};
        if (env.viewImages.size() >= kMaxImagesPerMessage) return {false, "read: too many images in one batch"};
        env.viewImages.push_back({mime, base64Encode(bytes.value)});
        emit(env, "view " + path);
        return {true, path + ": " + mime + ", " + std::to_string(bytes.value.size()) +
                          " bytes — attached to the next message for visual inspection"};
    }
    size_t cap = env.cfg ? env.cfg->outputLimitBytes : 262144;
    std::string out = path + " (from line " + std::to_string(offset) + "):\n";
    long line = 1, emitted = 0;
    size_t scanned = 0;
    bool start = true;
    char buf[16384];
    while (line - offset < limit) {
        if (env.cancel && env.cancel->load()) return {false, "cancelled"};
        ssize_t n = read(fd.value, buf, sizeof(buf));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return {false, "read failed"};
        if (!n) break;
        scanned += n;
        if (scanned > (64u << 20)) return {false, "read scan exceeds 64 MiB; use a targeted bash command"};
        for (ssize_t i = 0; i < n && line - offset < limit; ++i) {
            if (line >= offset) {
                if (start) { out += std::to_string(line) + "| "; ++emitted; }
                out += buf[i];
                if (out.size() + 64 >= cap) { out += "\n[... output truncated; narrow the range ...]\n"; return {true, out}; }
            }
            start = buf[i] == '\n';
            if (start) ++line;
        }
    }
    if (!emitted && scanned == 0 && offset == 1) return {true, out + "[empty file]\n"};
    if (!emitted) return {false, "read: offset beyond EOF"};
    emit(env, "read " + path + " (" + std::to_string(emitted) + " lines)");
    return {true, out};
}

ToolResult toolWrite(ToolEnv& env, const json::Value& args) {
    ToolResult r;
    std::string path = args.at("path").asStr();
    std::string content = args.at("content").asStr();
    if (path.empty()) {
        r.output = "write: missing path";
        return r;
    }
    if (!args.has("content")) {
        r.output = "write: missing content";
        return r;
    }
    auto lock = sessionWorkspaceLock(env.workspace, env.cancel);
    if (!lock.ok) return {false, lock.error};
    struct Lease { int fd; ~Lease() { close(fd); } } lease{lock.value};
    auto old = boxRead(*env.auth, path, 4 << 20);
    bool existed = boxExists(*env.auth, path).value;
    auto w = boxWrite(*env.auth, path, content, 0644);
    if (!w.ok) {
        r.output = w.error;
        return r;
    }
    emit(env, "write " + path + " (" + std::to_string(content.size()) + " bytes)");
    r.ok = true;
    rememberUndo(env, path, old, existed, content);
    close(lease.fd); lease.fd = -1;  // hooks may launch a recursive Pocket
    r.output = "wrote " + path + " (" + std::to_string(content.size()) + " bytes)";
    // The pre-image is only read up to 4 MiB. Overwriting a larger existing
    // file is unrecoverable, and saying nothing would leave the model
    // believing /undo can bring the original back.
    if (existed && !old.ok) r.output += " [no undo pre-image: previous content exceeded 4 MiB]";
    r.output += afterChange(env, path, content);
    return r;
}

// Models often get indentation or trailing spaces slightly wrong. When the
// exact text is absent, a unique whole-line match that differs only in
// surrounding whitespace is accepted and new_text is re-indented to fit.
struct LineSpan { size_t begin, end; };
std::vector<LineSpan> lineSpans(const std::string& s) {
    std::vector<LineSpan> v;
    for (size_t b = 0; b <= s.size();) {
        size_t e = s.find('\n', b);
        if (e == std::string::npos) e = s.size();
        v.push_back({b, e});
        b = e + 1;
    }
    return v;
}
std::string indentOf(const std::string& s, LineSpan l) {
    size_t i = l.begin;
    while (i < l.end && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(l.begin, i - l.begin);
}
bool fuzzyEdit(std::string& text, const std::string& oldText, const std::string& newText, std::string* hint) {
    auto ol = lineSpans(oldText);
    while (!ol.empty() && trim(oldText.substr(ol.back().begin, ol.back().end - ol.back().begin)).empty()) ol.pop_back();
    while (!ol.empty() && trim(oldText.substr(ol.front().begin, ol.front().end - ol.front().begin)).empty())
        ol.erase(ol.begin());
    if (ol.empty()) return false;
    auto tl = lineSpans(text);
    auto lineAt = [](const std::string& s, LineSpan l) { return trim(s.substr(l.begin, l.end - l.begin)); };
    std::string first = lineAt(oldText, ol[0]);
    long at = -1, hits = 0, near = -1;
    for (size_t i = 0; i + ol.size() <= tl.size(); ++i) {
        if (lineAt(text, tl[i]) != first) continue;
        if (near < 0) near = (long)i;
        size_t k = 1;
        while (k < ol.size() && lineAt(text, tl[i + k]) == lineAt(oldText, ol[k])) ++k;
        if (k == ol.size()) { at = (long)i; ++hits; }
    }
    if (hits != 1) {
        if (hint && hits > 1) *hint = " Whitespace-insensitive match is ambiguous; add surrounding lines.";
        else if (hint && near >= 0)
            *hint = " The first line of old_text is at line " + std::to_string(near + 1) + " but later lines differ.";
        return false;
    }
    // Re-indent: new_text lines carrying old_text's base indent get the file's.
    std::string from = indentOf(oldText, ol[0]), to = indentOf(text, tl[at]);
    std::string body;
    for (auto l : lineSpans(newText)) {
        std::string line = newText.substr(l.begin, l.end - l.begin);
        if (from != to && startsWith(line, from) && !trim(line).empty()) line = to + line.substr(from.size());
        body += line + (l.end < newText.size() ? "\n" : "");
    }
    size_t b = tl[at].begin, e = tl[at + ol.size() - 1].end;
    if (endsWith(body, "\n") && endsWith(oldText, "\n")) body.pop_back();
    text = text.substr(0, b) + body + text.substr(e);
    return true;
}
// Lines the model needs to fix a failed edit without another read: where an
// ambiguous old_text occurs, or the current text closest to its first line.
std::string editContext(const std::string& text, const std::string& oldText, long found) {
    auto tl = lineSpans(text);
    auto lineNo = [&](size_t pos) {
        return std::upper_bound(tl.begin(), tl.end(), pos, [](size_t p, const LineSpan& l) { return p < l.begin; }) - tl.begin();
    };
    if (found > 1) {
        std::string at;
        size_t pos = 0;
        for (int n = 0; n < 8 && (pos = text.find(oldText, pos)) != std::string::npos; pos += oldText.size(), ++n)
            at += (n ? ", " : "") + std::to_string(lineNo(pos));
        return " Matches start at lines " + at + "; add surrounding lines or set expected_matches.";
    }
    std::string first;
    for (auto l : lineSpans(oldText)) {
        first = trim(oldText.substr(l.begin, l.end - l.begin));
        if (!first.empty()) break;
    }
    if (first.size() < 4) return "";
    size_t best = 0, bestScore = 0;
    for (size_t i = 0; i < tl.size(); ++i) {
        std::string line = trim(text.substr(tl[i].begin, tl[i].end - tl[i].begin));
        size_t k = 0;
        while (k < line.size() && k < first.size() && line[k] == first[k]) ++k;
        if (k > bestScore) { bestScore = k; best = i; }
    }
    if (bestScore < std::min<size_t>(8, first.size() / 2 + 1)) return "";
    std::string out = "\nClosest current text:";
    for (size_t i = best; i < tl.size() && i < best + 8; ++i)
        out += "\n" + std::to_string(i + 1) + "| " + text.substr(tl[i].begin, std::min<size_t>(tl[i].end - tl[i].begin, 200));
    return out;
}
ToolResult toolEdit(ToolEnv& env, const json::Value& args) {
    const std::string& path = args.at("path").asStr();
    // Models often mix both forms (edits plus a top-level pair, or edits:[]);
    // the top-level pair is then simply one more step.
    json::Array edits = args.has("edits") ? args.at("edits").asArr() : json::Array{};
    if (!args.has("edits") || args.has("old_text") || args.has("new_text")) edits.push_back(args);
    if (edits.empty() || edits.size() > 64) return {false, "edit: need 1..64 replacements"};
    auto lock = sessionWorkspaceLock(env.workspace, env.cancel);
    if (!lock.ok) return {false, lock.error};
    struct Lease { int fd; ~Lease() { close(fd); } } lease{lock.value};
    auto data = boxRead(*env.auth, path, 4 << 20);
    if (!data.ok) return {false, data.error};
    std::string updated = data.value;
    int fuzzy = 0;
    for (const auto& edit : edits) {
        const json::Value& oldV = edit.has("old_text") ? edit.at("old_text") : edit.at("old_string");
        const json::Value& newV = edit.has("new_text") ? edit.at("new_text") : edit.at("new_string");
        const std::string& oldText = oldV.asStr();
        const std::string& newText = newV.asStr();
        // A numeric string counts; 0 or null means "unspecified" (default 1).
        const json::Value& em = edit.at("expected_matches");
        double count = em.isStr() ? std::atof(em.asStr().c_str()) : em.asNum(1);
        if (em.isNull() || count == 0) count = 1;
        if (oldText.empty() || !newV.isStr() || count < 1 || count > 100000 || count != std::floor(count))
            return {false, "edit: need nonempty old_text, string new_text, positive integer expected_matches"};
        long found = countOccurrences(updated, oldText);
        std::string hint;
        if (found == 0 && count == 1 && fuzzyEdit(updated, oldText, newText, &hint)) { ++fuzzy; continue; }
        if (found != (long)count)
            return {false, "edit: found " + std::to_string(found) + " occurrence(s), expected " +
                    std::to_string((long)count) + "; file untouched." +
                    (hint.empty() ? std::string(" Read the exact current lines first.") : hint) + editContext(updated, oldText, found)};
        if (newText.size() > oldText.size() && newText.size() - oldText.size() >
            ((4u << 20) - updated.size()) / (size_t)found)
            return {false, "edit: result exceeds 4 MiB; file untouched"};
        updated = replaceAll(updated, oldText, newText);
    }
    auto w = boxWrite(*env.auth, path, updated, 0644);
    if (!w.ok) return {false, w.error};
    rememberUndo(env, path, data, true, updated);
    close(lease.fd); lease.fd = -1;
    emit(env, "edit " + path);
    return {true, "edited " + path + " (" + std::to_string(edits.size()) + " replacement step(s))" +
                      (fuzzy ? " [" + std::to_string(fuzzy) + " matched ignoring whitespace; re-read if unsure]" : std::string()) +
                      afterChange(env, path, updated)};
}

ToolResult spawnBash(ToolEnv& env, const std::string& cmd, long timeoutSec) {
    ToolResult r;
    // Read-only passes never spawn recursive pocket children (the allowlist
    // has no pocket), so they skip config staging too — which also keeps two
    // concurrent evidence workers from writing one shared staging dir.
    if (!env.readOnly && env.cfg && !env.sandboxHome.empty()) {
        auto staged = stageChildConfig(*env.cfg, env.sandboxHome);
        if (!staged.ok) return {false, "cannot stage child settings: " + staged.error};
    }
    ChildSpec cs;
    cs.auth = env.auth;  // read-only paths; the lambda below copies the pointer
    cs.workspace = env.workspace;
    cs.sessionTmp = env.sessionTmp;
    cs.allowNet = env.allowNet;
    cs.unsafe = env.unsafe;
    cs.providerCurl = false;

    SpawnOpts o;
    o.exe = "/bin/bash";
    // The command travels in the environment, not argv: `pkill -f pattern`
    // would otherwise match (and kill) this very shell via its cmdline.
    o.argv = {"bash", "--noprofile", "--norc", "-o", "pipefail", "-c",
              "export -n POCKET_BASH_CMD; eval \"$POCKET_BASH_CMD\""};
    o.env = buildChildEnv(env.cfg ? env.cfg->exposeEnv : std::vector<std::string>(), env.workspace,
                          env.sessionTmp, env.sandboxHome, env.auth);
    std::erase_if(o.env, [](const std::string& e) { return startsWith(e, "POCKET_BASH_CMD="); });
    o.env.push_back("POCKET_BASH_CMD=" + cmd);
    if (env.readOnly) {
        // Pinned pagers cannot execute, and git skips its index refresh
        // writes. Stripped first: getenv reads the first match, so a stale
        // duplicate must not shadow the pin.
        for (const char* kv : {"PAGER=cat", "GIT_PAGER=cat", "GIT_OPTIONAL_LOCKS=0"}) {
            std::string key(kv, (size_t)(strchr(kv, '=') - kv) + 1);
            std::erase_if(o.env, [&](const std::string& e) { return startsWith(e, key); });
            o.env.push_back(kv);
        }
    }
    o.workdir = env.workspace;
    o.timeoutMs = timeoutSec * 1000L;
    o.outLimit = env.cfg ? (size_t)env.cfg->outputLimitBytes : 262144;
    o.cancel = env.cancel;
    // Deeper Pocket instances finish teardown before their parent's deadline.
    o.terminateGraceMs = std::max(500, 1500 - 200 * env.depth);
    // `server &` keeps the output pipes open after the shell exits; stop
    // reading shortly after instead of blocking until the timeout.
    o.lingerMs = 1500;
    // Disk safety: a runaway writer (an unbounded ffmpeg apad once wrote 74 GiB
    // of silence) is stopped before it fills the disk; the file cap also binds
    // background children that outlive this call.
    const uint64_t GiB = 1ULL << 30;
    rlim_t maxFile = env.cfg && env.cfg->maxFileGb > 0 ? (rlim_t)env.cfg->maxFileGb * GiB : RLIM_INFINITY;
    o.diskGuardPath = env.workspace;
    o.diskBudgetBytes = env.cfg ? (uint64_t)env.cfg->diskBudgetGb * GiB : 40 * GiB;
    o.diskReserveBytes = env.cfg ? (uint64_t)env.cfg->diskReserveGb * GiB : 10 * GiB;
    startDiskWatchdog({env.workspace, env.sessionTmp}, o.diskReserveBytes);
    o.childSetup = [cs, maxFile]() {
        struct rlimit rl{maxFile, maxFile};
        setrlimit(RLIMIT_FSIZE, &rl);
        childEnterSandbox(cs);
    };

    SpawnResult sr = spawn(o);
    std::string out;
    if (!sr.diskGuard.empty()) {
        r.output = "stopped by disk guard: " + sr.diskGuard +
                   ". Something is writing far more data than intended (unbounded loop, infinite stream such as "
                   "ffmpeg apad/aevalsrc without a duration, runaway log). Find and bound it before rerunning; "
                   "delete the partial output.\n" + sr.out + sr.err;
        return r;
    }
    if (sr.cancelled) {
        r.output = "cancelled";
        return r;
    }
    if (sr.timedOut) {
        out = "timeout after " + std::to_string(timeoutSec) + "s (killed). Long jobs: run in the background "
              "(cmd >$TMPDIR/job.log 2>&1 &) and poll the log, or pass a larger timeout (max 3600).\n";
        r.output = out + sr.out + sr.err;
        return r;  // ok=false signals failure to the model
    }
    char hdr[128];
    if (sr.termSig != 0)
        snprintf(hdr, sizeof(hdr), "[exit: signal %d]\n", sr.termSig);
    else
        snprintf(hdr, sizeof(hdr), "[exit: %d]\n", sr.exitCode);
    out += hdr;
    if (!sr.out.empty()) out += sr.out;
    if (!sr.err.empty()) {
        if (!sr.out.empty() && sr.out.back() != '\n') out += "\n";
        out += "[stderr]\n" + sr.err;
    }
    if (sr.truncated) out += "[... output truncated ...]\n";
    if (sr.termSig == SIGXFSZ || sr.exitCode == 128 + SIGXFSZ || sr.err.find("File too large") != std::string::npos)
        out += "[note: a file reached the " + std::to_string(env.cfg ? env.cfg->maxFileGb : 32) +
               " GiB max_file_gb cap - almost always a runaway writer; bound it, do not raise the cap]\n";
    // Node walks up to every ancestor package.json; the sandbox denies those reads (EACCES, not ENOENT), which node treats as fatal.
    if (sr.err.find("Cannot read package config") != std::string::npos && sr.err.find("permission denied") != std::string::npos)
        out += "[note: node looked for a package.json above the workspace and the sandbox hides it. Put a package.json in the project root (e.g. {\"type\":\"module\"}) so the lookup stops there]\n";
    if (sr.detached)
        out += "[note: background process(es) still running; their later output is not captured - redirect it "
               "to a file, e.g. cmd >$TMPDIR/cmd.log 2>&1 &]\n";
    if (sr.exitCode == 127 && sr.out.empty() && sr.err.empty() && !sr.error.empty()) {
        r.output = sr.error;
        return r;
    }
    r.ok = (sr.termSig == 0 && sr.exitCode == 0);
    // 141 = SIGPIPE under pipefail: `producer | head` closed early, which is
    // what the command asked for, not a failure.
    if (sr.termSig == 0 && sr.exitCode == 141 && cmd.find('|') != std::string::npos) {
        r.ok = true;
        out += "[note: exit 141 = a pipe reader such as head stopped early; output above is complete for it]\n";
    }
    r.output = out;
    return r;
}

ToolResult toolBash(ToolEnv& env, const json::Value& args) {
    ToolResult r;
    std::string cmd = args.at("command").asStr();
    long timeoutSec = args.at("timeout").asInt(env.cfg ? env.cfg->bashTimeoutSec : 120);
    if (cmd.empty()) {
        r.output = "bash: missing command";
        return r;
    }
    if (timeoutSec < 1) timeoutSec = 1;
    if (timeoutSec > 3600) timeoutSec = 3600;
    // Backstop for the prompt-time verdict: a render started without the doctrine is stopped once, before the CPU is spent.
    if (!env.videoDocLoaded && !env.videoNagged && !env.readOnly && cmd.find("kit video") != std::string::npos) {
        env.videoNagged = true;
        r.output = "[harness] Video work: you have not read the video doctrine. Run skill(action=load, name=\"" + std::string(kVideoDocSkill) +
                   "\") first: publish vs private mode, the say/music/mix pipeline, art direction, and the QA gate. "
                   "Then rerun this command (this stop happens once).";
        return r;
    }
    if (env.readOnly) {
        if (timeoutSec > 120) timeoutSec = 120;  // evidence gathering stays quick
        std::string why;
        if (!isReadOnlyBash(cmd, &why)) {
            r.output = "blocked, read-only evidence pass: " + why;
            return r;
        }
    }

    GuardResult g = classifyCommand(cmd, env.workspace, env.allowNet);
    if (g.verdict == Verdict::Deny) {
        r.output = "blocked: " + g.reason;
        return r;
    }
    if (g.verdict == Verdict::Ask) {
        bool approved = false;
        if (env.interactive && env.askApproval) {
            approved = env.askApproval(cmd, g.reason);
        } else if (env.allowDestructive) {
            fprintf(stderr, "pocket: destructive command allowed by --allow-destructive: %s\n",
                    cmd.substr(0, 200).c_str());
            approved = true;
        }
        if (!approved) {
            r.output = "blocked, needs human approval: " + g.reason + "\nCommand: " + cmd;
            return r;
        }
    }

    // User hooks can run anything, so read-only passes skip them: the
    // allowlist above is the whole policy, with no hook-shaped hole.
    if (!env.readOnly && env.cfg && env.cfg->hooks.count("pre_bash"))
        for (const auto& hook : env.cfg->hooks.at("pre_bash")) {
            ToolResult h = runHook(env, hook, "cmd", cmd);
            if (!h.ok) return {false, "blocked by pre_bash hook: " + hook + "\n" + h.output.substr(0, 2000)};
        }
    emit(env, "$ " + (cmd.size() > 300 ? cmd.substr(0, 300) + "..." : cmd));
    ++env.bashRuns;
    return spawnBash(env, cmd, timeoutSec);
}

ToolResult toolSkill(ToolEnv& env, const json::Value& args) {
    ToolResult r;
    std::string action = toLower(args.at("action").asStr());
    auto all = skillDiscover(env.workspace);
    if (action == "list") {
        if (all.empty()) {
            r.ok = true;
            r.output = "no skills installed (add SKILL.md dirs under " + globalSkillDir() +
                       " or .pocket/skills/)";
            return r;
        }
        // Names only: the catalog stays small however many skills exist.
        std::string out = std::to_string(all.size()) + " skills (search {query} for descriptions):\n";
        for (size_t i = 0; i < all.size(); ++i) out += (i ? ", " : "") + all[i].name;
        r.ok = true;
        r.output = out + "\n";
        return r;
    }
    if (action == "search") {
        std::string q = args.at("query").asStr();
        auto hits = skillSearch(all, q);
        if (hits.empty()) {
            r.ok = true;
            r.output = "no skills match \"" + q + "\"";
            return r;
        }
        std::string out = std::to_string(hits.size()) + " match(es), best first:\n";
        for (size_t i = 0; i < hits.size() && i < 8; ++i) out += skillOneLine(hits[i]) + "\n";
        r.ok = true;
        r.output = out;
        return r;
    }
    if (action == "load") {
        std::string name = args.at("name").asStr();
        auto loaded = skillLoad(all, name);
        if (!loaded.ok) {
            r.output = loaded.error;
            return r;
        }
        emit(env, "skill load " + name);
        if (name == kUiDocSkill) env.uiDocLoaded = true;
        if (name == kVideoDocSkill) env.videoDocLoaded = true;
        r.ok = true;
        r.output = loaded.value +
                   "\n\n[harness] Tools here: read, write, edit, bash, skill. Where this skill names "
                   "another tool, use the bash equivalent (`pocket kit` covers web, search, browser "
                   "capture, images, SVG, springs, music/audio, ports/wait, HTTP timing, SEO, CSV profiling, "
                   "benchmarks and quality scans).";
        return r;
    }
    r.output = "skill: unknown action \"" + action + "\" (list|search|load)";
    return r;
}

}  // namespace

ToolResult runTool(ToolEnv& env, const std::string& name, const std::string& argsJson) {
    json::Value args(json::obj());
    if (!trim(argsJson).empty()) {
        auto v = json::parse(argsJson);
        if (!v.ok) {
            ToolResult r;
            r.output = name + ": invalid JSON args: " + v.error;
            return r;
        }
        if (!v.value.isObj()) {
            ToolResult r;
            r.output = name + ": args must be a JSON object";
            return r;
        }
        args = v.value;
    }
    static const auto schemas = [] {
        std::map<std::string, json::Value> out;
        for (const auto& def : nativeToolDefs()) out[def.name] = json::parse(def.paramsJson).value;
        return out;
    }();
    auto schema = schemas.find(name);
    if (schema == schemas.end()) return {false, "unknown tool: " + name};
    // Lenient intake: common aliases map to schema names, unknown or null keys
    // are dropped, numeric strings coerce, and an out-of-range optional integer
    // falls back to its default. Only a wrong required value is fatal.
    static const std::map<std::string, std::string> aliases = {
        {"file_path", "path"}, {"filePath", "path"}, {"file", "path"}, {"cmd", "command"},
        {"old_string", "old_text"}, {"new_string", "new_text"}, {"timeout_sec", "timeout"}};
    json::Object clean;
    for (const auto& [rawKey, raw] : args.asObj()) {
        auto alias = aliases.find(rawKey);
        const std::string& key = alias != aliases.end() && !args.has(alias->second) ? alias->second : rawKey;
        std::string type = schema->second.at("properties").at(key).at("type").asStr();
        if (type.empty() || raw.isNull()) continue;
        json::Value value = raw;
        if (type == "integer" && value.isStr()) {
            std::string s = trim(value.asStr());
            if (!s.empty() && s.size() < 10 && s.find_first_not_of("0123456789") == std::string::npos)
                value = json::Value((long)std::stol(s));
        }
        if (type == "string" && value.isNum()) value = json::stringify(value);
        bool valid = type == "string" ? value.isStr() : type == "array" ? value.isArr() :
                     type == "integer" && value.isNum() && value.asNum() == std::floor(value.asNum()) &&
                     value.asNum() >= 1 && value.asNum() <= 1000000000;
        if (!valid && type == "integer") continue;
        if (!valid) return {false, name + ": invalid argument " + key + " (expected " + type + ")"};
        clean[key] = std::move(value);
    }
    args = json::Value(std::move(clean));
    for (const auto& key : schema->second.at("required").asArr())
        if (!args.has(key.asStr())) return {false, name + ": missing " + key.asStr()};
    if (env.cancel && env.cancel->load()) return {false, "cancelled"};
    if (name != "skill" && !env.auth) return {false, "tool authority unavailable"};
    // Read-only evidence passes (/double streams) can observe but never
    // change state. This is the enforcement point: even a tool call the
    // model was never offered is refused here, not just filtered upstream.
    if (env.readOnly && name != "read" && name != "bash")
        return {false, name + " is unavailable in a read-only evidence pass"};
    if (name == "read" || name == "write" || name == "edit") {
        // Models write $TMPDIR, ${TMPDIR} and a bare $TMPDIR interchangeably.
        // Every spelling must expand here: passed through verbatim it resolves
        // inside the workspace and litters the user's repository with a
        // literal "$TMPDIR" directory that then shows up in git status.
        const std::string p = args.at("path").asStr();
        std::string rest;
        bool scratch = true;
        if (startsWith(p, "${TMPDIR}")) rest = p.substr(9);
        else if (startsWith(p, "$TMPDIR/")) rest = p.substr(8);
        else if (p == "$TMPDIR") rest.clear();
        else scratch = false;
        if (scratch) {
            if (env.sessionTmp.empty()) return {false, "session scratch unavailable"};
            args.asObj()["path"] = rest.empty() ? env.sessionTmp : env.sessionTmp + "/" + rest;
        }
    }
    ToolResult done;
    bool dispatched = true;
    if (name == "read") done = toolRead(env, args);
    else if (name == "write") done = toolWrite(env, args);
    else if (name == "edit") done = toolEdit(env, args);
    else if (name == "bash") done = toolBash(env, args);
    else if (name == "skill") done = toolSkill(env, args);
    else {
        dispatched = false;
        done.output = "unknown tool: " + name;
    }
    if (env.onToolDone) {
        std::string summary = done.output.substr(0, 400);
        env.onToolDone(name, done.ok && dispatched, summary);
    }
    return done;
}

std::string shellQuote(const std::string& s) {
    std::string o = "'";
    for (char c : s) o += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return o + "'";
}

ToolResult runHook(ToolEnv& env, std::string cmd, const std::string& var, const std::string& value) {
    if (!var.empty()) {
        std::string replacement = shellQuote(value);
        for (size_t p = 0; (p = cmd.find("{" + var + "}", p)) != std::string::npos; p += replacement.size())
            cmd.replace(p, var.size() + 2, replacement);
    }
    if (!env.auth) return {false, "hook: tool authority unavailable"};
    return spawnBash(env, cmd, env.cfg ? env.cfg->bashTimeoutSec : 120);
}

std::string undoLast(ToolEnv& env) {
    if (env.undo.empty()) return "nothing to undo";
    if (!env.auth) return "undo: tool authority unavailable";
    auto lock = sessionWorkspaceLock(env.workspace, env.cancel);
    if (!lock.ok) return "undo: " + lock.error;
    struct Lease { int fd; ~Lease() { close(fd); } } lease{lock.value};
    const UndoEntry& e = env.undo.back();
    auto current = boxRead(*env.auth, e.path, 4 << 20);
    if (!current.ok || current.value != e.written)
        return "undo: file changed since this session wrote it; inspect it before undoing";
    std::string path = e.path;
    if (!e.existed) {
        auto removed = boxRemove(*env.auth, e.path);
        if (!removed.ok) return "undo: " + removed.error;
        env.undo.pop_back();
        if (!env.sessionId.empty()) (void)sessionWorkspacePublish(env.workspace, env.sessionId, "undo", path);
        return "removed " + path + " (was created by the agent)";
    }
    auto w = boxWrite(*env.auth, e.path, e.content, 0644);
    if (!w.ok) return "undo: " + w.error;
    env.undo.pop_back();
    if (!env.sessionId.empty()) (void)sessionWorkspacePublish(env.workspace, env.sessionId, "undo", path);
    return "restored " + path;
}

ChildUsage collectChildUsage(ToolEnv& env) {
    ChildUsage delta;
    if (env.sessionTmp.empty()) return delta;
    DIR* d = opendir(env.sessionTmp.c_str());
    if (!d) return delta;
    size_t scanned = 0;
    while (auto* e = readdir(d)) {
        std::string name = e->d_name;
        if (!startsWith(name, "pocket-child-") || !endsWith(name, ".json")) continue;
        if (++scanned > 1024) break;
        auto data = readFileBounded(env.sessionTmp + "/" + name, 4096);
        if (!data.ok) continue;
        auto parsed = json::parse(data.value);
        if (!parsed.ok || !parsed.value.isObj()) continue;
        const auto& v = parsed.value;
        double cost = v.at("cost").asNum(-1), side = v.at("side_cost").asNum(-1);
        long count = v.at("children").asInt(-1);
        if (!std::isfinite(cost) || !std::isfinite(side) || cost < 0 || side < 0 || side > cost ||
            count < 0 || count > 1000000) continue;
        auto& prior = env.childUsage[name];
        if (cost < prior.cost || side < prior.sideCost || count + 1 < prior.count) continue;
        delta.cost += cost - prior.cost;
        delta.sideCost += side - prior.sideCost;
        delta.count += count + 1 - prior.count;
        delta.estimated |= v.at("estimated").asBool();
        delta.seen |= v.at("seen").asBool();
        delta.incomplete |= v.at("incomplete").asBool();
        prior = {cost, side, count + 1, v.at("estimated").asBool(), v.at("seen").asBool(), v.at("incomplete").asBool()};
    }
    closedir(d);
    return delta;
}


// Offline fallback for the Jev verdict that a request is UI/UX/GUI work.
bool looksLikeUiWork(const std::string& text) {
    std::string t = " " + toLower(text) + " ";
    for (const char* k : {"landing page", "website", "web page", "webpage", "frontend", "front-end", " ui ", " ux ",
                          "user interface", " gui ", "dashboard", "mockup", "wireframe", "stylesheet", " css",
                          "tailwind", "navbar", "hero section", "figma", "redesign", "web app", ".html"})
        if (t.find(k) != std::string::npos) return true;
    return false;
}

bool looksLikeVideoWork(const std::string& text) {
    std::string t = " " + toLower(text) + " ";
    for (const char* k : {"video", "motion graphic", "animation", "animated", "youtube", "shorts", "reel", "voiceover", "voice-over",
                          "narration", "explainer", "trailer", "kit video", ".mp4", "storyboard", "soundtrack"})
        if (t.find(k) != std::string::npos) return true;
    return false;
}

}  // namespace pocket
