// PocketHarness tests - config precedence + model resolution.
#include "mini.h"

#include "../src/config.h"

using namespace pocket;
using namespace pocket::test;

TEST(config_ResolveModel) {
    Config c = defaultConfig();
    auto r = resolveModel(c, "glm");
    CHECK(r.ok && r.value.model == "glm-5.3-flash");
    CHECK_EQ(r.value.provider.name, std::string("orcarouter"));
    r = resolveModel(c, "openrouter:hy4-preview@deepinfra");
    CHECK(r.ok && r.value.model == "hy4-preview" && r.value.routing == "deepinfra");
    r = resolveModel(c, "deepseek:deepseek-flash");
    CHECK(r.ok && r.value.routing.empty());
    r = resolveModel(c, "");
    CHECK(r.ok && r.value.model == "glm-5.3-flash");  // default
    r = resolveModel(c, "nope:x");
    CHECK(!r.ok);
    r = resolveModel(c, "nosuchalias");
    CHECK(!r.ok);
    return "";
}

TEST(config_Project_Cannot_Escalate) {
    std::string home = makeTempDir("pocket-cfg");
    CHECK(!home.empty());
    HomeGuard hg(home);
    std::string ws = home + "/ws";
    CHECK(ensureDir(ws + "/.pocket", 0755).ok);
    // User config grants nothing; project config tries to escalate.
    CHECK(ensureDir(userConfigDir(), 0755).ok);
    CHECK(atomicWriteFile(userConfigPath(), R"({"default_model":"glm"})", 0644).ok);
    CHECK(atomicWriteFile(projectConfigPath(ws),
                          R"({"tool_network":true,"allow_read":["/"],"allow_write":["/"],)"
                          R"("expose_env":["EVIL"],"default_model":"deepseek"})",
                          0644)
              .ok);
    auto c = loadConfig(ws);
    CHECK(c.ok);
    CHECK(!c.value.toolNetwork);
    CHECK(c.value.allowRead.empty());
    CHECK(c.value.allowWrite.empty());
    CHECK(c.value.exposeEnv.empty());
    CHECK_EQ(c.value.defaultModel, std::string("deepseek"));  // non-security key applies
    // User config security keys DO apply.
    CHECK(atomicWriteFile(userConfigPath(),
                          R"({"tool_network":true,"allow_read":["/tmp"],"expose_env":["FOO"]})",
                          0644)
              .ok);
    c = loadConfig(ws);
    CHECK(c.ok && c.value.toolNetwork);
    CHECK_EQ(c.value.allowRead.size(), (size_t)1);
    CHECK_EQ(c.value.exposeEnv.size(), (size_t)1);
    rmRf(home);
    return "";
}

TEST(config_Invalid) {
    std::string home = makeTempDir("pocket-cfgbad");
    CHECK(!home.empty());
    HomeGuard hg(home);
    CHECK(ensureDir(userConfigDir(), 0755).ok);
    CHECK(atomicWriteFile(userConfigPath(), "{not json", 0644).ok);
    auto c = loadConfig(home);
    CHECK(!c.ok);
    CHECK(atomicWriteFile(userConfigPath(), R"({"thinking":"ludicrous"})", 0644).ok);
    c = loadConfig(home);
    CHECK(!c.ok);
    CHECK(atomicWriteFile(userConfigPath(), R"({"models":{"x":{"provider":"zz","model":"m"}}})",
                          0644)
              .ok);
    c = loadConfig(home);
    CHECK(!c.ok);  // unknown provider reference
    rmRf(home);
    return "";
}

TEST(config_UiState_Roundtrip) {
    std::string home = makeTempDir("pocket-state");
    CHECK(!home.empty());
    HomeGuard hg(home);
    CHECK(saveUiState(UiState{"a:b@c", "high"}).ok);
    auto s = loadUiState();
    CHECK(s.ok);
    CHECK_EQ(s.value.lastModel, std::string("a:b@c"));
    CHECK_EQ(s.value.thinking, std::string("high"));
    rmRf(home);
    return "";
}
