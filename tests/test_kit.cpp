// PocketHarness tests - `pocket kit` pure helpers.
#include "mini.h"

#include <cmath>

#include "../src/common.h"
#include "../src/kit.h"

using namespace pocket;
using namespace pocket::test;

TEST(kit_Html_To_Text_Keeps_Structure_Drops_Noise) {
    std::vector<std::string> links;
    std::string t = htmlToText(
        "<html><head><title>T</title><style>x{}</style></head><body><script>evil()</script>"
        "<h2>Intro</h2><p>Hello&nbsp;<b>world</b> &amp; you</p><ul><li>one</li><li>two</li></ul>"
        "<a href=\"/doc\">docs</a><!-- c --></body></html>", &links);
    CHECK(t.find("evil") == std::string::npos && t.find("x{}") == std::string::npos);
    CHECK(t.find("# T") != std::string::npos && t.find("## Intro") != std::string::npos);
    CHECK(t.find("Hello world & you") != std::string::npos);
    CHECK(t.find("- one") != std::string::npos && t.find("docs [1]") != std::string::npos);
    CHECK(links.size() == 1 && links[0] == "/doc");
    CHECK(urlDecode("a%20b+c%2F") == "a b c/");
    return "";
}

TEST(kit_Note_Frequencies) {
    CHECK(std::fabs(noteFreq("A4") - 440) < 1e-6);
    CHECK(std::fabs(noteFreq("C4") - 261.6256) < 1e-3);
    CHECK(std::fabs(noteFreq("A#4") - noteFreq("Bb4")) < 1e-9);
    CHECK(noteFreq("H2") < 0 && noteFreq("A") < 0 && noteFreq("300") == 300);
    return "";
}

TEST(kit_Spring_Easing_Settles_At_One) {
    double ms = 0;
    std::string e = springEasing(170, 26, 1, &ms);
    CHECK(startsWith(e, "linear(0") && endsWith(e, ", 1)"));
    CHECK(ms > 100 && ms < 3000);
    return "";
}

TEST(kit_Slop_Scan_Finds_Placeholders_And_Tells) {
    auto code = slopScan("a.c", "int f() {\n    // ... existing code ...\n}\n<<<<<<< HEAD\n");
    CHECK(code.size() == 2);
    auto prose = slopScan("README.md", "Let us delve into this tapestry.\n");
    CHECK(prose.size() == 1);
    CHECK(slopScan("a.json", "{bad").size() == 1);
    CHECK(slopScan("clean.c", "int main(void) { return 0; }\n").empty());
    return "";
}

TEST(kit_Host_Probe_Reports_Basics) {
    std::string p = hostProbe("/");
    CHECK(p.find("cpu:") != std::string::npos && p.find("memory:") != std::string::npos);
    return "";
}
