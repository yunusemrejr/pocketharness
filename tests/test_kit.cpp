// PocketHarness tests - `pocket kit` pure helpers.
#include "mini.h"

#include <cmath>
#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "../src/common.h"
#include "../src/kit.h"
#include "../src/kit_lint.h"
#include "../src/kit_ops.h"

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
struct LoopbackListener {
    int fd = -1, client = -1;
    unsigned port = 0;
    explicit LoopbackListener(int backlog = 4, int family = AF_INET) {
        fd = socket(family, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) return;
        sockaddr_storage address{};
        socklen_t size = 0;
        if (family == AF_INET) {
            auto* ip = reinterpret_cast<sockaddr_in*>(&address);
            ip->sin_family = AF_INET;
            ip->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            size = sizeof(*ip);
        } else {
            auto* ip = reinterpret_cast<sockaddr_in6*>(&address);
            ip->sin6_family = AF_INET6;
            ip->sin6_addr = in6addr_loopback;
            size = sizeof(*ip);
        }
        if (bind(fd, reinterpret_cast<sockaddr*>(&address), size) != 0 || listen(fd, backlog) != 0 ||
            getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) != 0) return;
        port = ntohs(family == AF_INET ? reinterpret_cast<sockaddr_in*>(&address)->sin_port :
                                      reinterpret_cast<sockaddr_in6*>(&address)->sin6_port);
    }
    bool fill() {
        client = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (client < 0) return false;
        timeval timeout{1, 0};
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons((uint16_t)port);
        return connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
    }
    ~LoopbackListener() { if (client >= 0) close(client); if (fd >= 0) close(fd); }
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

TEST(kit_Csv_Seo_And_Bench_Commands) {
    std::string dir = "/tmp/pocket-kit-ops-" + std::to_string(getpid());
    CHECK(ensureDir(dir, 0700).ok);
    CHECK(atomicWriteFile(dir + "/d.csv", "id;score;label\n1;0.5;a\n2;;b\n3;1.5;a\n4;2;a\n").ok);
    auto csv = kitCall({"csv", dir + "/d.csv"});
    CHECK(csv.code == 0 && csv.out.find("4 rows × 3 columns (sep ';')") != std::string::npos);
    CHECK(csv.out.find("score: float · missing 1") != std::string::npos && csv.out.find("a=3 (75%)") != std::string::npos);
    CHECK(atomicWriteFile(dir + "/p.html", "<html><head><title>T</title></head><body><h2>x</h2><img src=a></body></html>").ok);
    auto seo = kitCall({"seo", dir + "/p.html"});
    CHECK(seo.code == 1 && seo.out.find("no lang") != std::string::npos && seo.out.find("0 <h1>") != std::string::npos &&
          seo.out.find("without alt") != std::string::npos);
    auto bench = kitCall({"bench", "-n", "2", "-w", "0", "true"});
    CHECK(bench.code == 0 && bench.out.find("2 run(s) after 0 warmup") != std::string::npos);
    CHECK(kitCall({"bench", "-n", "1", "exit 2"}).code == 1);
    CHECK(kitCall({"wait", "1", "0.3"}).code == 1);
    rmRf(dir);
    return "";
}

TEST(kit_Wait_Bounds_TCP_And_Rejects_Invalid_Targets) {
    LoopbackListener ready;
    CHECK(ready.port != 0);
    const std::string target = "127.0.0.1:" + std::to_string(ready.port);
    auto success = kitCall({"wait", target, ".1"});
    CHECK(success.code == 0 && success.out.find("accepts connections") != std::string::npos);
    auto reach = kitCall({"reach", target, ".1"});
    CHECK(reach.code == 0 && reach.out.find("resolver:") != std::string::npos && reach.out.find("TCP: connected") != std::string::npos);
    LoopbackListener blocked(0);
    CHECK(blocked.port != 0 && blocked.fill());
    const int64_t start = nowMs();
    auto timeout = kitCall({"wait", "127.0.0.1:" + std::to_string(blocked.port), ".1"});
    CHECK(timeout.code == 1 && timeout.err.find("timed out") != std::string::npos);
    CHECK(nowMs() - start < 1000);  // blocking connect previously exceeded 2s
    for (const auto& args : std::vector<std::vector<std::string>>{
             {"wait", "0", ".1"}, {"wait", "65536", ".1"}, {"wait", "1", "0.1junk"},
             {"wait", "1", "nan"}, {"wait", "1", "inf"}, {"wait", "1", "0"},
             {"wait", "bad:port", ".1"}, {"wait", "[::1]", ".1"}, {"wait", "::1:123", ".1"},
             {"wait", "1", ".1", "--any-status"}, {"reach", "bad host:80", ".1"}})
        CHECK(kitCall(args).code != 0);
    // Optional IPv6 hosts can be absent on a configured CI kernel.
    LoopbackListener ipv6(4, AF_INET6);
    if (ipv6.port) CHECK(kitCall({"wait", "[::1]:" + std::to_string(ipv6.port), ".1"}).code == 0);
    return "";
}

TEST(kit_Resolver_And_HTTP_Readiness_Honor_Deadline_And_Status) {
    KitScratch scratch;
    CHECK(!scratch.path.empty());
    EnvGuard path("PATH", scratch.path);
    CHECK(atomicWriteFile(scratch.path + "/getent", "#!/bin/sh\n/bin/sleep 2\n", 0700).ok);
    const int64_t start = nowMs();
    auto resolver = kitCall({"reach", "fixture.example:80", ".1"});
    CHECK(resolver.code != 0 && resolver.err.find("resolver deadline") != std::string::npos);
    CHECK(nowMs() - start < 1000);
    const std::string curlPath = scratch.path + "/curl";
    CHECK(atomicWriteFile(curlPath, "#!/bin/sh\nprintf 503\n", 0700).ok);
    auto unavailable = kitCall({"wait", "http://fixture.example/health", ".05"});
    CHECK(unavailable.code != 0 && unavailable.err.find("503") != std::string::npos);
    CHECK(kitCall({"wait", "http://fixture.example/health", ".1", "--any-status"}).code == 0);
    CHECK(atomicWriteFile(curlPath, "#!/bin/sh\nprintf 200\nexit 7\n", 0700).ok);
    CHECK(kitCall({"wait", "http://fixture.example/health", ".05"}).code != 0);
    CHECK(atomicWriteFile(curlPath, "#!/bin/sh\nprintf 204\n", 0700).ok);
    CHECK(kitCall({"wait", "http://fixture.example/health", ".1"}).code == 0);
    CHECK(atomicWriteFile(curlPath, "#!/bin/sh\n/bin/sleep 2\n", 0700).ok);
    const int64_t beforeHttp = nowMs();
    CHECK(kitCall({"wait", "http://fixture.example/health", ".1"}).code != 0);
    CHECK(nowMs() - beforeHttp < 1000);
    return "";
}

TEST(kit_Sys_Reports_Actual_Process_Without_Environment) {
    EnvGuard secret("POCKET_TEST_PRIVATE_ENV", "must-not-print-private-env");
    auto system = kitCall({"sys"});
    CHECK(system.code == 0 && system.out.find("MemAvailable:") != std::string::npos && system.out.find("pressure/cpu:") != std::string::npos);
    auto process = kitCall({"sys", std::to_string(getpid())});
    CHECK(process.code == 0 && process.out.find("State:") != std::string::npos && process.out.find("Threads:") != std::string::npos);
    CHECK(process.out.find("must-not-print-private-env") == std::string::npos);
    for (const char* pid : {"0", "-1", "2147483648", "1oops", "../self", ""}) CHECK(kitCall({"sys", pid}).code != 0);
    CHECK(kitCall({"sys", "2147483647"}).code != 0);
    return "";
}

TEST(kit_Seo_Ignores_Comments_Scripts_And_Arbitrary_Length_Targets) {
    KitScratch scratch;
    CHECK(!scratch.path.empty());
    const std::string file = scratch.path + "/page.html";
    const std::string page = "<!doctype html><HTML LANG = 'tr'><HEAD><TITLE>İş</TITLE><META CHARSET = 'utf-8'>"
                             "<META NAME = 'description' CONTENT = 'Kısa ve doğru.'>"
                             "<LINK REL = 'canonical' HREF = 'https://example.com/about'>"
                             "<!-- <meta name=robots content=noindex><h1>fake</h1><img src=fake> -->"
                             "<SCRIPT>const sample='<h1>sample</h1><img src=x>'; const text='application/ld+json';</SCRIPT>"
                             "<SCRIPT TYPE = 'application/ld+json'>{\"@context\":\"https://schema.org\",\"@type\":\"WebPage\"}</SCRIPT>"
                             "</HEAD><BODY><H1>About</H1><IMG ALT = '' TITLE = 'a > b' SRC = 'decorative.png'></BODY></HTML>";
    CHECK(atomicWriteFile(file, page).ok);
    auto seo = kitCall({"seo", file});
    CHECK(seo.code == 0 && seo.out.find("no definite source issues") != std::string::npos);
    CHECK(seo.out.find("1 images · 1 JSON-LD") != std::string::npos);
    CHECK(seo.out.find("title (2 characters)") != std::string::npos);
    for (const char* falsePositive : {"no lang", "without alt", "noindex", "thin content", "aim 15", "aim 50", "2 <h1>"})
        CHECK(seo.out.find(falsePositive) == std::string::npos);
    CHECK(atomicWriteFile(file, "<html lang=en><title>About</title><h1>About</h1><meta name=robots content='NOINDEX, FOLLOW'>").ok);
    auto blocked = kitCall({"seo", file});
    CHECK(blocked.code == 1 && blocked.out.find("blocks indexing") != std::string::npos);
    CHECK(atomicWriteFile(file, "<html lang=en><title>About</title><h1>About</h1><link rel=canonical href=''><script type='application/ld+json'>{broken}</script>").ok);
    auto malformed = kitCall({"seo", file});
    CHECK(malformed.code == 1 && malformed.out.find("no usable href") != std::string::npos && malformed.out.find("not valid JSON-LD") != std::string::npos);
    return "";
}

TEST(kit_Net_And_Seo_Reject_HTTP_Failure_And_Redact_Cookies) {
    KitScratch scratch;
    CHECK(!scratch.path.empty());
    EnvGuard path("PATH", scratch.path);
    const std::string curlPath = scratch.path + "/curl";
    CHECK(atomicWriteFile(curlPath, "#!/bin/sh\nprintf '<html lang=en><title>Error</title>\\n@@404'\n", 0700).ok);
    auto errorPage = kitCall({"seo", "http://fixture.example/missing"});
    CHECK(errorPage.code == 1 && errorPage.err.find("HTTP 404") != std::string::npos);
    CHECK(atomicWriteFile(curlPath, "#!/bin/sh\nprintf 'HTTP/1.1 200 OK\\r\\nSet-Cookie: fixture=must-not-print-cookie\\r\\nCache-Control: no-cache\\r\\n\\r\\n\\n@@200 1.1 127.0.0.1 0 0 0.01 0.02 0 0.03 0.04 0 http://fixture.example/'\n", 0700).ok);
    auto net = kitCall({"net", "http://fixture.example/"});
    CHECK(net.code == 0 && net.out.find("Set-Cookie: [redacted]") != std::string::npos);
    CHECK(net.out.find("must-not-print-cookie") == std::string::npos);
    CHECK(atomicWriteFile(curlPath, "#!/bin/sh\nprintf '\\n@@200 1.1 127.0.0.1 0 0 0.01 0.02 0 0.03 0.04 0 http://fixture.example/'\nexit 7\n", 0700).ok);
    CHECK(kitCall({"net", "http://fixture.example/"}).code != 0);
    return "";
}

namespace {
bool lintHas(const std::vector<std::string>& v, const std::string& needle) {
    for (const auto& x : v)
        if (x.find(needle) != std::string::npos) return true;
    return false;
}
}  // namespace

TEST(lint_Security_And_Backend_Rules) {
    auto py = lintScan("a.py", "import requests\ndef f(x, acc=[]):\n    requests.get(x)\n    cur.execute(\"select * from t where a=%s\" % x)\n"
                                "    subprocess.run(x, shell=True)\n    try:\n        pass\n    except:\n        pass\n", 'L');
    CHECK(lintHas(py, "mutable default"));
    CHECK(lintHas(py, "without timeout"));
    CHECK(lintHas(py, "SQL built"));
    CHECK(lintHas(py, "shell=True"));
    CHECK(lintHas(py, "bare except"));
    auto c = lintScan("a.c", "int m(){ gets(b);\n fgets(b,8,stdin);\n strcpy(a,b); }\n");
    CHECK(lintHas(c, "gets cannot bound") && lintHas(c, "unbounded copy"));
    CHECK(!lintHas(c, "L2"));
    auto js = lintScan("a.js", "for (const id of ids) {\n  const r = await db.query(sql, [id]);\n}\neval(x);\n");
    CHECK(lintHas(js, "inside a loop") && lintHas(js, "dynamic code"));
    return "";
}

TEST(lint_Secrets_Flag_Literals_But_Not_Config_Reads) {
    CHECK(lintHas(lintScan("a.js", "const password = \"hunter2hunter2\";\n"), "hard-coded secret"));
    CHECK(lintHas(lintScan("a.js", "k = 'AKIAABCDEFGHIJKLMNOP'\n"), "credential-shaped"));
    CHECK(lintScan("a.js", "const password = process.env.PASSWORD;\nif (token == \"abcdefgh12\") {}\nconst maxTokens = 1;\n").empty());
    CHECK(lintScan("tests/test_a.py", "password = 'hunter2hunter2'\n").empty());  // fixtures are exempt
    return "";
}

TEST(lint_Ui_Slop_A11y_And_Contrast) {
    std::string page = "<html><head><style>\nbody { font-family: 'Inter', sans-serif; }\n"
                       ".b { background: linear-gradient(90deg,#6366f1,#ec4899); color:#ccc; background-color:#fff; }\n"
                       "</style></head><body><img src=a.png><h1>Build faster. Ship smarter.</h1></body></html>\n";
    auto f = lintScan("p.html", page);
    for (const char* n : {"indigo/purple gradient", "contrast below", "without alt", "no lang", "no viewport", "names no capability"})
        CHECK(lintHas(f, n));
    std::string good = "<!doctype html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width\"></head>"
                       "<body><style>p{color:#111;background:#fff;font-family:'Fraunces',serif}</style><img src=a.png alt=\"a chart\"></body></html>\n";
    CHECK(lintScan("g.html", good).empty());
    return "";
}

TEST(lint_Svg_Hygiene) {
    auto f = lintScan("a.svg", "<svg viewBox=\"0 0 1 1\"><script>x()</script><text>t</text></svg>\n");
    CHECK(lintHas(f, "<script>") && lintHas(f, "accessible name") && lintHas(f, "<text>"));
    CHECK(lintScan("ok.svg", "<svg viewBox=\"0 0 8 8\" role=\"img\"><title>Dot</title><circle cx=\"4\" cy=\"4\" r=\"3\"/></svg>\n").empty());
    return "";
}

TEST(lint_Dry_Flags_Repeated_Blocks_And_Skips_Vendored) {
    std::string block;
    for (int i = 0; i < 6; ++i) block += "    result.push_back(computeSomethingLong(argumentNumber" + std::to_string(i) + ", other));\n";
    auto f = lintScan("a.cpp", "void a() {\n" + block + "}\nvoid b() {\n" + block + "}\n");
    CHECK(lintHas(f, "repeated within this file"));
    CHECK(lintScan("node_modules/x/a.js", "eval(x);\n").empty());
    CHECK(lintScan("notes.txt", "eval(x);\n").empty());
    return "";
}
