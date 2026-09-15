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
