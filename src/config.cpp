// PocketHarness - configuration implementation.
#include "config.h"

#include <unistd.h>

namespace pocket {

std::string userConfigDir() { return homeDir() + "/.config/pocketharness"; }
std::string userConfigPath() { return userConfigDir() + "/config.json"; }
std::string stateDir() { return homeDir() + "/.local/share/pocketharness"; }
std::string sessionDir() { return stateDir() + "/sessions"; }
std::string statePath() { return stateDir() + "/state.json"; }
std::string cacheDir() { return homeDir() + "/.cache/pocketharness"; }
std::string globalSkillDir() { return userConfigDir() + "/skills"; }
std::string userSystemPath() { return userConfigDir() + "/system.md"; }
std::string projectSystemPath(const std::string& workspace) {
    return projectDir(workspace) + "/system.md";
}
std::string projectDir(const std::string& workspace) { return workspace + "/.pocket"; }
std::string projectConfigPath(const std::string& workspace) {
    return projectDir(workspace) + "/config.json";
}
std::string projectSkillDir(const std::string& workspace) {
    return projectDir(workspace) + "/skills";
}

Config defaultConfig() {
    Config c;
    c.defaultModel = "orcarouter:glm-5.3-flash";
    c.providers = {
        {"orcarouter", "openai", "https://api.orcarouter.ai/v1", "ORCAROUTER_API_KEY"},
        {"openrouter", "openai", "https://openrouter.ai/api/v1", "OPENROUTER_API_KEY"},
        {"deepseek", "openai", "https://api.deepseek.com/v1", "DEEPSEEK_API_KEY"},
        {"friendli", "openai", "https://api.friendli.ai/v1", "FRIENDLI_API_KEY"},
        {"together", "openai", "https://api.together.xyz/v1", "TOGETHER_API_KEY"},
        {"deepinfra", "openai", "https://api.deepinfra.com/v1/openai", "DEEPINFRA_API_KEY"},
        {"anthropic", "anthropic", "https://api.anthropic.com/v1", "ANTHROPIC_API_KEY"},
    };
    c.models = {
        {"glm", "orcarouter", "glm-5.3-flash", "", 200000},
        {"deepseek", "deepseek", "deepseek-chat", "", 128000},
    };
    return c;
}

namespace {

const ProviderCfg* findProvider(const Config& c, const std::string& name) {
    for (const auto& p : c.providers)
        if (p.name == name) return &p;
    return nullptr;
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

    if (v.has("default_model")) {
        if (!o.at("default_model").isStr()) return typeErr("default_model", "a string");
        cfg.defaultModel = o.at("default_model").asStr();
    }
    if (v.has("thinking")) {
        if (!o.at("thinking").isStr()) return typeErr("thinking", "a string");
        std::string t = toLower(o.at("thinking").asStr());
        if (t != "off" && t != "low" && t != "medium" && t != "high" && t != "max")
            return VoidResult::Err("config key \"thinking\" must be one of off/low/medium/high/max");
        cfg.thinking = t;
    }
    if (v.has("providers")) {
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
            if (pc.protocol != "openai" && pc.protocol != "anthropic")
                return VoidResult::Err("provider \"" + kv.first +
                                       "\": protocol must be \"openai\" or \"anthropic\"");
            pc.baseUrl = kv.second.at("base_url").asStr();
            if (pc.baseUrl.empty())
                return VoidResult::Err("provider \"" + kv.first + "\" needs \"base_url\"");
            pc.keyEnv = kv.second.at("key_env").asStr();
            if (pc.keyEnv.empty())
                return VoidResult::Err("provider \"" + kv.first + "\" needs \"key_env\"");
            bool replaced = false;
            for (auto& e : cfg.providers)
                if (e.name == pc.name) {
                    e = pc;
                    replaced = true;
                }
            if (!replaced) cfg.providers.push_back(std::move(pc));
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
            mc.context = kv.second.at("context").asInt(200000);
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
        long t = o.at("bash_timeout").asInt(-1);
        if (t < 1 || t > 3600) return typeErr("bash_timeout", "1..3600 seconds");
        cfg.bashTimeoutSec = (int)t;
    }
    if (secKey("output_limit")) {
        long t = o.at("output_limit").asInt(-1);
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
    return Result<Config>::Ok(std::move(cfg));
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
        rm.context = 200000;
        // Inherit context size from a matching alias if one exists.
        for (const auto& m : cfg.models)
            if (m.provider == pname && m.model == rm.model) {
                rm.context = m.context;
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
            rm.spec = s;
            return Result<ResolvedModel>::Ok(std::move(rm));
        }
    return Result<ResolvedModel>::Err("unknown model \"" + s +
                                      "\" (use provider:model, or a models{} alias)");
}

Result<UiState> loadUiState() {
    UiState st;
    if (access(statePath().c_str(), R_OK) != 0) return Result<UiState>::Ok(st);
    auto t = readFileBounded(statePath(), 65536);
    if (!t.ok) return Result<UiState>::Ok(st);  // corrupt state is not fatal
    auto v = json::parse(t.value);
    if (!v.ok) return Result<UiState>::Ok(st);
    st.lastModel = v.value.at("last_model").asStr();
    st.thinking = v.value.at("thinking").asStr();
    return Result<UiState>::Ok(st);
}

VoidResult saveUiState(const UiState& st) {
    auto r = ensureDir(stateDir());
    if (!r.ok) return r;
    json::Object o;
    o["last_model"] = json::Value(st.lastModel);
    o["thinking"] = json::Value(st.thinking);
    return atomicWriteFile(statePath(), json::stringify(json::Value(o), true) + "\n", 0600);
}

}  // namespace pocket
