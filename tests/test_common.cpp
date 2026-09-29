// PocketHarness tests - shared helpers: base64.
#include "mini.h"

#include "../src/common.h"

using namespace pocket;
using namespace pocket::test;

TEST(common_Base64_Vectors) {
    CHECK_EQ(base64Encode(""), std::string(""));
    CHECK_EQ(base64Encode("f"), std::string("Zg=="));
    CHECK_EQ(base64Encode("fo"), std::string("Zm8="));
    CHECK_EQ(base64Encode("foo"), std::string("Zm9v"));
    CHECK_EQ(base64Encode("foob"), std::string("Zm9vYg=="));
    CHECK_EQ(base64Encode("fooba"), std::string("Zm9vYmE="));
    CHECK_EQ(base64Encode("foobar"), std::string("Zm9vYmFy"));
    // Binary incl. NUL and 0xFF round-trips through the alphabet only.
    std::string bin("\x00\xff\x10\x80", 4);
    std::string enc = base64Encode(bin);
    CHECK_EQ(enc.size(), (size_t)8);
    for (char c : enc)
        CHECK((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '+' || c == '/' || c == '=');
    return "";
}

TEST(common_Terminal_CSI_Punctuation_Does_Not_Eat_Text) {
    CHECK_EQ(pocket::sanitizeTerminal("a\033[2~hello\033[?25lb"), std::string("ahellob"));
    CHECK_EQ(pocket::sanitizeTerminal("a\0337b"), std::string("ab"));
    return "";
}

TEST(common_EnsureDir_Separates_Every_Relative_Component) {
    std::string ws = makeTempDir("pocket-ed");
    CHECK(!ws.empty());
    // A one-character prefix is still a component: "a/b" must not mkdir "ab".
    CHECK(ensureDir(ws + "/a/b/c", 0755).ok);
    CHECK(access((ws + "/a/b/c").c_str(), F_OK) == 0);
    CHECK(access((ws + "/a/bc").c_str(), F_OK) != 0);
    CHECK(access((ws + "/ab").c_str(), F_OK) != 0);
    // Repeated and absolute separators stay well-formed.
    CHECK(ensureDir(ws + "/a//b//c", 0755).ok);
    CHECK(ensureDir(ws + "/a/b/./c", 0755).ok);
    // A relative output path is what "pocket kit mix a/b/out.wav" resolves to.
    CHECK(ensureDir(ws + "/out/dir", 0755).ok);
    CHECK(writeOutputFile(ws + "/out/dir/f.txt", "x").ok);
    CHECK_EQ(readFileBounded(ws + "/out/dir/f.txt", 10).value, std::string("x"));
    rmRf(ws);
    return "";
}
