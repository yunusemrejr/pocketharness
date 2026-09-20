// PocketHarness tests - JSON.
#include "mini.h"

#include "../src/json.h"

using namespace pocket;
using namespace pocket::test;
using namespace pocket::json;

TEST(json_Scalars) {
    auto v = parse("null");
    CHECK(v.ok && v.value.isNull());
    v = parse("true");
    CHECK(v.ok && v.value.asBool(false));
    v = parse("false");
    CHECK(v.ok && !v.value.asBool(true));
    v = parse("42");
    CHECK(v.ok && v.value.asInt(-1) == 42);
    v = parse("-3.5e2");
    CHECK(v.ok && v.value.asNum(0) == -350.0);
    v = parse("\"hi\"");
    CHECK(v.ok && v.value.asStr() == "hi");
    return "";
}

TEST(json_Escapes_And_Unicode) {
    auto v = parse(R"("a\nb\t\"q\"\\\/\b\f\r")");
    CHECK(v.ok);
    CHECK_EQ(v.value.asStr(), std::string("a\nb\t\"q\"\\/\b\f\r"));
    v = parse(R"("\u00e9")");  // é
    CHECK(v.ok && v.value.asStr() == "\xc3\xa9");
    v = parse(R"("\ud83d\ude00")");  // 😀 surrogate pair
    CHECK(v.ok && v.value.asStr() == "\xf0\x9f\x98\x80");
    v = parse(R"("\ud800")");  // lone high surrogate
    CHECK(!v.ok);
    CHECK(!parse(R"("\udddd")").ok);  // lone low surrogate (e.g. surrogateescape)
    v = parse(R"("bad \x")");
    CHECK(!v.ok);
    return "";
}

TEST(json_Structures) {
    auto v = parse(R"({"a":[1,2,{"b":null}],"c":{}})");
    CHECK(v.ok);
    CHECK_EQ(v.value.at("a").at(2).at("b").isNull(), true);
    CHECK_EQ(v.value.at("a").size(), (size_t)3);
    CHECK(v.value.has("c") && !v.value.has("zzz"));
    CHECK(v.value.at("missing").isNull());
    CHECK(v.value.at("a").at(99).isNull());
    return "";
}

TEST(json_Malformed) {
    const char* bad[] = {"", "{", "[1,", "{\"a\":}", "[1 2]", "tru", "{,}", "{\"a\":1,}",
                         "00", "01", ".5", "\"unterminated", "[1,]", nullptr};
    for (const char** p = bad; *p; ++p) {
        auto v = parse(*p);
        if (v.ok) return std::string("should fail: ") + *p;
    }
    auto v = parse("{} garbage");
    CHECK(!v.ok);
    // Deep nesting is rejected, not a stack overflow.
    v = parse(std::string(500, '[') + std::string(500, ']'));
    CHECK(!v.ok);
    return "";
}

TEST(json_Serialize_Roundtrip) {
    const char* docs[] = {"null", "true", "42", "-1.5", "\"s\\ntring\"",
                          "[1,\"a\",null,true]", "{\"b\":2,\"a\":[{}]}", nullptr};
    for (const char** p = docs; *p; ++p) {
        auto v = parse(*p);
        CHECK(v.ok);
        auto v2 = parse(stringify(v.value));
        CHECK(v2.ok);
        CHECK_EQ(stringify(v2.value), stringify(v.value));
    }
    // Control chars escape; pretty mode indents deterministically (map order).
    Value o(Object{{"b", Value(1)}, {"a", Value("x\ty")}});
    std::string pretty = stringify(o, true);
    CHECK(pretty.find("\"a\": \"x\\ty\"") != std::string::npos);
    CHECK(pretty.find('\n') != std::string::npos);
    CHECK(parse(pretty).ok);
    return "";
}

TEST(json_Serialize_Only_Unicode_Scalars) {
    // Preserve valid UTF-8, including scalar boundaries, Turkish and emoji.
    std::string valid = "VERİLEN ÇEKLER 😀\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80"
                        "\xed\x9f\xbf\xee\x80\x80\xef\xbf\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf";
    CHECK_EQ(stringify(Value(valid)), "\"" + valid + "\"");
    // Invalid lead/continuation bytes, truncated sequences, overlong encodings,
    // encoded surrogates and values above U+10FFFF must never reach the wire.
    for (const std::string bad : {"\xdd", "\x80", "\xff", "\xc3", "\xe2\x82", "\xf0\x9f\x98",
                                  "\xc0\xaf", "\xe0\x80\x80", "\xf0\x80\x80\x80",
                                  "\xed\xa0\x80", "\xed\xb3\x9d", "\xf4\x90\x80\x80", "\xf8\x88\x80\x80\x80"}) {
        std::string escaped, decoded;
        for (size_t i = 0; i < bad.size(); ++i) { escaped += "\\ufffd"; decoded += "\xef\xbf\xbd"; }
        CHECK_EQ(stringify(Value(bad)), "\"" + escaped + "\"");
        CHECK_EQ(parse(stringify(Value(bad))).value.asStr(), decoded);
        CHECK_EQ(stringify(Value(Object{{bad, bad}})), "{\"" + escaped + "\":\"" + escaped + "\"}");
    }
    CHECK_EQ(stringify(Value("VER\xddLEN \xc3(")), std::string("\"VER\\ufffdLEN \\ufffd(\""));
    CHECK_EQ(stringify(Value(std::string("a\0b", 3))), std::string("\"a\\u0000b\""));
    return "";
}
