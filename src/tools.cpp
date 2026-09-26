// PocketHarness - native tool implementations.
#include "tools.h"

#include <unistd.h>

#include <cstdio>
#include <algorithm>
#include <cmath>

#include "brain.h"
#include "kit.h"
#include "oversee.h"
#include "process.h"
#include "skills.h"

namespace pocket {

std::vector<ToolDef> nativeToolDefs() {
    return {
        {"read",
         "Read a file (lines are 1-based, numbered). Paths relative to the workspace "
         "or absolute inside allowed roots (/tmp is readable too). Bounded output.",
         R"({"type":"object","properties":{"path":{"type":"string"},"offset":{"type":"integer"},"limit":{"type":"integer"}},"required":["path"]})"},
        {"write",
         "Create or replace a file atomically. Parent directories are created inside "
         "allowed roots. Refuses to follow symlinks.",
         R"({"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}},"required":["path","content"]})"},
        {"edit",
         "Atomic exact replacement(s). Supply old_text/new_text OR edits array. Each old_text must occur exactly expected_matches times "
         "(default 1), else the edit fails without touching the file.",
         R"({"type":"object","properties":{"path":{"type":"string"},"old_text":{"type":"string"},"new_text":{"type":"string"},"expected_matches":{"type":"integer"},"edits":{"type":"array","items":{"type":"object","properties":{"old_text":{"type":"string"},"new_text":{"type":"string"},"expected_matches":{"type":"integer"}},"required":["old_text","new_text"]}}},"required":["path"]})"},
        {"bash",
         "Run a Linux command (bash -c) with captured stdout/stderr, timeout, "
         "filesystem sandboxing and network access. The workspace is already the "
         "working directory: never cd there first. Use normal programs (git, grep, "
         "make, ssh, ...) through this tool. When the session is offline, network "
         "commands are blocked for the whole session: do not retry them.",
         R"({"type":"object","properties":{"command":{"type":"string"},"timeout":{"type":"integer"}},"required":["command"]})"},
        {"skill",
         "Discover and load Markdown skills. Check the catalog (list) before domain "
         "tasks. Actions: list (compact catalog), search {query}, load {name} "
         "(full instructions).",
         R"({"type":"object","properties":{"action":{"type":"string"},"query":{"type":"string"},"name":{"type":"string"}},"required":["action"]})"},
    };
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

void rememberUndo(ToolEnv& env, const std::string& path) {
    auto old = boxRead(*env.auth, path, 4 << 20);
    auto exists = boxExists(*env.auth, path);
    if (!old.ok && exists.ok && exists.value) return;  // too large/unreadable: no journal
    env.undo.push_back({path, old.ok, old.ok ? old.value : ""});
    if (env.undo.size() > kMaxUndo) env.undo.erase(env.undo.begin());
}

// Guardian pass after every successful change: built-in anti-slop checks
// plus post_edit hooks. Findings go straight back to the model.
std::string afterChange(ToolEnv& env, const std::string& path, const std::string& content) {
    if (std::find(env.changedFiles.begin(), env.changedFiles.end(), path) == env.changedFiles.end())
        env.changedFiles.push_back(path);
    std::string out;
    auto findings = slopScan(path, content);
    if (!findings.empty()) {
        out += "\n[quality] fix before finishing:";
        for (size_t i = 0; i < findings.size() && i < 8; ++i) out += "\n  " + findings[i];
    }
    // Taste check: Jev spots template-grade UI and copy that no regex can.
    std::string lp = toLower(path);
    bool visual = false;
    for (const char* ext : {".html", ".css", ".jsx", ".tsx", ".vue", ".svelte", ".md", ".astro", ".scss"})
        visual = visual || endsWith(lp, ext);
    if (visual && env.cfg && content.size() >= 300) {
        auto v = decide(*env.cfg, json::Object{{"file", path}, {"content", content.substr(0, 20000)}},
                        {{"generic", "Is this generic AI-template work (stock purple/blue gradients, emoji decoration, "
                                     "pulsing dots, buzzword hero copy, glassmorphism everywhere, lorem-style filler)?"},
                         {"fake", "Does it contain placeholder, fake or made-up content presented as real?"}},
                        false, &env.sideCost);
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
    if (!emitted) return {false, "read: offset beyond EOF (or empty file)"};
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
    rememberUndo(env, path);
    auto w = boxWrite(*env.auth, path, content, 0644);
    if (!w.ok) {
        r.output = w.error;
        return r;
    }
    emit(env, "write " + path + " (" + std::to_string(content.size()) + " bytes)");
    r.ok = true;
    r.output = "wrote " + path + " (" + std::to_string(content.size()) + " bytes)" +
               afterChange(env, path, content);
    return r;
}

ToolResult toolEdit(ToolEnv& env, const json::Value& args) {
    const std::string& path = args.at("path").asStr();
    json::Array edits = args.has("edits") ? args.at("edits").asArr() : json::Array{args};
    if (edits.empty() || edits.size() > 64) return {false, "edit: need 1..64 replacements"};
    if (args.has("edits") && (args.has("old_text") || args.has("new_text")))
        return {false, "edit: use edits OR old_text/new_text"};
    auto data = boxRead(*env.auth, path, 4 << 20);
    if (!data.ok) return {false, data.error};
    std::string updated = data.value;
    for (const auto& edit : edits) {
        const std::string& oldText = edit.at("old_text").asStr();
        const std::string& newText = edit.at("new_text").asStr();
        double count = edit.at("expected_matches").asNum(1);
        if (oldText.empty() || !edit.at("new_text").isStr() || count < 1 || count > 100000 ||
            count != std::floor(count) || (edit.has("expected_matches") && !edit.at("expected_matches").isNum()))
            return {false, "edit: need nonempty old_text, string new_text, positive integer expected_matches"};
        long found = countOccurrences(updated, oldText);
        if (found != (long)count)
            return {false, "edit: found " + std::to_string(found) + " occurrence(s), expected " +
                    std::to_string((long)count) + "; file untouched. Read exact content first."};
        if (newText.size() > oldText.size() && newText.size() - oldText.size() >
            ((4u << 20) - updated.size()) / (size_t)found)
            return {false, "edit: result exceeds 4 MiB; file untouched"};
        updated = replaceAll(updated, oldText, newText);
    }
    rememberUndo(env, path);
    auto w = boxWrite(*env.auth, path, updated, 0644);
    if (!w.ok) return {false, w.error};
    emit(env, "edit " + path);
    return {true, "edited " + path + " (" + std::to_string(edits.size()) + " replacement step(s))" +
                      afterChange(env, path, updated)};
}

ToolResult spawnBash(ToolEnv& env, const std::string& cmd, long timeoutSec) {
    ToolResult r;
    ChildSpec cs;
    cs.auth = env.auth;  // read-only paths; the lambda below copies the pointer
    cs.workspace = env.workspace;
    cs.sessionTmp = env.sessionTmp;
    cs.allowNet = env.allowNet;
    cs.unsafe = env.unsafe;
    cs.providerCurl = false;

    SpawnOpts o;
    o.exe = "/bin/bash";
    o.argv = {"bash", "--noprofile", "--norc", "-o", "pipefail", "-c", cmd};
    o.env = buildChildEnv(env.cfg ? env.cfg->exposeEnv : std::vector<std::string>(), env.workspace,
                          env.sessionTmp, env.sandboxHome);
    o.workdir = env.workspace;
    o.timeoutMs = timeoutSec * 1000L;
    o.outLimit = env.cfg ? (size_t)env.cfg->outputLimitBytes : 262144;
    o.cancel = env.cancel;
    o.childSetup = [cs]() { childEnterSandbox(cs); };

    SpawnResult sr = spawn(o);
    std::string out;
    if (sr.cancelled) {
        r.output = "cancelled";
        return r;
    }
    if (sr.timedOut) {
        out = "timeout after " + std::to_string(timeoutSec) + "s (killed)\n";
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
    if (sr.exitCode == 127 && sr.out.empty() && sr.err.empty() && !sr.error.empty()) {
        r.output = sr.error;
        return r;
    }
    r.ok = (sr.termSig == 0 && sr.exitCode == 0);
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

    if (env.cfg && env.cfg->hooks.count("pre_bash"))
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
        r.ok = true;
        r.output = loaded.value +
                   "\n\n[harness] Tools here: read, write, edit, bash, skill. Where this skill names "
                   "another tool, use the bash equivalent (`pocket kit` covers web, search, browser "
                   "capture, images, SVG, springs, audio and quality scans).";
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
    for (const auto& key : schema->second.at("required").asArr())
        if (!args.has(key.asStr())) return {false, name + ": missing " + key.asStr()};
    for (const auto& [key, value] : args.asObj()) {
        std::string type = schema->second.at("properties").at(key).at("type").asStr();
        bool valid = type == "string" ? value.isStr() : type == "array" ? value.isArr() :
                     type == "integer" && value.isNum() && value.asNum() == std::floor(value.asNum()) &&
                     value.asNum() >= 1 && value.asNum() <= 1000000000;
        if (!valid) return {false, name + ": invalid argument " + key};
    }
    if (env.cancel && env.cancel->load()) return {false, "cancelled"};
    if (name != "skill" && !env.auth) return {false, "tool authority unavailable"};
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
    if (!var.empty())
        for (size_t p; (p = cmd.find("{" + var + "}")) != std::string::npos;)
            cmd.replace(p, var.size() + 2, shellQuote(value));
    if (!env.auth) return {false, "hook: tool authority unavailable"};
    return spawnBash(env, cmd, env.cfg ? env.cfg->bashTimeoutSec : 120);
}

std::string undoLast(ToolEnv& env) {
    if (env.undo.empty()) return "nothing to undo";
    if (!env.auth) return "undo: tool authority unavailable";
    UndoEntry e = std::move(env.undo.back());
    env.undo.pop_back();
    if (!e.existed) {
        // The file was created by the agent: remove it again.
        std::string p = e.path[0] == '/' ? e.path : env.workspace + "/" + e.path;
        return unlink(p.c_str()) == 0 ? "removed " + e.path + " (was created by the agent)"
                                      : "undo: cannot remove " + e.path;
    }
    auto w = boxWrite(*env.auth, e.path, e.content, 0644);
    return w.ok ? "restored " + e.path + " (" + std::to_string(e.content.size()) + " bytes)" : "undo: " + w.error;
}

}  // namespace pocket
