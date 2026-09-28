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
#include <map>
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


// Scene lint, run in the page at sampled times. Returns [[kind, message], ...]; kinds dedupe across samples.
const char* kLintJs = R"JS((() => {
    const W = innerWidth, H = innerHeight, S = Math.min(W, H), margin = Math.round(S * .05), minPx = S * .022;
    const out = [], boxes = [];
    const alphaOf = e => { let a = 1; for (let p = e; p && p.nodeType === 1; p = p.parentElement) a *= parseFloat(getComputedStyle(p).opacity); return a; };
    const shown = e => { for (let p = e; p && p.nodeType === 1; p = p.parentElement) { const c = getComputedStyle(p); if (c.display === 'none' || c.visibility === 'hidden') return false; } return alphaOf(e) > .3; };
    const moving = e => { for (let p = e; p && p.nodeType === 1; p = p.parentElement)
        for (const a of p.getAnimations()) { const t = a.effect && a.effect.getComputedTiming(); if (t && t.progress > 0 && t.progress < 1 && t.activeDuration < 2500) return true; } return false; };  // long ones are beat windows or camera moves
    const rgba = c => { const m = c.match(/rgba?\(([^)]+)\)/); if (!m) return null; const v = m[1].split(/[ ,\/]+/).filter(Boolean).map(Number); return {r: v[0], g: v[1], b: v[2], a: v.length > 3 ? v[3] : 1}; };
    const lum = c => { const f = x => { x /= 255; return x <= .03928 ? x / 12.92 : Math.pow((x + .055) / 1.055, 2.4); }; return .2126 * f(c.r) + .7152 * f(c.g) + .0722 * f(c.b); };
    const backdrop = e => { const layers = []; for (let p = e; p && p.nodeType === 1; p = p.parentElement) {
        const c = getComputedStyle(p); if (c.backgroundImage !== 'none') return null;
        const b = rgba(c.backgroundColor); if (b && b.a > 0) { layers.push(b); if (b.a >= .99) break; } }
        let base = {r: 255, g: 255, b: 255}; for (let i = layers.length - 1; i >= 0; --i) { const l = layers[i]; base = {r: base.r * (1 - l.a) + l.r * l.a, g: base.g * (1 - l.a) + l.g * l.a, b: base.b * (1 - l.a) + l.b * l.a}; } return base; };
    const label = (e, t) => e.tagName.toLowerCase() + (typeof e.className === 'string' && e.className.trim() ? '.' + e.className.trim().split(/\s+/)[0] : '') + ' "' + t.slice(0, 26) + '"';
    for (const e of document.body.querySelectorAll('*')) {
        let text = '', r = null;
        for (const n of e.childNodes) if (n.nodeType === 3 && n.textContent.trim()) {
            text += n.textContent.trim() + ' ';
            const g = document.createRange(); g.selectNodeContents(n);
            for (const q of g.getClientRects()) if (q.width > 0 && q.height > 0)
                r = r ? {left: Math.min(r.left, q.left), top: Math.min(r.top, q.top), right: Math.max(r.right, q.right), bottom: Math.max(r.bottom, q.bottom)} : {left: q.left, top: q.top, right: q.right, bottom: q.bottom};
        }
        text = text.trim();
        if (!text || !r || !shown(e) || moving(e)) continue;
        const cs = getComputedStyle(e), fs = parseFloat(cs.fontSize), name = label(e, text), fam = cs.fontFamily.split(',')[0].replace(/["']/g, '').trim();
        boxes.push({e, r, name, a: alphaOf(e)});
        if (r.left < -2 || r.top < -2 || r.right > W + 2 || r.bottom > H + 2) out.push(['edge:' + name, name + ' is cut off by the frame edge']);
        else if (!e.closest('[data-bleed]') && (r.left < margin || r.top < margin || r.right > W - margin || r.bottom > H - margin))
            out.push(['margin:' + name, name + ' sits inside the ' + margin + 'px safe margin']);
        for (let p = e.parentElement; p && p !== document.body; p = p.parentElement) {
            const c = getComputedStyle(p), fs = parseFloat(getComputedStyle(e).fontSize);
            if (/hidden|clip/.test(c.overflow + c.overflowX + c.overflowY)) { const q = p.getBoundingClientRect();
                const slack = Math.max(2, fs * .18);  // Range boxes are the font's content area, taller than the ink
                if (r.right > q.right + 2 || r.left < q.left - 2 || r.bottom > q.bottom + slack || r.top < q.top - slack) { out.push(['clip:' + name, name + ' is clipped by an overflow:hidden container']); break; } }
        }
        if (fs < minPx) out.push(['small:' + name, name + ' is ' + Math.round(fs) + 'px, under the ' + Math.round(minPx) + 'px legibility floor']);
        if (/mono|courier|consolas|menlo|monaco/i.test(cs.fontFamily.split(',')[0]) && fs >= minPx * 1.6) out.push(['mono:' + fam, 'monospace (' + fam + ') used for display text; keep mono for code and data only']);
        if (/^(inter|space grotesk|geist|instrument serif)$/i.test(fam)) out.push(['font:' + fam, fam + ' is the default AI font rotation; pick a deliberate pair (kit theme)']);
        if (/^(arial|helvetica|times new roman|system-ui|sans-serif|serif|segoe ui|roboto)$/i.test(fam)) out.push(['sys:' + fam, 'text falls back to the system font "' + fam + '"; load a real typeface (kit asset font)']);
        const over = document.elementsFromPoint((r.left + r.right) / 2, (r.top + r.bottom) / 2).some(n => /^(IMG|CANVAS|VIDEO)$/i.test(n.tagName));
        const bg = over ? null : backdrop(e), fg = rgba(cs.color);  // over a photo or 3D view the backdrop is unknown
        if (bg && fg) { const a = Math.min(1, fg.a * alphaOf(e)), mix = {r: bg.r * (1 - a) + fg.r * a, g: bg.g * (1 - a) + fg.g * a, b: bg.b * (1 - a) + fg.b * a};
            const l1 = lum(mix), l2 = lum(bg), ratio = (Math.max(l1, l2) + .05) / (Math.min(l1, l2) + .05);
            if (ratio < (fs >= S * .033 ? 3 : 4.5)) out.push(['contrast:' + name, name + ' has contrast ' + ratio.toFixed(1) + ':1 against its background']); }
    }
    for (let i = 0; i < boxes.length && out.length < 40; ++i) for (let j = i + 1; j < boxes.length; ++j) {
        const a = boxes[i], b = boxes[j]; if (a.e.contains(b.e) || b.e.contains(a.e) || a.a < .6 || b.a < .6) continue;
        const w = Math.min(a.r.right, b.r.right) - Math.max(a.r.left, b.r.left), h = Math.min(a.r.bottom, b.r.bottom) - Math.max(a.r.top, b.r.top);
        if (w > 0 && h > 0 && w * h > .25 * Math.min((a.r.right - a.r.left) * (a.r.bottom - a.r.top), (b.r.right - b.r.left) * (b.r.bottom - b.r.top)))
            out.push(['overlap:' + a.name + b.name, a.name + ' overlaps ' + b.name]);
    }
    let media = 0;
    for (const m of document.body.querySelectorAll('svg, img, canvas, video')) { const q = m.getBoundingClientRect(); if (q.width * q.height > 400 && shown(m)) ++media; }
    if (!boxes.length && !media) out.push(['blank', 'nothing is visible: every beat is hidden or its animation has not started']);
    for (const img of document.images) if (!img.complete || !img.naturalWidth) out.push(['img:' + img.src, 'image failed to load: ' + img.src.slice(-60)]);
    if (document.fonts) for (const f of document.fonts) if (f.status === 'error') out.push(['fontfile:' + f.family, 'font file for ' + f.family + ' failed to load']);
    return JSON.stringify(out);
})())JS";

// Source-level tells the DOM cannot show: purple gradients and the default font names.
std::vector<std::string> styleTells(const std::string& html) {
    std::vector<std::string> out;
    for (size_t p = html.find("gradient("); p != std::string::npos; p = html.find("gradient(", p + 9)) {
        size_t end = html.find(')', p);
        std::string body = html.substr(p, end == std::string::npos ? 200 : std::min<size_t>(end - p, 300));
        int violet = 0;
        for (size_t h = body.find('#'); h != std::string::npos; h = body.find('#', h + 1)) {
            if (h + 7 > body.size()) break;
            const std::string digits = body.substr(h + 1, 6);
            char* e = nullptr;
            long v = strtol(digits.c_str(), &e, 16);
            if (*e) continue;
            double r = ((v >> 16) & 255) / 255.0, g = ((v >> 8) & 255) / 255.0, b = (v & 255) / 255.0;
            double mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn, hue = 0;
            if (d > 0.001) hue = mx == r ? 60 * std::fmod((g - b) / d + 6, 6) : mx == g ? 60 * ((b - r) / d + 2) : 60 * ((r - g) / d + 4);
            if (d / (mx > 0 ? mx : 1) > .4 && mx > .4 && hue >= 245 && hue <= 335) ++violet;
        }
        if (violet >= 2) { out.push_back("an indigo/purple gradient is the learned default look; derive colour from the subject (kit theme)"); break; }
    }
    return out;
}

const char* videoUsage = "usage: kit video SCENE.html OUT.mp4 --duration SECONDS [--size 1920x1080] [--fps 30] [--audio FILE] [--start SECONDS] [--timeout SECONDS] [--no-lint]";
const char* frameUsage = "usage: kit frame SCENE.html OUT.png [--time SECONDS] [--size 1280x720] [--timeout 120] [--no-lint]";


// Lint the scene at sample times; report what is stable across samples (a transient mid-reveal state is not a defect).
void lintScene(Cdp& cdp, const std::vector<double>& times, const std::string& source) {
    std::map<std::string, std::pair<int, std::pair<double, std::string>>> seen;
    std::vector<std::string> order;
    for (double t : times) {
        auto seek = cdp.evaluate("window.__pocketSeek(" + json::stringify(t) + ")");
        auto r = seek.ok ? cdp.evaluate(kLintJs) : seek;
        auto list = r.ok ? json::parse(r.value.at("result").at("value").asStr()) : Result<json::Value>::Err(r.error);
        if (!list.ok) { fprintf(stderr, "lint: could not run (%s)\n", sanitizeTerminal(list.error.substr(0, 300)).c_str()); return; }
        for (const auto& item : list.value.asArr()) {
            const std::string key = item.at(size_t(0)).asStr();
            if (!seen.count(key)) { seen[key] = {0, {t, item.at(size_t(1)).asStr()}}; order.push_back(key); }
            ++seen[key].first;
        }
    }
    const int need = times.size() > 1 ? 2 : 1;
    std::vector<std::string> lines;
    for (const auto& key : order)
        if (seen[key].first >= need) {
            char at[32];
            snprintf(at, sizeof at, "t=%.1fs ", seen[key].second.first);
            lines.push_back(std::string(at) + seen[key].second.second);
        }
    for (const auto& tell : styleTells(source)) lines.push_back(tell);
    if (lines.empty()) { fprintf(stderr, "lint: no layout, legibility or style faults at %zu sampled time%s\n", times.size(), times.size() == 1 ? "" : "s"); return; }
    fprintf(stderr, "lint: %zu issue%s; fix before the final render\n", lines.size(), lines.size() == 1 ? "" : "s");
    for (size_t i = 0; i < lines.size() && i < 12; ++i) fprintf(stderr, "  lint: %s\n", sanitizeTerminal(lines[i]).c_str());
    if (lines.size() > 12) fprintf(stderr, "  lint: ... and %zu more\n", lines.size() - 12);
}

std::string render(const std::vector<std::string>& args, bool still) {
    if (args.size() < 2) return still ? frameUsage : videoUsage;
    int width = 1280, height = 720, fps = 30;
    double duration = 0, at = 0, timeout = 0, start = 0;
    std::string audio;
    bool lint = true;
    for (size_t i = 2; i < args.size(); i += 2) {
        if (args[i] == "--no-lint") { lint = false; --i; continue; }
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
            if (key == "--duration" && !still && n > 0 && n <= 900) duration = n;
            else if (key == "--time" && still && n >= 0 && n <= 86400) at = n;
            else if (key == "--start" && !still && n >= 0 && n <= 86400) start = n;
            else if (key == "--fps" && !still && n >= 1 && n <= 60 && n == std::floor(n)) fps = (int)n;
            else if (key == "--timeout" && n >= 1 && n <= 14400) timeout = n;
            else return "unknown or out-of-range option " + key;
        }
    }
    if (!still && (duration <= 0 || width % 2 || height % 2)) return "video needs --duration (0..900s) and even dimensions";
    int frames = still ? 1 : (int)std::ceil(duration * fps - 1e-9);
    if (frames < 1 || frames > 54000) return "render must contain 1..54000 frames; render longer projects in parts with --start";
    if (timeout <= 0) timeout = std::min(14400.0, 120.0 + frames * 0.6);  // scales with the work; explicit --timeout wins
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
    browserOpts.argv = {chrome, "--headless=new", "--no-sandbox", "--hide-scrollbars",
        // Software WebGL (three.js scenes) and ES modules from the scene's own folder.
        "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist", "--allow-file-access-from-files",
        "--font-render-hinting=none", "--disable-dev-shm-usage", "--no-first-run", "--no-default-browser-check", "--disable-background-networking",
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
        await Promise.all(Array.from(document.images, image => image.decode().catch(() => {})));
        // A scene may be pure CSS/SVG animation: seeking the timeline is then all a frame needs.
        if (typeof window.renderFrame !== 'function') {
            if (!document.getAnimations().length && !document.querySelector('svg'))
                throw new Error('Nothing to render: define window.renderFrame(timeSeconds) or animate with CSS/SVG animations');
            window.renderFrame = () => {};
        }
        // getAnimations() forgets animations that have finished, and a forgotten one can never be sought back:
        // keep every animation ever seen so any time can be revisited (lint samples out of order, --start jumps).
        const seen = new Set(document.getAnimations());
        window.__pocketSeek = async (t) => {
            await window.renderFrame(t);
            for (const a of document.getAnimations()) seen.add(a);
            for (const a of seen) { a.pause(); a.currentTime = t * 1000; }
            for (const s of document.querySelectorAll('svg')) if (s.pauseAnimations) { s.pauseAnimations(); s.setCurrentTime(t); }
            return true;
        };
        document.body.classList.add('exporting');
        return true;
    })())JS");
    if (!ready.ok) return ready.error;
    if (lint) {
        std::vector<double> times;
        if (still) times.push_back(at);
        else for (double f : {.05, .15, .27, .4, .52, .64, .76, .88, .96}) times.push_back(start + duration * f);
        lintScene(cdp, times, readFileBounded(source.string(), 4 << 20).value);
    }
    if (!still) {
        SpawnOpts encode;
        encode.exe = ffmpeg;
        encode.argv = {ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y", "-threads", "2",
            "-f", "image2pipe", "-framerate", std::to_string(fps), "-vcodec", "png", "-i", "pipe:0"};
        if (!audio.empty() && start > 0) encode.argv.insert(encode.argv.end(), {"-ss", std::to_string(start)});
        if (!audio.empty()) encode.argv.insert(encode.argv.end(), {"-i", std::filesystem::absolute(audio).string()});
        // sRGB frames to BT.709 limited-range YUV, tagged, so players and YouTube show the colours the scene had.
        encode.argv.insert(encode.argv.end(), {"-map", "0:v:0", "-vf", "scale=in_range=full:out_range=tv:out_color_matrix=bt709,format=yuv420p",
            "-c:v", "libx264", "-threads", "2", "-preset", "veryfast", "-crf", "17", "-g", std::to_string(fps * 2),
            "-colorspace", "bt709", "-color_primaries", "bt709", "-color_trc", "bt709", "-color_range", "tv"});
        if (!audio.empty()) encode.argv.insert(encode.argv.end(), {"-map", "1:a:0", "-af", "apad", "-c:a", "aac", "-b:a", "192k", "-ar", "48000", "-ac", "2"});
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
        std::string seconds = json::stringify(still ? at : start + (double)frame / fps);
        // One transient capture stall (GC, heavy canvas) must not cost the whole render: redraw and retry once.
        Result<json::Value> shot = Result<json::Value>::Err("");
        for (int attempt = 0; attempt < 2; ++attempt) {
            auto draw = cdp.evaluate("window.__pocketSeek(" + seconds + ")");
            if (!draw.ok) return "frame " + std::to_string(frame) + ": " + draw.error;
            shot = cdp.call("Page.captureScreenshot", {{"format", "png"}, {"captureBeyondViewport", false}, {"fromSurface", true}});
            if (shot.ok || interrupted || browser.done.load()) break;
        }
        if (!shot.ok) return "frame " + std::to_string(frame) + " of " + std::to_string(frames) + ": " + shot.error +
            (still ? "" : " (the scene is too heavy for this frame; simplify it, or render in pieces with --start and join with ffmpeg concat)");
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
namespace {
// Number after "key" in an ffmpeg filter log line, "" when absent.
std::string logValue(const std::string& line, const std::string& key) {
    size_t p = line.find(key);
    if (p == std::string::npos) return "";
    p += key.size();
    while (p < line.size() && line[p] == ' ') ++p;
    size_t e = p;
    while (e < line.size() && (isdigit((unsigned char)line[e]) || line[e] == '.' || line[e] == '-')) ++e;
    return line.substr(p, e - p);
}
double logNumber(const std::string& line, const std::string& key) { return atof(logValue(line, key).c_str()); }
double jsonNumber(const json::Value& v) { return v.isStr() ? atof(v.asStr().c_str()) : v.asNum(); }
}  // namespace

// Judge a finished video from its own bytes: streams, duration, black or frozen
// spans, silence and clipping. Exit 0 = no mechanical fault found; that never
// proves the video is good, only that these faults are absent.
int kitVcheck(const std::vector<std::string>& args) {
    const char* usage = "usage: kit vcheck FILE.mp4 [--duration EXPECTED_SECONDS]";
    double expected = 0;
    if (args.size() == 1 && args[0] == "--help") { puts(usage); return 0; }
    if (args.empty() || args.size() == 2 || args.size() > 3 ||
        (args.size() == 3 && (args[1] != "--duration" || !number(args[2], expected) || expected <= 0))) {
        puts(usage);
        return 2;
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(args[0], ec)) {
        fprintf(stderr, "pocket kit: not a file: %s\n", sanitizeTerminal(args[0]).c_str());
        return 1;
    }
    const std::string ffprobe = whichExe("ffprobe"), ffmpeg = whichExe("ffmpeg");
    if (ffprobe.empty() || ffmpeg.empty()) {
        fprintf(stderr, "pocket kit: FFmpeg (ffmpeg and ffprobe) is needed to check video\n");
        return 1;
    }
    SpawnOpts probe;
    probe.exe = ffprobe;
    probe.argv = {ffprobe, "-v", "error", "-print_format", "json", "-show_format", "-show_streams", args[0]};
    probe.timeoutMs = 30000;
    SpawnResult pr = spawn(probe);
    auto info = json::parse(pr.out);
    if (!pr.ok || pr.exitCode != 0 || !info.ok) {
        printf("%s: PROBLEMS\n  PROBLEM: not readable as media: %s\n", sanitizeTerminal(args[0]).c_str(),
               sanitizeTerminal(pr.err.substr(0, 300)).c_str());
        return 1;
    }
    std::vector<std::string> problems, notes;
    bool video = false, audio = false;
    double vDur = 0, aDur = 0;
    const double total = jsonNumber(info.value.at("format").at("duration"));
    for (const auto& s : info.value.at("streams").asArr()) {
        const std::string type = s.at("codec_type").asStr();
        const double d = s.has("duration") ? jsonNumber(s.at("duration")) : total;
        if (type == "video" && !video) {
            video = true; vDur = d;
            notes.push_back(s.at("codec_name").asStr() + " " + std::to_string(s.at("width").asInt()) + "x" +
                            std::to_string(s.at("height").asInt()) + " @ " + s.at("r_frame_rate").asStr() + " fps");
        } else if (type == "audio" && !audio) {
            audio = true; aDur = d;
            notes.push_back(s.at("codec_name").asStr() + " " + s.at("sample_rate").asStr() + " Hz");
        }
    }
    char buf[200];
    snprintf(buf, sizeof buf, "duration %.2fs", total);
    notes.push_back(buf);
    if (!video) problems.push_back("no video stream");
    if (!audio) notes.push_back("no audio stream");
    if (total <= 0) problems.push_back("unknown or zero duration");
    if (expected > 0 && std::fabs(total - expected) > std::max(0.25, expected * 0.02)) {
        snprintf(buf, sizeof buf, "duration %.2fs differs from the expected %.2fs", total, expected);
        problems.push_back(buf);
    }
    if (video && audio && std::fabs(vDur - aDur) > 0.35) {
        snprintf(buf, sizeof buf, "audio (%.2fs) and video (%.2fs) lengths differ: truncation or sync", aDur, vDur);
        problems.push_back(buf);
    }
    if (video || audio) {
        SpawnOpts scan;
        scan.exe = ffmpeg;
        scan.argv = {ffmpeg, "-hide_banner", "-nostats", "-nostdin", "-i", args[0]};
        if (video) scan.argv.insert(scan.argv.end(), {"-vf", "blackdetect=d=0.4:pix_th=0.05,freezedetect=n=0.001:d=2"});
        if (audio) scan.argv.insert(scan.argv.end(), {"-af", "volumedetect,silencedetect=n=-50dB:d=2,ebur128=peak=true"});
        scan.argv.insert(scan.argv.end(), {"-f", "null", "-"});
        scan.timeoutMs = 600000;
        scan.outLimit = 4 << 20;
        SpawnResult sr = spawn(scan);
        if (!sr.ok || sr.timedOut || sr.exitCode != 0)
            problems.push_back("decoding failed: " + sanitizeTerminal(sr.err.substr(sr.err.size() > 300 ? sr.err.size() - 300 : 0)));
        int black = 0, frozen = 0, silent = 0;
        double mean = 0, peak = -99;
        bool haveVolume = false, wantI = false, haveI = false;
        double integrated = 0;
        for (const auto& line : splitLines(sr.err)) {
            if (line.find("black_start") != std::string::npos) {
                snprintf(buf, sizeof buf, "black frames from %.1fs to %.1fs", logNumber(line, "black_start:"), logNumber(line, "black_end:"));
                if (++black <= 3) problems.push_back(buf);
            } else if (line.find("freeze_duration") != std::string::npos) {
                snprintf(buf, sizeof buf, "picture frozen for %.1fs (fine only if the hold is intended)", logNumber(line, "freeze_duration:"));
                if (++frozen <= 3) notes.push_back(buf);
            } else if (line.find("silence_duration") != std::string::npos) {
                snprintf(buf, sizeof buf, "silence of %.1fs ending at %.1fs", logNumber(line, "silence_duration:"), logNumber(line, "silence_end:"));
                if (++silent <= 3) notes.push_back(buf);
            } else if (line.find("Integrated loudness") != std::string::npos) { wantI = true; }
            else if (wantI && line.find("I:") != std::string::npos) { integrated = logNumber(line, "I:"); haveI = true; wantI = false; }
            else if (line.find("mean_volume:") != std::string::npos) { mean = logNumber(line, "mean_volume:"); haveVolume = true; }
            else if (line.find("max_volume:") != std::string::npos) peak = logNumber(line, "max_volume:");
        }
        if (audio && haveVolume) {
            snprintf(buf, sizeof buf, "audio mean %.1f dB, peak %.1f dB", mean, peak);
            notes.push_back(buf);
            if (haveI) {
                snprintf(buf, sizeof buf, "integrated loudness %.1f LUFS (streaming platforms aim for about -14)", integrated);
                notes.push_back(buf);
                if (integrated < -20 && mean >= -45) problems.push_back("programme is quiet (" + std::to_string((int)std::lround(integrated)) + " LUFS): platforms will not raise it; mix to about -14 LUFS with kit mix");
                if (integrated > -9) problems.push_back("programme is very loud (" + std::to_string((int)std::lround(integrated)) + " LUFS): platforms turn it down and it may distort; mix to about -14 LUFS");
            }
            if (mean < -45) problems.push_back("audio is effectively silent");
            if (peak >= -0.05) problems.push_back("audio peaks at full scale (clipping)");
        }
    }
    printf("%s: %s\n", sanitizeTerminal(args[0]).c_str(), problems.empty() ? "no mechanical faults found" : "PROBLEMS");
    for (const auto& n : notes) printf("  %s\n", sanitizeTerminal(n).c_str());
    for (const auto& p : problems) printf("  PROBLEM: %s\n", p.c_str());
    if (problems.empty()) puts("  (content is not judged: still look at frames and listen to the mix)");
    return problems.empty() ? 0 : 1;
}

}  // namespace pocket
