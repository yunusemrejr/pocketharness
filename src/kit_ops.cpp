// PocketHarness - `pocket kit` operations: networking, SEO, data and timing.
#include "kit_ops.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <net/if.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <sstream>

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

SpawnResult curl(std::vector<std::string> args, long timeoutSec, long deadlineMs = 0) {
    SpawnOpts o;
    o.exe = "curl";
    const double timeout = deadlineMs > 0 ? std::min<double>(timeoutSec, deadlineMs / 1000.0) : timeoutSec;
    o.argv = {"curl", "--disable", "-sS", "--proto", "=http,https", "--proto-redir", "=http,https",
              "--max-time", std::to_string(timeout)};
    o.argv.insert(o.argv.end(), args.begin(), args.end());
    o.timeoutMs = deadlineMs > 0 ? deadlineMs : (timeoutSec + 3) * 1000;
    if (deadlineMs > 0) o.terminateGraceMs = 0;
    o.outLimit = 8 << 20;
    o.stopOnLimit = true;
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

bool seconds(const std::string& value, double* out, double max = 600) {
    if (value.empty()) return false;
    char* end = nullptr;
    errno = 0;
    *out = strtod(value.c_str(), &end);
    return !errno && end == value.c_str() + value.size() && std::isfinite(*out) && *out > 0 && *out <= max;
}

struct Endpoint { std::string host, port; };

bool endpoint(const std::string& target, Endpoint* out) {
    out->host = "127.0.0.1";
    out->port = target;
    if (!target.empty() && target[0] == '[') {
        size_t end = target.find(']');
        if (end == std::string::npos || end + 1 >= target.size() || target[end + 1] != ':') return false;
        out->host = target.substr(1, end - 1);
        out->port = target.substr(end + 2);
    } else if (size_t at = target.rfind(':'); at != std::string::npos) {
        // Brackets keep the boundary unambiguous for IPv6 addresses.
        if (target.find(':') != at) return false;
        out->host = target.substr(0, at);
        out->port = target.substr(at + 1);
    }
    if (out->host.empty() || out->host.size() > 253 || out->host[0] == '-' ||
        out->host.find_first_of(" \t\r\n/@?[]") != std::string::npos || out->port.empty() || out->port.size() > 5)
        return false;
    unsigned port = 0;
    for (unsigned char c : out->port) {
        if (!std::isdigit(c)) return false;
        port = port * 10 + c - '0';
    }
    return port > 0 && port <= 65535;
}

struct SocketAddress {
    sockaddr_storage value{};
    socklen_t size = 0;
    std::string text;
};

bool numericAddress(const std::string& host, const std::string& port, SocketAddress* out) {
    auto* v4 = reinterpret_cast<sockaddr_in*>(&out->value);
    if (inet_pton(AF_INET, host.c_str(), &v4->sin_addr) == 1) {
        v4->sin_family = AF_INET;
        v4->sin_port = htons((uint16_t)std::stoi(port));
        out->size = sizeof(*v4);
        out->text = host;
        return true;
    }
    std::string base = host;
    unsigned scope = 0;
    if (size_t at = base.find('%'); at != std::string::npos) {
        const std::string zone = base.substr(at + 1);
        base.resize(at);
        if (zone.empty() || zone.size() >= IF_NAMESIZE) return false;
        if (std::all_of(zone.begin(), zone.end(), [](unsigned char c) { return std::isdigit(c); })) {
            char* end = nullptr;
            errno = 0;
            unsigned long parsed = strtoul(zone.c_str(), &end, 10);
            if (errno || *end || parsed > UINT32_MAX) return false;
            scope = (unsigned)parsed;
        } else scope = if_nametoindex(zone.c_str());
        if (!scope) return false;
    }
    auto* v6 = reinterpret_cast<sockaddr_in6*>(&out->value);
    if (inet_pton(AF_INET6, base.c_str(), &v6->sin6_addr) != 1) return false;
    v6->sin6_family = AF_INET6;
    v6->sin6_port = htons((uint16_t)std::stoi(port));
    v6->sin6_scope_id = scope;
    out->size = sizeof(*v6);
    out->text = host;
    return true;
}

std::vector<SocketAddress> resolve(const Endpoint& target, int64_t deadline, std::string* error) {
    SocketAddress direct;
    if (numericAddress(target.host, target.port, &direct)) return {direct};
    // getaddrinfo can wait on NSS plugins/DNS well past the caller's deadline.
    // Run the Linux resolver utility in a cancellable process; numeric targets
    // never need a subprocess and hostnames still honor the real NSS settings.
    SpawnOpts opts;
    opts.exe = "getent";
    opts.argv = {"getent", "--", "ahosts", target.host};
    opts.timeoutMs = std::max<int64_t>(1, deadline - nowMs());
    opts.terminateGraceMs = 0;
    opts.outLimit = 32768;
    opts.stopOnLimit = true;
    auto result = spawn(opts);
    std::vector<SocketAddress> addresses;
    if (!result.ok || result.exitCode != 0 || result.timedOut || result.cancelled || result.truncated) {
        *error = result.timedOut ? "resolver deadline exceeded" :
                 result.exitCode == 127 ? "getent unavailable; use a numeric address or install the Linux resolver utility" :
                 "system resolver found no address for " + target.host;
        return addresses;
    }
    std::set<std::string> seen;
    for (const auto& line : splitLines(result.out)) {
        std::istringstream input(line);
        std::string ip;
        input >> ip;
        SocketAddress address;
        if (seen.insert(ip).second && numericAddress(ip, target.port, &address)) addresses.push_back(address);
        if (addresses.size() >= 16) break;
    }
    if (addresses.empty()) *error = "system resolver returned no usable TCP address for " + target.host;
    return addresses;
}

bool tcpUp(const std::vector<SocketAddress>& addresses, int64_t deadline, std::string* last) {
    std::vector<pollfd> pending;
    std::vector<std::string> names;
    struct CloseSockets {
        std::vector<pollfd>& pending;
        ~CloseSockets() { for (const auto& entry : pending) if (entry.fd >= 0) close(entry.fd); }
    } cleanup{pending};
    for (const auto& address : addresses) {
        if (nowMs() >= deadline) { *last = "TCP deadline exceeded"; break; }
        int fd = socket(address.value.ss_family, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (fd < 0) { *last = strerror(errno); continue; }
        if (connect(fd, reinterpret_cast<const sockaddr*>(&address.value), address.size) == 0) {
            close(fd);
            *last = address.text;
            return true;
        }
        const int error = errno;
        if (error == EINPROGRESS || error == EINTR) {
            pending.push_back({fd, POLLOUT, 0});
            names.push_back(address.text);
            continue;
        }
        *last = address.text + ": " + strerror(error);
        close(fd);
    }
    size_t active = pending.size();
    while (active && nowMs() < deadline) {
        const int ready = poll(pending.data(), pending.size(), (int)std::max<int64_t>(1, deadline - nowMs()));
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) { *last = ready == 0 ? "TCP deadline exceeded" : strerror(errno); break; }
        for (size_t i = 0; i < pending.size(); ++i) {
            auto& entry = pending[i];
            if (entry.fd < 0 || !entry.revents) continue;
            int error = 0;
            socklen_t size = sizeof(error);
            if (getsockopt(entry.fd, SOL_SOCKET, SO_ERROR, &error, &size) != 0) error = errno;
            if (!error) { *last = names[i]; return true; }
            *last = names[i] + ": " + strerror(error);
            close(entry.fd);
            entry.fd = -1;
            --active;
        }
    }
    return false;
}

struct HtmlTag {
    std::string name;
    std::map<std::string, std::string> attrs;
    std::string content;  // raw-text elements only (script/style/title/textarea)
};

std::vector<HtmlTag> htmlTags(const std::string& html) {
    const std::string lower = toLower(html);
    std::vector<HtmlTag> out;
    for (size_t p = 0; (p = html.find('<', p)) != std::string::npos;) {
        if (html.compare(p, 4, "<!--") == 0) {
            size_t end = html.find("-->", p + 4);
            p = end == std::string::npos ? html.size() : end + 3;
            continue;
        }
        const size_t tagStart = p;
        size_t cursor = p + 1;
        char quote = 0;
        for (; cursor < html.size(); ++cursor) {
            const char c = html[cursor];
            if (quote) { if (c == quote) quote = 0; }
            else if (c == '\'' || c == '"') quote = c;
            else if (c == '>') break;
        }
        if (cursor == html.size()) break;
        const size_t end = cursor++;
        size_t start = p + 1;
        p = cursor;
        if (start == end || !std::isalpha((unsigned char)html[start])) continue;
        HtmlTag tag;
        while (start < end && (std::isalnum((unsigned char)html[start]) || html[start] == '-'))
            tag.name += (char)std::tolower((unsigned char)html[start++]);
        tag.attrs = htmlAttrs(std::string_view(html).substr(tagStart, end - tagStart + 1));
        if (tag.name == "script" || tag.name == "style" || tag.name == "title" || tag.name == "textarea") {
            const std::string closing = "</" + tag.name;
            size_t close = p;
            while ((close = lower.find(closing, close)) != std::string::npos) {
                const size_t after = close + closing.size();
                if (after == lower.size() || lower[after] == '>' || std::isspace((unsigned char)lower[after])) break;
                ++close;
            }
            tag.content = html.substr(p, close == std::string::npos ? std::string::npos : close - p);
            p = close == std::string::npos ? html.size() : close;
        }
        out.push_back(std::move(tag));
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
    const char* usage = "usage: kit wait PORT|HOST:PORT|[IPv6]:PORT|URL [SECONDS=30] [--any-status]";
    if (a.empty() || a.size() > 3) return fail(usage);
    double limit = 30;
    bool anyStatus = false, hadSeconds = false;
    for (size_t i = 1; i < a.size(); ++i) {
        if (a[i] == "--any-status" && !anyStatus) anyStatus = true;
        else if (!hadSeconds && seconds(a[i], &limit)) hadSeconds = true;
        else return fail(usage);
    }
    const std::string& target = a[0];
    Endpoint parsed;
    if ((!isUrl(target) && (!endpoint(target, &parsed) || anyStatus)) ||
        (isUrl(target) && target.find_first_of("\r\n") != std::string::npos)) return fail(usage);
    const int64_t start = nowMs(), deadline = start + (int64_t)std::ceil(limit * 1000);
    std::vector<SocketAddress> addresses;
    std::string last;
    while (nowMs() < deadline) {
        if (isUrl(target)) {
            SpawnResult r = curl({"-o", "/dev/null", "-w", "%{http_code}", "--url", target}, 5,
                                 (long)std::max<int64_t>(1, deadline - nowMs()));
            last = trim(r.out) + " " + trim(r.err);
            const std::string code = trim(r.out);
            const bool validCode = code.size() == 3 && std::all_of(code.begin(), code.end(), [](unsigned char c) { return std::isdigit(c); });
            const int status = validCode ? std::stoi(code) : 0;
            if (r.ok && r.exitCode == 0 && !r.timedOut && !r.cancelled && !r.truncated && status >= 100 && status <= 599 &&
                (anyStatus || (status >= 200 && status < 400))) {
                printf("up: %s answered HTTP %s after %.2fs\n", target.c_str(), code.c_str(), (nowMs() - start) / 1000.0);
                return 0;
            }
        } else {
            if (addresses.empty()) addresses = resolve(parsed, deadline, &last);
            if (!addresses.empty() && tcpUp(addresses, std::min<int64_t>(deadline, nowMs() + 1000), &last)) {
                printf("up: %s accepts connections via %s after %.2fs\n", target.c_str(), last.c_str(), (nowMs() - start) / 1000.0);
                return 0;
            }
        }
        const int64_t remaining = deadline - nowMs();
        if (remaining > 0) usleep((useconds_t)std::min<int64_t>(250000, remaining * 1000));
    }
    return fail("timed out after " + std::to_string(limit) + "s waiting for " + target +
                (last.empty() ? "" : " (last: " + trim(last).substr(0, 200) + ")") + "; check the server log");
}

int kitReach(const std::vector<std::string>& a) {
    if (a.empty() || a.size() > 2) return fail("usage: kit reach HOST:PORT|[IPv6]:PORT [SECONDS=3]");
    Endpoint target;
    double limit = 3;
    if (!endpoint(a[0], &target) || (a.size() == 2 && !seconds(a[1], &limit, 60)))
        return fail("reach needs a valid host/port and finite seconds in (0,60]");
    const int64_t start = nowMs(), deadline = start + (int64_t)std::ceil(limit * 1000);
    std::string detail;
    auto addresses = resolve(target, deadline, &detail);
    if (addresses.empty()) return fail("resolver: " + detail);
    printf("resolver: %s ->", target.host.c_str());
    for (const auto& address : addresses) printf(" %s", address.text.c_str());
    printf(" (%.0f ms; system NSS for hostnames)\n", (double)(nowMs() - start));
    const bool connected = tcpUp(addresses, deadline, &detail);
    printf("TCP: %s port %s (%s) after %.0f ms\n", connected ? "connected" : "unreachable", target.port.c_str(),
           detail.c_str(), (double)(nowMs() - start));
    printf("TCP success does not verify TLS or application readiness; use kit net or kit wait URL.\n");
    return connected ? 0 : 1;
}

int kitSys(const std::vector<std::string>& a) {
    if (a.size() > 1) return fail("usage: kit sys [PID]   (Linux pressure and process evidence)");
    auto show = [](const char* label, const std::string& path, size_t cap) {
        auto data = readFileBounded(path, cap);
        printf("%s: %s\n", label, data.ok ? trim(data.value).c_str() : "unavailable");
        return data.ok;
    };
    if (a.empty()) {
        utsname host{};
        if (uname(&host) == 0) printf("kernel: %s %s %s\n", host.sysname, host.release, host.machine);
        show("load", "/proc/loadavg", 4096);
        auto memory = readFileBounded("/proc/meminfo", 65536);
        if (!memory.ok) return fail("/proc/meminfo unavailable; Linux procfs is required");
        for (const auto& line : splitLines(memory.value))
            for (const auto* key : {"MemTotal:", "MemAvailable:", "SwapTotal:", "SwapFree:"})
                if (startsWith(line, key)) printf("%s\n", line.c_str());
        for (const auto* resource : {"cpu", "memory", "io"})
            show((std::string("pressure/") + resource).c_str(), std::string("/proc/pressure/") + resource, 4096);
        struct statvfs disk{};
        if (statvfs(".", &disk) == 0)
            printf("workspace disk: %llu MiB available, %llu free inodes\n",
                   (unsigned long long)disk.f_bavail * disk.f_frsize / (1 << 20), (unsigned long long)disk.f_favail);
        printf("snapshot only: load includes runnable and uninterruptible tasks; pressure reports stall time.\n");
        return 0;
    }
    if (a[0].empty() || a[0].size() > 10 ||
        !std::all_of(a[0].begin(), a[0].end(), [](unsigned char c) { return std::isdigit(c); }) ||
        strtoul(a[0].c_str(), nullptr, 10) == 0 || strtoul(a[0].c_str(), nullptr, 10) > INT32_MAX)
        return fail("PID must be a positive integer");
    const std::string base = "/proc/" + a[0];
    auto status = readFileBounded(base + "/status", 65536);
    if (!status.ok) return fail("process status unavailable: " + a[0] + " (exited or permission denied)");
    printf("process %s:\n", a[0].c_str());
    for (const auto& line : splitLines(status.value))
        for (const auto* key : {"Name:", "State:", "Uid:", "Gid:", "Threads:", "VmRSS:", "VmSize:", "VmSwap:",
                               "FDSize:", "voluntary_ctxt_switches:", "nonvoluntary_ctxt_switches:"})
            if (startsWith(line, key)) printf("  %s\n", line.c_str());
    show("wait channel", base + "/wchan", 4096);
    show("I/O counters", base + "/io", 8192);
    printf("no environment or command-line secrets are read; use repeated snapshots to establish growth.\n");
    return 0;
}

int kitNet(const std::vector<std::string>& a) {
    if (a.size() != 1 || !isUrl(a[0])) return fail("usage: kit net URL   (timing, TLS, headers, redirects)");
    SpawnResult r = curl({"-L", "--max-redirs", "10", "--compressed", "-o", "/dev/null", "-D", "-", "-w",
                          "\n@@%{http_code} %{http_version} %{remote_ip} %{num_redirects} %{size_download} "
                          "%{time_namelookup} %{time_connect} %{time_appconnect} %{time_starttransfer} %{time_total} "
                          "%{ssl_verify_result} %{url_effective}", a[0]},
                         30);
    size_t at = r.out.rfind("\n@@");
    if (!r.ok || r.exitCode != 0 || r.timedOut || r.cancelled || r.truncated || at == std::string::npos)
        return fail("request failed: " + trim(r.err.empty() ? r.error : r.err).substr(0, 300));
    char eff[2048] = {}, ver[16] = {}, ip[64] = {};
    int code = 0, redirects = 0, verify = 0;
    double size = 0, dns = 0, conn = 0, tls = 0, ttfb = 0, total = 0;
    if (sscanf(r.out.c_str() + at + 3, "%d %15s %63s %d %lf %lf %lf %lf %lf %lf %d %2047s", &code, ver, ip, &redirects,
               &size, &dns, &conn, &tls, &ttfb, &total, &verify, eff) != 12 || code < 100 || code > 599)
        return fail("curl returned incomplete HTTP diagnostic data");
    printf("%s -> HTTP %d (HTTP/%s) from %s, %d redirect(s), %.0f bytes\n", a[0].c_str(), code, ver, ip, redirects, size);
    if (redirects) printf("final: %s\n", eff);
    if (startsWith(a[0], "https://") && startsWith(eff, "http://")) printf("transport: redirected to HTTP; final response has no TLS protection\n");
    printf("timing ms: dns %.0f · connect %.0f · tls %.0f · first byte %.0f · total %.0f\n", dns * 1000,
           (conn - dns) * 1000, tls > 0 ? (tls - conn) * 1000 : 0, (ttfb - std::max(tls, conn)) * 1000, total * 1000);
    if (startsWith(a[0], "https://") && verify != 0) printf("TLS: certificate verification FAILED (%d)\n", verify);
    // Headers of the final response only (earlier blocks were redirects).
    std::string head = r.out.substr(0, at);
    size_t lastBlock = head.rfind("\r\n\r\nHTTP/");
    if (lastBlock != std::string::npos) head = head.substr(lastBlock + 4);
    else if ((lastBlock = head.rfind("\n\nHTTP/")) != std::string::npos) head = head.substr(lastBlock + 2);
    std::string low = toLower(head);
    printf("headers:\n");
    for (const auto& line : splitLines(head)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string name = toLower(trim(line.substr(0, colon)));
        if (name == "set-cookie" || name == "authorization" || name == "proxy-authorization")
            printf("  %s: [redacted]\n", trim(line.substr(0, colon)).c_str());
        else printf("  %s\n", trim(line).substr(0, 200).c_str());
    }
    std::vector<std::string> missing;
    for (const char* h : {"strict-transport-security", "content-security-policy", "x-content-type-options",
                          "cache-control", "content-encoding"})
        if ((std::string(h) != "strict-transport-security" || startsWith(eff, "https://")) &&
            low.find(std::string("\n") + h + ":") == std::string::npos) missing.push_back(h);
    if (!missing.empty()) {
        printf("header context checks (requirements depend on the resource):");
        for (const auto& m : missing) printf(" %s", m.c_str());
        printf("\n");
    }
    return code >= 200 && code < 400 ? 0 : 1;
}

int kitSeo(const std::vector<std::string>& a) {
    if (a.size() != 1) return fail("usage: kit seo FILE.html|URL   (source metadata, indexability and accessibility evidence)");
    std::string html;
    if (isUrl(a[0])) {
        SpawnResult r = curl({"-L", "--max-redirs", "10", "--compressed", "-A", "Mozilla/5.0 (compatible; pocket-seo)",
                              "-w", "\n@@%{http_code}", "--url", a[0]}, 20);
        const size_t marker = r.out.rfind("\n@@");
        if (!r.ok || r.exitCode != 0 || r.timedOut || r.cancelled || r.truncated || marker == std::string::npos)
            return fail("fetch failed: " + trim(r.err.empty() ? r.error : r.err).substr(0, 300));
        const std::string code = trim(r.out.substr(marker + 3));
        if (code.size() != 3 || code[0] != '2' ||
            !std::all_of(code.begin(), code.end(), [](unsigned char c) { return std::isdigit(c); }))
            return fail("SEO source fetch returned HTTP " + code + "; inspect delivery with kit net");
        html = r.out.substr(0, marker);
        printf("HTTP %s source retrieved\n", code.c_str());
    } else {
        auto f = readFileBounded(a[0], 8 << 20);
        if (!f.ok) return fail(f.error);
        html = std::move(f.value);
    }
    const auto tags = htmlTags(html);
    std::vector<std::string> issues, recommendations, facts;
    if (tags.size() >= 5000) issues.push_back("source scan reached the 5000-tag limit; audit is incomplete");
    auto value = [](const HtmlTag& tag, const std::string& key) {
        auto it = tag.attrs.find(key);
        return it == tag.attrs.end() ? std::string() : trim(it->second);
    };
    auto meta = [&](const std::string& key) {
        for (const auto& tag : tags)
            if (tag.name == "meta" && (toLower(value(tag, "name")) == key || toLower(value(tag, "property")) == key))
                return value(tag, "content");
        return std::string();
    };
    auto characters = [](const std::string& s) {
        return std::count_if(s.begin(), s.end(), [](unsigned char c) { return (c & 0xc0) != 0x80; });
    };
    std::string lang, title;
    size_t titles = 0, canonicals = 0, h1 = 0, images = 0, noAlt = 0, links = 0, emptyLinks = 0, ld = 0;
    int previous = 0;
    bool charset = false, blocked = false;
    std::string outline;
    for (const auto& tag : tags) {
        if (tag.name == "html" && lang.empty()) lang = value(tag, "lang");
        if (tag.name == "title") { ++titles; if (title.empty()) title = trim(htmlToText(tag.content)); }
        if (tag.name == "meta") {
            const std::string name = toLower(value(tag, "name"));
            std::string content = toLower(value(tag, "content"));
            charset |= !value(tag, "charset").empty() ||
                       (toLower(value(tag, "http-equiv")) == "content-type" && content.find("charset=") != std::string::npos);
            if (name == "robots" || name == "googlebot") {
                for (char& c : content) if (c == ',' || c == ';') c = ' ';
                std::istringstream directives(content);
                std::string directive;
                while (directives >> directive) blocked |= directive == "noindex" || directive == "none";
            }
        }
        if (tag.name == "link") {
            std::istringstream rels(toLower(value(tag, "rel")));
            std::string rel;
            while (rels >> rel)
                if (rel == "canonical") {
                    ++canonicals;
                    const std::string href = value(tag, "href");
                    if (href.empty() || href[0] == '#' || startsWith(toLower(href), "javascript:"))
                        issues.push_back("canonical link has no usable href");
                    else facts.push_back("canonical: " + href.substr(0, 200));
                }
        }
        if (tag.name.size() == 2 && tag.name[0] == 'h' && tag.name[1] >= '1' && tag.name[1] <= '6') {
            const int level = tag.name[1] - '0';
            h1 += level == 1;
            if (previous && level > previous + 1)
                recommendations.push_back("heading jumps h" + std::to_string(previous) + " -> h" + std::to_string(level) + "; inspect the document outline");
            previous = level;
            outline += tag.name + " ";
        }
        if (tag.name == "img") { ++images; noAlt += !tag.attrs.count("alt"); }
        if (tag.name == "a") {
            ++links;
            const std::string href = value(tag, "href");
            emptyLinks += href.empty() || href == "#" || startsWith(toLower(href), "javascript:");
        }
        if (tag.name == "script" && toLower(value(tag, "type")) == "application/ld+json") {
            ++ld;
            const auto parsed = json::parse(tag.content);
            if (!parsed.ok || (!parsed.value.isObj() && !parsed.value.isArr()))
                issues.push_back("JSON-LD block " + std::to_string(ld) + " is not valid JSON-LD object/array syntax");
        }
    }
    if (lang.empty()) issues.push_back("<html> has no lang attribute");
    if (title.empty()) issues.push_back("missing or empty <title>");
    else facts.push_back("title (" + std::to_string(characters(title)) + " characters): " + title.substr(0, 160));
    if (titles > 1) issues.push_back("multiple <title> elements");
    const std::string description = meta("description");
    if (description.empty()) recommendations.push_back("no meta description; write one when it usefully describes this page");
    else facts.push_back("description: " + std::to_string(characters(description)) + " characters");
    if (meta("viewport").empty()) recommendations.push_back("missing viewport meta; verify mobile rendering");
    if (!charset) recommendations.push_back("no source charset declared; verify the HTTP Content-Type or add UTF-8 metadata");
    if (blocked) issues.push_back("robots meta blocks indexing with noindex/none (verify that this page is intended to be public)");
    if (!canonicals) recommendations.push_back("no canonical link; inspect duplicate URL policy before adding one");
    if (canonicals > 1) issues.push_back("multiple canonical links; select one intended canonical URL");
    for (const char* key : {"og:title", "og:description", "og:image", "twitter:card"})
        if (meta(key).empty()) recommendations.push_back(std::string("missing/empty ") + key + " (optional share-card metadata)");
    if (h1 != 1) recommendations.push_back(std::to_string(h1) + " <h1> elements; inspect whether the page has a clear main heading");
    if (!outline.empty()) facts.push_back("outline: " + outline.substr(0, 120));
    if (noAlt) issues.push_back(std::to_string(noAlt) + "/" + std::to_string(images) + " <img> without alt");
    if (emptyLinks) recommendations.push_back(std::to_string(emptyLinks) + " link(s) with empty/# /javascript: href; verify real navigation");
    size_t words = 0;
    const std::string text = htmlToText(html);
    bool inWord = false;
    for (unsigned char c : text) {
        const bool word = std::isalnum(c) || c >= 0x80;
        words += word && !inWord;
        inWord = word;
    }
    facts.push_back("lang " + (lang.empty() ? std::string("-") : lang) + " · " + std::to_string(words) + " source words · " +
                    std::to_string(links) + " links · " + std::to_string(images) + " images · " + std::to_string(ld) + " JSON-LD");
    for (const auto& fact : facts) printf("%s\n", fact.c_str());
    printf("source audit only: robots.txt, response indexing headers, rendered content and ranking are not verified.\n");
    if (issues.empty()) printf("no definite source issues found\n");
    else {
        printf("issues (%zu):\n", issues.size());
        for (const auto& issue : issues) printf("  - %s\n", issue.c_str());
    }
    if (!recommendations.empty()) {
        printf("context checks (%zu; advisory, not ranking requirements):\n", recommendations.size());
        for (const auto& item : recommendations) printf("  - %s\n", item.c_str());
    }
    return issues.empty() ? 0 : 1;
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
