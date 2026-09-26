#include "kit_video.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <thread>

#include "common.h"
#include "json.h"
#include "process.h"

namespace pocket {
namespace {

volatile sig_atomic_t interrupted = 0;
void stopRender(int) { interrupted = 1; }
struct Signals {
    struct sigaction oldInt{}, oldTerm{}, oldPipe{};
    Signals() {
        interrupted = 0;
        struct sigaction sa{};
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = stopRender;
        sigaction(SIGINT, &sa, &oldInt); sigaction(SIGTERM, &sa, &oldTerm);
        sa.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &sa, &oldPipe);
    }
    ~Signals() {
        sigaction(SIGINT, &oldInt, nullptr); sigaction(SIGTERM, &oldTerm, nullptr);
        sigaction(SIGPIPE, &oldPipe, nullptr);
    }
};
struct Fd {
    int fd = -1;
    ~Fd() { reset(); }
    void reset() { if (fd >= 0) close(fd); fd = -1; }
};
struct Pipe {
    Fd read, write;
    bool open() {
        int p[2];
        if (pipe2(p, O_CLOEXEC) != 0) return false;
        // Keep original descriptors away from child protocol fds 0,3,4,5.
        read.fd = fcntl(p[0], F_DUPFD_CLOEXEC, 10);
        write.fd = fcntl(p[1], F_DUPFD_CLOEXEC, 10);
        close(p[0]); close(p[1]);
        return read.fd >= 0 && write.fd >= 0;
    }
};
struct Child {
    std::atomic<bool> cancel{false}, done{false};
    SpawnResult result;
    std::thread worker;
    void start(SpawnOpts opts) {
        opts.cancel = &cancel;
        worker = std::thread([this, opts = std::move(opts)] { result = spawn(opts); done.store(true); });
    }
    void join() { if (worker.joinable()) worker.join(); }
    ~Child() { cancel.store(true); join(); }
};
struct Temp {
    std::string path;
    bool directory = false;
    ~Temp() {
        if (path.empty()) return;
        if (directory) { std::error_code ec; std::filesystem::remove_all(path, ec); }
        else unlink(path.c_str());
    }
};

bool number(const std::string& s, double& value) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    value = strtod(s.c_str(), &end);
    return !errno && end == s.c_str() + s.size() && std::isfinite(value);
}
std::string fileUrl(const std::string& path) {
    std::string url = "file://";
    for (unsigned char c : path) {
        if ((c < 128 && isalnum(c)) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') url += (char)c;
        else { char b[4]; snprintf(b, sizeof b, "%%%02X", c); url += b; }
    }
    return url;
}
Result<std::string> pngBytes(const std::string& text, unsigned width, unsigned height) {
    if (text.empty() || text.size() % 4 || text.size() > (48u << 20))
        return Result<std::string>::Err("invalid or oversized browser image");
    auto digit = [](unsigned char c) {
        if (c >= 'A' && c <= 'Z') return (int)c - 'A';
        if (c >= 'a' && c <= 'z') return (int)c - 'a' + 26;
        if (c >= '0' && c <= '9') return (int)c - '0' + 52;
        return c == '+' ? 62 : c == '/' ? 63 : -1;
    };
    std::string bytes;
    bytes.reserve(text.size() / 4 * 3);
    for (size_t i = 0; i < text.size(); i += 4) {
        int a = digit(text[i]), b = digit(text[i+1]);
        int c = text[i+2] == '=' ? 0 : digit(text[i+2]);
        int d = text[i+3] == '=' ? 0 : digit(text[i+3]);
        bool last = i + 4 == text.size();
        if (a < 0 || b < 0 || c < 0 || d < 0 ||
            (!last && (text[i+2] == '=' || text[i+3] == '=')) ||
            (text[i+2] == '=' && (text[i+3] != '=' || (b & 15))) ||
            (text[i+3] == '=' && text[i+2] != '=' && (c & 3)))
            return Result<std::string>::Err("invalid browser image encoding");
        bytes += (char)((a << 2) | (b >> 4));
        if (text[i+2] != '=') bytes += (char)((b << 4) | (c >> 2));
        if (text[i+3] != '=') bytes += (char)((c << 6) | d);
    }
    if (bytes.size() < 45 || bytes.compare(0, 8, "\x89PNG\r\n\x1a\n", 8))
        return Result<std::string>::Err("browser did not return PNG data");
    auto be = [&](size_t at) {
        uint32_t n = 0;
        for (size_t i = at; i < at + 4; ++i) n = (n << 8) | (unsigned char)bytes[i];
        return n;
    };
    if (be(8) != 13 || bytes.compare(12, 4, "IHDR") || be(16) != width || be(20) != height)
        return Result<std::string>::Err("browser image has invalid header or unexpected dimensions");
    bool data = false, end = false;
    for (size_t p = 8; p < bytes.size();) {
        if (bytes.size() - p < 12 || be(p) > bytes.size() - p - 12)
            return Result<std::string>::Err("truncated browser PNG");
        uint32_t size = be(p);
        if (!bytes.compare(p + 4, 4, "IDAT") && size > 0) data = true;
        if (!bytes.compare(p + 4, 4, "IEND")) {
            end = size == 0 && p + 12 == bytes.size();
            break;
        }
        p += 12 + size;
    }
    if (!data || !end) return Result<std::string>::Err("incomplete browser PNG");
    return Result<std::string>::Ok(std::move(bytes));
}

bool completedMp4(int fd, uint64_t length) {
    bool format = false, movie = false, data = false;
    for (uint64_t p = 0; p < length;) {
        unsigned char header[16];
        if (length - p < 8 || pread(fd, header, 8, (off_t)p) != 8) return false;
        uint64_t size = 0;
        for (int i = 0; i < 4; ++i) size = (size << 8) | header[i];
        uint64_t minimum = 8;
        if (size == 1) {
            if (length - p < 16 || pread(fd, header + 8, 8, (off_t)p + 8) != 8) return false;
            size = 0; minimum = 16;
            for (int i = 8; i < 16; ++i) size = (size << 8) | header[i];
        } else if (size == 0) size = length - p;
        if (size < minimum || size > length - p) return false;
        const std::string_view type((const char*)header + 4, 4);
        format = format || (type == "ftyp" && p == 0 && size >= 16);
        movie = movie || (type == "moov" && size > minimum);
        data = data || (type == "mdat" && size > minimum);
        p += size;
    }
    return format && movie && data;
}

bool writeBounded(int fd, std::string_view data, int64_t deadline, const Child* child = nullptr) {
    size_t offset = 0;
    while (offset < data.size() && !interrupted && nowMs() < deadline && (!child || !child->done.load())) {
        pollfd p{fd, POLLOUT, 0};
        int ready = poll(&p, 1, 50);
        if (ready < 0 && errno != EINTR) return false;
        if (ready <= 0) continue;
        ssize_t n = write(fd, data.data() + offset, std::min<size_t>(65536, data.size() - offset));
        if (n > 0) offset += (size_t)n;
        else if (n < 0 && errno != EINTR && errno != EAGAIN) return false;
    }
    return offset == data.size();
}

// Chrome's --remote-debugging-pipe speaks NUL-delimited CDP JSON on fd3/4.
// One bounded request at a time; unsolicited events never reset its deadline.
struct Cdp {
    int input, output;
    Child& browser;
    int64_t deadline;
    long serial = 0;
    std::string pending, session;
    Result<json::Value> call(const std::string& method, json::Object params = {}) {
        int64_t until = std::min(deadline, nowMs() + 15000);
        json::Object request{{"id", ++serial}, {"method", method}, {"params", std::move(params)}};
        if (!session.empty()) request["sessionId"] = session;
        std::string wire = json::stringify(request); wire += '\0';
        if (!writeBounded(input, wire, until, &browser)) return Result<json::Value>::Err("browser pipe closed or timed out");
        while (!interrupted && nowMs() < until) {
            size_t end = pending.find('\0');
            if (end != std::string::npos) {
                auto message = json::parse(std::string_view(pending).substr(0, end));
                pending.erase(0, end + 1);
                if (!message.ok) return Result<json::Value>::Err("invalid browser protocol response");
                if (message.value.at("id").asInt() != serial) continue;
                if (message.value.has("error")) return Result<json::Value>::Err(method + ": " +
                    message.value.at("error").at("message").asStr().substr(0, 600));
                return Result<json::Value>::Ok(message.value.at("result"));
            }
            if (browser.done.load()) break;
            pollfd p{output, POLLIN, 0};
            int ready = poll(&p, 1, 50);
            if (ready < 0 && errno != EINTR) break;
            if (ready <= 0) continue;
            char buf[65536];
            ssize_t n = read(output, buf, sizeof buf);
            if (n <= 0) { if (n < 0 && (errno == EAGAIN || errno == EINTR)) continue; break; }
            if (pending.size() + (size_t)n > (64u << 20)) return Result<json::Value>::Err("browser response exceeded 64 MiB");
            pending.append(buf, (size_t)n);
        }
        return Result<json::Value>::Err(interrupted ? "render cancelled" : "browser request timed out or exited: " + method);
    }
    Result<json::Value> evaluate(const std::string& expression) {
        auto r = call("Runtime.evaluate", {{"expression", expression}, {"awaitPromise", true}, {"returnByValue", true}});
        if (r.ok && r.value.has("exceptionDetails")) {
            auto detail = r.value.at("exceptionDetails");
            return Result<json::Value>::Err("scene JavaScript failed: " +
                (detail.at("exception").at("description").asStr() + " " + detail.at("text").asStr()).substr(0, 1000));
        }
        return r;
    }
};

const char* videoUsage = "usage: kit video SCENE.html OUT.mp4 --duration SECONDS [--size 1280x720] [--fps 30] [--audio FILE] [--timeout 120]";
const char* frameUsage = "usage: kit frame SCENE.html OUT.png [--time SECONDS] [--size 1280x720] [--timeout 120]";

std::string render(const std::vector<std::string>& args, bool still) {
    if (args.size() < 2) return still ? frameUsage : videoUsage;
    int width = 1280, height = 720, fps = 30;
    double duration = 0, at = 0, timeout = 120;
    std::string audio;
    for (size_t i = 2; i < args.size(); i += 2) {
        if (i + 1 == args.size()) return "missing value for " + args[i];
        const std::string& key = args[i];
        const std::string& value = args[i+1];
        double n = 0;
        if (key == "--size") {
            size_t x = value.find('x');
            double w = 0, h = 0;
            if (x == std::string::npos || !number(value.substr(0, x), w) || !number(value.substr(x+1), h) ||
                w < 2 || h < 2 || w > 3840 || h > 3840 || w * h > 8294400 || w != std::floor(w) || h != std::floor(h))
                return "size must be integral WxH, 2..3840 per side, at most 8.3 megapixels";
            width = (int)w; height = (int)h;
        } else if (key == "--audio" && !still) audio = value;
        else {
            if (!number(value, n)) return "invalid numeric value for " + key;
            if (key == "--duration" && !still && n > 0 && n <= 120) duration = n;
            else if (key == "--time" && still && n >= 0 && n <= 86400) at = n;
            else if (key == "--fps" && !still && n >= 1 && n <= 60 && n == std::floor(n)) fps = (int)n;
            else if (key == "--timeout" && n >= 1 && n <= 600) timeout = n;
            else return "unknown or out-of-range option " + key;
        }
    }
    if (!still && (duration <= 0 || width % 2 || height % 2)) return "video needs --duration (0..120s) and even dimensions";
    int frames = still ? 1 : (int)std::ceil(duration * fps - 1e-9);
    if (frames < 1 || frames > 3600) return "render must contain 1..3600 frames; render longer projects in scenes";
    if (!endsWith(toLower(args[1]), still ? ".png" : ".mp4")) return still ? "frame output must end in .png" : "video output must end in .mp4";
    std::error_code ec;
    auto source = std::filesystem::canonical(args[0], ec);
    if (ec || !std::filesystem::is_regular_file(source, ec)) return "scene must be an existing local HTML file";
    if (!audio.empty() && !std::filesystem::is_regular_file(audio, ec)) return "audio must be an existing regular file";
    auto destination = std::filesystem::absolute(args[1], ec);
    if (ec || !std::filesystem::is_directory(destination.parent_path(), ec)) return "output directory does not exist";
    struct stat existing{};
    if (lstat(destination.c_str(), &existing) == 0 && !S_ISREG(existing.st_mode)) return "output must be a regular file, not a symlink or device";
    if (std::filesystem::equivalent(source, destination, ec) ||
        (!audio.empty() && std::filesystem::equivalent(audio, destination, ec))) return "output must not replace a source file";
    std::string chrome;
    for (const char* name : {"google-chrome", "chromium", "chromium-browser", "google-chrome-stable"})
        if (!(chrome = whichExe(name)).empty()) break;
    if (chrome.empty()) return "Chrome/Chromium is needed for frames; inspect `pocket kit probe`";
    std::string ffmpeg = still ? "" : whichExe("ffmpeg");
    if (!still && ffmpeg.empty()) return "FFmpeg is needed for video encoding; frame export remains available";

    Signals signals;
    int64_t deadline = nowMs() + (int64_t)(timeout * 1000);
    Temp profile, staged;
    profile.directory = true;
    std::string temp = (getenv("TMPDIR") && *getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    std::string profileTemplate = temp + "/pocket-render-XXXXXX";
    if (!mkdtemp(profileTemplate.data())) return "cannot create private browser profile";
    profile.path = profileTemplate;
    std::string outputTemplate = destination.string() + ".pocket-XXXXXX";
    Fd output;
    int created = mkstemp(outputTemplate.data());
    if (created < 0) return "cannot stage output in its destination directory";
    staged.path = outputTemplate;
    output.fd = fcntl(created, F_DUPFD_CLOEXEC, 10); close(created);
    if (output.fd < 0) return "cannot retain staged output descriptor";
    Pipe commands, responses, pictures;
    if (!commands.open() || !responses.open() || (!still && !pictures.open())) return "cannot create renderer pipes";
    fcntl(commands.write.fd, F_SETFL, O_NONBLOCK);
    fcntl(responses.read.fd, F_SETFL, O_NONBLOCK);
    if (!still) fcntl(pictures.write.fd, F_SETFL, O_NONBLOCK);
    Child browser, encoder;
    SpawnOpts browserOpts;
    browserOpts.exe = chrome;
    browserOpts.argv = {chrome, "--headless=new", "--no-sandbox", "--disable-gpu", "--hide-scrollbars",
        "--disable-dev-shm-usage", "--no-first-run", "--no-default-browser-check", "--disable-background-networking",
        "--remote-debugging-pipe", "--user-data-dir=" + profile.path, "about:blank"};
    browserOpts.timeoutMs = (long)(timeout * 1000);
    browserOpts.outLimit = 8192;
    browserOpts.terminateGraceMs = 100;
    int commandFd = commands.read.fd, responseFd = responses.write.fd;
    browserOpts.childSetup = [=] { if (dup2(commandFd, 3) < 0 || dup2(responseFd, 4) < 0) _exit(125); };
    browser.start(std::move(browserOpts));
    Cdp cdp{commands.write.fd, responses.read.fd, browser, deadline, 0, {}, {}};
    auto target = cdp.call("Target.createTarget", {{"url", "about:blank"}});
    if (!target.ok) return target.error;
    auto attach = cdp.call("Target.attachToTarget", {{"targetId", target.value.at("targetId").asStr()}, {"flatten", true}});
    if (!attach.ok) return attach.error;
    cdp.session = attach.value.at("sessionId").asStr();
    if (cdp.session.empty()) return "browser did not create a page session";
    auto metrics = cdp.call("Emulation.setDeviceMetricsOverride", {{"width", width}, {"height", height},
        {"deviceScaleFactor", 1}, {"mobile", false}});
    if (!metrics.ok) return metrics.error;
    const std::string url = fileUrl(source.string());
    auto navigation = cdp.call("Page.navigate", {{"url", url}});
    if (!navigation.ok) return navigation.error;
    if (!navigation.value.at("errorText").asStr().empty()) return "cannot load scene: " + navigation.value.at("errorText").asStr();
    bool loaded = false;
    int64_t loadDeadline = std::min(deadline, nowMs() + 15000);
    while (!loaded && !interrupted && nowMs() < loadDeadline) {
        auto ready = cdp.evaluate("location.href === " + json::stringify(url) + " && document.readyState === 'complete'");
        if (!ready.ok) return ready.error;
        loaded = ready.value.at("result").at("value").asBool();
        if (!loaded) poll(nullptr, 0, 20);
    }
    if (!loaded) return "scene did not finish loading before the deadline";
    auto ready = cdp.evaluate(R"JS((async()=>{
        if (window.renderReady !== undefined) await window.renderReady;
        if (document.fonts) await document.fonts.ready;
        await Promise.all(Array.from(document.images, image => image.decode()));
        if (typeof window.renderFrame !== 'function') throw new Error('Define window.renderFrame(timeSeconds) to render each frame deterministically');
        document.body.classList.add('exporting');
        return true;
    })())JS");
    if (!ready.ok) return ready.error;
    if (!still) {
        SpawnOpts encode;
        encode.exe = ffmpeg;
        encode.argv = {ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y", "-threads", "2",
            "-f", "image2pipe", "-framerate", std::to_string(fps), "-vcodec", "png", "-i", "pipe:0"};
        if (!audio.empty()) encode.argv.insert(encode.argv.end(), {"-i", std::filesystem::absolute(audio).string()});
        encode.argv.insert(encode.argv.end(), {"-map", "0:v:0", "-c:v", "libx264", "-threads", "2", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p"});
        if (!audio.empty()) encode.argv.insert(encode.argv.end(), {"-map", "1:a:0", "-af", "apad", "-c:a", "aac", "-b:a", "192k"});
        encode.argv.insert(encode.argv.end(), {"-t", std::to_string((double)frames / fps), "-movflags", "+faststart", "-f", "mp4", "/proc/self/fd/5"});
        encode.timeoutMs = (long)(timeout * 1000);
        encode.outLimit = 8192;
        encode.terminateGraceMs = 100;
        int inputFd = pictures.read.fd, outputFd = output.fd;
        encode.childSetup = [=] { if (dup2(inputFd, 0) < 0 || dup2(outputFd, 5) < 0) _exit(125); };
        encoder.start(std::move(encode));
    }
    int64_t started = nowMs();
    for (int frame = 0; frame < frames; ++frame) {
        if (interrupted || nowMs() >= deadline) return interrupted ? "render cancelled" : "render time budget exhausted";
        std::string seconds = json::stringify(still ? at : (double)frame / fps);
        auto draw = cdp.evaluate("(async()=>{const t=" + seconds + "; await window.renderFrame(t); "
            "for(const a of document.getAnimations()){a.pause();a.currentTime=t*1000;} "
            "for(const s of document.querySelectorAll('svg')){if(s.pauseAnimations){s.pauseAnimations();s.setCurrentTime(t);}} return true;})()");
        if (!draw.ok) return draw.error;
        auto shot = cdp.call("Page.captureScreenshot", {{"format", "png"}, {"captureBeyondViewport", false}, {"fromSurface", true}});
        if (!shot.ok) return shot.error;
        auto png = pngBytes(shot.value.at("data").asStr(), (unsigned)width, (unsigned)height);
        if (!png.ok) return png.error;
        int fd = still ? output.fd : pictures.write.fd;
        if (!writeBounded(fd, png.value, deadline, still ? nullptr : &encoder)) {
            if (!still && encoder.done.load()) {
                encoder.join();
                return "FFmpeg stopped before receiving all frames: " + encoder.result.err.substr(0, 1200);
            }
            return interrupted ? "render cancelled" : "encoder pipe closed or render time budget exhausted";
        }
        if (!still && (frame == 0 || (frame + 1) % fps == 0 || frame + 1 == frames))
            fprintf(stderr, "rendered %d/%d frames\n", frame + 1, frames);
    }
    if (!still) {
        pictures.write.reset();
        while (!encoder.done.load() && !interrupted && nowMs() < deadline) poll(nullptr, 0, 25);
        if (!encoder.done.load()) return interrupted ? "render cancelled" : "encoder time budget exhausted";
        encoder.join();
        if (!encoder.result.ok || encoder.result.exitCode != 0)
            return "FFmpeg failed: " + encoder.result.err.substr(0, 1200);
    }
    struct stat st{};
    if (fstat(output.fd, &st) || st.st_size < 24 || (!still && !completedMp4(output.fd, (uint64_t)st.st_size)) || fsync(output.fd))
        return "output was not completed and synced";
    if (interrupted || nowMs() >= deadline) return interrupted ? "render cancelled" : "render time budget exhausted";
    if (rename(staged.path.c_str(), destination.c_str())) return "cannot publish completed output";
    staged.path.clear();
    printf("%s: %s %dx%d, %d frame%s, %.3fs media, %.2fs render\n", sanitizeTerminal(args[1]).c_str(),
        still ? "PNG" : "H.264 MP4", width, height, frames, frames == 1 ? "" : "s",
        still ? at : (double)frames / fps, (nowMs() - started) / 1000.0);
    return "";
}
int runRender(const std::vector<std::string>& args, bool still) {
    interrupted = 0;
    if (args.size() == 1 && args[0] == "--help") { puts(still ? frameUsage : videoUsage); return 0; }
    std::string error = render(args, still);
    if (error.empty()) return 0;
    fprintf(stderr, "pocket kit: %s\n", sanitizeTerminal(error).c_str());
    return interrupted ? 130 : 1;
}
}  // namespace
int kitVideo(const std::vector<std::string>& args) { return runRender(args, false); }
int kitFrame(const std::vector<std::string>& args) { return runRender(args, true); }
}  // namespace pocket
