// Native renderer integration, with dependency-free CDP and encoder processes.
#include "mini.h"
#include "../src/common.h"
#include "../src/json.h"
#include "../src/kit_video.h"
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
