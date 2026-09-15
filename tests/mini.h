// PocketHarness tests - tiny framework (no gtest dependency, obviously).
#pragma once

#include <ftw.h>
#include <stdlib.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace pocket {
namespace test {

struct Case {
    std::string name;
    std::function<std::string()> fn;  // returns "" on pass, else failure text
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}

inline int failures = 0;
inline int passes = 0;

#define TEST(name)                                                                    \
    static std::string test_##name();                                                 \
    static bool reg_##name = [] {                                                     \
        pocket::test::registry().push_back({#name, test_##name});                     \
        return true;                                                                  \
    }();                                                                              \
    static std::string test_##name()

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            return std::string(__FILE__) + ":" + std::to_string(__LINE__) +   \
                   ": CHECK failed: " #cond;                                 \
        }                                                                    \
    } while (0)

#define CHECK_EQ(a, b)                                                                \
    do {                                                                              \
        auto va = (a);                                                                \
        auto vb = (b);                                                                \
        if (!(va == vb)) {                                                            \
            return std::string(__FILE__) + ":" + std::to_string(__LINE__) +            \
                   ": CHECK_EQ failed: " #a " != " #b;                                 \
        }                                                                             \
    } while (0)

// Scratch dir under /tmp (removed by rmRf when the test is done).
inline std::string makeTempDir(const std::string& prefix) {
    std::string tpl = "/tmp/" + prefix + "-XXXXXX";
    std::vector<char> buf(tpl.begin(), tpl.end());
    buf.push_back('\0');
    if (!mkdtemp(buf.data())) return "";
    return buf.data();
}

inline int rmRfCb(const char* p, const struct stat*, int, struct FTW*) {
    return remove(p) == 0 || errno == ENOENT ? 0 : -1;
}
inline void rmRf(const std::string& p) { nftw(p.c_str(), rmRfCb, 16, FTW_DEPTH | FTW_PHYS); }

// Redirect $HOME (and friends) for config/session/skill tests.
struct HomeGuard {
    std::string oldHome, oldXdg;
    bool hadXdg = false;
    explicit HomeGuard(const std::string& tmp) {
        if (const char* h = getenv("HOME")) oldHome = h;
        if (const char* x = getenv("XDG_CONFIG_HOME")) {
            oldXdg = x;
            hadXdg = true;
        }
        setenv("HOME", tmp.c_str(), 1);
    }
    ~HomeGuard() {
        setenv("HOME", oldHome.c_str(), 1);
        if (hadXdg) setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    }
};

struct EnvGuard {
    std::string name, old;
    bool had = false;
    EnvGuard(const std::string& n, const std::string& v) : name(n) {
        if (const char* e = getenv(n.c_str())) {
            old = e;
            had = true;
        }
        setenv(n.c_str(), v.c_str(), 1);
    }
    ~EnvGuard() {
        if (had) setenv(name.c_str(), old.c_str(), 1);
        else unsetenv(name.c_str());
    }
};

}  // namespace test
}  // namespace pocket
