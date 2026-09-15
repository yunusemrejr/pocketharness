// PocketHarness - native tool implementations.
#include "tools.h"

#include <unistd.h>

#include <cstdio>

#include "process.h"
#include "skills.h"

namespace pocket {

std::vector<ToolDef> nativeToolDefs() {
    return {
        {"read",
         "Read a file (lines are 1-based, numbered). Paths relative to the workspace "
         "or absolute inside allowed roots. Bounded output.",
         R"({"type":"object","properties":{"path":{"type":"string"},"offset":{"type":"integer"},"limit":{"type":"integer"}},"required":["path"]})"},
        {"write",
         "Create or replace a file atomically. Parent directories are created inside "
         "allowed roots. Refuses to follow symlinks.",
         R"({"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}},"required":["path","content"]})"},
        {"edit",
         "Exact text replacement. old_text must occur exactly expected_matches times "
         "(default 1), else the edit fails without touching the file.",
         R"({"type":"object","properties":{"path":{"type":"string"},"old_text":{"type":"string"},"new_text":{"type":"string"},"expected_matches":{"type":"integer"}},"required":["path","old_text","new_text"]})"},
        {"bash",
         "Run a Linux command (bash -c) in the workspace with captured stdout/stderr, "
         "timeout, filesystem sandboxing and network access. Use normal "
         "programs (git, grep, make, ssh, ...) through this tool.",
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

ToolResult toolRead(ToolEnv& env, const json::Value& args) {
    ToolResult r;
    std::string path = args.at("path").asStr();
    long offset = args.at("offset").asInt(1);
    long limit = args.at("limit").asInt(2000);
    if (path.empty()) {
        r.output = "read: missing path";
        return r;
    }
    if (offset < 1) offset = 1;
    if (limit < 1) limit = 1;
    if (limit > 20000) limit = 20000;
    size_t cap = env.cfg ? (size_t)env.cfg->outputLimitBytes : 262144;
    auto data = boxRead(*env.auth, path, cap);
    if (!data.ok) {
        r.output = data.error;
        return r;
    }
    std::vector<std::string> lines = splitLines(data.value);
    long total = (long)lines.size();
    if (offset > total) {
        r.output = path + ": offset " + std::to_string(offset) + " beyond EOF (" +
                   std::to_string(total) + " lines)";
        return r;
    }
    long end = offset + limit - 1;
    if (end > total) end = total;
    std::string out = path + " (lines " + std::to_string(offset) + "-" + std::to_string(end) +
                      " of " + std::to_string(total) + "):\n";
    char num[32];
    for (long i = offset; i <= end; ++i) {
        snprintf(num, sizeof(num), "%6ld| ", i);
        out += num;
        out += lines[(size_t)i - 1];
        out += "\n";
        if (out.size() > cap) {
            out += "[... truncated ...]\n";
            break;
        }
    }
    emit(env, "read " + path + " (" + std::to_string(end - offset + 1) + " lines)");
    r.ok = true;
    r.output = out;
    return r;
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
    auto w = boxWrite(*env.auth, path, content, 0644);
    if (!w.ok) {
        r.output = w.error;
        return r;
    }
    emit(env, "write " + path + " (" + std::to_string(content.size()) + " bytes)");
    r.ok = true;
    r.output = "wrote " + path + " (" + std::to_string(content.size()) + " bytes)";
    return r;
}

ToolResult toolEdit(ToolEnv& env, const json::Value& args) {
    ToolResult r;
    std::string path = args.at("path").asStr();
    std::string oldText = args.at("old_text").asStr();
    std::string newText = args.at("new_text").asStr();
    long expected = args.at("expected_matches").asInt(1);
    if (path.empty() || !args.has("old_text") || !args.has("new_text")) {
        r.output = "edit: need path, old_text, new_text";
        return r;
    }
    if (oldText.empty()) {
        r.output = "edit: old_text must not be empty";
        return r;
    }
    if (expected < 1 || expected > 100000) {
        r.output = "edit: expected_matches out of range";
        return r;
    }
    auto data = boxRead(*env.auth, path, 4 << 20);
    if (!data.ok) {
        r.output = data.error;  // absent target fails here, clearly
        return r;
    }
    long found = countOccurrences(data.value, oldText);
    if (found != expected) {
        r.output = "edit: found " + std::to_string(found) + " occurrence(s), expected " +
                   std::to_string(expected) + "; file untouched. " +
                   (found == 0 ? "Check exact whitespace/content with read first."
                               : "Refine old_text or set expected_matches.");
        return r;
    }
    std::string updated = replaceAll(data.value, oldText, newText);
    auto w = boxWrite(*env.auth, path, updated, 0644);
    if (!w.ok) {
        r.output = w.error;
        return r;
    }
    emit(env, "edit " + path + " (" + std::to_string(found) + " match)");
    r.ok = true;
    r.output = "edited " + path + " (" + std::to_string(found) + " replacement)";
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

    emit(env, "$ " + (cmd.size() > 300 ? cmd.substr(0, 300) + "..." : cmd));
    ChildSpec cs;
    cs.auth = env.auth;  // read-only paths; the lambda below copies the pointer
    cs.workspace = env.workspace;
    cs.sessionTmp = env.sessionTmp;
    cs.allowNet = env.allowNet;
    cs.unsafe = env.unsafe;
    cs.providerCurl = false;

    SpawnOpts o;
    o.exe = "/bin/bash";
    o.argv = {"bash", "-c", cmd};
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
        std::string out = std::to_string(all.size()) + " skill(s):\n";
        for (const auto& m : all) out += skillOneLine(m) + "\n";
        r.ok = true;
        r.output = out;
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
        std::string out = std::to_string(hits.size()) + " match(es):\n";
        for (const auto& m : hits) out += skillOneLine(m) + "\n";
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
        r.output = loaded.value;
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

}  // namespace pocket
