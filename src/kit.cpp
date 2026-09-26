// PocketHarness - `pocket kit` native superpowers.
#include "kit.h"

#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
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

std::string attr(std::string_view tag, const std::string& name) {
    std::string lt = toLower(tag);
    size_t p = 0;
    while ((p = lt.find(name + "=", p)) != std::string::npos) {
        if (p > 0 && !isspace((unsigned char)lt[p - 1])) { ++p; continue; }
        p += name.size() + 1;
        char q = p < tag.size() ? tag[p] : 0;
        if (q == '"' || q == '\'') {
            size_t e = tag.find(q, p + 1);
            return std::string(tag.substr(p + 1, e == std::string_view::npos ? std::string_view::npos : e - p - 1));
        }
        size_t e = tag.find_first_of(" \t\n>", p);
        return std::string(tag.substr(p, e == std::string_view::npos ? std::string_view::npos : e - p));
    }
    return "";
}

}  // namespace

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
            if (!close) { href = attr(tag, "href"); linkStart = out.size(); }
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

double noteFreq(const std::string& note) {
    if (note.empty()) return -1;
    if (isdigit((unsigned char)note[0])) {
        double f = atof(note.c_str());
        return f > 0 && f < 30000 ? f : -1;
    }
    static const int semis[] = {9, 11, 0, 2, 4, 5, 7};  // A B C D E F G
    char n = (char)toupper((unsigned char)note[0]);
    if (n < 'A' || n > 'G') return -1;
    int s = semis[n - 'A'];
    size_t i = 1;
    if (i < note.size() && note[i] == '#') ++s, ++i;
    else if (i < note.size() && note[i] == 'b') --s, ++i;
    if (i >= note.size() || !(isdigit((unsigned char)note[i]) || note[i] == '-')) return -1;
    int oct = atoi(note.c_str() + i);
    int midi = (oct + 1) * 12 + s;
    return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
}

std::string springEasing(double k, double c, double m, double* durationMs) {
    double x = 0, v = 0, dt = 1.0 / 1000;
    std::vector<double> xs;
    int settled = 0;
    for (int i = 0; i < 10000; ++i) {
        double a = (-k * (x - 1) - c * v) / m;
        v += a * dt;
        x += v * dt;
        xs.push_back(x);
        settled = std::fabs(x - 1) < 0.001 && std::fabs(v) < 0.01 ? settled + 1 : 0;
        if (settled > 20) break;
    }
    if (durationMs) *durationMs = (double)xs.size();
    std::string s = "linear(0";
    const int n = 40;
    for (int i = 1; i <= n; ++i) {
        char b[16];
        snprintf(b, sizeof b, ", %.3g", i == n ? 1.0 : xs[(xs.size() - 1) * i / n]);
        s += b;
    }
    return s + ")";
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
    return s;
}

namespace {

std::string chromeExe() {
    for (const char* c : {"google-chrome", "chromium", "chromium-browser", "google-chrome-stable"})
        if (!whichExe(c).empty()) return c;
    return "";
}

std::vector<std::string> chromeArgs(const std::string& exe) {
    const char* td = getenv("TMPDIR");
    std::string prof = std::string(td && *td ? td : "/tmp") + "/pocket-chrome-" + std::to_string(getuid());
    // Already inside Landlock+seccomp: Chrome's namespace sandbox can't nest.
    return {exe, "--headless=new", "--disable-gpu", "--no-sandbox", "--hide-scrollbars",
            "--no-first-run", "--disable-dev-shm-usage", "--user-data-dir=" + prof, "--virtual-time-budget=4000"};
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
                        const std::string& snippetEnd) {
    std::vector<Hit> out;
    for (size_t p = 0; (p = h.find(anchorMark, p)) != std::string::npos && out.size() < 10;) {
        size_t open = h.rfind("<a", p), tagEnd = h.find('>', p), close = h.find("</a>", tagEnd);
        if (open == std::string::npos || tagEnd == std::string::npos || close == std::string::npos) break;
        Hit hit;
        hit.url = decodeEntities(attr(std::string_view(h).substr(open, tagEnd - open), "href"));
        hit.title = trim(htmlToText(h.substr(tagEnd + 1, close - tagEnd - 1)));
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
        const char *url, *anchor, *snip, *snipEnd;
    } engines[] = {
        {"https://html.duckduckgo.com/html/?q=", "class=\"result__a\"", "class=\"result__snippet\"", "</a>"},
        {"https://lite.duckduckgo.com/lite/?q=", "class='result-link'", "class='result-snippet'", "</td>"},
    };
    for (const auto& e : engines) {
        auto body = fetch(e.url + enc, 12);
        if (!body.ok) continue;
        std::vector<Hit> hits = scrape(body.value, e.anchor, e.snip, e.snipEnd);
        if (hits.empty()) continue;
        for (size_t i = 0; i < hits.size(); ++i)
            printf("%zu. %s\n   %s\n   %s\n", i + 1, hits[i].title.c_str(), hits[i].url.c_str(), hits[i].snippet.c_str());
        return 0;
    }
    return fail("no results: every keyless engine refused (rate limit); retry later or kit web a known URL");
}

int kitShot(const std::vector<std::string>& a, bool dom) {
    if (a.size() < (dom ? 1u : 2u)) return fail(dom ? "usage: kit dom URL" : "usage: kit shot URL OUT.png [WxH]");
    std::string exe = chromeExe();
    if (exe.empty()) return fail("no Chrome/Chromium on PATH");
    auto args = chromeArgs(exe);
    if (dom) args.push_back("--dump-dom");
    else {
        std::string size = a.size() > 2 ? a[2] : "1280x800";
        std::replace(size.begin(), size.end(), 'x', ',');
        args.push_back("--window-size=" + size);
        args.push_back("--screenshot=" + a[1]);
    }
    args.push_back(a[0]);
    SpawnResult r = run(args, 60000, 16 << 20);
    if (!r.ok || r.exitCode != 0) return fail("chrome failed: " + trim(r.err.substr(0, 400)));
    if (dom) printf("%s", htmlToText(r.out).c_str());
    else printf("screenshot: %s\n", a[1].c_str());
    return 0;
}

int kitImg(const std::vector<std::string>& a) {
    for (const auto& path : a) {
        auto t = readFileBounded(path, 64 << 20);
        if (!t.ok) { printf("%s: %s\n", path.c_str(), t.error.c_str()); continue; }
        const auto* b = (const unsigned char*)t.value.data();
        size_t n = t.value.size();
        long w = -1, h = -1;
        std::string kind = "unknown";
        auto be16 = [&](size_t i) { return (long)(b[i] << 8 | b[i + 1]); };
        auto be32 = [&](size_t i) { return (long)((unsigned long)b[i] << 24 | b[i + 1] << 16 | b[i + 2] << 8 | b[i + 3]); };
        if (n > 24 && !memcmp(b, "\x89PNG", 4)) kind = "png", w = be32(16), h = be32(20);
        else if (n > 10 && !memcmp(b, "GIF8", 4)) kind = "gif", w = b[6] | b[7] << 8, h = b[8] | b[9] << 8;
        else if (n > 30 && !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WEBP", 4)) {
            kind = "webp";
            if (!memcmp(b + 12, "VP8X", 4)) w = 1 + (b[24] | b[25] << 8 | b[26] << 16), h = 1 + (b[27] | b[28] << 8 | b[29] << 16);
            else if (!memcmp(b + 12, "VP8 ", 4)) w = (b[26] | b[27] << 8) & 0x3fff, h = (b[28] | b[29] << 8) & 0x3fff;
            else if (!memcmp(b + 12, "VP8L", 4)) w = 1 + ((b[21] | b[22] << 8) & 0x3fff), h = 1 + ((b[22] >> 6 | b[23] << 2 | b[24] << 10) & 0x3fff);
        } else if (n > 4 && b[0] == 0xff && b[1] == 0xd8) {
            kind = "jpeg";
            for (size_t i = 2; i + 9 < n;) {
                if (b[i] != 0xff) { ++i; continue; }
                unsigned m = b[i + 1];
                if (m >= 0xc0 && m <= 0xcf && m != 0xc4 && m != 0xc8 && m != 0xcc) { h = be16(i + 5); w = be16(i + 7); break; }
                i += 2 + (size_t)be16(i + 2);
            }
        } else if (t.value.find("<svg") != std::string::npos) {
            kind = "svg";
            size_t p = t.value.find("<svg");
            std::string tag = t.value.substr(p, t.value.find('>', p) - p);
            printf("%s: svg viewBox=\"%s\" width=%s height=%s, %zu bytes\n", path.c_str(), attr(tag, "viewbox").c_str(),
                   attr(tag, "width").c_str(), attr(tag, "height").c_str(), n);
            continue;
        }
        printf("%s: %s %ldx%ld, %zu bytes\n", path.c_str(), kind.c_str(), w, h, n);
    }
    return a.empty() ? fail("usage: kit img FILE...") : 0;
}

void putLe(std::string& s, uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) s += (char)(v >> (8 * i) & 0xff);
}

int kitWav(const std::vector<std::string>& a) {
    if (a.size() < 2) return fail("usage: kit wav OUT.wav \"C4:.25 E4:.25 R:.25 440:.5\" [--wave sine|square|saw|tri] [--bpm N] [--gain G]");
    std::string wave = "sine";
    double bpm = 0, gain = 0.3;
    for (size_t i = 2; i + 1 < a.size(); i += 2) {
        if (a[i] == "--wave") wave = a[i + 1];
        else if (a[i] == "--bpm") bpm = atof(a[i + 1].c_str());
        else if (a[i] == "--gain") gain = std::clamp(atof(a[i + 1].c_str()), 0.0, 1.0);
    }
    const int rate = 44100;
    std::string pcm;
    double phase = 0;
    std::string spec = a[1];
    std::replace(spec.begin(), spec.end(), ',', ' ');
    std::vector<std::string> events;
    for (const auto& l : splitLines(spec))
        for (size_t p = 0; p < l.size();) {
            size_t q = l.find(' ', p);
            if (q != p) events.push_back(l.substr(p, q == std::string::npos ? std::string::npos : q - p));
            p = q == std::string::npos ? l.size() : q + 1;
        }
    for (const auto& ev : events) {
        size_t colon = ev.find(':');
        std::string note = ev.substr(0, colon);
        double dur = colon == std::string::npos ? 0.25 : atof(ev.c_str() + colon + 1);
        if (bpm > 0) dur *= 60.0 / bpm;
        double f = toupper((unsigned char)note[0]) == 'R' ? 0 : noteFreq(note);
        if (f < 0 || dur <= 0 || dur > 60) return fail("bad event \"" + ev + "\"");
        long n = (long)(dur * rate);
        for (long i = 0; i < n; ++i) {
            double t = (double)i / rate, env = std::min({1.0, t / 0.005, (dur - t) / 0.03});
            phase += f / rate;
            phase -= std::floor(phase);
            double s = wave == "square" ? (phase < 0.5 ? 1 : -1)
                       : wave == "saw"  ? 2 * phase - 1
                       : wave == "tri"  ? 1 - 4 * std::fabs(phase - 0.5)
                                        : std::sin(2 * M_PI * phase);
            putLe(pcm, (uint16_t)(int16_t)std::lround(f > 0 ? s * std::max(0.0, env) * gain * 32767 : 0), 2);
        }
    }
    std::string wav = "RIFF";
    putLe(wav, 36 + (uint32_t)pcm.size(), 4);
    wav += "WAVEfmt ";
    putLe(wav, 16, 4), putLe(wav, 1, 2), putLe(wav, 1, 2), putLe(wav, rate, 4), putLe(wav, rate * 2, 4);
    putLe(wav, 2, 2), putLe(wav, 16, 2);
    wav += "data";
    putLe(wav, (uint32_t)pcm.size(), 4);
    auto w = atomicWriteFile(a[0], wav + pcm);
    if (!w.ok) return fail(w.error);
    printf("%s: %.2fs mono 16-bit %d Hz (%s)\n", a[0].c_str(), pcm.size() / 2.0 / rate, rate, wave.c_str());
    return 0;
}

int kitAudio(const std::vector<std::string>& a) {
    if (a.empty()) return fail("usage: kit audio FILE.wav");
    auto t = readFileBounded(a[0], 512 << 20);
    if (!t.ok) return fail(t.error);
    const std::string& d = t.value;
    if (d.size() < 44 || d.compare(0, 4, "RIFF") || d.compare(8, 4, "WAVE")) return fail("not a RIFF/WAVE file (convert with ffmpeg -i in.x out.wav)");
    auto le = [&](size_t i, int n) { uint32_t v = 0; for (int k = n - 1; k >= 0; --k) v = v << 8 | (unsigned char)d[i + k]; return v; };
    int ch = 0, bits = 0;
    long rate = 0;
    size_t data = 0, len = 0;
    for (size_t p = 12; p + 8 <= d.size();) {
        uint32_t sz = le(p + 4, 4);
        if (!d.compare(p, 4, "fmt ")) ch = (int)le(p + 10, 2), rate = le(p + 12, 4), bits = (int)le(p + 22, 2);
        if (!d.compare(p, 4, "data")) { data = p + 8; len = std::min<size_t>(sz, d.size() - data); break; }
        p += 8 + sz + (sz & 1);
    }
    if (!data || bits != 16 || ch < 1 || rate <= 0) return fail("need 16-bit PCM WAV");
    size_t frames = len / (2 * (size_t)ch);
    std::vector<float> mono(frames);
    double sum = 0, peak = 0;
    long clip = 0;
    for (size_t i = 0; i < frames; ++i) {
        double s = 0;
        for (int c = 0; c < ch; ++c) {
            double v = (int16_t)le(data + (i * ch + c) * 2, 2) / 32768.0;
            if (std::fabs(v) > 0.999) ++clip;
            s += v;
        }
        s /= ch;
        mono[i] = (float)s;
        sum += s * s;
        peak = std::max(peak, std::fabs(s));
    }
    auto db = [](double x) { return x > 0 ? 20 * std::log10(x) : -120.0; };
    // Pitch: autocorrelation over the loudest 4096-sample window.
    size_t win = 4096, best = 0;
    double bestE = -1;
    for (size_t s = 0; s + win <= frames; s += win / 2) {
        double e = 0;
        for (size_t i = s; i < s + win; i += 4) e += mono[i] * mono[i];
        if (e > bestE) bestE = e, best = s;
    }
    double pitch = 0;
    if (frames >= win) {
        double bestR = 0;
        for (long lag = rate / 2000; lag <= rate / 50; ++lag) {
            double r = 0;
            for (size_t i = best; i + (size_t)lag < best + win; ++i) r += mono[i] * mono[i + lag];
            if (r > bestR) bestR = r, pitch = (double)rate / lag;
        }
    }
    long silent = 0;
    for (size_t s = 0; s + rate / 10 <= frames; s += rate / 10) {
        double e = 0;
        for (size_t i = s; i < s + rate / 10; ++i) e += mono[i] * mono[i];
        if (db(std::sqrt(e / (rate / 10))) < -50) ++silent;
    }
    printf("%s: %.2fs, %d ch, %ld Hz\npeak %.1f dBFS · rms %.1f dBFS · clipped samples %ld\n"
           "dominant pitch ~%.1f Hz · silence %.1fs\n",
           a[0].c_str(), (double)frames / rate, ch, rate, db(peak), db(std::sqrt(sum / std::max<size_t>(1, frames))),
           clip, pitch, silent / 10.0);
    return 0;
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
        std::string id = attr(tag, "id");
        if (!id.empty() && ++ids[id] == 2) problems.push_back("duplicate id \"" + id + "\"");
        if (name == "svg" && stack.empty() && attr(tag, "viewbox").empty()) problems.push_back("root <svg> lacks viewBox (won't scale)");
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
    double k = a.size() > 0 ? atof(a[0].c_str()) : 170, c = a.size() > 1 ? atof(a[1].c_str()) : 26,
           m = a.size() > 2 ? atof(a[2].c_str()) : 1;
    if (k <= 0 || c < 0 || m <= 0) return fail("usage: kit spring [stiffness=170] [damping=26] [mass=1]");
    double ms = 0;
    std::string e = springEasing(k, c, m, &ms);
    double zeta = c / (2 * std::sqrt(k * m));
    printf("spring k=%g c=%g m=%g · damping ratio %.2f (%s) · settles in %.0f ms\n", k, c, m, zeta,
           zeta < 1 ? "bouncy" : zeta == 1 ? "critical" : "overdamped", ms);
    printf("css: transition: transform %.0fms %s;\n", ms, e.c_str());
    printf("presets: gentle 120 14 · wobbly 180 12 · stiff 210 20 · slow 280 60 · snappy 400 30\n");
    return 0;
}

int kitSlop(const std::vector<std::string>& a) {
    int found = 0;
    for (const auto& p : a) {
        auto t = readFileBounded(p, 16 << 20);
        if (!t.ok) { printf("%s: %s\n", p.c_str(), t.error.c_str()); continue; }
        for (const auto& f : slopScan(p, t.value)) printf("%s: %s\n", p.c_str(), f.c_str()), ++found;
    }
    if (a.empty()) return fail("usage: kit slop FILE...");
    if (!found) printf("clean: no placeholders, stubs, conflict markers or AI-tell phrasing\n");
    return found ? 1 : 0;
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
    if (sub == "web") return kitWeb(a);
    if (sub == "search") return kitSearch(a);
    if (sub == "shot") return kitShot(a, false);
    if (sub == "dom") return kitShot(a, true);
    if (sub == "probe") { printf("%s", hostProbe(a.empty() ? "." : a[0]).c_str()); return 0; }
    if (sub == "img") return kitImg(a);
    if (sub == "wav") return kitWav(a);
    if (sub == "audio") return kitAudio(a);
    if (sub == "svg") return kitSvg(a);
    if (sub == "spring") return kitSpring(a);
    if (sub == "slop") return kitSlop(a);
    if (sub == "find") return kitFind(a);
    if (sub == "sym") return kitSym(a, false);
    if (sub == "refs") return kitSym(a, true);
    printf("pocket kit — native superpowers (run via bash)\n"
           "  web URL [--raw]        fetch a page as readable text + numbered links\n"
           "  search QUERY           web search (title, url, snippet)\n"
           "  dom URL                JS-rendered page text via headless Chrome\n"
           "  shot URL OUT.png [WxH] screenshot via headless Chrome (visual QA)\n"
           "  img FILE...            image type + dimensions (png/jpeg/gif/webp/svg)\n"
           "  svg FILE               SVG lint: structure, viewBox, ids, animation count\n"
           "  spring [k] [c] [m]     physical spring -> CSS linear() easing + duration\n"
           "  wav OUT \"C4:.25 R:.25\"  synthesize notes to WAV (--wave, --bpm, --gain)\n"
           "  audio FILE.wav         loudness, peak, clipping, pitch, silence\n"
           "  slop FILE...           placeholders, stubs, conflict markers, AI-tell prose\n"
           "  find QUERY             ranked code search (BM25 over chunks; no index to maintain)\n"
           "  sym NAME|.             definitions of NAME (\".\" = outline of every declaration)\n"
           "  refs NAME              whole-word references to NAME\n"
           "  probe [DIR]            host: OS, CPU, memory, disk, GPU, toolchain\n");
    return sub.empty() || sub == "help" ? 0 : 2;
}

}  // namespace pocket
