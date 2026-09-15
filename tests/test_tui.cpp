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
