// PocketHarness - `pocket kit lint`: the native rule engine behind the
// automatic write-time and end-of-turn quality checks.
#include "kit_lint.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <cstring>
#include <map>
#include <unordered_map>

#include "common.h"

namespace pocket {
namespace {

namespace fs = std::filesystem;

enum : unsigned { JS = 1, PY = 2, C = 4, SH = 8, GO = 16, PHP = 32, JAVA = 64, HTML = 128, CSS = 256, SVG = 512, CFG = 1024, SQL = 2048 };
constexpr unsigned CODE = JS | PY | C | SH | GO | PHP | JAVA;
constexpr unsigned SERVER = JS | PY | GO | PHP | JAVA;

unsigned langOf(const std::string& path) {
    static const std::map<std::string, unsigned> kExt = {
        {".js", JS}, {".mjs", JS}, {".cjs", JS}, {".ts", JS}, {".jsx", JS | HTML | CSS}, {".tsx", JS | HTML | CSS},
        {".vue", JS | HTML | CSS}, {".svelte", JS | HTML | CSS}, {".astro", JS | HTML | CSS},
        {".html", HTML | CSS | JS}, {".htm", HTML | CSS | JS}, {".css", CSS}, {".scss", CSS}, {".less", CSS},
        {".svg", SVG}, {".py", PY}, {".c", C}, {".cc", C}, {".cpp", C}, {".cxx", C}, {".h", C}, {".hpp", C},
        {".sh", SH}, {".bash", SH}, {".go", GO}, {".php", PHP}, {".java", JAVA}, {".kt", JAVA}, {".sql", SQL},
        {".json", CFG}, {".yaml", CFG}, {".yml", CFG}, {".toml", CFG}, {".ini", CFG}, {".conf", CFG}};
    auto it = kExt.find(toLower(fs::path(path).extension().string()));
    return it == kExt.end() ? 0 : it->second;
}

bool pathHas(const std::string& path, std::initializer_list<const char*> parts) {
    std::string p = "/" + toLower(path);
    for (const char* s : parts)
        if (p.find(s) != std::string::npos) return true;
    return false;
}
// The rule table quotes the very patterns it detects, so it is exempt like vendored code.
bool vendored(const std::string& p) {
    return pathHas(p, {"/node_modules/", "/vendor/", "/third_party/", "/dist/", "/build/", "/.git/", ".min.", "kit_lint.", "-lock.", "package-lock"});
}
bool testy(const std::string& p) { return pathHas(p, {"/test", "/spec", "_test.", ".test.", ".spec.", "/fixtures/", "/examples/", "/mock"}); }

// ---------------------------------------------------------------------------
// Rule table. pat: '&' joins groups that must all hold, a leading '!' negates
// a group, ';' separates alternatives (case-insensitive substrings), a leading
// '~' on an alternative demands a non-identifier character before it. ctx (if
// set) must appear somewhere in the file.
// ---------------------------------------------------------------------------
struct Rule {
    unsigned langs;
    char sev;
    const char* cat;
    const char* pat;
    const char* msg;
    const char* ctx = nullptr;
};

const Rule kRules[] = {
    // --- security ---
    {JS, 'H', "sec", "~eval(;new function(", "dynamic code execution: parse data, do not evaluate it"},
    {JS | HTML, 'M', "sec", ".innerhtml =;.innerhtml=;dangerouslysetinnerhtml;v-html;document.write(", "raw HTML sink: XSS unless the value is sanitized; use textContent or the framework's escaping"},
    {JS, 'M', "sec", "child_process.exec(;cp.exec(;execsync(;~exec(&${;\" +;' +;` +", "shell command built from strings: use execFile/spawn with an argument array"},
    {JS, 'M', "sec", "math.random()&token;secret;password;session;nonce;apikey", "Math.random is predictable: use crypto.randomBytes / crypto.getRandomValues"},
    {JS, 'M', "sec", "localstorage.setitem&token;jwt;password;secret", "credentials in localStorage are readable by any XSS: use an httpOnly cookie"},
    {JS, 'H', "sec", "algorithm: 'none';algorithms: ['none'];alg: 'none'", "JWT with alg none accepts forged tokens"},
    {JS, 'M', "api", "jwt.sign(&)&!expiresin&!exp:", "token minted without expiry: set expiresIn"},
    {JS, 'M', "api", "res.cookie(&)&!httponly", "cookie without httpOnly/secure/sameSite: set all three"},
    {JS, 'M', "api", "res.send(err);res.json(err);res.send(e.stack;err.stack)", "error object/stack sent to the client leaks internals: return a generic message and log the detail"},
    {JS | PY | GO | PHP | JAVA | CFG, 'M', "sec", "origin: '*';origin:'*';origin: \"*\";allow-origin: *;allow-origin\", \"*\";allow_origins=[\"*\"]", "CORS allows every origin: list the real ones"},
    {CODE | CFG, 'H', "sec", "rejectunauthorized: false;rejectunauthorized:false;verify=false;verify = false;insecureskipverify: true;strict_ssl=false", "TLS verification disabled: anyone on the path can impersonate the server"},
    {JS, 'H', "sec", "query(;execute(&select ;insert ;update ;delete &${;\" +;' +;` +", "SQL built by string concatenation/interpolation: use placeholders (parameterized queries)"},
    {PY, 'H', "sec", "execute(;executemany(&~f\";~f';.format(;\" % ;' % ;\" +;' +", "SQL built by string formatting: pass parameters as the second argument"},
    {GO, 'H', "sec", "query(;exec(;queryrow(&sprintf(;\" +", "SQL built with Sprintf/concatenation: use $1/? placeholders"},
    {PHP, 'H', "sec", "mysql_query(;mysqli_query(;->query(&$_get;$_post;$_request;\" .", "SQL built from request data: use prepared statements"},
    {JAVA, 'H', "sec", "createstatement;executequery(;executeupdate(&\" +", "SQL built by concatenation: use PreparedStatement"},
    {PY, 'H', "sec", "shell=true", "shell=True runs a shell: pass an argument list"},
    {PY, 'M', "sec", "os.system(;os.popen(", "shell command: use subprocess.run([...]) with an argument list"},
    {PY, 'H', "sec", "pickle.load;marshal.load;shelve.open", "unpickling untrusted data executes code: use JSON"},
    {PY, 'M', "sec", "yaml.load(&!safeloader&!safe_load", "yaml.load can construct arbitrary objects: use yaml.safe_load"},
    {PY, 'H', "sec", "~eval(;~exec(", "dynamic code execution: parse data, do not evaluate it"},
    {PY, 'M', "sec", "hashlib.md5(;hashlib.sha1(", "MD5/SHA-1 are broken for security: use SHA-256+, and argon2/bcrypt/scrypt for passwords"},
    {PY, 'M', "sec", "random.&token;secret;password;nonce;session", "random is predictable: use the secrets module"},
    {PY, 'M', "sec", "debug=true;app.debug = true", "debug mode exposes an interactive console: off outside development"},
    {PY, 'M', "sec", "tempfile.mktemp(", "mktemp is racy: use NamedTemporaryFile / mkstemp"},
    {PY, 'M', "api", "requests.get(;requests.post(;requests.put(;requests.delete(;requests.request(&)&!timeout", "HTTP call without timeout can hang the worker forever: pass timeout="},
    {C, 'H', "sec", "~gets(", "gets cannot bound its input: use fgets"},
    {C, 'H', "sec", "scanf(&%s", "unbounded %s read overflows the buffer: add a width specifier"},
    {C, 'M', "sec", "~strcpy(;~strcat(;~sprintf(&!snprintf&!vsnprintf", "unbounded copy: use snprintf / memcpy with a checked length"},
    {C, 'M', "sec", "~system(;~popen(;~mktemp(", "shell/temp-name call: use exec* with an argument vector; mkstemp for temp files"},
    {SH, 'H', "sec", "curl;wget&| sh;| bash;|sh;|bash;| sudo", "piping a download into a shell runs unverified code: download, verify, then run"},
    {SH, 'M', "sec", "chmod 777;chmod -r 777;chmod a+rwx", "world-writable permissions: grant the narrowest mode"},
    {SH, 'M', "sec", "~eval ", "eval on shell text re-parses input: avoid it"},
    {GO, 'M', "sec", "md5.new(;sha1.new(;md5.sum(;sha1.sum(", "MD5/SHA-1 are broken for security: use SHA-256+ (bcrypt/argon2 for passwords)"},
    {GO, 'M', "sec", "exec.command(\"sh\";exec.command(\"bash\"", "shell -c with built strings is injectable: exec the program directly"},
    {GO, 'M', "api", "readall(&body", "reading a whole request/response body without a limit: wrap with http.MaxBytesReader / io.LimitReader"},
    {PHP, 'H', "sec", "eval(;unserialize(&$_;extract($_", "request data reaches code execution/object injection"},
    {JAVA, 'M', "sec", "runtime.getruntime().exec(;messagedigest.getinstance(\"md5\";messagedigest.getinstance(\"sha-1\";objectinputstream", "unsafe primitive: shell exec, weak hash or Java deserialization"},
    // --- performance / efficiency ---
    {CODE | SQL, 'L', "perf", "select * from;select *from", "SELECT * ships every column: list only what the caller uses"},
    {CSS, 'L', "perf", "transition: all;transition:all", "transition: all animates layout properties: list the properties (transform, opacity)"},
    {CSS, 'L', "ui", "100vh", "100vh clips behind mobile browser bars: prefer 100dvh"},
    {JS, 'L', "perf", "readfilesync(;writefilesync(;execsync(;existssync(", "synchronous I/O blocks the event loop in a server: use the async API", "express;createserver;fastify;app.listen;koa;hono"},
    {JS | PY | GO | JAVA, 'L', "pattern", "// eslint-disable;# noqa;# type: ignore;@ts-ignore;@ts-nocheck;//nolint;@suppresswarnings", "suppressed check: fix the cause or justify it next to the suppression"},
    // --- coding patterns ---
    {PY, 'M', "pattern", "def &=[];= [];={};= {}", "mutable default argument is shared between calls: default to None"},
    {PY, 'M', "pattern", "except:", "bare except swallows KeyboardInterrupt/SystemExit: catch what you can handle"},
    {PY, 'M', "pattern", "except&pass", "error silenced with pass: handle, log or re-raise"},
    {JS | JAVA, 'L', "pattern", "catch (e) {};catch(e){};catch {};catch (err) {};catch(err){};catch (exception e) {}", "empty catch hides failures: handle or log"},
    // --- UI / accessibility / anti-slop ---
    {HTML, 'M', "a11y", "<img &>&!alt=", "image without alt text: describe it, or alt=\"\" when decorative"},
    {HTML, 'M', "a11y", "user-scalable=no;maximum-scale=1", "pinch-zoom disabled: low-vision users need it"},
    {HTML | CSS, 'M', "a11y", "outline:none;outline: none;outline:0;outline: 0", "focus ring removed: restyle :focus-visible instead of hiding it"},
    {HTML, 'M', "a11y", "<div&onclick=;<span&onclick=;<div&@click;<span&@click", "clickable non-control is not keyboard accessible: use <button> or <a>"},
    {HTML, 'L', "a11y", "<a href=\"#\"", "link to nowhere: use a button, or a real target"},
    {HTML, 'M', "slop", "lorem ipsum", "placeholder text: write the real content"},
    {HTML, 'M', "slop", "99.9%;99.99%;10k+;50k+;100k+;1m+;2m+;10m+;trusted by", "invented stat or social proof? use sourced data or a visibly labelled placeholder"},
    {HTML, 'M', "slop", "build faster;ship smarter;supercharge;revolutioniz;next-gen;game-chang;unlock the power;10x your;seamless experience", "copy names no capability and fits any competitor: state what this product does"},
    {HTML | CSS | JS, 'M', "slop", "from-indigo-;from-violet-;from-purple-;via-purple-;to-purple-;to-fuchsia-;bg-indigo-500;bg-violet-500;bg-purple-500", "indigo/violet Tailwind gradient is the default AI look: derive the palette from the subject"},
};

bool has(const std::string& l, std::string_view alt) {
    bool word = !alt.empty() && alt[0] == '~';
    if (word) alt.remove_prefix(1);
    if (alt.empty()) return false;
    for (size_t p = 0; (p = l.find(alt, p)) != std::string::npos; ++p)
        if (!word || p == 0 || !(isalnum((unsigned char)l[p - 1]) || l[p - 1] == '_' || l[p - 1] == '.')) return true;
    return false;
}

bool matches(const std::string& l, std::string_view pat) {
    while (!pat.empty()) {
        size_t amp = pat.find('&');
        std::string_view g = pat.substr(0, amp);
        pat = amp == std::string_view::npos ? std::string_view() : pat.substr(amp + 1);
        bool neg = !g.empty() && g[0] == '!';
        if (neg) g.remove_prefix(1);
        bool any = false;
        for (size_t a = 0; !any;) {
            size_t semi = g.find(';', a);
            any = has(l, g.substr(a, semi == std::string_view::npos ? semi : semi - a));
            if (semi == std::string_view::npos) break;
            a = semi + 1;
        }
        if (any == neg) return false;
    }
    return true;
}

struct Hit {
    char sev;
    std::string cat, msg;
    std::vector<int> lines;
};
struct Sink {
    std::vector<Hit> hits;
    void add(char sev, const std::string& cat, const std::string& msg, int line) {
        for (auto& h : hits)
            if (h.msg == msg) {
                if (h.lines.size() < 40) h.lines.push_back(line);
                return;
            }
        hits.push_back({sev, cat, msg, {line}});
    }
};

// ---------------------------------------------------------------------------
// Colour helpers
// ---------------------------------------------------------------------------
struct Rgb { double r, g, b; };

std::vector<Rgb> hexColors(const std::string& s) {
    std::vector<Rgb> out;
    for (size_t i = 0; (i = s.find('#', i)) != std::string::npos; ++i) {
        size_t n = 0;
        while (i + 1 + n < s.size() && isxdigit((unsigned char)s[i + 1 + n])) ++n;
        if ((n != 3 && n != 6) || (i + 1 + n < s.size() && isalnum((unsigned char)s[i + 1 + n]))) continue;
        auto v = [&](size_t k) { return (double)strtol(s.substr(i + 1 + k, 1).c_str(), nullptr, 16); };
        if (n == 3) out.push_back({v(0) * 17 / 255, v(1) * 17 / 255, v(2) * 17 / 255});
        else out.push_back({(v(0) * 16 + v(1)) / 255, (v(2) * 16 + v(3)) / 255, (v(4) * 16 + v(5)) / 255});
    }
    return out;
}

void toHsl(Rgb c, double& h, double& s, double& l) {
    double mx = std::max({c.r, c.g, c.b}), mn = std::min({c.r, c.g, c.b}), d = mx - mn;
    l = (mx + mn) / 2;
    s = d == 0 ? 0 : d / (1 - std::fabs(2 * l - 1));
    h = d == 0 ? 0 : mx == c.r ? 60 * std::fmod((c.g - c.b) / d + 6, 6) : mx == c.g ? 60 * ((c.b - c.r) / d + 2) : 60 * ((c.r - c.g) / d + 4);
}

double luminance(Rgb c) {
    auto f = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * f(c.r) + 0.7152 * f(c.g) + 0.0722 * f(c.b);
}

std::vector<double> numbers(const std::string& s, size_t from, size_t max) {
    std::vector<double> out;
    for (size_t i = from; i < s.size() && out.size() < max; ++i)
        if (isdigit((unsigned char)s[i]) || (s[i] == '.' && i + 1 < s.size() && isdigit((unsigned char)s[i + 1]))) {
            char* end = nullptr;
            out.push_back(strtod(s.c_str() + i, &end));
            i = (size_t)(end - s.c_str()) - 1;
        }
    return out;
}

// ---------------------------------------------------------------------------
// Custom checks
// ---------------------------------------------------------------------------
bool placeholderish(const std::string& v) {
    for (const char* p : {"${", "{{", "process.env", "environ", "getenv", "example", "changeme", "your", "xxx", "<", "***", "placeholder", "todo", "dummy", "secret_here"})
        if (v.find(p) != std::string::npos) return true;
    return false;
}

void secrets(const std::string& raw, const std::string& low, int ln, Sink& sink) {
    // Detector regexes and pattern lists name the marker without containing a key.
    if (low.find("-----begin") != std::string::npos && low.find("private key") != std::string::npos && low.find("(?") == std::string::npos &&
        low.find("[^") == std::string::npos && low.find("/-----") == std::string::npos && low.find("[\"") == std::string::npos)
        sink.add('H', "sec", "private key material in source: remove it, rotate the key, load it from the environment", ln);
    auto run = [&](const char* prefix, size_t minLen) {
        for (size_t p = 0; (p = raw.find(prefix, p)) != std::string::npos; p += strlen(prefix)) {
            size_t n = 0;
            while (p + strlen(prefix) + n < raw.size() && (isalnum((unsigned char)raw[p + strlen(prefix) + n]) || raw[p + strlen(prefix) + n] == '_' || raw[p + strlen(prefix) + n] == '-')) ++n;
            if (n >= minLen) return true;
        }
        return false;
    };
    if (run("AKIA", 16) || run("ghp_", 30) || run("github_pat_", 30) || run("xoxb-", 20) || run("xoxp-", 20) || run("AIza", 30) ||
        run("sk_live_", 16) || run("sk-ant-", 20) || run("sk-or-", 20))
        sink.add('H', "sec", "credential-shaped token in source: revoke it and load secrets from the environment", ln);
    if (placeholderish(low)) return;
    for (const char* key : {"secret", "password", "passwd", "api_key", "apikey", "api-key", "token", "private_key"}) {
        size_t p = low.find(key), klen = strlen(key);
        if (p == std::string::npos) continue;
        size_t eq = p + klen;  // the key must end the identifier: tokens/tokenParameter are not secrets
        if (eq < low.size() && (isalnum((unsigned char)low[eq]) || low[eq] == '_')) continue;
        eq = low.find_first_of("=:", eq);
        if (eq == std::string::npos || eq - p > klen + 6 || (eq + 1 < low.size() && low[eq + 1] == '=') || (eq > 0 && (low[eq - 1] == '!' || low[eq - 1] == '=')))
            continue;
        size_t q = low.find_first_of("\"'", eq);
        if (q == std::string::npos || q - eq > 4) continue;
        size_t e = low.find(low[q], q + 1);
        if (e == std::string::npos || e - q - 1 < 8) continue;
        std::string lit = raw.substr(q + 1, e - q - 1);
        bool digit = lit.find_first_of("0123456789") != std::string::npos;
        if (lit.find(' ') == std::string::npos && (digit || lit.size() >= 16)) {
            sink.add('H', "sec", "hard-coded secret: load it from the environment or a secret store, never the repo", ln);
            return;
        }
    }
}

int indentOf(const std::string& l) {
    int n = 0;
    for (char c : l) {
        if (c == ' ') ++n;
        else if (c == '\t') n += 4;
        else break;
    }
    return n;
}

const char* hotCalls(unsigned L) {
    if (L & JS) return "fetch(;.query(;.findone(;.findbyid(;.execute(;new regexp(";
    if (L & PY) return "requests.;.execute(;.query(;re.compile(;urlopen(";
    if (L & GO) return "db.query;db.exec;http.get(;regexp.mustcompile(;regexp.compile(";
    if (L & (PHP | JAVA)) return "->query(;mysqli_query(;.executequery(;preparestatement(;pattern.compile(";
    return nullptr;
}

bool loopHeader(const std::string& low) {
    std::string t = trim(low);
    for (const char* k : {"for ", "for(", "while ", "while(", "foreach ", "foreach(", "} while"})
        if (startsWith(t, k)) return true;
    return t.find(".foreach(") != std::string::npos;
}

void styleChecks(const std::string& content, const std::vector<std::string>& lines, unsigned L, const std::string& path, Sink& sink) {
    // Contrast: each brace block that sets both color and a hex background.
    if (L & CSS) {
        std::vector<size_t> open;
        int line = 1;
        std::vector<int> openLine;
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == '\n') ++line;
            else if (content[i] == '{') { open.push_back(i); openLine.push_back(line); }
            else if (content[i] == '}' && !open.empty()) {
                std::string body = content.substr(open.back() + 1, i - open.back() - 1);
                int at = openLine.back();
                open.pop_back(); openLine.pop_back();
                if (body.find('{') != std::string::npos) continue;
                std::vector<Rgb> fg, bg;
                size_t start = 0;
                while (start < body.size()) {
                    size_t end = body.find_first_of(";\n", start);
                    std::string d = body.substr(start, end == std::string::npos ? end : end - start);
                    start = end == std::string::npos ? body.size() : end + 1;
                    size_t colon = d.find(':');
                    if (colon == std::string::npos) continue;
                    std::string name = toLower(trim(d.substr(0, colon)));
                    auto cols = hexColors(d.substr(colon + 1));
                    if (cols.empty()) continue;
                    if (name == "color") fg = cols;
                    else if (name == "background" || name == "background-color") bg = cols;
                }
                if (fg.empty() || bg.empty()) continue;
                double a = luminance(fg[0]), b = luminance(bg[0]);
                double ratio = (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
                if (ratio < 4.5) sink.add('M', "a11y", "text/background contrast below WCAG AA 4.5:1 (e.g. " + std::to_string(ratio).substr(0, 4) + ":1): darken the text or lighten the surface", at);
            }
        }
    }
    bool cream = false, terracotta = false;
    int terraLine = 1, ln = 0;
    bool anim = false, reduced = content.find("prefers-reduced-motion") != std::string::npos;
    int animLine = 1, emoji = 0, emojiLine = 1;
    for (const std::string& raw : lines) {
        ++ln;
        std::string low = toLower(raw);
        if ((L & CSS) && (low.find("@keyframes") != std::string::npos || low.find("animation:") != std::string::npos) && !anim) { anim = true; animLine = ln; }
        auto cols = hexColors(raw);
        if (low.find("gradient(") != std::string::npos) {
            int purple = 0;
            for (auto& c : cols) {
                double h, s, l;
                toHsl(c, h, s, l);
                if (h >= 235 && h <= 335 && s >= 0.3 && l >= 0.2 && l <= 0.85) ++purple;
            }
            if (purple && cols.size() >= 2) sink.add('M', "slop", "indigo/purple gradient is the default AI-SaaS look: derive the palette from the subject", ln);
        }
        for (const char* tw : {"#6366f1", "#4f46e5", "#8b5cf6", "#7c3aed", "#a855f7", "#9333ea", "#818cf8"})
            if (low.find(tw) != std::string::npos) { sink.add('M', "slop", "Tailwind indigo/violet demo accent: pick an accent that comes from the subject", ln); break; }
        for (auto& c : cols) {
            double h, s, l;
            toHsl(c, h, s, l);
            if (h >= 30 && h <= 55 && s >= 0.1 && s <= 0.5 && l >= 0.9) cream = true;
            if (h >= 8 && h <= 25 && s >= 0.45 && s <= 0.85 && l >= 0.35 && l <= 0.58) { terracotta = true; terraLine = ln; }
        }
        size_t ff = low.find("font-family");
        if (ff != std::string::npos) {
            size_t c = low.find(':', ff);
            std::string fam = c == std::string::npos ? "" : low.substr(c + 1);
            fam = trim(fam.substr(0, fam.find_first_of(",;}")));
            fam.erase(std::remove_if(fam.begin(), fam.end(), [](char ch) { return ch == '\'' || ch == '"'; }), fam.end());
            fam = trim(fam);
            if (fam == "inter" || fam == "space grotesk" || fam == "geist" || fam == "instrument serif")
                sink.add('M', "slop", "default AI type choice (Inter / Space Grotesk / Geist / Instrument Serif): choose type that fits the subject", ln);
        }
        size_t fsz = low.find("font-size:");
        if (fsz != std::string::npos) {
            auto n = numbers(low, fsz + 10, 1);
            size_t px = low.find_first_not_of(" 0123456789.", fsz + 10);
            if (!n.empty() && n[0] > 0 && n[0] < 12 && px != std::string::npos && low.compare(px, 2, "px") == 0)
                sink.add('M', "a11y", "text under 12px is hard to read: raise the size", ln);
        }
        size_t bs = low.find("box-shadow");
        if (bs != std::string::npos && low.find("inset") == std::string::npos) {
            auto n = numbers(low, bs + 10, 3);
            if (n.size() == 3 && n[0] == 0 && n[1] == 0 && n[2] >= 20 && (low.find("rgba(") != std::string::npos || !cols.empty()))
                sink.add('M', "slop", "glow halo (zero offset, wide blur) is decoration: use a real elevation shadow or none", ln);
        }
        if (L & HTML) {
            for (size_t i = 0; i + 3 < raw.size(); ++i) {
                unsigned char b0 = raw[i], b1 = raw[i + 1];
                if ((b0 == 0xF0 && b1 == 0x9F) || (b0 == 0xE2 && (b1 == 0x98 || b1 == 0x9C || b1 == 0x9D || b1 == 0xAD))) {
                    if (!emoji++) emojiLine = ln;
                    i += b0 == 0xF0 ? 3 : 2;
                }
            }
        }
    }
    if (cream && terracotta) sink.add('M', "slop", "cream background with terracotta accent is the second default AI look: derive the palette from the subject", terraLine);
    if (anim && !reduced) sink.add('M', "a11y", "animation without a prefers-reduced-motion fallback", animLine);
    if (emoji >= 3) sink.add('M', "slop", "emoji used as icons/decoration (" + std::to_string(emoji) + "): use real icons or nothing", emojiLine);
    std::string all = toLower(content);
    size_t html = all.find("<html");
    if ((L & HTML) && html != std::string::npos && (endsWith(toLower(path), ".html") || endsWith(toLower(path), ".htm"))) {
        size_t gt = all.find('>', html);
        int at = 1 + (int)std::count(content.begin(), content.begin() + (long)html, '\n');
        if (all.substr(html, gt == std::string::npos ? gt : gt - html).find("lang=") == std::string::npos)
            sink.add('M', "a11y", "<html> has no lang: screen readers guess the language", at);
        // A fixed-canvas video scene is rendered at a set size, never opened on a phone.
        if (all.find("viewport") == std::string::npos && all.find("renderframe") == std::string::npos && !pathHas(path, {"scene"}))
            sink.add('M', "ui", "no viewport meta: the page will not scale on phones", at);
    }
}

void svgChecks(const std::string& s, const std::vector<std::string>& lines, Sink& sink) {
    std::string low = toLower(s);
    auto lineOf = [&](size_t pos) { return 1 + (int)std::count(s.begin(), s.begin() + (long)std::min(pos, s.size()), '\n'); };
    size_t p;
    if ((p = low.find("<script")) != std::string::npos) sink.add('H', "sec", "<script> in an SVG runs when the file is opened directly: remove it", lineOf(p));
    for (const char* ev : {" onload=", " onclick=", " onmouseover=", " onerror=", " onfocus="})
        if ((p = low.find(ev)) != std::string::npos) { sink.add('H', "sec", "event-handler attribute in SVG executes script: remove it", lineOf(p)); break; }
    if ((p = low.find("href=\"http")) != std::string::npos) sink.add('M', "sec", "external reference: leaks the viewer's request and fails offline; inline the asset", lineOf(p));
    for (const char* d : {"data:image/png", "data:image/jpeg", "data:image/webp", "data:image/gif"})
        if ((p = low.find(d)) != std::string::npos) { sink.add('M', "perf", "embedded raster inside an SVG is not vector and bloats it: use the source image or trace it", lineOf(p)); break; }
    for (const char* m : {"inkscape:", "sodipodi:", "illustrator", "<metadata", "sketch"})
        if ((p = low.find(m)) != std::string::npos) { sink.add('L', "perf", "editor metadata: strip it", lineOf(p)); break; }
    size_t paths = 0, wasted = 0;
    for (size_t i = 0; (i = low.find("<path", i)) != std::string::npos; ++i) ++paths;
    for (size_t i = 0; (i = low.find(" d=\"", i)) != std::string::npos; ++i) {
        size_t e = low.find('"', i + 4);
        if (e == std::string::npos) break;
        size_t dec = 0;
        bool frac = false;
        for (size_t k = i + 4; k < e; ++k) {
            if (low[k] == '.') { frac = true; dec = 0; }
            else if (isdigit((unsigned char)low[k]) && frac) { if (++dec > 2) ++wasted; }
            else frac = false;
        }
    }
    if (paths > 300) sink.add('M', "perf", std::to_string(paths) + " paths: heavy to parse and paint; simplify or merge paths", 1);
    if (wasted > 200 && wasted * 20 > s.size())
        sink.add('L', "perf", "path coordinates carry ~" + std::to_string(wasted) + " digits beyond 2 decimals (" + std::to_string(wasted * 100 / s.size()) + "% of the file): round to 2", 1);
    size_t filters = 0;
    for (size_t i = 0; (i = low.find("<filter", i)) != std::string::npos; ++i) ++filters;
    if (filters > 2 || low.find("fegaussianblur") != std::string::npos) sink.add('M', "perf", "SVG filters/blur are repainted every frame when animated: bake or drop them", 1);
    if (low.find("<title") == std::string::npos && low.find("aria-label") == std::string::npos && low.find("aria-hidden") == std::string::npos)
        sink.add('M', "a11y", "no accessible name: add <title> with role=\"img\", or aria-hidden=\"true\" when decorative", 1);
    if (low.find("<text") != std::string::npos) sink.add('M', "ui", "<text> depends on installed fonts and shifts between machines: convert to paths or embed the font", lineOf(low.find("<text")));
    std::map<std::string, int> fills;
    for (const char* attr : {"fill=\"#", "stroke=\"#"})
        for (size_t i = 0; (i = low.find(attr, i)) != std::string::npos; ++i) {
            size_t e = low.find('"', i + strlen(attr));
            if (e != std::string::npos) fills[low.substr(i + strlen(attr) - 1, e - i - strlen(attr) + 1)]++;
        }
    if (fills.size() > 6 && low.find("currentcolor") == std::string::npos && low.find("var(--") == std::string::npos)
        sink.add('L', "ui", std::to_string(fills.size()) + " hard-coded colours and no currentColor/CSS variables: the graphic cannot follow the theme", 1);
    int unused = 0, emptyG = 0, ln = 0;
    for (size_t i = 0; (i = low.find(" id=\"", i)) != std::string::npos; ++i) {
        size_t e = low.find('"', i + 5);
        if (e == std::string::npos) break;
        std::string id = low.substr(i + 5, e - i - 5);
        if (low.find("#" + id) == std::string::npos && low.find("\"" + id + "\"", e + 1) == std::string::npos) ++unused;
    }
    for (const auto& l : lines) { ++ln; std::string t = trim(toLower(l)); if (t == "<g></g>" || t == "<g/>") ++emptyG; }
    if (unused > 3) sink.add('L', "perf", std::to_string(unused) + " ids are never referenced: strip them", 1);
    if (emptyG) sink.add('L', "perf", "empty <g> groups: remove", 1);
    if (s.size() > 200 * 1024) sink.add('M', "perf", "SVG over 200 KB: simplify, or ship a raster", 1);
}

// Cross-file duplicate detector: sliding window of 6 normalised lines.
struct DryIndex {
    std::unordered_map<size_t, std::pair<std::string, int>> seen;
};

void dryScan(DryIndex& idx, const std::string& path, const std::vector<std::string>& lines, Sink& sink) {
    constexpr size_t W = 6;
    std::vector<std::pair<int, std::string>> norm;
    int ln = 0;
    for (const auto& l : lines) {
        ++ln;
        std::string t;
        for (char c : l)
            if (!isspace((unsigned char)c)) t += c;
        if (t.size() >= 6 && !startsWith(t, "//") && !startsWith(t, "#include") && !startsWith(t, "import")) norm.push_back({ln, t});
    }
    size_t skip = 0;
    for (size_t i = 0; i + W <= norm.size(); ++i) {
        std::string blob;
        for (size_t k = 0; k < W; ++k) blob += norm[i + k].second;
        if (blob.size() < 200) continue;
        size_t h = std::hash<std::string>{}(blob);
        auto it = idx.seen.find(h);
        if (it == idx.seen.end()) { idx.seen[h] = {path, norm[i].first}; continue; }
        if (i < skip) continue;
        skip = i + W;
        const auto& [op, ol] = it->second;
        if (op == path) {
            if (norm[i].first - ol >= (int)W) sink.add('M', "dry", "block repeated within this file (first at line " + std::to_string(ol) + "): extract a helper", norm[i].first);
        } else sink.add('M', "dry", "block duplicated from " + op + ":" + std::to_string(ol) + ": share one implementation", norm[i].first);
    }
}

std::vector<std::string> scanOne(const std::string& path, const std::string& content, char minSev, DryIndex& dry) {
    unsigned L = langOf(path);
    if (!L || vendored(path) || content.size() > (2u << 20)) return {};
    const bool skipSec = testy(path);
    Sink sink;
    auto lines = splitLines(content);
    std::string lowAll = toLower(content);
    const char* hot = hotCalls(L);
    std::vector<int> loops;
    int ln = 0, deepAt = 0, deepest = 0;
    for (const std::string& raw : lines) {
        ++ln;
        std::string t = trim(raw);
        if (t.empty()) continue;
        std::string low = toLower(raw);
        bool comment = startsWith(t, "//") || startsWith(t, "/*") || startsWith(t, "* ") || startsWith(t, "<!--") ||
                       ((L & (PY | SH | PHP)) && startsWith(t, "#"));
        if (!comment) {
            for (const Rule& r : kRules)
                if ((r.langs & L) && !(skipSec && !strcmp(r.cat, "sec")) && matches(low, r.pat) && (!r.ctx || matches(lowAll, r.ctx)))
                    sink.add(r.sev, r.cat, r.msg, ln);
            if (hot) {
                int in = indentOf(raw);
                while (!loops.empty() && in <= loops.back()) loops.pop_back();
                if (loopHeader(low)) loops.push_back(in);
                else if (!loops.empty() && matches(low, hot))
                    sink.add('M', "perf", "call inside a loop: batch it (one query, Promise.all, hoisted compile); N+1 and serial I/O scale badly", ln);
            }
            int depth = indentOf(raw) / 4;
            if (depth > deepest) { deepest = depth; deepAt = ln; }
        }
        if (!skipSec && (L & (CODE | CFG))) secrets(raw, low, ln, sink);
    }
    if (deepest >= 8 && (L & CODE)) sink.add('L', "pattern", "nesting " + std::to_string(deepest) + " levels deep: use early returns or extract functions", deepAt);
    if (lines.size() > 1200 && (L & CODE)) sink.add('L', "pattern", "file is " + std::to_string(lines.size()) + " lines: split it by responsibility", 1);
    if (L & (CSS | HTML)) styleChecks(content, lines, L, path, sink);
    if (L & SVG) svgChecks(content, lines, sink);
    if ((L & CODE) && !skipSec) dryScan(dry, path, lines, sink);
    auto rank = [](char s) { return s == 'H' ? 0 : s == 'M' ? 1 : 2; };
    auto floor = minSev == 'L' ? 2 : minSev == 'H' ? 0 : 1;
    std::stable_sort(sink.hits.begin(), sink.hits.end(), [&](const Hit& a, const Hit& b) { return rank(a.sev) < rank(b.sev); });
    std::vector<std::string> out;
    for (const Hit& h : sink.hits) {
        if (rank(h.sev) > floor) continue;
        std::string at = "L";
        for (size_t i = 0; i < h.lines.size() && i < 4; ++i) at += (i ? "," : "") + std::to_string(h.lines[i]);
        if (h.lines.size() > 4) at += " (+" + std::to_string(h.lines.size() - 4) + ")";
        out.push_back(at + ": [" + h.cat + "/" + h.sev + "] " + h.msg);
    }
    return out;
}

}  // namespace

std::vector<std::string> lintScan(const std::string& path, const std::string& content, char minSev) {
    DryIndex dry;
    return scanOne(path, content, minSev, dry);
}

std::string lintPaths(const std::vector<std::string>& paths, char minSev) {
    std::vector<std::string> files;
    for (const auto& p : paths) {
        std::error_code ec;
        if (fs::is_directory(p, ec)) {
            for (fs::recursive_directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
                std::string name = it->path().filename().string(), full = it->path().string();
                if (it->is_directory(ec)) {
                    if ((!name.empty() && name[0] == '.') || vendored(full + "/")) it.disable_recursion_pending();
                } else if (it->is_regular_file(ec) && langOf(full) && !vendored(full) && files.size() < 600) files.push_back(full);
            }
        } else files.push_back(p);
    }
    std::sort(files.begin(), files.end());
    DryIndex dry;
    std::string out;
    size_t found = 0, filesHit = 0;
    for (const auto& f : files) {
        auto t = readFileBounded(f, 1 << 20);
        if (!t.ok) continue;
        auto findings = scanOne(f, t.value, minSev, dry);
        if (findings.empty()) continue;
        ++filesHit;
        out += f + "\n";
        for (const auto& x : findings) out += "  " + x + "\n", ++found;
    }
    if (found) out += std::to_string(found) + " finding(s) in " + std::to_string(filesHit) + " file(s)\n";
    return out;
}

int kitLint(const std::vector<std::string>& args) {
    char sev = 'M';
    std::vector<std::string> paths;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--min" && i + 1 < args.size()) sev = (char)toupper((unsigned char)args[++i][0]);
        else if (args[i] == "--all") sev = 'L';
        else paths.push_back(args[i]);
    }
    if (sev != 'H' && sev != 'M' && sev != 'L') { fprintf(stderr, "pocket kit: usage: kit lint [PATH...] [--min h|m|l | --all]\n"); return 2; }
    if (paths.empty()) paths.push_back(".");
    std::string r = lintPaths(paths, sev);
    if (r.empty()) { printf("clean: no %s findings (security, perf, DRY, UI/a11y, SVG)\n", sev == 'H' ? "high" : "medium+");
        return 0; }
    printf("%s", r.c_str());
    return r.find("/H]") != std::string::npos ? 1 : 0;
}

}  // namespace pocket
