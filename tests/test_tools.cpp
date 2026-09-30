// PocketHarness tests - native tools end to end (contained temp workspace).
#include "mini.h"
#include <sys/stat.h>

#include <filesystem>

#include "../src/config.h"
#include "../src/kit_audio.h"
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

TEST(tools_Read_Offset_Past_End_Fails_Without_Scanning) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(runTool(f.env, "write", R"({"path":"small.txt","content":"a\nb\nc\n"})").ok);
    // A line number can never exceed the byte count, so this is unsatisfiable
    // and must be reported without walking the whole scan budget first.
    ToolResult r = runTool(f.env, "read", R"({"path":"small.txt","offset":1000000})");
    CHECK(!r.ok);
    CHECK(r.output.find("past the end") != std::string::npos);
    // A satisfiable offset near the end still works.
    r = runTool(f.env, "read", R"({"path":"small.txt","offset":3,"limit":1})");
    CHECK(r.ok && r.output.find("3| c") != std::string::npos);
    // The last valid line number is still accepted.
    r = runTool(f.env, "read", R"({"path":"small.txt","offset":3,"limit":5})");
    CHECK(r.ok && r.output.find("3| c") != std::string::npos);
    return "";
}

TEST(tools_Read_Oversize_Image_Reports_Size_Not_A_Vague_Error) {
    ToolFixture f;
    CHECK(f.ok);
    // A PNG signature followed by padding past the 5 MiB image cap.
    std::string png = "\x89PNG\r\n\x1a\n" + std::string((5u << 20) + 4096, '\0');
    CHECK(atomicWriteFile(f.ws + "/big.png", png, 0644).ok);
    ToolResult r = runTool(f.env, "read", R"({"path":"big.png"})");
    CHECK(!r.ok);
    CHECK(r.output.find("KiB, over the 5 MiB limit") != std::string::npos);
    CHECK(r.output.find("unreadable") == std::string::npos);
    return "";
}

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

TEST(tools_Private_Scratch_Uses_The_Same_Path_Across_Native_Tools) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(authorityAddWriteRoot(f.auth, f.tmp).ok);
    CHECK(runTool(f.env, "write", R"({"path":"$TMPDIR/parts/code.js","content":"one"})").ok);
    CHECK(readFileBounded(f.tmp + "/parts/code.js", 100).value == "one");
    CHECK(runTool(f.env, "edit", R"({"path":"$TMPDIR/parts/code.js","old_text":"one","new_text":"two"})").ok);
    auto read = runTool(f.env, "read", R"({"path":"$TMPDIR/parts/code.js"})");
    CHECK(read.ok && read.output.find("two") != std::string::npos);
    CHECK(f.env.changedFiles.empty());
    CHECK(!runTool(f.env, "write", R"({"path":"$TMPDIR/../other-session.txt","content":"no"})").ok);
    CHECK(access((f.base + "/other-session.txt").c_str(), F_OK) != 0);
    CHECK(ensureDir(f.base + "/peer", 0700).ok);
    CHECK(symlink((f.base + "/peer").c_str(), (f.tmp + "/escape").c_str()) == 0);
    CHECK(!runTool(f.env, "write", R"({"path":"$TMPDIR/escape/no.txt","content":"no"})").ok);
    return "";
}

TEST(tools_Private_Scratch_Expands_Every_Tmpdir_Spelling) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(authorityAddWriteRoot(f.auth, f.tmp).ok);
    // Models write ${TMPDIR} and a bare $TMPDIR as readily as $TMPDIR/x. An
    // unexpanded form resolves inside the workspace and leaves a literal
    // "$TMPDIR" directory in the user's repository.
    CHECK(runTool(f.env, "write", R"({"path":"${TMPDIR}/braced.txt","content":"b"})").ok);
    CHECK(readFileBounded(f.tmp + "/braced.txt", 100).value == "b");
    CHECK(runTool(f.env, "write", R"({"path":"$TMPDIR/plain.txt","content":"p"})").ok);
    CHECK(readFileBounded(f.tmp + "/plain.txt", 100).value == "p");
    // A bare $TMPDIR resolves to the scratch root, which is a directory: the
    // write is refused instead of creating a file literally named "$TMPDIR".
    CHECK(!runTool(f.env, "write", R"({"path":"$TMPDIR","content":"root"})").ok);
    CHECK(access((f.base + "/$TMPDIR").c_str(), F_OK) != 0);
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

TEST(tools_Bash_Caps_File_Size) {
    ToolFixture f;
    CHECK(f.ok);
    f.cfg.maxFileGb = 2;  // ulimit -f reports 1 KiB blocks
    ToolResult r = runTool(f.env, "bash", R"({"command":"ulimit -f"})");
    CHECK(r.ok && r.output.find("2097152") != std::string::npos);
    f.cfg.maxFileGb = 0;
    r = runTool(f.env, "bash", R"({"command":"ulimit -f"})");
    CHECK(r.ok && r.output.find("unlimited") != std::string::npos);
    return "";
}

TEST(tools_Staged_Native_Audio_Runs_Without_Network) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(!f.env.allowNet && !f.env.unsafe);
    f.cfg.toolNetwork = false;
    CHECK(ensureDir(f.tmp + "/bin", 0700).ok);
    std::error_code error;
    CHECK(std::filesystem::copy_file("./pocket", f.tmp + "/bin/pocket", error));
    CHECK(!error && chmod((f.tmp + "/bin/pocket").c_str(), 0700) == 0);
    const auto result = runTool(f.env, "bash", R"({"command":"pocket kit sfx cue.wav chime --freq 660 --duration .1 --gain .2 && pocket kit audio cue.wav","timeout":30})");
    CHECK(result.ok && result.output.find("PCM16") != std::string::npos);
    const auto file = readFileBounded(f.ws + "/cue.wav", 1 << 20);
    CHECK(file.ok);
    const auto audio = analyzeWav(file.value);
    CHECK(audio.ok && audio.value.frames == 4410 && audio.value.rate == 44100 && audio.value.channels == 1);
    CHECK(audio.value.peak > .001 && audio.value.clippedSamples == 0);
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

TEST(tools_Batched_Edit_Is_Atomic) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(boxWrite(f.auth, "a", "alpha beta").ok);
    auto r = runTool(f.env, "edit", R"({"path":"a","edits":[{"old_text":"alpha","new_text":"A"},{"old_text":"missing","new_text":"B"}]})");
    CHECK(!r.ok);
    CHECK_EQ(boxRead(f.auth, "a", 100).value, std::string("alpha beta"));
    r = runTool(f.env, "edit", R"({"path":"a","edits":[{"old_text":"alpha","new_text":"A"},{"old_text":"beta","new_text":"B"}]})");
    CHECK(r.ok);
    CHECK_EQ(boxRead(f.auth, "a", 100).value, std::string("A B"));
    // Mixed forms: the top-level pair is one more step, even beside edits:[].
    r = runTool(f.env, "edit", R"({"path":"a","edits":[{"old_text":"A","new_text":"a"}],"old_text":"B","new_text":"b"})");
    CHECK(r.ok);
    r = runTool(f.env, "edit", R"({"path":"a","edits":[],"old_text":"a b","new_text":"A B","expected_matches":1})");
    CHECK(r.ok);
    CHECK_EQ(boxRead(f.auth, "a", 100).value, std::string("A B"));
    // A wrong type must not silently become an empty replacement or empty file.
    CHECK(!runTool(f.env, "write", R"({"path":"a","content":null})").ok);
    CHECK(!runTool(f.env, "edit", R"({"path":"a","old_text":"A","new_text":false})").ok);
    CHECK_EQ(boxRead(f.auth, "a", 100).value, std::string("A B"));
    return "";
}

TEST(tools_Large_File_Ranges_And_Edit_Limit) {
    ToolFixture f;
    CHECK(f.ok);
    std::string large;
    for (int i = 0; i < 50000; ++i) large += std::string(100, 'x') + "\n";
    large += "TAIL\n";
    CHECK(boxWrite(f.auth, "large", large).ok);
    auto r = runTool(f.env, "read", R"({"path":"large","offset":50001,"limit":1})");
    CHECK(r.ok && r.output.find("50001| TAIL") != std::string::npos);
    CHECK(r.output.size() < 100);
    r = runTool(f.env, "edit", R"({"path":"large","old_text":"xxx","new_text":"X"})");
    CHECK(!r.ok);
    CHECK_EQ(readFileBounded(f.ws + "/large", 8 << 20).value, large);
    CHECK(mkfifo((f.ws + "/fifo").c_str(), 0600) == 0);
    CHECK(!runTool(f.env, "read", R"({"path":"fifo"})").ok);  // must not block
    return "";
}

TEST(tools_Pipelines_And_Command_Validation) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(!runTool(f.env, "bash", R"({"command":"false | cat"})").ok);
    CHECK(!runTool(f.env, "bash", R"({"command":"echo OK\u0000; echo HIDDEN"})").ok);
    // Lenient intake: a fractional timeout falls back to the default, unknown
    // keys are ignored, aliases and numeric strings are accepted.
    CHECK(runTool(f.env, "bash", R"({"command":"echo OK","timeout":1.2})").ok);
    CHECK(runTool(f.env, "bash", R"({"cmd":"echo OK","timeout":"30","why":"x"})").ok);
    CHECK(runTool(f.env, "write", R"({"file_path":"lenient.txt","content":"a b"})").ok);
    CHECK(runTool(f.env, "edit", R"({"path":"lenient.txt","expected_matches":0,)"
                                 R"("edits":[{"old_string":"a","new_string":"c","expected_matches":"1"}]})").ok);
    CHECK(runTool(f.env, "read", R"({"path":"lenient.txt","limti":4})").output.find("c b") != std::string::npos);
    CHECK(!runTool(f.env, "bash", R"({"command":["echo"]})").ok);
    // The shell is not visible in argv: pkill -f must not kill itself.
    auto self = runTool(f.env, "bash", R"({"command":"pkill -f zzq_no_such_proc_marker; echo alive"})");
    CHECK(self.output.find("alive") != std::string::npos);
    for (const char* command : {"/bin/rm -rf /", "/sbin/mkfs.ext4 /dev/sda", "/usr/bin/curl https://x"})
        CHECK(classifyCommand(command, f.ws, false).verdict == Verdict::Deny);
    return "";
}

TEST(tools_Undo_Protects_Other_Writers_And_Failed_Writes) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(runTool(f.env, "write", R"({"path":"a","content":"ours"})").ok);
    CHECK(boxWrite(f.auth, "a", "theirs").ok);
    CHECK(undoLast(f.env).find("changed since") != std::string::npos);
    CHECK_EQ(boxRead(f.auth, "a", 100).value, std::string("theirs"));
    CHECK_EQ(f.env.undo.size(), size_t(1));
    CHECK(boxWrite(f.auth, "a", "ours").ok);
    CHECK(ensureDir(f.ws + "/directory", 0700).ok);
    CHECK(!runTool(f.env, "write", R"({"path":"directory","content":"bad"})").ok);
    CHECK_EQ(f.env.undo.size(), size_t(1));
    CHECK(undoLast(f.env).find("removed a") != std::string::npos);
    CHECK(!boxExists(f.auth, "a").value);
    return "";
}

TEST(tools_Undo_Rejects_Replaced_Parent_Symlink) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(runTool(f.env, "write", R"({"path":"sub/a","content":"same"})").ok);
    CHECK(rename((f.ws + "/sub").c_str(), (f.ws + "/old").c_str()) == 0);
    CHECK(atomicWriteFile(f.tmp + "/a", "same").ok);
    CHECK(symlink(f.tmp.c_str(), (f.ws + "/sub").c_str()) == 0);
    CHECK(undoLast(f.env).find("undo:") == 0);
    CHECK_EQ(readFileBounded(f.tmp + "/a", 100).value, std::string("same"));
    return "";
}

TEST(tools_Hook_Substitution_Is_Single_Pass_And_Quoted) {
    ToolFixture f;
    CHECK(f.ok);
    auto r = runHook(f.env, "printf '%s' {file}", "file", "{file} $(touch BAD) 'quote'");
    CHECK(r.ok);
    CHECK(r.output.find("{file} $(touch BAD) 'quote'") != std::string::npos);
    CHECK(!boxExists(f.auth, "BAD").value);
    return "";
}

TEST(tools_Empty_File_Is_Readable_And_Private_Modes_Persist) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(boxWrite(f.auth, "private", "", 0600).ok);
    CHECK(runTool(f.env, "read", R"({"path":"private"})").ok);
    CHECK(runTool(f.env, "write", R"({"path":"private","content":"secret"})").ok);
    struct stat st{};
    CHECK(stat((f.ws + "/private").c_str(), &st) == 0);
    CHECK_EQ(st.st_mode & 0777, mode_t(0600));
    CHECK(boxWrite(f.auth, "script", "old", 0700).ok);
    CHECK(runTool(f.env, "edit", R"({"path":"script","old_text":"old","new_text":"new"})").ok);
    CHECK(stat((f.ws + "/script").c_str(), &st) == 0);
    CHECK_EQ(st.st_mode & 0777, mode_t(0700));
    return "";
}

TEST(tools_Child_Usage_Is_Cumulative_And_Not_Double_Counted) {
    ToolFixture f;
    CHECK(f.ok);
    std::string path = f.tmp + "/pocket-child-one.json";
    CHECK(atomicWriteFile(path, R"({"cost":0.25,"side_cost":0.05,"children":2,"estimated":true,"seen":true})").ok);
    auto a = collectChildUsage(f.env);
    CHECK_EQ(a.cost, .25);
    CHECK_EQ(a.count, 3L);
    CHECK(a.estimated && a.seen);
    auto b = collectChildUsage(f.env);
    CHECK_EQ(b.cost, 0.0);
    CHECK_EQ(b.count, 0L);
    CHECK(atomicWriteFile(path, R"({"cost":0.5,"side_cost":0.1,"children":2,"estimated":true,"seen":true})").ok);
    auto c = collectChildUsage(f.env);
    CHECK_EQ(c.cost, .25);
    CHECK_EQ(c.count, 0L);
    return "";
}

TEST(tools_Child_Receipt_FIFO_Is_Refused_Without_Blocking) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(mkfifo((f.tmp + "/pocket-child-fifo.json").c_str(), 0600) == 0);
    CHECK_EQ(collectChildUsage(f.env).count, 0L);
    return "";
}

TEST(tools_ReadOnly_Blocks_Mutations_Allows_Evidence) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(boxWrite(f.auth, "note.txt", "hello", 0644).ok);
    f.env.readOnly = true;
    // Mutations refused at the enforcement point, files untouched.
    CHECK(!runTool(f.env, "write", R"({"path":"evil.txt","content":"x"})").ok);
    CHECK(!runTool(f.env, "edit", R"({"path":"note.txt","old_text":"hello","new_text":"bye"})").ok);
    CHECK(!runTool(f.env, "skill", R"({"action":"list"})").ok);
    CHECK(!runTool(f.env, "bash", R"({"command":"rm -f note.txt"})").ok);
    CHECK(!runTool(f.env, "bash", R"({"command":"echo x > note.txt"})").ok);
    CHECK(!runTool(f.env, "bash", R"({"command":"ls | head"})").ok);
    CHECK(!boxExists(f.auth, "evil.txt").value);
    CHECK_EQ(boxRead(f.auth, "note.txt", 100).value, std::string("hello"));
    // Evidence surface works: read plus proven-read-only bash.
    auto rd = runTool(f.env, "read", R"({"path":"note.txt"})");
    CHECK(rd.ok && rd.output.find("hello") != std::string::npos);
    auto ls = runTool(f.env, "bash", R"({"command":"ls"})");
    CHECK(ls.ok && ls.output.find("note.txt") != std::string::npos);
    auto defs = readOnlyToolDefs();
    CHECK_EQ(defs.size(), (size_t)2);
    CHECK(defs[0].name == "read" && defs[1].name == "bash");
    return "";
}

TEST(tools_Edit_Tolerates_Whitespace_Drift_And_Pipe_Close) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(runTool(f.env, "write", R"({"path":"w.c","content":"int f() {\n    if (x) {\n        y();\n    }\n}\n"})").ok);
    // Indentation lost and trailing space added: still one unique block.
    ToolResult r = runTool(f.env, "edit", R"({"path":"w.c","old_text":"if (x) {\n    y(); \n}","new_text":"if (x) {\n    z();\n}"})");
    CHECK(r.ok && r.output.find("ignoring whitespace") != std::string::npos);
    CHECK_EQ(readFileBounded(f.auth.workspace + "/w.c", 1000).value,
             std::string("int f() {\n    if (x) {\n        z();\n    }\n}\n"));
    r = runTool(f.env, "edit", R"({"path":"w.c","old_text":"if (x) {\n  q();","new_text":"no"})");
    CHECK(!r.ok && r.output.find("at line 2") != std::string::npos);
    // A closed pipe retains exit evidence; without stage statuses it is not certified as success.
    r = runTool(f.env, "bash", R"({"command":"yes | head -n 2"})");
    CHECK(!r.ok && r.output.find("exit 141") != std::string::npos);
    return "";
}

TEST(tools_UI_Files_Demand_The_Design_Doctrine_Until_Loaded) {
    CHECK(looksLikeUiWork("Build a landing page for my bakery"));
    CHECK(looksLikeUiWork("redesign the settings screen UI"));
    CHECK(!looksLikeUiWork("fix the parser off-by-one"));
    ToolFixture f;
    CHECK(f.ok);
    ToolResult r = runTool(f.env, "write", R"({"path":"a.css","content":"body{margin:0}"})");
    CHECK(r.ok && r.output.find(kUiDocSkill) != std::string::npos);
    r = runTool(f.env, "write", R"({"path":"notes.txt","content":"hi"})");
    CHECK(r.ok && r.output.find(kUiDocSkill) == std::string::npos);
    f.env.uiDocLoaded = true;
    r = runTool(f.env, "write", R"({"path":"b.css","content":"body{margin:0}"})");
    CHECK(r.ok && r.output.find(kUiDocSkill) == std::string::npos);
    return "";
}

TEST(tools_Failed_Edit_Shows_Where_To_Look) {
    ToolFixture f;
    CHECK(f.ok);
    CHECK(boxWrite(f.auth, "c", "int a;\nint main() {\n  return compute(1);\n}\nint z;\nint z;\n").ok);
    auto r = runTool(f.env, "edit", R"({"path":"c","old_text":"int z;","new_text":"int y;"})");
    CHECK(!r.ok);
    CHECK(r.output.find("lines 5, 6") != std::string::npos);
    r = runTool(f.env, "edit", R"({"path":"c","old_text":"  return compute(2);","new_text":"x"})");
    CHECK(!r.ok);
    CHECK(r.output.find("3|   return compute(1);") != std::string::npos);  // closest text, no re-read needed
    return "";
}

TEST(tools_Rendering_A_Video_Waits_Once_For_The_Video_Doctrine) {
    CHECK(looksLikeVideoWork("make a YouTube explainer about backups"));
    CHECK(looksLikeVideoWork("render an mp4 with voiceover"));
    CHECK(!looksLikeVideoWork("fix the parser off-by-one"));
    ToolFixture f;
    CHECK(f.ok);
    ToolResult r = runTool(f.env, "bash", R"({"command":"echo kit video scene.html out.mp4"})");
    CHECK(r.output.find(kVideoDocSkill) != std::string::npos && r.output.find("this stop happens once") != std::string::npos);
    CHECK(f.env.videoNagged);
    r = runTool(f.env, "bash", R"({"command":"echo kit video scene.html out.mp4"})");
    CHECK(r.ok && r.output.find("this stop happens once") == std::string::npos);  // only once
    ToolFixture g;
    CHECK(g.ok);
    g.env.videoDocLoaded = true;
    r = runTool(g.env, "bash", R"({"command":"echo kit video x"})");
    CHECK(r.ok && r.output.find("this stop happens once") == std::string::npos);
    return "";
}

TEST(tools_Write_Reports_Lint_Findings_Without_Being_Asked) {
    ToolFixture f;
    CHECK(f.ok);
    ToolResult r = runTool(f.env, "write", R"({"path":"api.py","content":"import requests\ndef go(u, acc=[]):\n    return requests.get(u)\n"})");
    CHECK(r.ok && r.output.find("[quality]") != std::string::npos && r.output.find("mutable default") != std::string::npos &&
          r.output.find("without timeout") != std::string::npos);
    r = runTool(f.env, "write", R"({"path":"ok.py","content":"def go(u):\n    return u\n"})");
    CHECK(r.ok && r.output.find("[quality]") == std::string::npos);
    return "";
}

TEST(tools_Loaded_Skill_Names_Are_Canonical_And_Deduplicated) {
    std::string home = makeTempDir("pocket-skill-loaded");
    HomeGuard isolated(home);
    CHECK(ensureDir(bundledSkillDir() + "/ai-design-slop", 0700).ok);
    CHECK(atomicWriteFile(bundledSkillDir() + "/ai-design-slop/SKILL.md", "# Design\n\nPreserve the project identity.").ok);
    ToolEnv env;
    env.workspace = home;
    CHECK(runTool(env, "skill", R"({"action":"load","name":" ai-design-slop "})").ok);
    CHECK(env.uiDocLoaded);
    CHECK(runTool(env, "skill", R"({"action":"load","name":"ai-design-slop"})").ok);
    CHECK_EQ(env.loadedSkills.size(), size_t(1));
    CHECK_EQ(env.loadedSkills.front(), std::string("ai-design-slop"));
    rmRf(home);
    return "";
}

TEST(tools_No_Op_Edit_Skips_Undo_Checks_And_Hooks) {
    ToolFixture f;
    CHECK(f.ok);
    f.cfg.jev = false;
    f.cfg.hooks["post_edit"] = {"echo ran >> edits.log"};
    auto r = runTool(f.env, "write", R"({"path":"same.py","content":"def sum(a, b): return a + b"})");
    CHECK(r.ok && r.changed);
    CHECK(chmod((f.ws + "/same.py").c_str(), 0700) == 0);
    struct stat prior{}, after{};
    CHECK(stat((f.ws + "/same.py").c_str(), &prior) == 0);
    f.env.changedFiles.clear();
    size_t undo = f.env.undo.size();
    r = runTool(f.env, "write", R"({"path":"same.py","content":"def sum(a, b): return a + b"})");
    CHECK(r.ok && !r.changed && r.output == "unchanged same.py");
    r = runTool(f.env, "edit", R"({"path":"same.py","old_text":"a + b","new_text":"a + b"})");
    CHECK(r.ok && !r.changed);
    CHECK(stat((f.ws + "/same.py").c_str(), &after) == 0);
    CHECK(prior.st_ino == after.st_ino && prior.st_mode == after.st_mode &&
          prior.st_mtim.tv_sec == after.st_mtim.tv_sec && prior.st_mtim.tv_nsec == after.st_mtim.tv_nsec);
    CHECK(f.env.changedFiles.empty() && f.env.undo.size() == undo);
    CHECK_EQ(readFileBounded(f.ws + "/edits.log", 100).value, std::string("ran\n"));
    CHECK(symlink("same.py", (f.ws + "/alias.py").c_str()) == 0);
    CHECK(!runTool(f.env, "write", R"({"path":"alias.py","content":"def sum(a, b): return a + b"})").ok);
    CHECK(ensureDir(f.base + "/readonly", 0700).ok);
    CHECK(atomicWriteFile(f.base + "/readonly/same", "same").ok);
    CHECK(authorityAddReadRoot(f.auth, f.base + "/readonly").ok);
    CHECK(!runTool(f.env, "write", json::stringify(json::Object{{"path", f.base + "/readonly/same"}, {"content", "same"}})).ok);
    return "";
}

TEST(tools_Later_Edit_Reports_New_Medium_Findings_Only) {
    ToolFixture f;
    CHECK(f.ok);
    f.cfg.jev = false;
    CHECK(runTool(f.env, "write", R"({"path":"logic.py","content":"def go(items):\n    return items\n"})").ok);
    auto r = runTool(f.env, "edit", R"({"path":"logic.py","old_text":"def go(items):","new_text":"def go(items=[]):"})");
    CHECK(r.ok && r.changed && r.output.find("mutable default") != std::string::npos);
    r = runTool(f.env, "edit", R"json({"path":"logic.py","old_text":"return items","new_text":"return list(items)"})json");
    CHECK(r.ok && r.changed && r.output.find("mutable default") == std::string::npos);
    return "";
}

TEST(tools_Semantic_Check_Reassesses_Material_Revision_With_Budget) {
    ToolFixture f;
    CHECK(f.ok);
    f.cfg.jev = false;
    std::string first(500, 'a'), second(500, 'b');
    auto write = [&](const std::string& text) {
        return runTool(f.env, "write", json::stringify(json::Object{{"path", "guide.md"}, {"content", text}}));
    };
    CHECK(write("draft").ok);
    CHECK(f.env.semanticChecks.empty());
    CHECK(write(first).ok);
    CHECK_EQ(f.env.semanticChecks["guide.md"].calls, 1);
    CHECK(write(first + " ").ok);
    CHECK_EQ(f.env.semanticChecks["guide.md"].calls, 1);
    CHECK(write(second).ok);
    CHECK_EQ(f.env.semanticChecks["guide.md"].calls, 2);
    CHECK(write(first).ok);
    CHECK_EQ(f.env.semanticChecks["guide.md"].calls, 2);
    return "";
}

TEST(tools_Post_Bash_Receives_Quoted_Outcome_Without_Recursive_Expansion) {
    ToolFixture f;
    CHECK(f.ok);
    f.cfg.hooks["post_bash"] = {"printf '%s\\n' {cmd} {ok} {result} > hook.txt"};
    std::string cmd = "printf '%s\\n' 'literal {ok} $(touch injected)'";
    auto r = runTool(f.env, "bash", json::stringify(json::Object{{"command", cmd}}));
    CHECK(r.ok);
    auto hook = readFileBounded(f.ws + "/hook.txt", 10000);
    CHECK(hook.ok && hook.value.find(cmd) == 0);
    CHECK(hook.value.find("literal {ok} $(touch injected)") != std::string::npos);
    CHECK(!boxExists(f.auth, "injected").value);
    f.cfg.hooks["post_bash"] = {"false"};
    r = runTool(f.env, "bash", R"({"command":"printf success"})");
    CHECK(!r.ok && r.output.find("[exit: 0]") != std::string::npos &&
          r.output.find("[hook post_bash failed]") != std::string::npos);
    f.cfg.hooks["post_bash"].clear();
    r = runTool(f.env, "bash", R"({"command":"bash -c 'exit 141' | cat"})");
    CHECK(!r.ok);
    return "";
}
