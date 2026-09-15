// PocketHarness - configuration: one user config + optional project config.
// Providers are data (wire protocols), not vendor classes.
#pragma once

#include <string>
#include <vector>

#include "common.h"
#include "json.h"

namespace pocket {

// Filesystem locations. Source code lives in ~/src/pocketharness; everything
// below is runtime/config state in hidden dirs.
std::string userConfigDir();    // ~/.config/pocketharness
std::string userConfigPath();   // ~/.config/pocketharness/config.json
std::string stateDir();         // ~/.local/share/pocketharness
std::string sessionDir();       // ~/.local/share/pocketharness/sessions
std::string globalSkillDir();   // ~/.config/pocketharness/skills
std::string userSystemPath();   // ~/.config/pocketharness/system.md (optional override)
std::string projectSystemPath(const std::string& workspace);  // <ws>/.pocket/system.md
std::string projectDir(const std::string& workspace);  // <ws>/.pocket
std::string projectConfigPath(const std::string& workspace);
std::string projectSkillDir(const std::string& workspace);

struct ProviderCfg {
    std::string name;
    std::string protocol = "openai";  // "openai" | "anthropic"
    std::string baseUrl;
    std::string keyEnv;  // env var holding the API key (never the key itself)
};

struct ModelCfg {
    std::string alias;      // short name, e.g. "glm"
    std::string provider;   // provider name
    std::string model;      // wire model id
    std::string routing;    // e.g. "deepinfra" for OpenRouter order ("" = auto)
    long context = 200000;  // context window tokens (guess unless contextSet)
    bool contextSet = false;  // true only when "context" appears in config
};

struct Config {
    std::string defaultModel;  // model spec, e.g. "orcarouter:glm-5.3-flash"
    std::string thinking = "medium";
    std::vector<ProviderCfg> providers;
    std::vector<ModelCfg> models;
    // Security-relevant knobs. Only the USER config may grant authority;
    // project config values for these are ignored (see config.cpp).
    bool toolNetwork = true;  // model tools may use the network unless denied
    int bashTimeoutSec = 120;
    int maxRounds = 100;  // model<->tool rounds per user turn before stopping
    long outputLimitBytes = 262144;  // 256 KiB per tool result
    std::vector<std::string> allowRead;
    std::vector<std::string> allowWrite;
    std::vector<std::string> exposeEnv;
};

struct ResolvedModel {
    ProviderCfg provider;
    std::string model;    // wire model id
    std::string routing;  // "" = automatic
    long context = 200000;
    std::string spec;     // canonical spec string for display/state
};

// Load user config (missing file => built-in defaults) then overlay the
// project config's NON-security keys. Precise errors on invalid JSON.
Result<Config> loadConfig(const std::string& workspace);

// Resolve "alias" | "provider:model" | "provider:model@routing" | "" (default).
Result<ResolvedModel> resolveModel(const Config& cfg, const std::string& spec);

// True when an alias pins an explicit context for this provider+model id
// (explicit config wins over dynamic /models lookup).
bool hasExplicitContext(const Config& cfg, const std::string& provider,
                        const std::string& model);

// Built-in provider defaults used when no user config exists.
Config defaultConfig();

}  // namespace pocket
