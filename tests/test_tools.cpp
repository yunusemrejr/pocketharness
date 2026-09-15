// PocketHarness tests - native tools end to end (contained temp workspace).
#include "mini.h"

#include "../src/config.h"
#include "../src/sandbox.h"
#include "../src/tools.h"

using namespace pocket;
using namespace pocket::test;

namespace {
struct ToolFixture {
    std::string base, ws, tmp;
    Authority auth;
    Config cfg;
    ToolEnv env;
    bool ok = false;
    std::string err;
    ToolFixture() {
        base = makeTempDir("pocket-tools");
        if (base.empty()) {
            err = "tempdir";
            return;
        }
        ws = base + "/ws";
        tmp = base + "/tmp";
        if (!ensureDir(ws, 0755).ok || !ensureDir(tmp + "/home", 0700).ok) {
            err = "mkdir";
            return;
        }
        auto a = authorityInit(ws, {}, {}, false);
        if (!a.ok) {
            err = a.error;
            return;
        }
        auth = a.value;
        cfg = defaultConfig();
        env.auth = &auth;
        env.cfg = &cfg;
        env.workspace = auth.workspace;
        env.sessionTmp = tmp;
        env.sandboxHome = tmp + "/home";
        ok = true;
    }
    ~ToolFixture() {
        if (ok) authorityClose(auth);
        if (!base.empty()) rmRf(base);
    }
};
}  // namespace

TEST(tools_Read_Write_Edit) {
    ToolFixture f;
    CHECK(f.ok);
    ToolResult r = runTool(f.env, "write", R"({"path":"a.txt","content":"one\ntwo\none\n"})");
    CHECK(r.ok);
    r = runTool(f.env, "read", R"({"path":"a.txt"})");
    CHECK(r.ok && r.output.find("1| one") != std::string::npos);
    r = runTool(f.env, "read", R"({"path":"a.txt","offset":2,"limit":1})");
    CHECK(r.ok && r.output.find("two") != std::string::npos &&
          r.output.find("one") == std::string::npos);
    // Ambiguous edit fails without touching the file.
    r = runTool(f.env, "edit", R"({"path":"a.txt","old_text":"one","new_text":"1"})");
    CHECK(!r.ok && r.output.find("2 occurrence") != std::string::npos);
    r = runTool(f.env, "read", R"({"path":"a.txt"})");
    CHECK(r.ok && r.output.find("one") != std::string::npos);
    // Exact edit works; absent target and bad args fail clearly.
    r = runTool(f.env, "edit",
                R"({"path":"a.txt","old_text":"two","new_text":"2","expected_matches":1})");
    CHECK(r.ok);
    r = runTool(f.env, "edit", R"({"path":"missing.txt","old_text":"x","new_text":"y"})");
    CHECK(!r.ok);
    r = runTool(f.env, "write", R"({"path":"sub/deep/f.txt","content":"d"})");
    CHECK(r.ok);  // parents created inside the root
    r = runTool(f.env, "frobnicate", "{}");
    CHECK(!r.ok);
    return "";
}

TEST(tools_Bash_Capture_And_Guard) {
    ToolFixture f;
    CHECK(f.ok);
    ToolResult r = runTool(f.env, "bash", R"({"command":"echo hello; echo oops >&2"})");
    CHECK(r.ok && r.output.find("[exit: 0]") != std::string::npos);
    CHECK(r.output.find("hello") != std::string::npos &&
          r.output.find("oops") != std::string::npos);
    r = runTool(f.env, "bash", R"({"command":"exit 3"})");
    CHECK(!r.ok && r.output.find("[exit: 3]") != std::string::npos);
    // Guard: hard deny.
    r = runTool(f.env, "bash", R"({"command":"rm -rf /"})");
    CHECK(!r.ok && r.output.find("blocked") != std::string::npos);
    // Guard: Ask fails closed in non-interactive mode.
    r = runTool(f.env, "bash", R"({"command":"rm -rf ."})");
    CHECK(!r.ok && r.output.find("approval") != std::string::npos);
    return "";
}

TEST(tools_Bash_Env_Scrubbed) {
    ToolFixture f;
    CHECK(f.ok);
    EnvGuard g("POCKETTEST_HARNESS_KEY", "supersecret-value");
    ToolResult r = runTool(f.env, "bash", R"({"command":"env | sort"})");
    CHECK(r.ok);
    CHECK(r.output.find("supersecret-value") == std::string::npos);
    CHECK(r.output.find("POCKETTEST_HARNESS_KEY") == std::string::npos);
    CHECK(r.output.find("TMPDIR=" + f.tmp) != std::string::npos);
    return "";
}

TEST(tools_Skill_List_Load) {
    std::string home = makeTempDir("pocket-toolskill");
    CHECK(!home.empty());
    HomeGuard hg(home);
    ToolFixture f;
    CHECK(f.ok);
    CHECK(ensureDir(globalSkillDir() + "/demo", 0755).ok);
    CHECK(atomicWriteFile(globalSkillDir() + "/demo/SKILL.md", "# Demo\n\nDo demo.\n", 0644).ok);
    ToolResult r = runTool(f.env, "skill", R"({"action":"list"})");
    CHECK(r.ok && r.output.find("demo") != std::string::npos);
    r = runTool(f.env, "skill", R"({"action":"load","name":"demo"})");
    CHECK(r.ok && r.output.find("Do demo") != std::string::npos);
    r = runTool(f.env, "skill", R"({"action":"load","name":"nope"})");
    CHECK(!r.ok);
    rmRf(home);
    return "";
}
