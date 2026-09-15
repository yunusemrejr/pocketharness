// PocketHarness tests - terminal line rendering + escape safety.
#include "mini.h"

#include "../src/tui.h"

using namespace pocket;
using namespace pocket::test;

TEST(tui_RenderLine) {
    bool fence = false;
    CHECK(renderLine("# Title", fence).find("Title") != std::string::npos);
    CHECK(!fence);
    CHECK(renderLine("- item", fence).find("item") != std::string::npos);
    renderLine("```", fence);
    CHECK(fence);
    std::string code = renderLine("rm -rf x", fence);
    CHECK(code.find("rm -rf x") != std::string::npos);
    renderLine("```", fence);
    CHECK(!fence);
    CHECK(renderLine("use `code` here", fence).find("code") != std::string::npos);
    // Untrusted escape sequences are neutralized, never passed through.
    std::string evil = renderLine("\033]0;pwned\a\033[2Jtext", fence);
    CHECK(evil.find("\033") == std::string::npos && evil.find("text") != std::string::npos);
    std::string c0 = renderLine("a\x01\x07" "b", fence);
    CHECK(c0.find("ab") != std::string::npos);
    return "";
}

TEST(tui_VisibleWidth) {
    CHECK(visibleWidth("hello") == 5);
    CHECK(visibleWidth("") == 0);
    // ANSI color sequences are zero-width (prompt width math depends on this).
    CHECK(visibleWidth("\033[1mhi\033[0m") == 2);
    CHECK(visibleWidth("\033]0;title\aok") == 2);
    // Tabs advance to 8-column stops, like the terminal does.
    CHECK(visibleWidth("a\tb") == 9);
    CHECK(visibleWidth("\t") == 8);
    // C0 controls / DEL are dropped, multibyte chars count 1.
    CHECK(visibleWidth("a\x01\x07" "b") == 2);
    CHECK(visibleWidth("\xc3\xa9") == 1);
    return "";
}

TEST(tui_FmtK) {
    CHECK(fmtK(0) == "0");
    CHECK(fmtK(999) == "999");
    CHECK(fmtK(1000) == "1k");
    CHECK(fmtK(12400) == "12.4k");
    CHECK(fmtK(200000) == "200k");
    CHECK(fmtK(1500000) == "1.5M");
    CHECK(fmtK(-5) == "0");
    return "";
}
