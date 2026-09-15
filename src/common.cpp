// PocketHarness - shared helpers implementation.
#include "common.h"

#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace pocket {

std::string trim(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r'))
        ++a;
    while (b > a &&
           (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' || s[b - 1] == '\r'))
        --b;
    return std::string(s.substr(a, b - a));
}

bool startsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() &&
           s.substr(s.size() - suffix.size()) == suffix;
}

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == '\n') {
            std::string line = s.substr(start, i - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            out.push_back(std::move(line));
            start = i + 1;
        }
    }
    return out;
}

std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

std::string toLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = (char)tolower((unsigned char)c);
    return out;
}

std::string sanitizeTerminal(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size();) {
        unsigned char c = (unsigned char)in[i];
        if (c == 0x1b) {
            // ESC: skip CSI (...letter), OSC (...BEL or ST), or single-char seq.
            ++i;
            if (i < in.size() && in[i] == '[') {
                ++i;
                while (i < in.size() &&
                       !((in[i] >= 'A' && in[i] <= 'Z') || (in[i] >= 'a' && in[i] <= 'z')))
                    ++i;
                if (i < in.size()) ++i;
            } else if (i < in.size() && in[i] == ']') {
                ++i;
                while (i < in.size() && in[i] != '\x07') {
                    if (in[i] == 0x1b && i + 1 < in.size() && in[i + 1] == '\\') {
                        i += 2;
                        break;
                    }
                    ++i;
                }
                if (i < in.size() && in[i] == '\x07') ++i;
            } else if (i < in.size() && (in[i] == '(' || in[i] == ')' || in[i] == '#')) {
                i += 2;  // charset selection etc.
            }
            continue;
        }
        if (c < 0x20 && c != '\n' && c != '\t') {
            ++i;  // drop other C0 controls
            continue;
        }
        if (c == 0x7f) {
            ++i;  // drop DEL
            continue;
        }
        out.push_back(in[i]);
        ++i;
    }
    return out;
}

std::string expandHome(std::string_view path) {
    if (path == "~" || startsWith(path, "~/")) {
        std::string h = homeDir();
        if (path.size() == 1) return h;
        return h + std::string(path.substr(1));
    }
    return std::string(path);
}

Result<std::string> readFileBounded(const std::string& path, size_t maxBytes) {
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return Result<std::string>::Err("cannot open " + path);
    std::string out;
    char buf[65536];
    size_t total = 0;
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            close(fd);
            return Result<std::string>::Err("cannot read " + path);
        }
        if (n == 0) break;
        total += (size_t)n;
        if (total > maxBytes) {
            close(fd);
            return Result<std::string>::Err("file too large: " + path);
        }
        out.append(buf, (size_t)n);
    }
    close(fd);
    return Result<std::string>::Ok(std::move(out));
}

VoidResult atomicWriteFile(const std::string& path, const std::string& data, mode_t mode) {
    std::string tmp = path + ".tmp." + randHex(4);
    int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    if (fd < 0) return VoidResult::Err("cannot write temp file for " + path);
    size_t off = 0;
    while (off < data.size()) {
        ssize_t n = write(fd, data.data() + off, data.size() - off);
        if (n <= 0) {
            close(fd);
            unlink(tmp.c_str());
            return VoidResult::Err("write failed for " + path);
        }
        off += (size_t)n;
    }
    if (fsync(fd) != 0) {
        close(fd);
        unlink(tmp.c_str());
        return VoidResult::Err("fsync failed for " + path);
    }
    close(fd);
    if (rename(tmp.c_str(), path.c_str()) != 0) {
        unlink(tmp.c_str());
        return VoidResult::Err("rename failed for " + path);
    }
    return VoidResult::Ok();
}

VoidResult appendLine(const std::string& path, const std::string& line) {
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd < 0) return VoidResult::Err("cannot open session file " + path);
    std::string rec = line + "\n";
    size_t off = 0;
    while (off < rec.size()) {
        ssize_t n = write(fd, rec.data() + off, rec.size() - off);
        if (n <= 0) {
            close(fd);
            return VoidResult::Err("session write failed");
        }
        off += (size_t)n;
    }
    fsync(fd);
    close(fd);
    return VoidResult::Ok();
}

VoidResult ensureDir(const std::string& path, mode_t mode) {
    if (path.empty()) return VoidResult::Err("empty dir path");
    std::string cur;
    size_t i = 0;
    if (path[0] == '/') {
        cur = "/";
        i = 1;
    }
    while (i <= path.size()) {
        size_t j = path.find('/', i);
        if (j == std::string::npos) j = path.size();
        std::string part = path.substr(i, j - i);
        if (!part.empty() && part != ".") {
            if (cur.size() > 1) cur += "/";
            if (cur.empty()) cur = part;
            else if (cur != "/") cur += part;
            else cur += part;
            if (mkdir(cur.c_str(), mode) != 0 && errno != EEXIST)
                return VoidResult::Err("cannot create dir " + cur);
        }
        i = j + 1;
    }
    return VoidResult::Ok();
}

std::string homeDir() {
    if (const char* h = getenv("HOME")) {
        if (h[0] != '\0') return h;
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return pw->pw_dir;
    return "/tmp";
}

int64_t nowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

std::string randHex(size_t bytes) {
    static const char* hexd = "0123456789abcdef";
    std::string out;
    out.reserve(bytes * 2);
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    unsigned char buf[64];
    size_t got = 0;
    if (fd >= 0) {
        while (got < bytes) {
            ssize_t n = read(fd, buf, sizeof(buf) < bytes - got ? sizeof(buf) : bytes - got);
            if (n <= 0) break;
            for (ssize_t k = 0; k < n; ++k) {
                out.push_back(hexd[buf[k] >> 4]);
                out.push_back(hexd[buf[k] & 15]);
            }
            got += (size_t)n;
        }
        close(fd);
    }
    while (out.size() < bytes * 2) {
        unsigned v = (unsigned)rand();
        out.push_back(hexd[v & 15]);
        out.push_back(hexd[(v >> 4) & 15]);
    }
    return out;
}

std::string baseName(const std::string& path) {
    size_t i = path.find_last_of('/');
    if (i == std::string::npos) return path;
    return path.substr(i + 1);
}

}  // namespace pocket
