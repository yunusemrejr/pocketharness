// PocketHarness - `pocket kit` operations: networking, SEO, data and timing.
#include "kit_ops.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

#include "json.h"
#include "kit.h"
#include "process.h"

namespace pocket {

namespace {

int fail(const std::string& msg) {
    fprintf(stderr, "pocket kit: %s\n", msg.c_str());
    return 1;
}

bool isUrl(const std::string& s) { return startsWith(s, "http://") || startsWith(s, "https://"); }

SpawnResult curl(std::vector<std::string> args, long timeoutSec) {
    SpawnOpts o;
    o.exe = "curl";
    o.argv = {"curl", "--disable", "-sS", "--proto", "=http,https", "--max-time", std::to_string(timeoutSec)};
    o.argv.insert(o.argv.end(), args.begin(), args.end());
    o.timeoutMs = (timeoutSec + 3) * 1000;
    o.outLimit = 8 << 20;
    return spawn(o);
}

// Listening TCP sockets from /proc/net/tcp{,6}: inode -> "port addr".
std::map<std::string, std::pair<int, std::string>> listeners() {
    std::map<std::string, std::pair<int, std::string>> out;
    for (const char* f : {"/proc/net/tcp", "/proc/net/tcp6"}) {
        auto data = readFileBounded(f, 4 << 20);
        if (!data.ok) continue;
        auto lines = splitLines(data.value);
        for (size_t i = 1; i < lines.size(); ++i) {
            char local[128] = {}, rem[128] = {}, st[8] = {};
            unsigned long inode = 0;
            if (sscanf(lines[i].c_str(), "%*d: %127s %127s %7s %*s %*s %*s %*s %*s %lu", local, rem, st, &inode) != 4 ||
                std::string(st) != "0A")
                continue;
            std::string l = local;
            size_t colon = l.rfind(':');
            int port = (int)strtol(l.substr(colon + 1).c_str(), nullptr, 16);
            std::string hex = l.substr(0, colon), addr;
            if (hex.size() == 8) {
                uint32_t a = (uint32_t)strtoul(hex.c_str(), nullptr, 16);
                char b[32];
                snprintf(b, sizeof b, "%u.%u.%u.%u", a & 255, (a >> 8) & 255, (a >> 16) & 255, a >> 24);
                addr = b;
            } else addr = hex.find_first_not_of('0') == std::string::npos ? "::" :
                          hex == "00000000000000000000000001000000" ? "::1" : "ipv6";
            out[std::to_string(inode)] = {port, addr};
        }
    }
    return out;
}

bool tcpUp(const std::string& host, const std::string& port) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) return false;
    bool up = false;
    for (addrinfo* a = res; a && !up; a = a->ai_next) {
        int fd = socket(a->ai_family, a->ai_socktype | SOCK_CLOEXEC, a->ai_protocol);
        if (fd < 0) continue;
        up = connect(fd, a->ai_addr, a->ai_addrlen) == 0;
        close(fd);
    }
    freeaddrinfo(res);
    return up;
}

std::string between(const std::string& s, const std::string& open, const std::string& close) {
    size_t a = s.find(open);
    if (a == std::string::npos) return "";
    a += open.size();
    size_t b = s.find(close, a);
    return s.substr(a, b == std::string::npos ? std::string::npos : b - a);
}

// Every opening tag named `name` (lowercase html), as raw tag text.
std::vector<std::string> tags(const std::string& html, const std::string& lower, const std::string& name) {
    std::vector<std::string> out;
    for (size_t p = 0; (p = lower.find("<" + name, p)) != std::string::npos; ++p) {
        char c = p + name.size() + 1 < lower.size() ? lower[p + name.size() + 1] : '>';
        if (!isspace((unsigned char)c) && c != '>' && c != '/') continue;
        size_t e = lower.find('>', p);
        if (e == std::string::npos) break;
        out.push_back(html.substr(p, e - p + 1));
        if (out.size() >= 5000) break;
    }
    return out;
}

double percentile(std::vector<double> v, double q) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, (size_t)std::ceil(q * (v.size() - 1)))];
}

}  // namespace

int kitPorts(const std::vector<std::string>& a) {
    auto socks = listeners();
    std::map<std::string, std::string> owner;  // inode -> "pid cmdline"
    if (DIR* proc = opendir("/proc")) {
        while (dirent* d = readdir(proc)) {
            if (!isdigit((unsigned char)d->d_name[0])) continue;
            std::string base = std::string("/proc/") + d->d_name;
            DIR* fds = opendir((base + "/fd").c_str());
            if (!fds) continue;
            while (dirent* f = readdir(fds)) {
                char link[64] = {};
                ssize_t n = readlink((base + "/fd/" + f->d_name).c_str(), link, sizeof link - 1);
                if (n <= 9 || strncmp(link, "socket:[", 8)) continue;
                std::string inode(link + 8, (size_t)n - 9);
                if (!socks.count(inode) || owner.count(inode)) continue;
                std::string cmd = readFileBounded(base + "/cmdline", 4096).value;
                std::replace(cmd.begin(), cmd.end(), '\0', ' ');
                owner[inode] = std::string(d->d_name) + "\t" + trim(cmd).substr(0, 120);
            }
            closedir(fds);
        }
        closedir(proc);
    }
    std::set<std::string> rows;
    for (const auto& [inode, pa] : socks) {
        if (!a.empty() && a[0] != std::to_string(pa.first)) continue;
        char b[64];
        snprintf(b, sizeof b, "%-6d %-16s ", pa.first, pa.second.c_str());
        rows.insert(b + (owner.count(inode) ? owner[inode] : "?\t(other user or sandboxed)"));
    }
    if (rows.empty()) {
        if (a.empty()) printf("no listening TCP sockets\n");
        else printf("nothing listening on port %s\n", a[0].c_str());
        return a.empty() ? 0 : 1;
    }
    printf("PORT   ADDR             PID\tCOMMAND\n");
    for (const auto& r : rows) printf("%s\n", r.c_str());
    return 0;
}

int kitWait(const std::vector<std::string>& a) {
    if (a.empty()) return fail("usage: kit wait PORT|HOST:PORT|URL [SECONDS=30]");
    double limit = a.size() > 1 ? atof(a[1].c_str()) : 30;
    if (!(limit > 0 && limit <= 600)) return fail("seconds must be 0..600");
    std::string t = a[0], host = "127.0.0.1", port = t;
    if (!isUrl(t) && t.find(':') != std::string::npos) host = t.substr(0, t.rfind(':')), port = t.substr(t.rfind(':') + 1);
    int64_t start = nowMs();
    std::string last;
    while (true) {
        if (isUrl(t)) {
            SpawnResult r = curl({"-o", "/dev/null", "-w", "%{http_code}", t}, 5);
            last = trim(r.out) + " " + trim(r.err);
            if (r.ok && r.out != "000" && !r.out.empty()) {
                printf("up: %s answered HTTP %s after %.1fs\n", t.c_str(), r.out.c_str(), (nowMs() - start) / 1000.0);
                return 0;
            }
        } else if (tcpUp(host, port)) {
            printf("up: %s:%s accepts connections after %.1fs\n", host.c_str(), port.c_str(), (nowMs() - start) / 1000.0);
            return 0;
        }
        if (nowMs() - start > limit * 1000)
            return fail("timed out after " + std::to_string((int)limit) + "s waiting for " + t +
                        (last.empty() ? "" : " (last: " + trim(last).substr(0, 200) + ")") +
                        "; check the server log");
        usleep(250000);
    }
}

int kitNet(const std::vector<std::string>& a) {
    if (a.empty() || !isUrl(a[0])) return fail("usage: kit net URL   (timing, TLS, headers, redirects)");
    SpawnResult r = curl({"-L", "--max-redirs", "10", "--compressed", "-o", "/dev/null", "-D", "-", "-w",
                          "\n@@%{http_code} %{http_version} %{remote_ip} %{num_redirects} %{size_download} "
                          "%{time_namelookup} %{time_connect} %{time_appconnect} %{time_starttransfer} %{time_total} "
                          "%{ssl_verify_result} %{url_effective}", a[0]},
                         30);
    size_t at = r.out.rfind("\n@@");
    if (!r.ok || at == std::string::npos) return fail("request failed: " + trim(r.err).substr(0, 300));
    char eff[2048] = {}, ver[16] = {}, ip[64] = {};
    int code = 0, redirects = 0, verify = 0;
    double size = 0, dns = 0, conn = 0, tls = 0, ttfb = 0, total = 0;
    sscanf(r.out.c_str() + at + 3, "%d %15s %63s %d %lf %lf %lf %lf %lf %lf %d %2047s", &code, ver, ip, &redirects,
           &size, &dns, &conn, &tls, &ttfb, &total, &verify, eff);
    printf("%s -> HTTP %d (HTTP/%s) from %s, %d redirect(s), %.0f bytes\n", a[0].c_str(), code, ver, ip, redirects, size);
    if (redirects) printf("final: %s\n", eff);
    printf("timing ms: dns %.0f · connect %.0f · tls %.0f · first byte %.0f · total %.0f\n", dns * 1000,
           (conn - dns) * 1000, tls > 0 ? (tls - conn) * 1000 : 0, (ttfb - std::max(tls, conn)) * 1000, total * 1000);
    if (startsWith(a[0], "https://") && verify != 0) printf("TLS: certificate verification FAILED (%d)\n", verify);
    // Headers of the final response only (earlier blocks were redirects).
    std::string head = r.out.substr(0, at);
    size_t lastBlock = head.rfind("\r\n\r\nHTTP/");
    if (lastBlock != std::string::npos) head = head.substr(lastBlock + 4);
    std::string low = toLower(head);
    printf("headers:\n");
    for (const auto& line : splitLines(head))
        if (line.find(':') != std::string::npos) printf("  %s\n", trim(line).substr(0, 200).c_str());
    std::vector<std::string> missing;
    for (const char* h : {"strict-transport-security", "content-security-policy", "x-content-type-options",
                          "cache-control", "content-encoding"})
        if (low.find(std::string("\n") + h + ":") == std::string::npos) missing.push_back(h);
    if (!missing.empty()) {
        printf("missing:");
        for (const auto& m : missing) printf(" %s", m.c_str());
        printf("\n");
    }
    return code >= 200 && code < 400 ? 0 : 1;
}

int kitSeo(const std::vector<std::string>& a) {
    if (a.empty()) return fail("usage: kit seo FILE.html|URL   (on-page SEO and share-card audit)");
    std::string html;
    if (isUrl(a[0])) {
        SpawnResult r = curl({"-L", "--compressed", "-A", "Mozilla/5.0 (compatible; pocket-seo)", a[0]}, 20);
        if (!r.ok || r.exitCode != 0) return fail("fetch failed: " + trim(r.err).substr(0, 300));
        html = std::move(r.out);
    } else {
        auto f = readFileBounded(a[0], 8 << 20);
        if (!f.ok) return fail(f.error);
        html = std::move(f.value);
    }
    std::string low = toLower(html);
    std::vector<std::string> issues, facts;
    auto meta = [&](const std::string& key) {
        for (const auto& t : tags(html, low, "meta")) {
            std::string k = toLower(htmlAttr(t, "name") + htmlAttr(t, "property"));
            if (k == key) return trim(htmlAttr(t, "content"));
        }
        return std::string("\x01");  // absent
    };
    std::string lang = htmlAttr(between(html, "<html", ">"), "lang");
    if (lang.empty()) issues.push_back("<html> has no lang attribute");
    size_t ts = low.find("<title"), tb = ts == std::string::npos ? ts : low.find('>', ts);
    size_t te = tb == std::string::npos ? tb : low.find("</title", tb);
    std::string title = te == std::string::npos ? "" : trim(htmlToText(html.substr(tb + 1, te - tb - 1)));
    if (title.empty()) issues.push_back("missing <title>");
    else {
        facts.push_back("title (" + std::to_string(title.size()) + " chars): " + title.substr(0, 90));
        if (title.size() < 15 || title.size() > 65) issues.push_back("title length " + std::to_string(title.size()) + " (aim 15-65)");
    }
    std::string desc = meta("description");
    if (desc == "\x01" || desc.empty()) issues.push_back("missing meta description");
    else if (desc.size() < 50 || desc.size() > 160) issues.push_back("meta description length " + std::to_string(desc.size()) + " (aim 50-160)");
    if (meta("viewport") == "\x01") issues.push_back("missing viewport meta (mobile rendering)");
    if (low.find("<meta charset") == std::string::npos && low.find("charset=") == std::string::npos) issues.push_back("no charset declared");
    std::string robots = meta("robots");
    if (robots != "\x01" && robots.find("noindex") != std::string::npos) issues.push_back("robots meta says noindex");
    bool canonical = false;
    for (const auto& t : tags(html, low, "link"))
        if (toLower(htmlAttr(t, "rel")) == "canonical") canonical = true, facts.push_back("canonical: " + htmlAttr(t, "href"));
    if (!canonical) issues.push_back("no canonical link");
    for (const char* k : {"og:title", "og:description", "og:image", "twitter:card"})
        if (meta(k) == "\x01") issues.push_back(std::string("missing ") + k + " (share cards)");
    // Headings: one h1, no skipped levels.
    int prev = 0, h1 = 0;
    std::string outline;
    for (size_t p = 0; (p = low.find("<h", p)) != std::string::npos; ++p) {
        if (p + 2 >= low.size() || low[p + 2] < '1' || low[p + 2] > '6') continue;
        char after = p + 3 < low.size() ? low[p + 3] : '>';
        if (after != '>' && !isspace((unsigned char)after)) continue;
        int level = low[p + 2] - '0';
        h1 += level == 1;
        if (prev && level > prev + 1) issues.push_back("heading jumps h" + std::to_string(prev) + " -> h" + std::to_string(level));
        prev = level;
        outline += "h" + std::to_string(level) + " ";
    }
    if (h1 != 1) issues.push_back(std::to_string(h1) + " <h1> elements (want exactly 1)");
    if (!outline.empty()) facts.push_back("outline: " + outline.substr(0, 120));
    size_t imgs = 0, noAlt = 0;
    for (const auto& t : tags(html, low, "img")) {
        ++imgs;
        if (toLower(t).find(" alt=") == std::string::npos) ++noAlt;
    }
    if (noAlt) issues.push_back(std::to_string(noAlt) + "/" + std::to_string(imgs) + " <img> without alt");
    size_t links = 0, emptyLinks = 0;
    for (const auto& t : tags(html, low, "a")) {
        ++links;
        std::string href = trim(htmlAttr(t, "href"));
        if (href.empty() || href == "#" || startsWith(toLower(href), "javascript:")) ++emptyLinks;
    }
    if (emptyLinks) issues.push_back(std::to_string(emptyLinks) + " link(s) with empty/# /javascript: href");
    size_t ld = 0;
    for (size_t p = 0; (p = low.find("application/ld+json", p)) != std::string::npos; ++p) {
        ++ld;
        size_t s = low.find('>', p), e = low.find("</script", p);
        if (s != std::string::npos && e != std::string::npos && s < e && !json::parse(html.substr(s + 1, e - s - 1)).ok)
            issues.push_back("JSON-LD block " + std::to_string(ld) + " is not valid JSON");
    }
    size_t words = 0;
    std::string text = htmlToText(html);
    for (size_t i = 0; i < text.size(); ++i)
        words += isalnum((unsigned char)text[i]) && (i == 0 || isspace((unsigned char)text[i - 1]));
    facts.push_back("lang " + (lang.empty() ? std::string("-") : lang) + " · " + std::to_string(words) + " words · " +
                    std::to_string(links) + " links · " + std::to_string(imgs) + " images · " + std::to_string(ld) +
                    " JSON-LD");
    if (words < 250) issues.push_back("thin content: " + std::to_string(words) + " words");
    for (const auto& f : facts) printf("%s\n", f.c_str());
    if (issues.empty()) { printf("no on-page SEO issues found\n"); return 0; }
    printf("issues (%zu):\n", issues.size());
    for (const auto& i : issues) printf("  - %s\n", i.c_str());
    return 1;
}

int kitCsv(const std::vector<std::string>& a) {
    if (a.empty()) return fail("usage: kit csv FILE [--sep C]   (dataset profile: types, missing, stats, balance)");
    auto f = readFileBounded(a[0], 256u << 20);
    if (!f.ok) return fail(f.error);
    const std::string& s = f.value;
    char sep = 0;
    if (a.size() > 2 && a[1] == "--sep") sep = a[2] == "\\t" || a[2] == "tab" ? '\t' : a[2][0];
    if (!sep) {  // most frequent candidate on the first line
        std::string first = s.substr(0, s.find('\n'));
        size_t best = 0;
        for (char c : {',', '\t', ';', '|'})
            if (size_t n = std::count(first.begin(), first.end(), c); n > best) best = n, sep = c;
        if (!sep) sep = ',';
    }
    // RFC 4180: quoted fields may hold separators, doubled quotes and newlines.
    std::vector<std::vector<std::string>> rows(1, std::vector<std::string>(1));
    bool quoted = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (quoted) {
            if (c == '"' && i + 1 < s.size() && s[i + 1] == '"') rows.back().back() += '"', ++i;
            else if (c == '"') quoted = false;
            else rows.back().back() += c;
        } else if (c == '"' && rows.back().back().empty()) quoted = true;
        else if (c == sep) rows.back().emplace_back();
        else if (c == '\n') rows.emplace_back(1);
        else if (c != '\r') rows.back().back() += c;
    }
    while (!rows.empty() && rows.back().size() == 1 && rows.back()[0].empty()) rows.pop_back();
    if (rows.size() < 2) return fail("need a header row and at least one data row");
    const auto& head = rows[0];
    size_t ragged = 0;
    printf("%zu rows × %zu columns (sep '%s')\n", rows.size() - 1, head.size(), sep == '\t' ? "\\t" : std::string(1, sep).c_str());
    for (size_t c = 0; c < head.size(); ++c) {
        size_t missing = 0, nums = 0, ints = 0;
        double sum = 0, sq = 0, lo = INFINITY, hi = -INFINITY;
        std::map<std::string, size_t> counts;
        bool capped = false;
        for (size_t r = 1; r < rows.size(); ++r) {
            if (c == 0 && rows[r].size() != head.size()) ++ragged;
            std::string v = c < rows[r].size() ? trim(rows[r][c]) : "";
            std::string lv = toLower(v);
            if (v.empty() || lv == "na" || lv == "nan" || lv == "null" || lv == "none" || lv == "?") { ++missing; continue; }
            char* end = nullptr;
            double d = strtod(v.c_str(), &end);
            if (end && *end == 0 && std::isfinite(d)) {
                ++nums;
                ints += d == std::floor(d) && v.find_first_of(".eE") == std::string::npos;
                sum += d, sq += d * d, lo = std::min(lo, d), hi = std::max(hi, d);
            }
            if (counts.size() < 10000 || counts.count(v)) ++counts[v];
            else capped = true;
        }
        size_t present = rows.size() - 1 - missing;
        std::string type = present == 0 ? "empty" : nums == present ? (ints == nums ? "int" : "float") :
                           nums * 10 >= present * 9 ? "mostly-numeric" : "text";
        printf("\n[%zu] %s: %s · missing %zu (%.1f%%) · unique %s%zu\n", c, head[c].c_str(), type.c_str(), missing,
               100.0 * missing / (rows.size() - 1), capped ? ">" : "", counts.size());
        if (nums && (type == "int" || type == "float" || type == "mostly-numeric")) {
            double mean = sum / nums, sd = std::sqrt(std::max(0.0, sq / nums - mean * mean));
            printf("    min %g · max %g · mean %.4g · std %.4g\n", lo, hi, mean, sd);
            if (nums != present) printf("    %zu non-numeric value(s) mixed in\n", present - nums);
        }
        if (counts.size() <= 20 && counts.size() < present) {  // categorical: class balance
            std::vector<std::pair<size_t, std::string>> top;
            for (const auto& [k, n] : counts) top.push_back({n, k});
            std::sort(top.rbegin(), top.rend());
            printf("   ");
            for (const auto& [n, k] : top) printf(" %s=%zu (%.0f%%)", k.substr(0, 24).c_str(), n, 100.0 * n / present);
            printf("\n");
        } else if (counts.size() == present && type == "text") printf("    all values distinct (identifier?)\n");
    }
    if (ragged) printf("\nwarning: %zu row(s) have a different column count than the header\n", ragged);
    return 0;
}

int kitBench(const std::vector<std::string>& a) {
    constexpr const char* usage = "usage: kit bench [-n RUNS=10] [-w WARMUP=1] 'COMMAND'   (wall time, CPU, peak RSS)";
    long runs = 10, warm = 1;
    std::string cmd;
    for (size_t i = 0; i < a.size(); ++i) {
        if ((a[i] == "-n" || a[i] == "-w") && i + 1 < a.size()) {
            long v = atol(a[i + 1].c_str());
            (a[i] == "-n" ? runs : warm) = v;
            ++i;
        }
        else cmd += (cmd.empty() ? "" : " ") + a[i];
    }
    if (cmd.empty() || runs < 1 || runs > 10000 || warm < 0 || warm > 100) return fail(usage);
    std::vector<double> wall, cpu;
    long peakKb = 0;
    int failures = 0, lastCode = 0;
    for (long i = 0; i < warm + runs; ++i) {
        timespec a0{}, a1{};
        clock_gettime(CLOCK_MONOTONIC, &a0);
        pid_t pid = fork();
        if (pid < 0) return fail("fork failed");
        if (pid == 0) {
            int null = open("/dev/null", O_RDWR);
            dup2(null, 1), dup2(null, 2);
            execl("/bin/bash", "bash", "--noprofile", "--norc", "-c", cmd.c_str(), (char*)nullptr);
            _exit(127);
        }
        int status = 0;
        rusage ru{};
        if (wait4(pid, &status, 0, &ru) < 0) return fail("wait failed");
        clock_gettime(CLOCK_MONOTONIC, &a1);
        int code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        if (i < warm) continue;
        if (code) ++failures, lastCode = code;
        wall.push_back((a1.tv_sec - a0.tv_sec) * 1e3 + (a1.tv_nsec - a0.tv_nsec) / 1e6);
        cpu.push_back((ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1e3 + (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1e3);
        peakKb = std::max(peakKb, ru.ru_maxrss);
    }
    double mean = 0;
    for (double w : wall) mean += w / wall.size();
    double var = 0;
    for (double w : wall) var += (w - mean) * (w - mean) / wall.size();
    printf("%ld run(s) after %ld warmup: min %.2f · median %.2f · mean %.2f ± %.2f · p95 %.2f · max %.2f ms\n", runs,
           warm, percentile(wall, 0), percentile(wall, .5), mean, std::sqrt(var), percentile(wall, .95),
           percentile(wall, 1));
    printf("cpu median %.2f ms · peak RSS %.1f MiB\n", percentile(cpu, .5), peakKb / 1024.0);
    if (failures) printf("%d run(s) failed (last exit %d)\n", failures, lastCode);
    return failures ? 1 : 0;
}

}  // namespace pocket
