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
