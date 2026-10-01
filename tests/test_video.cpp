// Native renderer integration, with dependency-free CDP and encoder processes.
#include "mini.h"
#include "../src/common.h"
#include "../src/json.h"
#include "../src/kit_video.h"
#include "../src/kit_media.h"
#include "../src/process.h"

#include <algorithm>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <filesystem>

using namespace pocket;
using namespace pocket::test;

namespace {
std::string fixtureMode() {
    const char* mode = getenv("POCKET_VIDEO_FIXTURE");
    return mode ? mode : "ok";
}
bool writeAll(int fd, std::string_view data) {
    while (!data.empty()) {
        ssize_t n = write(fd, data.data(), data.size());
        if (n > 0) data.remove_prefix((size_t)n);
        else if (n < 0 && errno == EINTR) continue;
        else return false;
    }
    return true;
}
void record(const std::string& message) {
    const char* file = getenv("POCKET_VIDEO_LOG");
    if (!file) return;
    int fd = open(file, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (fd >= 0) { writeAll(fd, message + "\n"); close(fd); }
}
std::string fixturePng(unsigned width, unsigned height) {
    // Real grayscale PNG: uncompressed DEFLATE keeps the fixture independent
    // of zlib and gives the encoder structurally and pixel-valid input.
    auto be32 = [](std::string& out, uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) out += (char)(value >> shift);
    };
    std::string png("\x89PNG\r\n\x1a\n", 8);
    auto chunk = [&](const char* type, const std::string& data) {
        be32(png, (uint32_t)data.size());
        const size_t start = png.size();
        png.append(type, 4); png += data;
        uint32_t crc = 0xffffffff;
        for (size_t i = start; i < png.size(); ++i) {
            crc ^= (unsigned char)png[i];
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
        }
        be32(png, ~crc);
    };
    std::string header;
    be32(header, width); be32(header, height);
    header += (char)8; header.append(4, '\0');
    chunk("IHDR", header);
    const size_t rawBytes = (size_t)height * (width + 1);
    std::string compressed("\x78\x01", 2);
    size_t remaining = rawBytes;
    do {
        uint16_t length = (uint16_t)std::min<size_t>(remaining, 65535);
        compressed += remaining <= 65535 ? '\x01' : '\0';
        compressed += (char)length; compressed += (char)(length >> 8);
        compressed += (char)~length; compressed += (char)(~length >> 8);
        compressed.append(length, '\0');
        remaining -= length;
    } while (remaining);
    be32(compressed, ((uint32_t)(rawBytes % 65521) << 16) | 1);  // Adler-32 of zero bytes.
    chunk("IDAT", compressed); chunk("IEND", "");
    return png;
}

int browserFixture() {
    record("pid " + std::to_string(getpid()));
    const std::string mode = fixtureMode();
    unsigned width = 0, height = 0;
    std::string input;
    for (;;) {
        size_t end = input.find('\0');
        if (end == std::string::npos) {
            char bytes[4096];
            ssize_t n = read(3, bytes, sizeof bytes);
            if (n <= 0) return 0;
            input.append(bytes, (size_t)n);
            if (input.size() > (1u << 20)) return 10;
            continue;
        }
        auto request = json::parse(input.substr(0, end));
        input.erase(0, end + 1);
        if (!request.ok) return 11;
        const auto& params = request.value.at("params");
        const std::string method = request.value.at("method").asStr();
        record(method + " " + json::stringify(params));
        json::Object reply{{"id", request.value.at("id")}, {"result", json::Object{}}};
        if (method == "Target.createTarget") reply["result"] = json::Object{{"targetId", "page-1"}};
        else if (method == "Target.attachToTarget") reply["result"] = json::Object{{"sessionId", "session-1"}};
        else if (method == "Emulation.setDeviceMetricsOverride") {
            width = (unsigned)params.at("width").asInt();
            height = (unsigned)params.at("height").asInt();
        } else if (method == "Page.navigate" && mode == "navigation_error")
            reply["result"] = json::Object{{"errorText", "ERR_FILE_NOT_FOUND"}};
        else if (method == "Runtime.evaluate") {
            const std::string expression = params.at("expression").asStr();
            if (mode == "js_error" && expression.find("renderReady") != std::string::npos)
                reply["result"] = json::Object{{"exceptionDetails", json::Object{{"text", "Uncaught"},
                    {"exception", json::Object{{"description", "fixture rendering failure"}}}}}};
            else if (expression.find("JSON.stringify(out)") != std::string::npos)  // the scene lint asks for a JSON list
                reply["result"] = json::Object{{"result", json::Object{{"type", "string"}, {"value", "[]"}}}};
            else reply["result"] = json::Object{{"result", json::Object{{"type", "boolean"}, {"value", true}}}};
        } else if (method == "Page.captureScreenshot") {
            if (mode == "timeout" || mode == "cancel") {
                if (mode == "cancel") kill(getppid(), SIGTERM);
                for (;;) pause();
            }
            if (mode == "browser_exit") return 9;
            if (mode == "protocol_error") {
                reply.erase("result"); reply["error"] = json::Object{{"message", "fixture screenshot rejected"}};
            } else {
                std::string png = fixturePng(width + (mode == "wrong_size" ? 1 : 0), height);
                if (mode == "truncated_png") png.resize(24);
                if (mode == "not_png") png.assign(33, 'x');
                std::string data = mode == "bad_base64" ? "!!!!" : base64Encode(png);
                reply["result"] = json::Object{{"data", data}};
            }
        }
        // Fragment every response and interleave an unsolicited event. CDP
        // dispatch must preserve its request ID and framing across reads.
        std::string wire = json::stringify(json::Object{{"method", "Page.lifecycleEvent"}});
        wire += '\0'; wire += json::stringify(reply); wire += '\0';
        if (!writeAll(4, std::string_view(wire).substr(0, 3)) || !writeAll(4, std::string_view(wire).substr(3))) return 0;
    }
}
int encoderFixture(int argc, char** argv) {
    record("pid " + std::to_string(getpid()));
    for (int i = 1; i < argc; ++i) record("encoder " + std::string(argv[i]));
    std::string input;
    char bytes[4096];
    for (;;) {
        ssize_t n = read(STDIN_FILENO, bytes, sizeof bytes);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        input.append(bytes, (size_t)n);
        if (input.size() > (1u << 20)) return 12;
    }
    record("encoded bytes " + std::to_string(input.size()));
    if (fixtureMode() == "encoder_empty") return 0;
    if (fixtureMode() == "encoder_fail") {
        writeAll(5, "partial");
        writeAll(STDERR_FILENO, "fixture encoder failure\n");
        return 7;
    }
    if (input.empty()) return 13;
    // Plausible top-level boxes; real codec decoding is tested separately.
    std::string mp4("\0\0\0\x10" "ftypisom\0\0\0\0", 16);
    mp4.append("\0\0\0\x09" "moovx", 9); mp4.append("\0\0\0\x09" "mdatx", 9);
    if (fixtureMode() == "encoder_junk") mp4.assign(34, 'x');
    if (!writeAll(5, mp4)) return 14;
    if (fixtureMode() == "cancel_after_encode") kill(getppid(), SIGTERM);
    return 0;
}
struct VideoScratch {
    std::string path = makeTempDir("pocket-video");
    ~VideoScratch() { if (!path.empty()) rmRf(path); }
    bool setup(bool chrome = true, bool encoder = true) {
        if (path.empty() || !ensureDir(path + "/bin").ok) return false;
        char exe[4096];
        ssize_t count = readlink("/proc/self/exe", exe, sizeof exe - 1);
        if (count <= 0) return false;
        exe[count] = 0;
        if (chrome && symlink(exe, (path + "/bin/google-chrome").c_str())) return false;
        if (encoder && symlink(exe, (path + "/bin/ffmpeg").c_str())) return false;
        return atomicWriteFile(path + "/scene a #.html", "<script>window.renderFrame=t=>{}</script>").ok;
    }
    std::string scene() const { return path + "/scene a #.html"; }
    bool cleaned() const {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            std::string name = entry.path().filename().string();
            if (startsWith(name, "pocket-render-") || name.find(".pocket-") != std::string::npos) return false;
        }
        auto log = readFileBounded(path + "/log", 100000);
        if (log.ok) for (const auto& line : splitLines(log.value)) {
            if (!startsWith(line, "pid ")) continue;
            errno = 0;
            if (kill((pid_t)strtol(line.c_str() + 4, nullptr, 10), 0) == 0 || errno != ESRCH) return false;
        }
        return true;
    }
    SpawnResult render(std::string mode, bool frame, std::vector<std::string> options = {}, std::string output = "") const {
        EnvGuard search("PATH", path + "/bin"), temp("TMPDIR", path), fixture("POCKET_VIDEO_FIXTURE", mode), log("POCKET_VIDEO_LOG", path + "/log");
        SpawnOpts opts;
        opts.exe = "/proc/self/exe";
        opts.argv = {"pocket-video-test", frame ? "--fixture-frame" : "--fixture-video", scene(),
                     output.empty() ? path + (frame ? "/out.png" : "/out.mp4") : output};
        opts.argv.insert(opts.argv.end(), options.begin(), options.end());
        opts.timeoutMs = 8000;
        opts.outLimit = 10000;
        return spawn(opts);
    }
};
}  // namespace

// Called before the ordinary test runner initializes HOME or runs test cases.
int videoTestFixture(int argc, char** argv) {
    std::string name = std::filesystem::path(argv[0]).filename().string();
    if (name == "google-chrome") return browserFixture();
    if (name == "ffmpeg") return encoderFixture(argc, argv);
    if (argc > 1 && (std::string(argv[1]) == "--fixture-frame" || std::string(argv[1]) == "--fixture-video")) {
        std::vector<std::string> args(argv + 2, argv + argc);
        return std::string(argv[1]) == "--fixture-frame" ? kitFrame(args) : kitVideo(args);
    }
    return -1;
}

TEST(video_Frame_And_Sequence_Use_Explicit_Times) {
    VideoScratch scratch;
    CHECK(scratch.setup());
    auto still = scratch.render("ok", true, {"--size", "320x240", "--time", "1.25"});
    CHECK(still.ok && still.exitCode == 0);
    CHECK(readFileBounded(scratch.path + "/out.png", 1 << 20).value == fixturePng(320, 240));
    CHECK(atomicWriteFile(scratch.path + "/music.wav", "audio fixture input").ok);
    auto movie = scratch.render("ok", false, {"--duration", "1", "--fps", "2", "--size", "320x240", "--audio", scratch.path + "/music.wav"});
    CHECK(movie.ok && movie.exitCode == 0);
    CHECK(readFileBounded(scratch.path + "/out.mp4", 1000).value.size() == 34);
    auto log = readFileBounded(scratch.path + "/log", 100000);
    CHECK(log.ok && log.value.find("scene%20a%20%23.html") != std::string::npos);
    for (const char* time : {"__pocketSeek(1.25)", "__pocketSeek(0)", "__pocketSeek(0.5)"}) CHECK(log.value.find(time) != std::string::npos);
    CHECK(log.value.find("encoded bytes " + std::to_string(2 * fixturePng(320, 240).size())) != std::string::npos);
    CHECK(log.value.find("encoder 1:a:0") != std::string::npos && log.value.find("encoder apad") != std::string::npos);
    const size_t freeze = log.value.find("Page.addScriptToEvaluateOnNewDocument"), navigate = log.value.find("Page.navigate");
    CHECK(freeze != std::string::npos && navigate != std::string::npos && freeze < navigate);
    CHECK(log.value.find("animation-play-state:paused") != std::string::npos);
    CHECK(log.value.find("optimizeForSpeed") != std::string::npos);
    CHECK(scratch.cleaned());
    return "";
}

TEST(video_Frame_Times_Option_Validation) {
    VideoScratch scratch;
    CHECK(scratch.setup());
    for (const char* bad : {"abc", "1,,2", "-3,4", "1,x"}) {
        auto r = scratch.render("ok", true, {"--size", "320x240", "--times", bad});
        CHECK(r.ok && r.exitCode == 1 && r.err.find("--times needs") != std::string::npos);
    }
    auto many = scratch.render("ok", true, {"--size", "320x240", "--times", "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25"});
    CHECK(many.ok && many.exitCode == 1 && many.err.find("at most 24") != std::string::npos);
    auto cols = scratch.render("ok", true, {"--size", "320x240", "--times", "1,2", "--cols", "9"});
    CHECK(cols.ok && cols.exitCode == 1 && cols.err.find("--cols") != std::string::npos);
    // One moment is just --time.
    auto one = scratch.render("ok", true, {"--size", "320x240", "--times", "2.5"});
    CHECK(one.ok && one.exitCode == 0);
    auto log = readFileBounded(scratch.path + "/log", 100000);
    CHECK(log.ok && log.value.find("__pocketSeek(2.5)") != std::string::npos);
    CHECK(scratch.cleaned());
    return "";
}

TEST(video_Source_Review_Preserves_Palette_And_Separates_Generic_Copy) {
    VideoScratch scratch;
    CHECK(scratch.setup());
    const std::string brand = "<style>\n"
        "body{font-family:Inter,sans-serif;background:linear-gradient(90deg,#6366f1,#ec4899)}\n"
        "</style><h1>Original score at 64 beats per minute</h1>";
    CHECK(atomicWriteFile(scratch.scene(), brand).ok);
    auto intentional = scratch.render("ok", true, {"--size", "320x240"});
    CHECK(intentional.ok && intentional.exitCode == 0);
    CHECK(intentional.err.find("source review suggestion") == std::string::npos);
    CHECK(intentional.err.find("purple gradient") == std::string::npos);
    CHECK(atomicWriteFile(scratch.scene(), brand + "<p>Build faster. Ship smarter.</p>").ok);
    auto generic = scratch.render("ok", true, {"--size", "320x240"});
    CHECK(generic.ok && generic.exitCode == 0);
    CHECK(generic.err.find("source review suggestion") != std::string::npos);
    CHECK(generic.err.find("indigo/purple gradient") != std::string::npos);
    CHECK(generic.err.find("copy names no capability") != std::string::npos);
    CHECK(generic.err.find("fix before the final render") == std::string::npos);
    CHECK(scratch.cleaned());
    return "";
}

TEST(video_Browser_Failures_Preserve_Output_And_Clean_Children) {
    for (const char* mode : {"navigation_error", "js_error", "protocol_error", "bad_base64", "not_png", "truncated_png", "wrong_size", "browser_exit"}) {
        VideoScratch scratch;
        CHECK(scratch.setup());
        const std::string previous = "previous output must survive";
        CHECK(atomicWriteFile(scratch.path + "/out.png", previous).ok);
        auto result = scratch.render(mode, true, {"--size", "320x240", "--timeout", "1"});
        if (!result.ok || result.exitCode != 1) return std::string(mode) + ": expected failure, got " + std::to_string(result.exitCode) + " " + result.out + result.err;
        CHECK(readFileBounded(scratch.path + "/out.png", 1 << 20).value == previous);
        CHECK(scratch.cleaned());
    }
    return "";
}

TEST(video_Encoder_Failures_Preserve_Output) {
    for (const char* mode : {"encoder_fail", "encoder_empty", "encoder_junk"}) {
        VideoScratch scratch;
        CHECK(scratch.setup());
        const std::string previous = "previous completed video";
        CHECK(atomicWriteFile(scratch.path + "/out.mp4", previous).ok);
        auto result = scratch.render(mode, false, {"--duration", "1", "--fps", "2", "--size", "320x240"});
        CHECK(result.ok && result.exitCode == 1 && result.out.empty());
        CHECK(readFileBounded(scratch.path + "/out.mp4", 1000).value == previous);
        CHECK(scratch.cleaned());
    }
    return "";
}

TEST(video_Timeout_And_Cancellation_Are_Bounded) {
    for (const char* mode : {"timeout", "cancel"}) {
        VideoScratch scratch;
        CHECK(scratch.setup());
        CHECK(atomicWriteFile(scratch.path + "/out.png", "previous frame").ok);
        int64_t start = nowMs();
        auto result = scratch.render(mode, true, {"--timeout", "1"});
        CHECK(result.ok && result.exitCode == (std::string(mode) == "cancel" ? 130 : 1));
        CHECK(nowMs() - start < 5000);
        CHECK(readFileBounded(scratch.path + "/out.png", 1 << 20).value == "previous frame");
        CHECK(scratch.cleaned());
    }
    return "";
}

TEST(video_Cancellation_After_Encoding_Preserves_Previous_Movie) {
    VideoScratch scratch;
    CHECK(scratch.setup());
    CHECK(atomicWriteFile(scratch.path + "/out.mp4", "previous movie").ok);
    auto result = scratch.render("cancel_after_encode", false, {"--duration", "1", "--fps", "1", "--size", "320x240"});
    CHECK(result.ok && result.exitCode == 130 && result.out.empty());
    CHECK(readFileBounded(scratch.path + "/out.mp4", 100).value == "previous movie");
    CHECK(scratch.cleaned());
    return "";
}

TEST(video_Validation_And_Missing_Dependencies_Have_No_Side_Effects) {
    VideoScratch scratch;
    CHECK(scratch.setup(false, false));
    for (const auto& options : {std::vector<std::string>{"--duration", "nan"}, {"--duration", "1", "--fps", "0"},
                               {"--duration", "1", "--size", "321x240"}, {"--duration", "120", "--fps", "60"},
                               {"--duration", "1", "--unknown", "2"}, {"--duration"}}) {
        auto result = scratch.render("ok", false, options);
        CHECK(result.ok && result.exitCode == 1 && result.out.empty());
    }
    auto chrome = scratch.render("ok", true);
    CHECK(chrome.exitCode == 1 && chrome.err.find("Chrome/Chromium") != std::string::npos);
    char self[4096];
    ssize_t count = readlink("/proc/self/exe", self, sizeof self - 1);
    CHECK(count > 0); self[count] = 0;
    CHECK(symlink(self, (scratch.path + "/bin/google-chrome").c_str()) == 0);
    auto ffmpeg = scratch.render("ok", false, {"--duration", "1"});
    CHECK(ffmpeg.exitCode == 1 && ffmpeg.err.find("FFmpeg") != std::string::npos);
    CHECK(atomicWriteFile(scratch.path + "/protected", "keep").ok);
    CHECK(symlink((scratch.path + "/protected").c_str(), (scratch.path + "/out.png").c_str()) == 0);
    auto symlinked = scratch.render("ok", true);
    CHECK(symlinked.exitCode == 1 && readFileBounded(scratch.path + "/protected", 100).value == "keep");
    CHECK(scratch.render("ok", true, {}, scratch.path + "/missing/out.png").exitCode == 1);
    CHECK(unlink((scratch.path + "/out.png").c_str()) == 0);
    CHECK(link(scratch.scene().c_str(), (scratch.path + "/out.png").c_str()) == 0);
    auto sourceAlias = scratch.render("ok", true);
    CHECK(sourceAlias.exitCode == 1 && sourceAlias.err.find("must not replace a source") != std::string::npos);
    CHECK(atomicWriteFile(scratch.path + "/audio.wav", "audio to preserve").ok);
    CHECK(link((scratch.path + "/audio.wav").c_str(), (scratch.path + "/out.mp4").c_str()) == 0);
    auto audioAlias = scratch.render("ok", false, {"--duration", "1", "--audio", scratch.path + "/audio.wav"});
    CHECK(audioAlias.exitCode == 1 && audioAlias.err.find("must not replace a source") != std::string::npos);
    CHECK(readFileBounded(scratch.path + "/audio.wav", 100).value == "audio to preserve");
    CHECK(scratch.cleaned());
    CHECK(access((scratch.path + "/log").c_str(), F_OK) != 0);
    return "";
}

TEST(video_Vcheck_Rejects_Bad_Usage_And_Non_Media) {
    CHECK_EQ(kitVcheck({}), 2);
    CHECK_EQ(kitVcheck({"a.mp4", "--duration"}), 2);
    CHECK_EQ(kitVcheck({"a.mp4", "--duration", "-3"}), 2);
    CHECK_EQ(kitVcheck({"/nonexistent/none.mp4"}), 1);
    return "";
}

TEST(media_Gltf_Preflight_Validates_Dependencies_Ranges_And_Scene_Graph) {
    const std::string dir = makeTempDir("pocket-gltf");
    struct Cleanup { std::string path; ~Cleanup() { rmRf(path); } } cleanup{dir};
    CHECK(!dir.empty());
    CHECK(atomicWriteFile(dir + "/mesh.bin", std::string(36, '\0')).ok);
    auto parsed = json::parse(R"({"asset":{"version":"2.0"},"buffers":[{"uri":"mesh.bin","byteLength":36}],
      "bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})");
    CHECK(parsed.ok);
    auto checkDoc = [&](const json::Value& doc) {
        auto saved = atomicWriteFile(dir + "/model.gltf", json::stringify(doc));
        return saved.ok ? inspectGltf(dir + "/model.gltf") : Result<json::Value>::Err(saved.error);
    };
    auto good = checkDoc(parsed.value);
    CHECK(good.ok && good.value.at("valid").asBool());
    CHECK(good.value.at("triangles").asInt() == 1 && good.value.at("vertex_references").asInt() == 3);
    CHECK(good.value.at("dependencies").at(size_t(0)).at("status").asStr() == "local");
    for (int fault = 0; fault < 7; ++fault) {
        auto doc = parsed.value;
        if (fault == 0) doc.asObj()["buffers"].asArr()[0].asObj()["uri"] = "missing.bin";
        if (fault == 1) doc.asObj()["bufferViews"].asArr()[0].asObj()["byteLength"] = 64;
        if (fault == 2) doc.asObj()["accessors"].asArr()[0].asObj()["count"] = 100;
        if (fault == 3) doc.asObj()["nodes"].asArr()[0].asObj()["children"] = json::Array{0};
        if (fault == 4) doc.asObj()["buffers"].asArr()[0].asObj()["uri"] = "../outside.bin";
        if (fault == 5) doc.asObj()["accessors"].asArr()[0].asObj()["componentType"] = 123;
        if (fault == 6) doc.asObj()["images"] = json::Array{json::Object{{"uri", "missing.png"}}};
        auto bad = checkDoc(doc);
        CHECK(bad.ok && !bad.value.at("valid").asBool() && !bad.value.at("issues").asArr().empty());
    }
    auto compressed = parsed.value;
    compressed.asObj()["extensionsRequired"] = json::Array{"KHR_draco_mesh_compression", "EXT_meshopt_compression", "KHR_texture_basisu"};
    auto requirements = checkDoc(compressed);
    CHECK(requirements.ok && requirements.value.at("loader_notes").asArr().size() == 3);
    return "";
}

TEST(media_Glb_Preflight_Checks_Framing_Without_Decoding_Payload) {
    const std::string dir = makeTempDir("pocket-glb");
    struct Cleanup { std::string path; ~Cleanup() { rmRf(path); } } cleanup{dir};
    auto put = [](std::string& s, uint32_t v) { for (int i = 0; i < 4; ++i) s += char(v >> (i * 8)); };
    std::string doc = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}]})";
    while (doc.size() % 4) doc += ' ';
    std::string glb = "glTF";
    put(glb, 2); put(glb, uint32_t(12 + 8 + doc.size() + 8 + 36));
    put(glb, uint32_t(doc.size())); put(glb, 0x4e4f534a); glb += doc;
    put(glb, 36); put(glb, 0x004e4942); glb.append(36, '\0');
    CHECK(atomicWriteFile(dir + "/model.glb", glb).ok);
    auto good = inspectGltf(dir + "/model.glb");
    CHECK(good.ok && good.value.at("valid").asBool() && good.value.at("format").asStr() == "glb");
    for (int fault = 0; fault < 3; ++fault) {
        std::string broken = glb;
        if (fault == 0) broken.pop_back();
        if (fault == 1) broken[4] = 1;
        if (fault == 2) broken[12] = 1;
        CHECK(atomicWriteFile(dir + "/bad.glb", broken).ok);
        CHECK(!inspectGltf(dir + "/bad.glb").ok);
    }
    return "";
}

TEST(media_Asset_Options_Are_Checked_Before_Output_Directory_Creation) {
    const std::string dir = makeTempDir("pocket-asset-options");
    struct Cleanup { std::string path; ~Cleanup() { rmRf(path); } } cleanup{dir};
    for (const auto& extra : {std::vector<std::string>{"--res"}, {"--unknown", "x"}, {"--res", "99k"}}) {
        std::vector<std::string> args = {"get", "invalid-source", dir + "/out"};
        args.insert(args.end(), extra.begin(), extra.end());
        CHECK(kitAsset(args) == 1);
        CHECK(!std::filesystem::exists(dir + "/out"));
    }
    return "";
}

TEST(media_Asset_Download_Stages_Privately_And_Preserves_Previous_Output) {
    const std::string dir = makeTempDir("pocket-asset-download");
    struct Cleanup { std::string path; ~Cleanup() { rmRf(path); } } cleanup{dir};
    CHECK(ensureDir(dir + "/bin").ok && ensureDir(dir + "/assets").ok);
    const std::string curl = dir + "/bin/curl";
    CHECK(atomicWriteFile(curl, R"SH(#!/bin/sh
output=''
while [ "$#" -gt 0 ]; do
  if [ "$1" = '-o' ]; then output=$2; shift 2; else shift; fi
done
printf 'fixture downloaded asset' > "$output"
if [ "$POCKET_ASSET_DOWNLOAD_FAIL" = 1 ]; then exit 22; fi
)SH").ok);
    CHECK(chmod(curl.c_str(), 0700) == 0);
    CHECK(atomicWriteFile(dir + "/protected", "preserved source").ok);
    CHECK(symlink((dir + "/protected").c_str(), (dir + "/assets/test.bin.part").c_str()) == 0);
    EnvGuard path("PATH", dir + "/bin"), mode("POCKET_ASSET_DOWNLOAD_FAIL", "0");
    CHECK(kitAsset({"get", "https://example.invalid/test.bin", dir + "/assets", "--credit", "fixture"}) == 0);
    CHECK(readFileBounded(dir + "/protected", 100).value == "preserved source");
    CHECK(readFileBounded(dir + "/assets/test.bin", 100).value == "fixture downloaded asset");
    CHECK(atomicWriteFile(dir + "/assets/test.bin", "previous completed asset").ok);
    {
        EnvGuard failure("POCKET_ASSET_DOWNLOAD_FAIL", "1");
        CHECK(kitAsset({"get", "https://example.invalid/test.bin", dir + "/assets"}) == 1);
    }
    CHECK(readFileBounded(dir + "/assets/test.bin", 100).value == "previous completed asset");
    for (const auto& entry : std::filesystem::directory_iterator(dir + "/assets"))
        CHECK(entry.path().filename().string().find(".part-") == std::string::npos);
    return "";
}
