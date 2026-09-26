// PocketHarness tests - `pocket kit` pure helpers.
#include "mini.h"

#include <cmath>
#include <algorithm>

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

namespace {
struct KitResult { int code = -1; std::string out, err; };
KitResult kitCall(std::vector<std::string> args) {
    // Exercise the actual command dispatcher while keeping expected diagnostics
    // out of the test runner's output. Tests run sequentially, with no live worker.
    KitResult result;
    FILE* out = tmpfile();
    FILE* err = tmpfile();
    if (!out || !err) { if (out) fclose(out); if (err) fclose(err); return result; }
    fflush(nullptr);
    int savedOut = dup(STDOUT_FILENO), savedErr = dup(STDERR_FILENO);
    if (savedOut < 0 || savedErr < 0) {
        if (savedOut >= 0) close(savedOut);
        if (savedErr >= 0) close(savedErr);
        fclose(out); fclose(err);
        return result;
    }
    dup2(fileno(out), STDOUT_FILENO); dup2(fileno(err), STDERR_FILENO);
    args.insert(args.begin(), "kit");
    std::vector<char*> argv;
    for (auto& arg : args) argv.push_back(arg.data());
    result.code = kitMain((int)argv.size(), argv.data());
    fflush(nullptr);
    dup2(savedOut, STDOUT_FILENO); dup2(savedErr, STDERR_FILENO);
    close(savedOut); close(savedErr);
    auto read = [](FILE* file) {
        rewind(file);
        std::string text;
        char bytes[4096];
        while (size_t n = fread(bytes, 1, sizeof bytes, file)) text.append(bytes, n);
        fclose(file);
        return text;
    };
    result.out = read(out); result.err = read(err);
    return result;
}
struct KitScratch {
    std::string path = makeTempDir("pocket-kit");
    ~KitScratch() { if (!path.empty()) rmRf(path); }
};
std::string pngHeader(unsigned width, unsigned height) {
    std::string data("\x89PNG\r\n\x1a\n\0\0\0\x0dIHDR", 16);
    for (unsigned dimension : {width, height})
        for (int i = 3; i >= 0; --i) data += (char)(dimension >> (i * 8));
    data.append(9, '\0');
    return data;
}
std::vector<double> easingSamples(const std::string& css) {
    std::vector<double> values;
    if (!startsWith(css, "linear(")) return values;
    const char* p = css.c_str() + 7;
    while (*p) {
        char* end = nullptr;
        values.push_back(strtod(p, &end));
        if (end == p || *end == ')') break;
        p = end + 1;
    }
    return values;
}
}  // namespace

TEST(kit_Spring_Analytic_Response_And_Bounds) {
    double ms = 0;
    auto critical = easingSamples(springEasing(100, 20, 1, &ms));
    CHECK(critical.size() == 41 && ms > 900 && ms < 1000);
    for (size_t i = 0; i + 1 < critical.size(); ++i) {
        double t = ms / 1000 * i / (critical.size() - 1);
        CHECK(std::fabs(critical[i] - (1 - (1 + 10 * t) * std::exp(-10 * t))) < 0.000002);
    }
    auto stiff = springEasing(1e9, 100000, 1, &ms);
    CHECK(!stiff.empty() && ms > 0 && ms < 10);
    CHECK(stiff.find("nan") == std::string::npos && stiff.find("inf") == std::string::npos);
    auto bouncy = easingSamples(springEasing(180, 12, 1, &ms));
    CHECK(std::any_of(bouncy.begin(), bouncy.end(), [](double x) { return x > 1.05; }));
    for (auto params : {std::vector<double>{170, 0, 1}, {170, -1, 1}, {1e9, 1, 1}, {1, 1e9, 1}}) {
        CHECK(springEasing(params[0], params[1], params[2], &ms).empty());
        CHECK(ms == 0);
    }
    return "";
}

TEST(kit_Spring_Command_Rejects_Invalid_Parameters) {
    for (const char* value : {"nan", "inf", "1e999", "1e-999", "170garbage", "", "0", "-2"}) {
        auto result = kitCall({"spring", value});
        CHECK(result.code != 0 && result.out.empty() && !result.err.empty());
    }
    CHECK(kitCall({"spring", "170", "0"}).code != 0);
    CHECK(kitCall({"spring", "170", "26", "1", "extra"}).code != 0);
    CHECK(kitCall({"spring"}).code == 0);
    return "";
}

TEST(kit_Slop_Read_Failures_Do_Not_Report_Clean) {
    KitScratch scratch;
    CHECK(!scratch.path.empty());
    CHECK(atomicWriteFile(scratch.path + "/clean.c", "int main(void) { return 0; }\n").ok);
    auto result = kitCall({"slop", scratch.path + "/clean.c", scratch.path + "/missing.c"});
    CHECK(result.code != 0 && result.out.find("clean:") == std::string::npos);
    CHECK(result.err.find("missing.c") != std::string::npos);
    CHECK(kitCall({"slop", scratch.path + "/clean.c"}).code == 0);
    return "";
}

TEST(kit_Image_Header_Errors_And_Valid_Types) {
    KitScratch scratch;
    CHECK(!scratch.path.empty());
    const std::string file = scratch.path + "/image";
    for (const auto& content : {std::string("plain text"), std::string("<svgnot-an-image>"),
                               pngHeader(0, 10), pngHeader(10, 10).substr(0, 24),
                               std::string("\xff\xd8\xff\xe0\0\x01\xff\xc0", 8)}) {
        CHECK(atomicWriteFile(file, content).ok);
        CHECK(kitCall({"img", file}).code != 0);
    }
    CHECK(atomicWriteFile(file, pngHeader(12, 34)).ok);
    auto png = kitCall({"img", file});
    CHECK(png.code == 0 && png.out.find("png 12x34") != std::string::npos);
    auto mixed = kitCall({"img", file, scratch.path + "/missing.png"});
    CHECK(mixed.code != 0 && mixed.out.find("png 12x34") != std::string::npos);
    // A minimal lossless WebP header was incorrectly rejected by the old n>30 check.
    std::string webp("RIFF\x12\0\0\0WEBPVP8L\x05\0\0\0\x2f\0\0\0\0\0", 26);
    CHECK(atomicWriteFile(file, webp).ok);
    auto lossless = kitCall({"img", file});
    CHECK(lossless.code == 0 && lossless.out.find("webp 1x1") != std::string::npos);
    // Fill bytes and a standalone TEM marker before the SOF are legal JPEG syntax.
    const unsigned char jpeg[] = {0xff,0xd8,0xff,0xff,0x01,0xff,0xc0,0,11,8,0,12,0,34,1,1,0x11,0};
    CHECK(atomicWriteFile(file, std::string((const char*)jpeg, sizeof jpeg)).ok);
    auto jpg = kitCall({"img", file});
    CHECK(jpg.code == 0 && jpg.out.find("jpeg 34x12") != std::string::npos);
    return "";
}

TEST(kit_Screenshot_Requires_Fresh_Output_And_Private_Profile) {
    KitScratch scratch;
    CHECK(!scratch.path.empty());
    EnvGuard tmp("TMPDIR", scratch.path), path("PATH", scratch.path);
    const std::string browser = scratch.path + "/google-chrome", output = scratch.path + "/out.png";
    const std::string log = scratch.path + "/profiles";
    EnvGuard captureLog("POCKET_KIT_CAPTURE_LOG", log), sourcePng("POCKET_KIT_PNG", scratch.path + "/input.png");
    CHECK(atomicWriteFile(scratch.path + "/input.png", pngHeader(320, 240)).ok);
    const std::string prefix = "#!/bin/sh\nfor arg do\n case \"$arg\" in\n --user-data-dir=*) /bin/mkdir -p \"${arg#*=}\"; printf '%s\\n' \"${arg#*=}\" >> \"$POCKET_KIT_CAPTURE_LOG\";;\n --screenshot=*) output=${arg#*=};;\n esac\ndone\n";
    CHECK(atomicWriteFile(browser, prefix + "exit 0\n", 0700).ok);
    CHECK(atomicWriteFile(output, "old screenshot").ok);
    auto missing = kitCall({"shot", "about:blank", output, "320x240"});
    CHECK(missing.code != 0 && missing.out.find("screenshot:") == std::string::npos);
    CHECK(readFileBounded(output, 100).value == "old screenshot");
    CHECK(atomicWriteFile(browser, prefix + "/bin/cp \"$POCKET_KIT_PNG\" \"$output\"\n", 0700).ok);
    CHECK(kitCall({"shot", "about:blank", scratch.path + "/missing/out.png", "320x240"}).code != 0);
    auto valid = kitCall({"shot", "about:blank", output, "320x240"});
    CHECK(valid.code == 0 && readFileBounded(output, 100).value == pngHeader(320, 240));
    auto profiles = readFileBounded(log, 4096);
    CHECK(profiles.ok);
    auto lines = splitLines(trim(profiles.value));
    CHECK(lines.size() == 3 && lines[0] != lines[1] && lines[1] != lines[2]);
    for (const auto& profile : lines) CHECK(access(profile.c_str(), F_OK) != 0);
    CHECK(atomicWriteFile(browser, prefix + "exit 7\n", 0700).ok);
    CHECK(kitCall({"shot", "about:blank", output, "320x240"}).code != 0);
    return "";
}

TEST(kit_Browser_Arguments_Are_Bounded) {
    KitScratch scratch;
    CHECK(!scratch.path.empty());
    EnvGuard tmp("TMPDIR", scratch.path), path("PATH", scratch.path);
    CHECK(atomicWriteFile(scratch.path + "/google-chrome", "#!/bin/sh\nexit 0\n", 0700).ok);
    const std::string output = scratch.path + "/unused.png";
    for (const char* size : {"1,2", "0x1", "1x0", "8193x1", "8192x8192", "1x1x1", "999999999999999999999x1"})
        CHECK(kitCall({"shot", "about:blank", output, size}).code != 0);
    CHECK(kitCall({"shot", "--disable-web-security", output}).code != 0);
    CHECK(kitCall({"shot", "about:blank", output, "1x1", "extra"}).code != 0);
    CHECK(kitCall({"dom", "about:blank", "extra"}).code != 0);
    return "";
}
