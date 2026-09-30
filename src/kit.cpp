// PocketHarness - `pocket kit` native superpowers.
#include "kit.h"
#include "kit_audio.h"
#include "config.h"
#include "kit_lint.h"
#include "kit_media.h"
#include "kit_ops.h"
#include "kit_studio.h"
#include "kit_video.h"

#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cerrno>
#include <filesystem>
#include <map>
#include <set>

#include "brain.h"
#include "common.h"
#include "json.h"
#include "process.h"

namespace pocket {

namespace {

constexpr const char* kUA =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140 Safari/537.36";

int fail(const std::string& msg) {
    fprintf(stderr, "pocket kit: %s\n", msg.c_str());
    return 1;
}

SpawnResult run(std::vector<std::string> argv, long timeoutMs, size_t limit = 8 << 20) {
    SpawnOpts o;
    o.exe = argv[0];
    o.argv = std::move(argv);
    o.timeoutMs = timeoutMs;
    o.outLimit = limit;
    return spawn(o);
}

Result<std::string> fetch(const std::string& url, long timeoutSec = 20) {
    if (!startsWith(url, "http://") && !startsWith(url, "https://"))
        return Result<std::string>::Err("URL must be http(s)");
    SpawnResult r = run({"curl", "--disable", "-sSL", "--compressed", "-A", kUA, "--max-time",
                         std::to_string(timeoutSec), "--max-filesize", "8000000", "--proto",
                         "=http,https", url},
                        (timeoutSec + 2) * 1000);
    if (!r.ok || r.exitCode != 0)
        return Result<std::string>::Err("fetch failed: " + trim(r.err.substr(0, 300)));
    return Result<std::string>::Ok(std::move(r.out));
}

std::string entity(std::string_view e) {
    static const std::map<std::string, std::string> named = {
        {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"}, {"nbsp", " "},
        {"mdash", "—"}, {"ndash", "–"}, {"hellip", "…"}, {"copy", "©"}, {"rsquo", "’"},
        {"lsquo", "‘"}, {"rdquo", "”"}, {"ldquo", "“"}, {"middot", "·"}, {"times", "×"}};
    if (!e.empty() && e[0] == '#') {
        long cp = e.size() > 1 && (e[1] == 'x' || e[1] == 'X') ? strtol(std::string(e.substr(2)).c_str(), nullptr, 16)
                                                                : atol(std::string(e.substr(1)).c_str());
        if (cp <= 0 || cp > 0x10FFFF) return "";
        std::string o;
        if (cp < 0x80) o += (char)cp;
        else if (cp < 0x800) o += (char)(0xC0 | cp >> 6), o += (char)(0x80 | (cp & 63));
        else if (cp < 0x10000)
            o += (char)(0xE0 | cp >> 12), o += (char)(0x80 | (cp >> 6 & 63)), o += (char)(0x80 | (cp & 63));
        else
            o += (char)(0xF0 | cp >> 18), o += (char)(0x80 | (cp >> 12 & 63)),
                o += (char)(0x80 | (cp >> 6 & 63)), o += (char)(0x80 | (cp & 63));
        return o;
    }
    auto it = named.find(std::string(e));
    return it == named.end() ? "" : it->second;
}

std::string decodeEntities(std::string_view s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        size_t semi = s[i] == '&' ? s.find(';', i) : std::string_view::npos;
        if (semi != std::string_view::npos && semi - i <= 10) {
            std::string d = entity(s.substr(i + 1, semi - i - 1));
            if (!d.empty()) { o += d; i = semi; continue; }
        }
        o += s[i];
    }
    return o;
}

std::map<std::string, std::string> attributes(std::string_view tag) {
    std::map<std::string, std::string> out;
    size_t p = 0;
    if (!tag.empty() && tag[0] == '<') {
        p = 1;
        if (p < tag.size() && tag[p] == '/') ++p;
        while (p < tag.size() && !isspace((unsigned char)tag[p]) && tag[p] != '>') ++p;
    }
    while (p < tag.size() && out.size() < 256) {
        while (p < tag.size() && isspace((unsigned char)tag[p])) ++p;
        if (p == tag.size() || tag[p] == '>') break;
        if (tag[p] == '/') { ++p; continue; }
        size_t begin = p;
        while (p < tag.size() && !isspace((unsigned char)tag[p]) && tag[p] != '=' && tag[p] != '>' && tag[p] != '/') ++p;
        if (begin == p) { ++p; continue; }
        std::string name = toLower(tag.substr(begin, p - begin));
        while (p < tag.size() && isspace((unsigned char)tag[p])) ++p;
        std::string value;
        if (p < tag.size() && tag[p] == '=') {
            ++p;
            while (p < tag.size() && isspace((unsigned char)tag[p])) ++p;
            char quote = p < tag.size() ? tag[p] : 0;
            if (quote == '\"' || quote == '\'') {
                begin = ++p;
                while (p < tag.size() && tag[p] != quote) ++p;
                value = std::string(tag.substr(begin, p - begin));
                if (p < tag.size()) ++p;
            } else {
                begin = p;
                while (p < tag.size() && !isspace((unsigned char)tag[p]) && tag[p] != '>') ++p;
                value = std::string(tag.substr(begin, p - begin));
            }
        }
        out.emplace(std::move(name), decodeEntities(value));
    }
    return out;
}

}  // namespace

std::map<std::string, std::string> htmlAttrs(std::string_view tag) { return attributes(tag); }
std::string htmlAttr(std::string_view tag, const std::string& name) {
    auto attrs = attributes(tag);
    auto it = attrs.find(toLower(name));
    return it == attrs.end() ? "" : it->second;
}

std::string urlDecode(std::string_view s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && isxdigit((unsigned char)s[i + 1]) &&
            isxdigit((unsigned char)s[i + 2])) {
            o += (char)strtol(std::string(s.substr(i + 1, 2)).c_str(), nullptr, 16);
            i += 2;
        } else o += s[i] == '+' ? ' ' : s[i];
    }
    return o;
}

std::string htmlToText(std::string_view html, std::vector<std::string>* links) {
    static const std::set<std::string> skip = {"script", "style", "noscript", "svg", "template", "iframe", "head"};
    static const std::set<std::string> block = {"p", "div", "br", "tr", "section", "article", "header", "footer",
                                                "ul", "ol", "table", "blockquote", "pre", "main", "nav", "form",
                                                "hr", "dd", "dt", "figure", "figcaption", "aside"};
    std::string out, title;
    std::string skipping;  // tag whose content is dropped
    std::string href;
    bool inTitle = false;
    size_t linkStart = 0;
    for (size_t i = 0; i < html.size();) {
        if (html[i] != '<') {
            size_t e = html.find('<', i);
            std::string_view t = html.substr(i, e == std::string_view::npos ? std::string_view::npos : e - i);
            if (inTitle) title += decodeEntities(t);
            else if (skipping.empty()) out += decodeEntities(t);
            i = e == std::string_view::npos ? html.size() : e;
            continue;
        }
        if (html.compare(i, 4, "<!--") == 0) {
            size_t e = html.find("-->", i);
            i = e == std::string_view::npos ? html.size() : e + 3;
            continue;
        }
        size_t e = html.find('>', i);
        if (e == std::string_view::npos) break;
        std::string_view tag = html.substr(i + 1, e - i - 1);
        i = e + 1;
        bool close = !tag.empty() && tag[0] == '/';
        std::string name;
        for (size_t k = close; k < tag.size() && (isalnum((unsigned char)tag[k])); ++k)
            name += (char)tolower((unsigned char)tag[k]);
        if (name == "title") { inTitle = !close; continue; }
        if (!skipping.empty()) {
            if (close && name == skipping) skipping.clear();
            continue;
        }
        if (!close && skip.count(name) && (tag.empty() || tag.back() != '/')) { skipping = name; continue; }
        if (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6')
            out += close ? "\n" : "\n\n" + std::string((size_t)(name[1] - '0'), '#') + " ";
        else if (name == "li") out += close ? "" : "\n- ";
        else if (name == "td" || name == "th") out += close ? "" : " | ";
        else if (block.count(name)) out += "\n";
        else if (name == "a" && links) {
            if (!close) { href = htmlAttr(tag, "href"); linkStart = out.size(); }
            else if (!href.empty() && !startsWith(href, "#") && !startsWith(href, "javascript:") &&
                     out.size() > linkStart) {
                links->push_back(decodeEntities(href));
                out += " [" + std::to_string(links->size()) + "]";
                href.clear();
            }
        }
    }
    // Collapse whitespace: single spaces within lines, at most one blank line.
    std::string clean;
    int blank = 0;
    for (std::string line : splitLines(out)) {
        std::string l;
        bool sp = false;
        for (char c : line) {
            if (c == ' ' || c == '\t' || c == '\r' || (unsigned char)c == 0xa0) { sp = !l.empty(); continue; }
            if (sp) l += ' ';
            sp = false;
            l += c;
        }
        if (l.empty()) { if (++blank == 1 && !clean.empty()) clean += "\n"; continue; }
        blank = 0;
        clean += l + "\n";
    }
    title = trim(title);
    if (title.empty() || startsWith(clean, "# " + title)) return clean;
    return "# " + title + "\n\n" + clean;
}

std::string springEasing(double k, double c, double m, double* durationMs) {
    if (durationMs) *durationMs = 0;
    if (!std::isfinite(k) || !std::isfinite(c) || !std::isfinite(m) || k <= 0 || c <= 0 || m <= 0) return "";
    const double omega = std::sqrt(k) / std::sqrt(m), alpha = (c / m) / 2;
    if (!std::isfinite(omega) || !std::isfinite(alpha) || omega <= 0 || alpha <= 0) return "";
    const double ratio = alpha / omega;
    const bool critical = std::fabs(ratio - 1) < 1e-8;
    const double beta = ratio < 1 && !critical ? omega * std::sqrt((1 - ratio) * (1 + ratio)) : 0;
    const double root = ratio > 1 && !critical ? std::sqrt((1 - 1 / ratio) * (1 + 1 / ratio)) : 0;
    const double slow = ratio > 1 ? -(omega / alpha) * omega / (1 + root) : 0;
    const double fast = ratio > 1 ? -alpha * (1 + root) : 0;
    if (!std::isfinite(beta) || !std::isfinite(fast) || (ratio > 1 && slow == 0)) return "";
    // Closed-form unit-step response, starting at rest. The Euler integrator
    // previously diverged for stiff springs and silently snapped unfinished ones.
    auto response = [&](double t, bool envelope) {
        double error, speed;
        if (critical) {
            const double wt = omega * t, decay = std::exp(-wt);
            error = (1 + wt) * decay;
            speed = omega * wt * decay;
        } else if (ratio < 1) {
            const double decay = std::exp(-alpha * t), q = alpha / beta;
            error = decay * (envelope ? std::hypot(1.0, q) : std::cos(beta * t) + q * std::sin(beta * t));
            speed = decay * (omega / beta) * omega;
        } else {
            const double a = -fast / (slow - fast), b = slow / (slow - fast);
            error = a * std::exp(slow * t) + b * std::exp(fast * t);
            speed = a * (-slow) * (std::exp(slow * t) - std::exp(fast * t));
        }
        return std::pair<double, double>{error, speed};
    };
    auto settled = [&](double t) {
        const auto [error, speed] = response(t, true);
        return std::isfinite(error) && std::isfinite(speed) && error <= 0.001 && speed <= 0.01;
    };
    // Bound both the settling horizon and the amount of generated CSS.
    double lo = 0, hi = 0.001;
    while (hi < 60 && !settled(hi)) hi = std::min(60.0, hi * 2);
    if (!settled(hi)) return "";
    for (int i = 0; i < 40; ++i) {
        double mid = (lo + hi) / 2;
        if (settled(mid)) hi = mid;
        else lo = mid;
    }
    const double intervals = std::max(40.0, std::ceil(beta * hi * 16 / M_PI));
    if (!std::isfinite(intervals) || intervals > 2048) return "";
    const int count = (int)intervals;
    std::string result = "linear(0";
    for (int i = 1; i < count; ++i) {
        const double value = 1 - response(hi * i / count, false).first;
        if (!std::isfinite(value)) return "";
        char text[32];
        snprintf(text, sizeof text, ", %.6g", value);
        result += text;
    }
    if (durationMs) *durationMs = hi * 1000;
    return result + ", 1)";
}

std::vector<std::string> slopScan(const std::string& path, const std::string& content) {
    std::vector<std::string> out;
    std::string lp = toLower(path);
    bool prose = endsWith(lp, ".md") || endsWith(lp, ".txt") || endsWith(lp, ".html") || endsWith(lp, ".rst");
    if (endsWith(lp, ".json") && !content.empty()) {
        auto v = json::parse(content);
        if (!v.ok) out.push_back("invalid JSON: " + v.error);
    }
    static const char* kCode[] = {"... existing code", "... rest of", "rest of the code", "rest of file",
                                  "remains unchanged", "remain unchanged", "same as before",
                                  "todo: implement", "not implemented", "implement this",
                                  "your code here", "placeholder implementation", "(omitted for brevity)",
                                  "omitted for brevity", "... (truncated)", "etc. etc."};
    static const char* kProse[] = {"as an ai", "i hope this helps", "delve", "tapestry", "testament to",
                                   "in today's fast-paced", "unlock the power", "elevate your",
                                   "seamlessly integrat", "game-changer", "cutting-edge solution"};
    int ln = 0;
    for (const std::string& raw : splitLines(content)) {
        ++ln;
        if (out.size() >= 20) break;
        if (startsWith(raw, "<<<<<<< ") || startsWith(raw, ">>>>>>> ") || raw == "=======")
            out.push_back("line " + std::to_string(ln) + ": merge conflict marker");
        std::string l = toLower(raw);
        for (const char* p : kCode)
            if (l.find(p) != std::string::npos) {
                out.push_back("line " + std::to_string(ln) + ": placeholder/stub \"" + p + "\"");
                break;
            }
        std::string t = trim(l);
        if (t == "// ..." || t == "# ..." || t == "/* ... */" || t == "..." || t == "<!-- ... -->")
            out.push_back("line " + std::to_string(ln) + ": elided code placeholder");
        if (prose)
            for (const char* p : kProse)
                if (l.find(p) != std::string::npos) {
                    out.push_back("line " + std::to_string(ln) + ": AI-tell phrasing \"" + p + "\"");
                    break;
                }
    }
    return out;
}

std::string hostProbe(const std::string& dir) {
    std::string s;
    struct utsname u{};
    if (uname(&u) == 0) s += std::string("os: ") + u.sysname + " " + u.release + " " + u.machine + "\n";
    if (auto t = readFileBounded("/etc/os-release", 8192); t.ok)
        for (const auto& l : splitLines(t.value))
            if (startsWith(l, "PRETTY_NAME=")) s += "distro: " + trim(l.substr(12)) + "\n";
    std::string cpu;
    if (auto t = readFileBounded("/proc/cpuinfo", 1 << 20); t.ok)
        for (const auto& l : splitLines(t.value))
            if (startsWith(l, "model name")) { cpu = trim(l.substr(l.find(':') + 1)); break; }
    s += "cpu: " + std::to_string(sysconf(_SC_NPROCESSORS_ONLN)) + " cores" + (cpu.empty() ? "" : " · " + cpu) + "\n";
    if (auto t = readFileBounded("/proc/meminfo", 65536); t.ok) {
        long total = 0, avail = 0;
        for (const auto& l : splitLines(t.value)) {
            if (startsWith(l, "MemTotal:")) total = atol(l.c_str() + 9);
            if (startsWith(l, "MemAvailable:")) avail = atol(l.c_str() + 13);
        }
        s += "memory: " + std::to_string(avail / 1048576) + " GiB free of " + std::to_string(total / 1048576) + " GiB\n";
    }
    if (auto t = readFileBounded("/proc/loadavg", 256); t.ok) s += "load: " + trim(t.value.substr(0, 14)) + "\n";
    struct statvfs fs{};
    if (statvfs(dir.c_str(), &fs) == 0)
        s += "disk: " + std::to_string((long)(fs.f_bavail * fs.f_frsize >> 30)) + " GiB free at " + dir + "\n";
    std::string gpu;
    if (access("/proc/driver/nvidia/version", R_OK) == 0) gpu = "nvidia";
    if (access("/dev/dri/renderD128", F_OK) == 0) gpu += gpu.empty() ? "dri" : "+dri";
    if (!gpu.empty()) s += "gpu: " + gpu + "\n";
    if (auto t = readFileBounded("/sys/class/power_supply/BAT0/capacity", 16); t.ok)
        s += "battery: " + trim(t.value) + "%\n";
    std::string have;
    for (const char* tool : {"git", "make", "cmake", "gcc", "g++", "clang", "python3", "node", "npm", "cargo",
                             "go", "java", "docker", "podman", "rg", "jq", "curl", "ffmpeg", "sox", "convert",
                             "google-chrome", "chromium", "sqlite3", "gdb", "valgrind", "strace"})
        if (!whichExe(tool).empty()) have += std::string(have.empty() ? "" : " ") + tool;
    s += "tools: " + have + "\n";
    bool chrome = false;
    for (const char* name : {"google-chrome", "chromium", "chromium-browser", "google-chrome-stable"})
        chrome = chrome || !whichExe(name).empty();
    s += "capabilities: native audio/SFX + analysis; frames " + std::string(chrome ? "available" : "need Chrome/Chromium") +
         "; video " + (chrome && !whichExe("ffmpeg").empty() ? "available (Chrome + FFmpeg)" : "needs Chrome/Chromium + FFmpeg") + "\n";
    {
        std::error_code ec;
        bool voice = false;
        if (std::filesystem::is_directory(voicesDir(), ec))
            for (auto& e : std::filesystem::directory_iterator(voicesDir(), ec)) voice = voice || e.path().extension() == ".onnx";
        s += std::string("narration: ") + (voice ? "neural voice installed (kit say)" : "run `pocket kit say --setup` once for a neural voice (Piper, ~90 MB)") + "\n";
    }
    s += "scene contract: local HTML with CSS/SVG animations or window.renderFrame(timeSeconds); use kit frame before kit video\n";
    return s;
}

namespace {

std::string chromeExe() {
    for (const char* c : {"google-chrome", "chromium", "chromium-browser", "google-chrome-stable"})
        if (!whichExe(c).empty()) return c;
    return "";
}

struct ChromeScratch {
    std::string path;
    ChromeScratch() {
        const char* td = getenv("TMPDIR");
        std::string pattern = std::string(td && *td ? td : "/tmp") + "/pocket-chrome-XXXXXX";
        std::vector<char> data(pattern.begin(), pattern.end());
        data.push_back('\0');
        if (mkdtemp(data.data())) path = data.data();
    }
    ~ChromeScratch() {
        std::error_code ec;
        if (!path.empty()) std::filesystem::remove_all(path, ec);
    }
};

std::vector<std::string> chromeArgs(const std::string& exe, const std::string& profile) {
    // Inside the harness Chrome inherits Landlock+seccomp confinement.
    return {exe, "--headless=new", "--disable-gpu", "--no-sandbox", "--hide-scrollbars",
            "--no-first-run", "--disable-dev-shm-usage", "--user-data-dir=" + profile, "--virtual-time-budget=4000"};
}

bool shotSize(const std::string& text, unsigned& width, unsigned& height) {
    size_t x = text.find('x');
    if (x == std::string::npos || x == 0 || x + 1 == text.size()) return false;
    auto dimension = [](std::string_view value, unsigned& out) {
        out = 0;
        for (char c : value) {
            if (c < '0' || c > '9') return false;
            out = out * 10 + (unsigned)(c - '0');
            if (out > 8192) return false;
        }
        return out > 0;
    };
    return dimension(std::string_view(text).substr(0, x), width) &&
           dimension(std::string_view(text).substr(x + 1), height) && (uint64_t)width * height <= 32000000;
}

bool pngSize(std::string_view data, unsigned& width, unsigned& height) {
    if (data.size() < 33 || data.substr(0, 8) != std::string_view("\x89PNG\r\n\x1a\n", 8) ||
        data.substr(8, 8) != std::string_view("\0\0\0\x0dIHDR", 8)) return false;
    auto be32 = [&](size_t i) {
        uint32_t n = 0;
        for (size_t j = i; j < i + 4; ++j) n = (n << 8) | (unsigned char)data[j];
        return n;
    };
    width = be32(16), height = be32(20);
    return width > 0 && height > 0;
}

int kitWeb(const std::vector<std::string>& a) {
    if (a.empty()) return fail("usage: kit web URL [--raw]");
    auto body = fetch(a[0]);
    if (!body.ok) return fail(body.error);
    bool raw = a.size() > 1 && a[1] == "--raw";
    std::string head = toLower(body.value.substr(0, 2048));
    if (raw || (head.find("<html") == std::string::npos && head.find("<!doctype") == std::string::npos)) {
        fwrite(body.value.data(), 1, std::min<size_t>(body.value.size(), 200000), stdout);
        return 0;
    }
    std::vector<std::string> links;
    std::string text = htmlToText(body.value, &links);
    if (text.size() > 120000) text = text.substr(0, 120000) + "\n[...truncated]\n";
    printf("%s\nlinks:\n", text.c_str());
    for (size_t i = 0; i < links.size() && i < 80; ++i) printf("[%zu] %s\n", i + 1, links[i].c_str());
    return 0;
}

struct Hit {
    std::string title, url, snippet;
};

// Generic result scraper: anchors carrying `anchorMark`, the next
// `snippetMark` element before the following anchor is the snippet.
std::vector<Hit> scrape(const std::string& h, const std::string& anchorMark, const std::string& snippetMark,
                        const std::string& snippetEnd, const std::string& titleMark = "") {
    std::vector<Hit> out;
    for (size_t p = 0; (p = h.find(anchorMark, p)) != std::string::npos && out.size() < 10;) {
        size_t open = h.rfind("<a", p), tagEnd = h.find('>', p), close = h.find("</a>", tagEnd);
        if (open == std::string::npos || tagEnd == std::string::npos || close == std::string::npos) break;
        Hit hit;
        hit.url = decodeEntities(htmlAttr(std::string_view(h).substr(open, tagEnd - open), "href"));
        hit.title = trim(htmlToText(h.substr(tagEnd + 1, close - tagEnd - 1)));
        if (!titleMark.empty()) {  // engines whose link also wraps the site name and breadcrumb
            size_t t = h.find(titleMark, tagEnd);
            if (t != std::string::npos && t < close) {
                size_t ts = h.find('>', t), te = h.find("</div>", ts);
                if (ts != std::string::npos && te != std::string::npos) hit.title = trim(htmlToText(h.substr(ts + 1, te - ts - 1)));
            }
        }
        size_t next = h.find(anchorMark, close), sp = h.find(snippetMark, close);
        if (sp != std::string::npos && sp < next) {
            size_t se = h.find('>', sp), sc = h.find(snippetEnd, se);
            if (se != std::string::npos && sc != std::string::npos) hit.snippet = trim(htmlToText(h.substr(se + 1, sc - se - 1)));
        }
        if (size_t u = hit.url.find("uddg="); u != std::string::npos)
            hit.url = urlDecode(hit.url.substr(u + 5, hit.url.find('&', u) - u - 5));
        if (startsWith(hit.url, "http") && hit.url.find("duckduckgo.com/y.js") == std::string::npos)
            out.push_back(std::move(hit));
        p = close;
    }
    return out;
}

int kitSearch(const std::vector<std::string>& a) {
    std::string q = join(a, " ");
    if (trim(q).empty()) return fail("usage: kit search QUERY");
    std::string enc;
    for (unsigned char c : q) {
        char b[4];
        if (isalnum(c) || c == '-' || c == '_' || c == '.') enc += (char)c;
        else if (c == ' ') enc += '+';
        else snprintf(b, sizeof b, "%%%02X", c), enc += b;
    }
    // Keyless endpoints in order (engines that serve decoys to scripts, like
    // Bing, are deliberately absent: no results beats wrong results).
    struct Engine {
        const char *url, *anchor, *snip, *snipEnd, *title;
    } engines[] = {
        {"https://html.duckduckgo.com/html/?q=", "class=\"result__a\"", "class=\"result__snippet\"", "</a>", ""},
        {"https://lite.duckduckgo.com/lite/?q=", "class='result-link'", "class='result-snippet'", "</td>", ""},
        // Third opinion when DuckDuckGo rate-limits: hashed class names change, so anchor on the stable ones.
        {"https://search.brave.com/search?q=", " l1\"><div class=\"site-name-wrapper", "class=\"content desktop-default-regular", "</div>", "search-snippet-title"},
    };
    for (const auto& e : engines) {
        auto body = fetch(e.url + enc, 12);
        if (!body.ok) continue;
        std::vector<Hit> hits = scrape(body.value, e.anchor, e.snip, e.snipEnd, e.title);
        if (hits.empty()) continue;
        for (size_t i = 0; i < hits.size(); ++i)
            printf("%zu. %s\n   %s\n   %s\n", i + 1, hits[i].title.c_str(), hits[i].url.c_str(), hits[i].snippet.c_str());
        return 0;
    }
    return fail("no results: every keyless engine refused (rate limit); retry later or kit web a known URL");
}

int kitShot(std::vector<std::string> a, bool dom) {
    // Models write `--size 1280x800` as often as the bare form: accept both.
    if (!dom && a.size() == 4 && a[2] == "--size") { a[2] = a[3]; a.pop_back(); }
    if (a.size() < (dom ? 1u : 2u) || a.size() > (dom ? 1u : 3u))
        return fail(dom ? "usage: kit dom URL" : "usage: kit shot URL OUT.png [WxH]");
    if (a[0].empty() || a[0][0] == '-' || a[0].find_first_of("\r\n") != std::string::npos)
        return fail("invalid browser URL");
    unsigned width = 1280, height = 800;
    if (!dom && a.size() == 3 && !shotSize(a[2], width, height))
        return fail("size must be WIDTHxHEIGHT, 1..8192 per dimension and at most 32 million pixels");
    std::string exe = chromeExe();
    if (exe.empty()) return fail("no Chrome/Chromium on PATH");
    ChromeScratch scratch;
    if (scratch.path.empty()) return fail("cannot create private Chrome directory");
    auto args = chromeArgs(exe, scratch.path + "/profile");
    const std::string output = scratch.path + "/frame.png";
    if (dom) args.push_back("--dump-dom");
    else {
        args.push_back("--window-size=" + std::to_string(width) + "," + std::to_string(height));
        args.push_back("--screenshot=" + output);
    }
    args.push_back(a[0]);
    SpawnResult r = run(args, 60000, 16 << 20);
    if (!r.ok || r.exitCode != 0 || r.truncated)
        return fail("chrome failed: " + (r.truncated ? std::string("output limit exceeded") : trim((r.err + r.error).substr(0, 400))));
    if (dom) printf("%s", htmlToText(r.out).c_str());
    else {
        auto png = readFileBounded(output, 128 << 20);
        unsigned actualWidth = 0, actualHeight = 0;
        if (!png.ok || !pngSize(png.value, actualWidth, actualHeight) || actualWidth != width || actualHeight != height)
            return fail("Chrome did not produce a valid screenshot at the requested size");
        auto saved = atomicWriteFile(a[1], png.value);
        if (!saved.ok) return fail(saved.error);
        printf("screenshot: %s\n", a[1].c_str());
    }
    return 0;
}

int kitImg(const std::vector<std::string>& a) {
    if (a.empty()) return fail("usage: kit img FILE...");
    bool failed = false;
    for (const auto& path : a) {
        auto t = readFileBounded(path, 64 << 20);
        if (!t.ok) { fprintf(stderr, "%s: %s\n", path.c_str(), t.error.c_str()); failed = true; continue; }
        const auto* b = (const unsigned char*)t.value.data();
        size_t n = t.value.size();
        unsigned w = 0, h = 0;
        std::string kind;
        auto be16 = [&](size_t i) { return (unsigned)(b[i] << 8 | b[i + 1]); };
        auto le32 = [&](size_t i) { return (uint32_t)b[i] | (uint32_t)b[i + 1] << 8 | (uint32_t)b[i + 2] << 16 | (uint32_t)b[i + 3] << 24; };
        if (pngSize(t.value, w, h)) kind = "png";
        else if (n >= 13 && (!memcmp(b, "GIF87a", 6) || !memcmp(b, "GIF89a", 6)))
            kind = "gif", w = b[6] | b[7] << 8, h = b[8] | b[9] << 8;
        else if (n >= 20 && !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WEBP", 4) &&
                 le32(4) >= 12 && le32(4) <= n - 8 && le32(16) <= le32(4) - 12) {
            kind = "webp";
            if (le32(16) >= 10 && n >= 30 && !memcmp(b + 12, "VP8X", 4))
                w = 1 + (b[24] | b[25] << 8 | b[26] << 16), h = 1 + (b[27] | b[28] << 8 | b[29] << 16);
            else if (le32(16) >= 10 && n >= 30 && !memcmp(b + 12, "VP8 ", 4) && !memcmp(b + 23, "\x9d\x01\x2a", 3))
                w = (b[26] | b[27] << 8) & 0x3fff, h = (b[28] | b[29] << 8) & 0x3fff;
            else if (le32(16) >= 5 && n >= 25 && !memcmp(b + 12, "VP8L", 4) && b[20] == 0x2f)
                w = 1 + ((b[21] | b[22] << 8) & 0x3fff), h = 1 + ((b[22] >> 6 | b[23] << 2 | b[24] << 10) & 0x3fff);
        } else if (n >= 4 && b[0] == 0xff && b[1] == 0xd8) {
            kind = "jpeg";
            for (size_t i = 2; i < n;) {
                if (b[i++] != 0xff) break;
                while (i < n && b[i] == 0xff) ++i;
                if (i == n) break;
                const unsigned marker = b[i++];
                if (marker == 0xd9 || marker == 0xda || marker == 0) break;
                if (marker == 1 || (marker >= 0xd0 && marker <= 0xd7)) continue;
                if (i + 2 > n) break;
                const size_t length = be16(i);
                if (length < 2 || length > n - i) break;
                if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
                    if (length >= 8 && b[i + 7] > 0 && length >= 8 + 3u * b[i + 7])
                        h = be16(i + 3), w = be16(i + 5);
                    break;
                }
                i += length;
            }
        } else {
            size_t p = t.value.find("<svg"), end = p == std::string::npos ? p : t.value.find('>', p);
            if (p != std::string::npos && end != std::string::npos && p + 4 < n &&
                (isspace(b[p + 4]) || b[p + 4] == '>' || b[p + 4] == '/')) {
                std::string tag = t.value.substr(p, end - p);
                printf("%s: svg viewBox=\"%s\" width=%s height=%s, %zu bytes\n", path.c_str(), htmlAttr(tag, "viewbox").c_str(),
                       htmlAttr(tag, "width").c_str(), htmlAttr(tag, "height").c_str(), n);
                continue;
            }
        }
        if (kind.empty() || !w || !h) {
            fprintf(stderr, "%s: unsupported or invalid image header\n", path.c_str());
            failed = true;
        } else printf("%s: %s %ux%u, %zu bytes\n", path.c_str(), kind.c_str(), w, h, n);
    }
    return failed ? 1 : 0;
}

int kitSvg(const std::vector<std::string>& a) {
    if (a.empty()) return fail("usage: kit svg FILE.svg");
    auto t = readFileBounded(a[0], 32 << 20);
    if (!t.ok) return fail(t.error);
    const std::string& s = t.value;
    std::vector<std::string> stack, problems;
    std::map<std::string, int> ids, counts;
    for (size_t i = 0; (i = s.find('<', i)) != std::string::npos;) {
        if (!s.compare(i, 4, "<!--")) { i = s.find("-->", i); if (i == std::string::npos) break; continue; }
        if (!s.compare(i, 9, "<![CDATA[")) { i = s.find("]]>", i); if (i == std::string::npos) break; continue; }
        size_t e = s.find('>', i);
        if (e == std::string::npos) { problems.push_back("unterminated tag"); break; }
        std::string tag = s.substr(i + 1, e - i - 1);
        i = e;
        if (tag.empty() || tag[0] == '?' || tag[0] == '!') continue;
        bool close = tag[0] == '/', self = tag.back() == '/';
        std::string name;
        for (size_t k = close; k < tag.size() && !isspace((unsigned char)tag[k]) && tag[k] != '/'; ++k) name += tag[k];
        if (close) {
            if (stack.empty() || stack.back() != name) problems.push_back("mismatched </" + name + ">");
            else stack.pop_back();
            continue;
        }
        ++counts[name];
        std::string id = htmlAttr(tag, "id");
        if (!id.empty() && ++ids[id] == 2) problems.push_back("duplicate id \"" + id + "\"");
        if (name == "svg" && stack.empty() && htmlAttr(tag, "viewbox").empty()) problems.push_back("root <svg> lacks viewBox (won't scale)");
        if (!self) stack.push_back(name);
    }
    for (const auto& n : stack) problems.push_back("unclosed <" + n + ">");
    long anim = counts["animate"] + counts["animateTransform"] + counts["animateMotion"] + counts["set"];
    size_t kf = 0;
    for (size_t p = 0; (p = s.find("@keyframes", p)) != std::string::npos; ++p) ++kf;
    printf("%s: %zu bytes, %ld elements, %ld SMIL animations, %zu CSS keyframes, %zu ids\n", a[0].c_str(),
           s.size(), [&] { long n = 0; for (auto& [k, v] : counts) n += v; return n; }(), anim, kf, ids.size());
    for (const auto& p : problems) printf("  problem: %s\n", p.c_str());
    return problems.empty() ? 0 : 1;
}

int kitSpring(const std::vector<std::string>& a) {
    double values[] = {170, 26, 1};
    if (a.size() > 3) return fail("usage: kit spring [stiffness=170] [damping=26] [mass=1]");
    for (size_t i = 0; i < a.size(); ++i) {
        char* end = nullptr;
        errno = 0;
        values[i] = strtod(a[i].c_str(), &end);
        if (end == a[i].c_str() || *end || errno == ERANGE || !std::isfinite(values[i]) || values[i] <= 0)
            return fail("spring parameters must be finite positive numbers; damping=0 never settles");
    }
    const double k = values[0], c = values[1], m = values[2];
    double ms = 0;
    std::string e = springEasing(k, c, m, &ms);
    if (e.empty()) return fail("spring cannot be represented by a settled easing within 60 seconds and 2048 samples");
    double zeta = (c / m) / 2 / (std::sqrt(k) / std::sqrt(m));
    printf("spring k=%g c=%g m=%g · damping ratio %.4g (%s) · settles in %.6g ms\n", k, c, m, zeta,
           std::fabs(zeta - 1) < 1e-8 ? "critical" : zeta < 1 ? "bouncy" : "overdamped", ms);
    printf("css: transition: transform %.6gms %s;\n", ms, e.c_str());
    printf("presets: gentle 120 14 · wobbly 180 12 · stiff 210 20 · slow 280 60 · snappy 400 30\n");
    return 0;
}

int kitSlop(const std::vector<std::string>& a) {
    int found = 0;
    bool failed = false;
    for (const auto& p : a) {
        auto t = readFileBounded(p, 16 << 20);
        if (!t.ok) { fprintf(stderr, "%s: %s\n", p.c_str(), t.error.c_str()); failed = true; continue; }
        for (const auto& f : slopScan(p, t.value)) printf("%s: %s\n", p.c_str(), f.c_str()), ++found;
    }
    if (a.empty()) return fail("usage: kit slop FILE...");
    if (!found && !failed) printf("clean: no placeholders, stubs, conflict markers or AI-tell phrasing\n");
    return found || failed ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Code index: BM25 over 40-line chunks + a symbol table from declaration
// patterns. Replaces vector DBs, embeddings and LSP servers for "where is X
// / what implements Y" with a few milliseconds of native scanning.
// ---------------------------------------------------------------------------
struct CodeFile {
    std::string path, text;
};

std::vector<CodeFile> codeFiles(const std::string& root) {
    std::vector<CodeFile> out;
    SpawnResult g = run({"git", "-C", root, "ls-files", "-co", "--exclude-standard"}, 10000, 16 << 20);
    std::vector<std::string> paths;
    if (g.ok && g.exitCode == 0) paths = splitLines(g.out);
    else {
        SpawnResult f = run({"find", root, "-type", "f", "-not", "-path", "*/.*", "-not", "-path", "*/node_modules/*",
                             "-not", "-path", "*/build/*", "-not", "-path", "*/dist/*"}, 10000, 16 << 20);
        for (auto& p : splitLines(f.out)) paths.push_back(p.substr(std::min(p.size(), root.size() + 1)));
    }
    size_t total = 0;
    for (const auto& p : paths) {
        if (p.empty() || out.size() >= 20000 || total > (256u << 20)) break;
        auto t = readFileBounded(root + "/" + p, 1 << 20);
        if (!t.ok || t.value.find('\0') != std::string::npos) continue;  // binary or huge
        total += t.value.size();
        out.push_back({p, std::move(t.value)});
    }
    return out;
}

// Declared name on a line, or "" (functions, classes, types, consts...).
std::string declName(const std::string& line) {
    static const char* kw[] = {"def ", "class ", "struct ", "enum ", "interface ", "type ", "fn ", "func ",
                               "function ", "trait ", "impl ", "module ", "namespace ", "const ", "let ", "var ",
                               "macro_rules! ", "#define ", "union "};
    std::string t = trim(line);
    for (const char* pre : {"export ", "pub ", "public ", "private ", "protected ", "static ", "async ",
                            "default ", "abstract ", "final ", "inline ", "virtual ", "extern "})
        while (startsWith(t, pre)) t = trim(t.substr(strlen(pre)));
    for (const char* k : kw)
        if (startsWith(t, k)) {
            std::string rest = trim(t.substr(strlen(k)));
            if (startsWith(rest, "(")) {  // Go method receiver: func (r *T) Name(
                size_t c = rest.find(')');
                rest = c == std::string::npos ? "" : trim(rest.substr(c + 1));
            }
            std::string n;
            for (char c : rest) {
                if (isalnum((unsigned char)c) || c == '_' || c == ':' || c == '$') n += c;
                else break;
            }
            return n;
        }
    // C/C++/Java-style definition: "type name(args) {" at column 0.
    if (!line.empty() && !isspace((unsigned char)line[0]) && line.find('(') != std::string::npos &&
        (endsWith(trim(line), "{") || endsWith(trim(line), ")")) && line.find(';') == std::string::npos &&
        line.find('=') == std::string::npos && !startsWith(t, "if") && !startsWith(t, "for") &&
        !startsWith(t, "while") && !startsWith(t, "return") && !startsWith(t, "#")) {
        size_t paren = line.find('(');
        size_t e = paren;
        while (e > 0 && isspace((unsigned char)line[e - 1])) --e;
        size_t b = e;
        while (b > 0 && (isalnum((unsigned char)line[b - 1]) || line[b - 1] == '_' || line[b - 1] == ':' ||
                         line[b - 1] == '~'))
            --b;
        return line.substr(b, e - b);
    }
    return "";
}

int kitFind(const std::vector<std::string>& a) {
    if (a.empty()) return fail("usage: kit find QUERY  (ranked code chunks)");
    auto files = codeFiles(".");
    std::vector<std::string> docs;
    std::vector<std::pair<size_t, int>> where;  // file index, first line
    for (size_t f = 0; f < files.size(); ++f) {
        auto lines = splitLines(files[f].text);
        for (size_t i = 0; i < lines.size(); i += 30) {
            std::string chunk = files[f].path + " " + files[f].path + "\n";
            for (size_t k = i; k < std::min(lines.size(), i + 40); ++k) chunk += lines[k] + "\n";
            docs.push_back(std::move(chunk));
            where.push_back({f, (int)i + 1});
        }
    }
    std::vector<double> sc = bm25(docs, join(a, " "));
    std::vector<size_t> order;
    for (size_t i = 0; i < sc.size(); ++i)
        if (sc[i] > 0) order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](size_t x, size_t y) { return sc[x] > sc[y]; });
    std::set<std::string> shown;
    for (size_t r = 0; r < order.size() && shown.size() < 12; ++r) {
        auto [f, line] = where[order[r]];
        auto lines = splitLines(files[f].text);
        // Show the best-matching line of the chunk as the preview.
        std::vector<std::string> q = words(join(a, " "));
        int best = line;
        size_t bestHits = 0;
        for (int k = line; k < line + 40 && k <= (int)lines.size(); ++k) {
            std::string l = toLower(lines[(size_t)k - 1]);
            size_t hits = 0;
            for (const auto& w : q) hits += l.find(w) != std::string::npos;
            if (hits > bestHits) bestHits = hits, best = k;
        }
        if (!shown.insert(files[f].path + ":" + std::to_string(best)).second) continue;
        printf("%s:%d  (%.1f)  %s\n", files[f].path.c_str(), best, sc[order[r]],
               trim(lines[(size_t)best - 1]).substr(0, 140).c_str());
    }
    if (order.empty()) printf("no matches in %zu files\n", files.size());
    return 0;
}

int kitSym(const std::vector<std::string>& a, bool refs) {
    if (a.empty()) return fail(refs ? "usage: kit refs NAME" : "usage: kit sym [NAME]  (definitions; no NAME = outline)");
    auto files = codeFiles(".");
    std::string want = a[0];
    int n = 0;
    for (const auto& f : files) {
        int ln = 0;
        for (const auto& line : splitLines(f.text)) {
            ++ln;
            if (n >= 200) break;
            if (refs) {
                for (size_t p = 0; (p = line.find(want, p)) != std::string::npos; p += want.size()) {
                    bool l = p == 0 || !(isalnum((unsigned char)line[p - 1]) || line[p - 1] == '_');
                    size_t e = p + want.size();
                    bool r = e >= line.size() || !(isalnum((unsigned char)line[e]) || line[e] == '_');
                    if (l && r) {
                        printf("%s:%d: %s\n", f.path.c_str(), ln, trim(line).substr(0, 160).c_str()), ++n;
                        break;
                    }
                }
                continue;
            }
            std::string d = declName(line);
            if (d.empty()) continue;
            std::string base = d.substr(d.rfind(':') == std::string::npos ? 0 : d.rfind(':') + 1);
            if (want == "." || base == want || d == want || fuzzyScore(want, base) >= 0.95)
                printf("%s:%d: %s\n", f.path.c_str(), ln, trim(line).substr(0, 160).c_str()), ++n;
        }
    }
    if (!n) printf("no %s for %s in %zu files\n", refs ? "references" : "definitions", want.c_str(), files.size());
    return 0;
}

}  // namespace

int kitMain(int argc, char** argv) {
    std::string sub = argc > 1 ? argv[1] : "";
    std::vector<std::string> a(argv + std::min(argc, 2), argv + argc);
    static const char* kHelp =
        "pocket kit — native superpowers (run via bash)\n"
        "  web URL [--raw]        fetch a page as readable text + numbered links\n"
        "  search QUERY           web search (title, url, snippet)\n"
        "  dom URL                JS-rendered page text via headless Chrome\n"
        "  shot URL OUT.png [WxH] screenshot via headless Chrome (visual QA)\n"
        "  frame HTML OUT.png     deterministic scene frame (--time, --size)\n"
        "  video HTML OUT.mp4     render a scene (--duration, --fps, --size, --audio, --start, --timeout)\n"
        "  vcheck FILE.mp4        finished-video QA: streams, duration, black/frozen/silent spans, clipping\n"
        "  vsheet FILE.mp4 OUT.png contact sheet of the whole video in one image (timestamped)\n"
        "  say OUT.wav \"text\"     neural narration (Piper) + timing json, srt, captions.html, cues.css; --setup once\n"
        "  asset search|get|font|inspect   open-licensed images/audio/3D/HDRI/fonts + three.js, with ATTRIBUTION.txt\n"
        "  theme \"topic\"          palette + font pair derived from the subject (avoids the default looks)\n"
        "  sfx OUT.wav PRESET     native click/chime/laser/whoosh/impact/tone/noise\n"
        "  music OUT.wav          generative stereo score: --style ambient/lofi/corporate/cinematic/tech/upbeat --duration --key --seed\n"
        "  mix OUT.wav            cue-sheet mixer: --voice --music (auto-ducked) --at SEC:FILE, loudness-normalised + limited\n"
        "  img FILE...            image type + dimensions (png/jpeg/gif/webp/svg)\n"
        "  svg FILE               SVG structure: viewBox, ids, animation count (kit lint covers the rest)\n"
        "  spring [k] [c] [m]     physical spring -> CSS linear() easing + duration\n"
        "  wav OUT \"C4:.25 R:.25\"  music to WAV: chords C4+E4, tracks a|b, drums K/S/H, saw> prefix\n"
        "  audio FILE.wav         loudness, peak, clipping, pitch, silence\n"
        "  lint [PATH...]         security, perf, DRY, backend/coding patterns, UI slop + a11y, SVG hygiene, stubs (runs automatically on every write)\n"
        "  quality FILE           native checks + targeted Jev review (--semantic, --intent TEXT); advisory findings\n"
        "  find QUERY             ranked code search (BM25 over chunks; no index to maintain)\n"
        "  sym NAME|.             definitions of NAME (\".\" = outline of every declaration)\n"
        "  refs NAME              whole-word references to NAME\n"
        "  ports [PORT]           listening TCP sockets with owning pid + command\n"
        "  wait PORT|URL [SEC]    block until a server accepts (instead of sleep)\n"
        "  net URL                HTTP timing (dns/connect/tls/ttfb), redirects, headers\n"
        "  reach HOST:PORT [SEC]   bounded DNS/NSS and TCP reachability evidence; bracket IPv6\n"
        "  sys [PID]              Linux pressure, memory/disk or sanitized process diagnostics\n"
        "  seo FILE|URL           on-page SEO: title, meta, canonical, headings, alt, OG, JSON-LD\n"
        "  csv FILE               dataset profile: types, missing, stats, class balance\n"
        "  bench [-n N] 'CMD'      timing: min/median/p95 wall, CPU, peak RSS\n"
        "  probe [DIR]            host: OS, CPU, memory, disk, GPU, toolchain\n";
    // `kit SUB --help`, and a bare `kit SUB` whose first argument is required, are questions,
    // not failures: print that line, exit 0 (agents probe usage this way).
    for (const auto& line : splitLines(kHelp))
        if (startsWith(line, "  " + sub + " ")) {
            std::string rest = trim(line.substr(3 + sub.size()));
            if ((!a.empty() && (a[0] == "--help" || a[0] == "-h")) || (a.empty() && !rest.empty() && rest[0] != '[')) {
                printf("usage: pocket kit %s\n", trim(line).c_str());
                return 0;
            }
        }
    if (sub == "--help" || sub == "-h") sub = "help";
    if (sub == "web") return kitWeb(a);
    if (sub == "search") return kitSearch(a);
    if (sub == "shot") return kitShot(a, false);
    if (sub == "dom") return kitShot(a, true);
    if (sub == "probe") { printf("%s", hostProbe(a.empty() ? "." : a[0]).c_str()); return 0; }
    if (sub == "img") return kitImg(a);
    if (sub == "video") return kitVideo(a);
    if (sub == "frame") return kitFrame(a);
    if (sub == "vcheck") return kitVcheck(a);
    if (sub == "sfx") return kitSfx(a);
    if (sub == "music") return kitMusic(a);
    if (sub == "mix") return kitMix(a);
    if (sub == "say") return kitSay(a);
    if (sub == "asset") return kitAsset(a);
    if (sub == "theme") return kitTheme(a);
    if (sub == "vsheet") return kitVsheet(a);
    if (sub == "wav") return kitWav(a);
    if (sub == "audio") return kitAudio(a);
    if (sub == "svg") return kitSvg(a);
    if (sub == "spring") return kitSpring(a);
    if (sub == "slop") return kitSlop(a);
    if (sub == "lint") return kitLint(a);
    if (sub == "quality") return kitQuality(a);
    if (sub == "find") return kitFind(a);
    if (sub == "sym") return kitSym(a, false);
    if (sub == "refs") return kitSym(a, true);
    if (sub == "ports") return kitPorts(a);
    if (sub == "wait") return kitWait(a);
    if (sub == "net") return kitNet(a);
    if (sub == "reach") return kitReach(a);
    if (sub == "sys") return kitSys(a);
    if (sub == "seo") return kitSeo(a);
    if (sub == "csv") return kitCsv(a);
    if (sub == "bench") return kitBench(a);
    printf("%s", kHelp);
    return sub.empty() || sub == "help" ? 0 : 2;
}

}  // namespace pocket
