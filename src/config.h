// PocketHarness - configuration: one user config + optional project config.
// Providers are data (wire protocols), not vendor classes.
#pragma once

#include <map>
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
std::string bundledSkillDir();  // ~/.local/share/pocketharness/skills (make install)
std::string userRolesPath();    // ~/.config/pocketharness/roles.json (/models writes it)
std::string userEnvPath();      // ~/.config/pocketharness/env (KEY=value, 0600)

struct ProviderCfg {
    std::string name;
    std::string protocol = "openai";  // "openai" | "anthropic" | "codex"
    std::string baseUrl;
    std::string keyEnv;  // env var holding the API key (never the key itself)
};

// Protocol differences stay data, not vendor classes. Overrides are explicit.
struct ModelOptions {
    long maxTokens = 8192;
    std::string reasoning = "auto";  // auto|effort|budget|adaptive|none
    std::string tokenParameter = "max_tokens";
    bool streamUsage = true;
    bool promptCache = true;
    // Catalog-derived (OpenRouter): how to honor thinking=off. "" sends
    // nothing, "disable" turns reasoning off, else a mandatory reasoner's
    // lowest effort (such models otherwise default to high/max effort).
    std::string thinkOff;
};

struct ModelCfg {
    std::string alias;      // short name, e.g. "glm"
    std::string provider;   // provider name
    std::string model;      // wire model id
    std::string routing;    // e.g. "deepinfra" for OpenRouter order ("" = auto)
    long context = 200000;  // context window tokens (guess unless contextSet)
    bool contextSet = false;  // true only when "context" appears in config
    ModelOptions options{};
};

struct Config {
    std::string defaultModel;  // model spec, e.g. "orcarouter:glm-5.3-flash"
    std::string thinking = "adaptive";  // per-round level, see Agent::effectiveThinking
    std::vector<ProviderCfg> providers;
    std::vector<ModelCfg> models;
    // Security-relevant knobs. Only the USER config may grant authority;
    // project config values for these are ignored (see config.cpp).
    bool toolNetwork = true;  // model tools may use the network unless denied
    int bashTimeoutSec = 120;
    int maxRounds = 100;  // model<->tool rounds per user turn before stopping
    long outputLimitBytes = 262144;  // 256 KiB per tool result
    // Disk safety for tool commands, in GiB (0 disables that check): a runaway
    // writer must never fill the user's disk.
    int diskBudgetGb = 40;   // free space one bash command may consume
    int diskReserveGb = 10;  // stop writers once free space is below this
    int maxFileGb = 32;      // RLIMIT_FSIZE: largest file a tool process may write
    std::vector<std::string> allowRead;
    std::vector<std::string> allowWrite;
    std::vector<std::string> exposeEnv;
    // Model roles: main (default), fast (summaries, judging), fallback (on
    // provider failure), review (the overseer council). Values are specs.
    std::map<std::string, std::string> roles;
    // Shell hooks run in the same sandbox as the bash tool. Events:
    // post_edit ({file} = changed path), pre_bash ({cmd}), stop (end of turn).
    std::map<std::string, std::vector<std::string>> hooks;
    bool review = true;    // overseer reviews changed work before a turn ends
    bool autonomy = true;  // nudge models that stop early or ask permission
    bool jev = true;       // OpenRouter Jev judge (when OPENROUTER_API_KEY is set)
    long workingContextTokens = 96000;  // soft summary checkpoint; 0 keeps only the model's hard window
    struct LocalLm {       // optional llama.cpp judge, started on demand
        std::string server, model, keyFile;
        int port = 18735, threads = 4, ctx = 4096;
    } localLm;
};

struct ResolvedModel {
    ProviderCfg provider;
    std::string model;    // wire model id
    std::string routing;  // "" = automatic
    long context = 200000;
    std::string spec;     // canonical spec string for display/state
    ModelOptions options{};
    double inPrice = -1, outPrice = -1;  // USD per 1M tokens (catalog), -1 unknown
};

bool validThinking(const std::string& level);

// Load user config (missing file => built-in defaults) then overlay the
// project config's NON-security keys. Precise errors on invalid JSON.
Result<Config> loadConfig(const std::string& workspace);

// Resolve "alias" | "provider:model" | "provider:model@routing" | "" (default).
Result<ResolvedModel> resolveModel(const Config& cfg, const std::string& spec);

// True when an alias pins an explicit context for this provider+model id
// (explicit config wins over dynamic /models lookup).
bool hasExplicitContext(const Config& cfg, const std::string& provider,
                        const std::string& model);

// True for plain-http loopback URLs (local daemons: LM Studio, Ollama,
// llama.cpp). Loopback providers need no API key: nothing secret crosses
// a trust boundary when the server is on this machine.
bool isLoopbackHttp(const std::string& url);

// Built-in provider defaults used when no user config exists.
Config defaultConfig();

// Persist one role -> spec in roles.json (user config dir, 0600).
VoidResult saveRole(const std::string& role, const std::string& spec);
// Credential-free settings for recursive Pocket processes in their fake HOME.
VoidResult stageChildConfig(const Config& cfg, const std::string& childHome);

// Load KEY=value / export KEY=value lines from the user env file into the
// process environment (never overriding). Refused unless owned by the user
// and not group/world accessible. Returns the number of keys loaded.
int loadEnvFile(const std::string& path);

}  // namespace pocket
