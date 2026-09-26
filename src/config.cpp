// PocketHarness - configuration implementation.
#include "config.h"

#include <arpa/inet.h>
#include <algorithm>
#include <cmath>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pocket {

std::string userConfigDir() { return homeDir() + "/.config/pocketharness"; }
std::string userConfigPath() { return userConfigDir() + "/config.json"; }
std::string stateDir() { return homeDir() + "/.local/share/pocketharness"; }
std::string sessionDir() { return stateDir() + "/sessions"; }
std::string globalSkillDir() { return userConfigDir() + "/skills"; }
std::string userSystemPath() { return userConfigDir() + "/system.md"; }
std::string projectSystemPath(const std::string& workspace) {
    return projectDir(workspace) + "/system.md";
}
std::string projectDir(const std::string& workspace) { return workspace + "/.pocket"; }
std::string bundledSkillDir() { return stateDir() + "/skills"; }
std::string userRolesPath() { return userConfigDir() + "/roles.json"; }
std::string userEnvPath() { return userConfigDir() + "/env"; }
std::string projectConfigPath(const std::string& workspace) {
    return projectDir(workspace) + "/config.json";
}
std::string projectSkillDir(const std::string& workspace) {
    return projectDir(workspace) + "/skills";
}

Config defaultConfig() {
    Config c;
    c.defaultModel = "orcarouter:z-ai/glm-5.3-flash";
    // Wire protocols are code; vendors are data. Every endpoint below is
    // OpenAI-compatible unless marked, and needs only its key in the env.
    c.providers = {
        {"orcarouter", "openai", "https://api.orcarouter.ai/v1", "ORCAROUTER_API_KEY"},
        {"openrouter", "openai", "https://openrouter.ai/api/v1", "OPENROUTER_API_KEY"},
        {"deepseek", "openai", "https://api.deepseek.com/v1", "DEEPSEEK_API_KEY"},
        {"friendli", "openai", "https://api.friendli.ai/serverless/v1", "FRIENDLI_API_KEY"},
        {"together", "openai", "https://api.together.xyz/v1", "TOGETHER_API_KEY"},
        {"deepinfra", "openai", "https://api.deepinfra.com/v1/openai", "DEEPINFRA_API_KEY"},
        {"cerebras", "openai", "https://api.cerebras.ai/v1", "CEREBRAS_API_KEY"},
        {"groq", "openai", "https://api.groq.com/openai/v1", "GROQ_API_KEY"},
        {"mistral", "openai", "https://api.mistral.ai/v1", "MISTRAL_API_KEY"},
        {"xai", "openai", "https://api.x.ai/v1", "XAI_API_KEY"},
        {"gemini", "openai", "https://generativelanguage.googleapis.com/v1beta/openai", "GEMINI_API_KEY"},
        {"nvidia", "openai", "https://integrate.api.nvidia.com/v1", "NVIDIA_API_KEY"},
        {"fireworks", "openai", "https://api.fireworks.ai/inference/v1", "FIREWORKS_API_KEY"},
        {"moonshot", "openai", "https://api.moonshot.ai/v1", "MOONSHOT_API_KEY"},
        {"zai", "openai", "https://api.z.ai/api/paas/v4", "ZAI_API_KEY"},
        {"agnes", "openai", "https://apihub.agnes-ai.com/v1", "AGNES_API_KEY"},
        {"atria", "openai", "https://api.atria-asi.ai/v1", "ATRIA_API_KEY"},
        {"longcat", "openai", "https://api.longcat.chat/openai/v1", "LONGCAT_API_KEY"},
        {"ollama-cloud", "openai", "https://ollama.com/v1", "OLLAMA_API_KEY"},
        {"qwen", "openai", "https://token-plan.ap-southeast-1.maas.aliyuncs.com/compatible-mode/v1", "QWEN_API_KEY"},
        {"runinfra", "openai", "https://api.runinfra.ai/v1", "RUNINFRA_API_KEY"},
        {"streamlake", "openai", "https://vanchin.streamlake.ai/api/gateway/coding/v1", "STREAMLAKE_API_KEY"},
        {"xiaomi", "openai", "https://token-plan-sgp.xiaomimimo.com/v1", "XIAOMI_TOKEN_PLAN_SGP_API_KEY"},
        {"stepfun", "openai", "https://api.stepfun.ai/step_plan/v1", "STEPFUN_API_KEY"},
        {"kimi-coding", "anthropic", "https://api.kimi.com/coding", "KIMI_API_KEY"},
        {"minimax", "anthropic", "https://api.minimax.io/anthropic", "MINIMAX_API_KEY"},
        {"anthropic", "anthropic", "https://api.anthropic.com/v1", "ANTHROPIC_API_KEY"},
        {"openai", "openai", "https://api.openai.com/v1", "OPENAI_API_KEY"},
        {"codex", "codex", "https://chatgpt.com/backend-api", ""},  // ChatGPT login (~/.codex/auth.json)
        {"ollama", "openai", "http://127.0.0.1:11434/v1", ""},
        {"lmstudio", "openai", "http://127.0.0.1:1234/v1", ""},
        {"llamacpp", "openai", "http://127.0.0.1:8080/v1", ""},
    };
    // Built-in ids + windows verified against live provider /models listings
    // and the OpenRouter catalog (ids must exist; windows must match).
    c.models = {
        {"glm", "orcarouter", "z-ai/glm-5.3-flash", "", 1310720, true},
        {"deepseek", "deepseek", "deepseek-flash", "", 1048576, true},
        {"codex", "codex", "gpt-5.5", "", 272000, true},
    };
    return c;
}

bool isLoopbackHttp(const std::string& u) {
    if (!startsWith(u, "http://")) return false;
    std::string host = u.substr(7, u.find('/', 7) - 7);
    if (host.empty()) return false;
    if (host.find_first_of("@?#\\ \t\r\n") != std::string::npos) return false;
    size_t end = host[0] == '[' ? host.find(']') + 1 : host.find(':');
    if (end != std::string::npos && end < host.size()) {
        std::string port = host.substr(end);
        if (port.size() < 2 || port[0] != ':' ||
            port.substr(1).find_first_not_of("0123456789") != std::string::npos ||
            port.size() > 6 || std::stol(port.substr(1)) > 65535) return false;
        host.resize(end);
    }
    if (host == "localhost" || host == "[::1]") return true;
    struct in_addr addr{};
    return inet_pton(AF_INET, host.c_str(), &addr) == 1 && (ntohl(addr.s_addr) >> 24) == 127;
}

bool validThinking(const std::string& t) {
    return t == "auto" || t == "off" || t == "none" || t == "minimal" || t == "low" ||
           t == "medium" || t == "high" || t == "xhigh" || t == "max";
}

namespace {

const ProviderCfg* findProvider(const Config& c, const std::string& name) {
    for (const auto& p : c.providers)
        if (p.name == name) return &p;
    return nullptr;
}

bool validEnvName(const std::string& n) {
    if (n.empty() || !(n[0] == '_' || isalpha((unsigned char)n[0]))) return false;
    for (char c : n)
        if (!(c == '_' || isalnum((unsigned char)c))) return false;
    return true;
}

// https anywhere; plain http only for loopback (local Ollama-style daemons).
bool validProviderUrl(const std::string& u) {
    if (u.find_first_of("\r\n\t \\?#") != std::string::npos || u.find('\0') != std::string::npos)
        return false;
    if (startsWith(u, "https://")) {
        std::string host = u.substr(8, u.find('/', 8) - 8);
        return !host.empty() && host.find_first_of("@?#") == std::string::npos &&
               u.find_first_of("?#") == std::string::npos;
    }
    return isLoopbackHttp(u);
}

// Parse one config JSON object into cfg. If isProject is true, security
// authority keys are rejected (silently ignoring would hide misunderstandings
// is worse; we record a warning string instead).
VoidResult parseInto(Config& cfg, const json::Value& v, bool isProject,
                     std::string& projWarn) {
    if (!v.isObj()) return VoidResult::Err("config root must be an object");
    const auto& o = v.asObj();

    auto typeErr = [](const std::string& k, const char* want) {
        return VoidResult::Err("config key \"" + k + "\" must be " + want);
    };
    auto integer = [](const json::Value& v) {
        double n = v.asNum(-1);
        return n == std::floor(n) ? v.asInt(-1) : -1L;
    };

    if (v.has("default_model")) {
        if (!o.at("default_model").isStr()) return typeErr("default_model", "a string");
        cfg.defaultModel = o.at("default_model").asStr();
    }
    if (v.has("thinking")) {
        if (!o.at("thinking").isStr()) return typeErr("thinking", "a string");
        std::string t = toLower(o.at("thinking").asStr());
        if (!validThinking(t))
            return VoidResult::Err("invalid thinking level (auto/off/none/minimal/low/medium/high/xhigh/max)");
        cfg.thinking = t;
    }
    if (v.has("providers")) {
        // Provider endpoints control where keys are sent: a hostile checkout
        // must never be able to redirect them. User config only.
        if (isProject) {
            projWarn += "ignoring project config key \"providers\" (user config only: "
                        "endpoints decide where API keys go)\n";
        } else {
            const auto& p = o.at("providers");
            if (!p.isObj()) return typeErr("providers", "an object");
            for (const auto& kv : p.asObj()) {
                if (!kv.second.isObj())
                    return VoidResult::Err("provider \"" + kv.first + "\" must be an object");
                ProviderCfg pc;
                pc.name = kv.first;
                pc.protocol = kv.second.at("protocol").asStr().empty()
                                  ? "openai"
                                  : toLower(kv.second.at("protocol").asStr());
                if (pc.protocol != "openai" && pc.protocol != "anthropic" && pc.protocol != "codex")
                    return VoidResult::Err("provider \"" + kv.first +
                                           "\": protocol must be \"openai\", \"anthropic\" or \"codex\"");
                pc.baseUrl = kv.second.at("base_url").asStr();
                if (pc.baseUrl.empty())
                    return VoidResult::Err("provider \"" + kv.first + "\" needs \"base_url\"");
                if (!validProviderUrl(pc.baseUrl))
                    return VoidResult::Err("provider \"" + kv.first +
                                           "\": base_url must be https, or http loopback "
                                           "(localhost/127./::1 for local daemons)");
                pc.keyEnv = kv.second.at("key_env").asStr();
                if (pc.keyEnv.empty() && pc.protocol != "codex") {
                    // Local daemons (LM Studio, Ollama, llama.cpp) usually run
                    // without auth; remote endpoints always need a key.
                    if (!isLoopbackHttp(pc.baseUrl))
                        return VoidResult::Err("provider \"" + kv.first +
                                               "\" needs \"key_env\"");
                } else if (!pc.keyEnv.empty() && !validEnvName(pc.keyEnv))
                    return VoidResult::Err("provider \"" + kv.first +
                                           "\": key_env must be a shell variable name");
                bool replaced = false;
                for (auto& e : cfg.providers)
                    if (e.name == pc.name) {
                        e = pc;
                        replaced = true;
                    }
                if (!replaced) cfg.providers.push_back(std::move(pc));
            }
        }
    }
    if (v.has("models")) {
        const auto& m = o.at("models");
        if (!m.isObj()) return typeErr("models", "an object");
        for (const auto& kv : m.asObj()) {
            if (!kv.second.isObj())
                return VoidResult::Err("model \"" + kv.first + "\" must be an object");
            ModelCfg mc;
            mc.alias = kv.first;
            mc.provider = kv.second.at("provider").asStr();
            mc.model = kv.second.at("model").asStr();
            mc.routing = kv.second.at("routing").asStr();
            if (kv.second.has("context")) {
                double n = kv.second.at("context").asNum(-1);
                if (n < 512 || n > 10000000 || n != std::floor(n))
                    return VoidResult::Err("model context must be an integer in 512..10000000");
                mc.context = (long)n;
                mc.contextSet = true;
            }
            const auto& v = kv.second;
            if (mc.provider == "openai") mc.options.tokenParameter = "max_completion_tokens";
            if (v.has("max_tokens")) {
                double n = v.at("max_tokens").asNum(-1);
                if (n < 1 || n > 1048576 || n != std::floor(n))
                    return VoidResult::Err("model max_tokens must be an integer in 1..1048576");
                mc.options.maxTokens = (long)n;
            }
            if (v.has("reasoning")) {
                std::string mode = v.at("reasoning").asStr();
                if (mode != "auto" && mode != "effort" && mode != "budget" &&
                    mode != "adaptive" && mode != "none")
                    return VoidResult::Err("model reasoning must be auto/effort/budget/adaptive/none");
                mc.options.reasoning = mode;
            }
            if (v.has("token_parameter")) {
                std::string p = v.at("token_parameter").asStr();
                if (p != "max_tokens" && p != "max_completion_tokens")
                    return VoidResult::Err("model token_parameter must be max_tokens/max_completion_tokens");
                mc.options.tokenParameter = p;
            }
            for (auto [key, dst] : {std::pair{"stream_usage", &mc.options.streamUsage},
                                   std::pair{"prompt_cache", &mc.options.promptCache}}) {
                if (!v.has(key)) continue;
                if (!v.at(key).isBool()) return typeErr(key, "a boolean");
                *dst = v.at(key).asBool();
            }
            if (mc.provider.empty() || mc.model.empty())
                return VoidResult::Err("model \"" + kv.first +
                                       "\" needs \"provider\" and \"model\"");
            if (!findProvider(cfg, mc.provider))
                return VoidResult::Err("model \"" + kv.first + "\": unknown provider \"" +
                                       mc.provider + "\"");
            bool replaced = false;
            for (auto& e : cfg.models)
                if (e.alias == mc.alias) {
                    e = mc;
                    replaced = true;
                }
            if (!replaced) cfg.models.push_back(std::move(mc));
        }
    }

    if (v.has("roles")) {
        if (!o.at("roles").isObj()) return typeErr("roles", "an object of role -> model spec");
        for (const auto& [role, spec] : o.at("roles").asObj()) {
            if (role != "main" && role != "fast" && role != "fallback" && role != "review" && role != "subagent")
                return VoidResult::Err("unknown model role: " + role);
            if (!spec.isStr()) return typeErr("roles", "an object of role -> model spec");
            cfg.roles[role] = spec.asStr();
        }
    }
    if (v.has("hooks") && isProject) projWarn += "project config ignored security key: hooks\n";
    if (v.has("hooks") && !isProject) {
        if (!o.at("hooks").isObj()) return typeErr("hooks", "an object of event -> [commands]");
        for (const auto& [ev, cmds] : o.at("hooks").asObj()) {
            if (ev != "post_edit" && ev != "pre_bash" && ev != "stop")
                return VoidResult::Err("unknown hook event \"" + ev + "\" (post_edit/pre_bash/stop)");
            if (!cmds.isArr()) return typeErr("hooks", "an object of event -> [commands]");
            for (const auto& c : cmds.asArr()) {
                if (!c.isStr()) return typeErr("hooks", "an object of event -> [commands]");
                cfg.hooks[ev].push_back(c.asStr());
            }
        }
    }
    for (auto [key, dst] : {std::pair{"review", &cfg.review}, std::pair{"autonomy", &cfg.autonomy},
                           std::pair{"jev", &cfg.jev}}) {
        if (!v.has(key)) continue;
        if (!o.at(key).isBool()) return typeErr(key, "a boolean");
        *dst = o.at(key).asBool();
    }
    if (v.has("working_context_tokens")) {
        long n = integer(o.at("working_context_tokens"));
        if (n != 0 && (n < 4096 || n > 1048576))
            return typeErr("working_context_tokens", "0 (disabled) or 4096..1048576 tokens");
        cfg.workingContextTokens = n;
    }
    // Security authority: user config only. Project config must not escalate.
    auto secKey = [&](const char* k) -> bool {
        if (!v.has(k)) return false;
        if (isProject) {
            projWarn += std::string("ignoring project config key \"") + k +
                        "\" (security authority comes from user config/CLI only)\n";
            return false;
        }
        return true;
    };
    if (secKey("tool_network")) {
        if (!o.at("tool_network").isBool()) return typeErr("tool_network", "a boolean");
        cfg.toolNetwork = o.at("tool_network").asBool();
    }
    if (secKey("bash_timeout")) {
        long t = integer(o.at("bash_timeout"));
        if (t < 1 || t > 3600) return typeErr("bash_timeout", "1..3600 seconds");
        cfg.bashTimeoutSec = (int)t;
    }
    if (secKey("max_rounds")) {
        long m = integer(o.at("max_rounds"));
        if (m < 1 || m > 1000) return typeErr("max_rounds", "1..1000 rounds");
        cfg.maxRounds = (int)m;
    }
    if (secKey("output_limit")) {
        long t = integer(o.at("output_limit"));
        if (t < 1024 || t > 8 * 1024 * 1024)
            return typeErr("output_limit", "1024..8388608 bytes");
        cfg.outputLimitBytes = t;
    }
    auto strList = [&](const char* k, std::vector<std::string>& dst) -> VoidResult {
        const auto& a = o.at(k);
        if (!a.isArr()) return typeErr(k, "an array of strings");
        for (const auto& e : a.asArr()) {
            if (!e.isStr()) return typeErr(k, "an array of strings");
            dst.push_back(expandHome(e.asStr()));
        }
        return VoidResult::Ok();
    };
    if (secKey("allow_read")) {
        auto r = strList("allow_read", cfg.allowRead);
        if (!r.ok) return r;
    }
    if (secKey("allow_write")) {
        auto r = strList("allow_write", cfg.allowWrite);
        if (!r.ok) return r;
    }
    if (secKey("local_lm")) {
        const auto& l = o.at("local_lm");
        if (!l.isObj()) return typeErr("local_lm", "an object");
        cfg.localLm.server = expandHome(l.at("server").asStr());
        cfg.localLm.model = expandHome(l.at("model").asStr());
        cfg.localLm.keyFile = expandHome(l.at("key_file").asStr());
        cfg.localLm.port = (int)l.at("port").asInt(18735);
        cfg.localLm.threads = (int)l.at("threads").asInt(4);
        cfg.localLm.ctx = (int)l.at("ctx").asInt(4096);
        if (cfg.localLm.port < 1024 || cfg.localLm.port > 65535 || cfg.localLm.threads < 1 ||
            cfg.localLm.threads > 64 || cfg.localLm.ctx < 512)
            return typeErr("local_lm", "valid port (1024..65535), threads (1..64), ctx (>=512)");
    }
    if (secKey("expose_env")) {
        auto r = strList("expose_env", cfg.exposeEnv);
        if (!r.ok) return r;
    }
    return VoidResult::Ok();
}

}  // namespace

Result<Config> loadConfig(const std::string& workspace) {
    Config cfg = defaultConfig();
    std::string warn;
    if (access(userConfigPath().c_str(), R_OK) == 0) {
        auto t = readFileBounded(userConfigPath(), 1 << 20);
        if (!t.ok) return Result<Config>::Err("cannot read " + userConfigPath() + ": " + t.error);
        auto v = json::parse(t.value);
        if (!v.ok)
            return Result<Config>::Err("invalid JSON in " + userConfigPath() + ": " + v.error);
        std::string dummy;
        auto r = parseInto(cfg, v.value, false, dummy);
        if (!r.ok) return Result<Config>::Err(userConfigPath() + ": " + r.error);
    }
    std::string pp = projectConfigPath(workspace);
    if (access(pp.c_str(), R_OK) == 0) {
        auto t = readFileBounded(pp, 1 << 20);
        if (!t.ok) return Result<Config>::Err("cannot read " + pp + ": " + t.error);
        auto v = json::parse(t.value);
        if (!v.ok) return Result<Config>::Err("invalid JSON in " + pp + ": " + v.error);
        auto r = parseInto(cfg, v.value, true, warn);
        if (!r.ok) return Result<Config>::Err(pp + ": " + r.error);
        if (!warn.empty()) fprintf(stderr, "pocket: %s", warn.c_str());
    }
    // roles.json (written by /models) wins over config roles.
    if (auto t = readFileBounded(userRolesPath(), 65536); t.ok) {
        auto v = json::parse(t.value);
        if (v.ok)
            for (const auto& [role, spec] : v.value.isObj() ? v.value.asObj() : json::Object{})
                if (spec.isStr()) cfg.roles[role] = spec.asStr();
    }
    if (cfg.roles.count("main") && !cfg.roles["main"].empty()) cfg.defaultModel = cfg.roles["main"];
    return Result<Config>::Ok(std::move(cfg));
}

VoidResult saveRole(const std::string& role, const std::string& spec) {
    if (role != "main" && role != "fast" && role != "fallback" && role != "review" && role != "subagent")
        return VoidResult::Err("unknown model role: " + role);
    auto d = ensureDir(userConfigDir(), 0700);
    if (!d.ok) return d;
    int fd = open((userRolesPath() + ".lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (fd < 0) return VoidResult::Err("cannot open model roles lock");
    struct Lease { int fd; ~Lease() { close(fd); } } lease{fd};
    struct stat st{};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) return VoidResult::Err("invalid model roles lock");
    while (flock(fd, LOCK_EX) != 0) if (errno != EINTR) return VoidResult::Err("cannot lock model roles");
    json::Object roles;
    if (auto t = readFileBounded(userRolesPath(), 65536); t.ok) {
        auto v = json::parse(t.value);
        if (!v.ok || !v.value.isObj()) return VoidResult::Err("invalid roles.json; refusing to overwrite");
        roles = v.value.asObj();
    }
    roles[role] = spec;  // explicit empty clears a role inherited from config.json
    return atomicWriteFile(userRolesPath(), json::stringify(json::Value(roles), true) + "\n", 0600);
}

VoidResult stageChildConfig(const Config& cfg, const std::string& childHome) {
    json::Object providers, models, roles;
    for (const auto& p : cfg.providers)
        providers[p.name] = json::Object{{"protocol", p.protocol}, {"base_url", p.baseUrl}, {"key_env", p.keyEnv}};
    for (const auto& m : cfg.models) {
        json::Object model{{"provider", m.provider}, {"model", m.model}, {"routing", m.routing},
                           {"max_tokens", m.options.maxTokens}, {"reasoning", m.options.reasoning},
                           {"token_parameter", m.options.tokenParameter}, {"stream_usage", m.options.streamUsage},
                           {"prompt_cache", m.options.promptCache}};
        if (m.contextSet) model["context"] = m.context;
        models[m.alias] = model;
    }
    auto canonical = [&](std::string spec) {
        std::replace(spec.begin(), spec.end(), ',', '\n');
        std::vector<std::string> resolved;
        for (const auto& one : splitLines(spec)) {
            if (trim(one).empty()) continue;
            auto m = resolveModel(cfg, trim(one));
            if (!m.ok) { resolved.push_back(trim(one)); continue; }
            resolved.push_back(m.value.provider.name + ":" + m.value.model +
                               (m.value.routing.empty() ? "" : "@" + m.value.routing));
        }
        return join(resolved, ",");
    };
    for (const char* role : {"main", "fast", "fallback", "review", "subagent"}) {
        auto entry = cfg.roles.find(role);
        roles[role] = entry == cfg.roles.end() ? "" : canonical(entry->second);
    }
    auto sub = cfg.roles.find("subagent");
    std::string model = canonical(sub != cfg.roles.end() && !sub->second.empty() ? sub->second : cfg.defaultModel);
    roles["main"] = model;
    json::Array exposed;
    for (const auto& name : cfg.exposeEnv) exposed.push_back(name);
    json::Value settings = json::Object{{"providers", providers}, {"models", models}, {"roles", roles},
        {"default_model", model}, {"thinking", cfg.thinking}, {"review", cfg.review}, {"autonomy", cfg.autonomy},
        {"jev", cfg.jev}, {"working_context_tokens", cfg.workingContextTokens},
        {"max_rounds", cfg.maxRounds}, {"bash_timeout", cfg.bashTimeoutSec},
        {"tool_network", cfg.toolNetwork}, {"output_limit", cfg.outputLimitBytes}, {"expose_env", exposed}};
    // The fake HOME is tool-writable. Anchor every parent directory so a model
    // cannot redirect the trusted harness into a symlink outside scratch.
    int fd = open(childHome.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return VoidResult::Err("cannot open child HOME directory");
    struct Lease { int fd; ~Lease() { close(fd); } } lease{fd};
    for (const char* part : {".config", "pocketharness"}) {
        if (mkdirat(lease.fd, part, 0700) != 0 && errno != EEXIST)
            return VoidResult::Err("cannot create child config directory");
        int next = openat(lease.fd, part, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0) return VoidResult::Err("child config directory must not be a symlink");
        close(lease.fd); lease.fd = next;
    }
    std::string path = "/proc/self/fd/" + std::to_string(lease.fd);
    auto writeSettings = [&](const char* name, const json::Value& value) {
        std::string body = json::stringify(value) + "\n", file = path + "/" + name;
        auto old = readFileBounded(file, 1 << 20);
        if (old.ok && old.value == body) return VoidResult::Ok();
        return atomicWriteFile(file, body, 0600);
    };
    auto written = writeSettings("config.json", settings);
    if (!written.ok) return written;
    // Roles override project defaults and replace stale child UI selections.
    return writeSettings("roles.json", roles);
}

int loadEnvFile(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    if (st.st_uid != geteuid() || (st.st_mode & 077)) {
        fprintf(stderr, "pocket: ignoring %s (must be owned by you, mode 0600)\n", path.c_str());
        return 0;
    }
    auto t = readFileBounded(path, 1 << 16);
    if (!t.ok) return 0;
    int n = 0;
    for (std::string line : splitLines(t.value)) {
        line = trim(line);
        if (startsWith(line, "export ")) line = trim(line.substr(7));
        size_t eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), val = trim(line.substr(eq + 1));
        if (val.size() >= 2 && (val[0] == '"' || val[0] == '\'') && val.back() == val[0])
            val = val.substr(1, val.size() - 2);
        if (!validEnvName(k) || val.empty()) continue;
        if (!getenv(k.c_str()) && setenv(k.c_str(), val.c_str(), 0) == 0) ++n;
    }
    return n;
}

Result<ResolvedModel> resolveModel(const Config& cfg, const std::string& spec) {
    std::string s = trim(spec);
    if (s.empty()) s = cfg.defaultModel;
    if (s.empty()) return Result<ResolvedModel>::Err("no model configured");
    // provider:model@routing
    ResolvedModel rm;
    size_t colon = s.find(':');
    if (colon != std::string::npos) {
        std::string pname = s.substr(0, colon);
        std::string rest = s.substr(colon + 1);
        const ProviderCfg* p = findProvider(cfg, pname);
        if (!p)
            return Result<ResolvedModel>::Err("unknown provider \"" + pname +
                                              "\" (see config.json providers)");
        rm.provider = *p;
        size_t at = rest.find('@');
        if (at != std::string::npos) {
            rm.model = rest.substr(0, at);
            rm.routing = rest.substr(at + 1);
        } else {
            rm.model = rest;
        }
        if (rm.model.empty()) return Result<ResolvedModel>::Err("empty model id in \"" + s + "\"");
        rm.spec = s;
        rm.context = 200000;  // last resort; dynamic /models lookup refines this
        if (isLoopbackHttp(p->baseUrl)) rm.context = 32768;  // conservative local fallback
        if (pname == "openai") rm.options.tokenParameter = "max_completion_tokens";
        // Inherit context size from a matching alias with explicit context.
        for (const auto& m : cfg.models)
            if (m.provider == pname && m.model == rm.model) {
                if (m.contextSet) rm.context = m.context;
                rm.options = m.options;
                break;
            }
        return Result<ResolvedModel>::Ok(std::move(rm));
    }
    // alias lookup
    for (const auto& m : cfg.models)
        if (m.alias == s) {
            const ProviderCfg* p = findProvider(cfg, m.provider);
            if (!p) return Result<ResolvedModel>::Err("alias \"" + s + "\": unknown provider");
            rm.provider = *p;
            rm.model = m.model;
            rm.routing = m.routing;
            rm.context = m.context;
            if (!m.contextSet && isLoopbackHttp(p->baseUrl)) rm.context = 32768;
            rm.options = m.options;
            rm.spec = m.provider + ":" + m.model + (m.routing.empty() ? "" : "@" + m.routing);
            return Result<ResolvedModel>::Ok(std::move(rm));
        }
    return Result<ResolvedModel>::Err("unknown model \"" + s +
                                      "\" (use provider:model, or a models{} alias)");
}

bool hasExplicitContext(const Config& cfg, const std::string& provider,
                        const std::string& model) {
    for (const auto& m : cfg.models)
        if (m.contextSet && m.provider == provider && m.model == model) return true;
    return false;
}

}  // namespace pocket
