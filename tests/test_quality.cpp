#include "mini.h"
#include "../src/kit_lint.h"
#include "../src/kit.h"

using namespace pocket;
using namespace pocket::test;

TEST(quality_Preserves_Intentional_Fonts_And_Palettes) {
    std::string brand = "body { font-family: Inter,sans-serif; color:#111;background:#fff; }\n"
        ".brand {background:linear-gradient(90deg,#6366f1,#ec4899)}\n";
    CHECK(lintScan("brand.css", brand).empty());
    CHECK(lintScan("brand.css", ".surface{background:#faf3e8}.accent{background:#b65334}.type{font-family:'Instrument Serif'}").empty());
    auto generic = lintScan("landing.html", brand + "<h1>Build faster. Ship smarter.</h1>");
    bool flag = false;
    for (const auto& f : generic) flag |= f.find("indigo/purple gradient") != std::string::npos;
    CHECK(flag);
    return "";
}

TEST(quality_Questions_Follow_Artifact_And_Risk) {
    CHECK(qualityQuestions("sum.py", "def add(a, b): return a + b").empty());
    CHECK(qualityQuestions("vendor/page.html", "<h1>Supercharge</h1>").empty());
    auto prose = qualityQuestions("guide.md", "The request and measured result.");
    CHECK_EQ(prose.size(), size_t(2));
    CHECK_EQ(prose[0].id, std::string("unsupported"));
    auto code = qualityQuestions("loader.py", "import pickle\nx = pickle.load(source)");
    CHECK_EQ(code.size(), size_t(1));
    CHECK_EQ(code.front().id, std::string("defect"));
    CHECK(qualityAdvice({{"defect", .5}, {"template", -1}}).empty());
    CHECK(qualityAdvice({{"defect", .95}}).find("Advisory signal") != std::string::npos);
    return "";
}

TEST(quality_Review_Excerpt_Contains_Late_Edit_And_Intent) {
    std::string before(30000, 'a');
    before += "old behavior";
    std::string content(30000, 'a');
    content += "new behavior";
    auto state = qualityState("logic.py", content, before, "fix the saved state");
    CHECK(state.at("content").asStr().find("new behavior") != std::string::npos);
    CHECK(state.at("before").asStr().find("old behavior") != std::string::npos);
    CHECK(state.at("content").asStr().size() <= 18000);
    CHECK(state.at("content_offset_bytes").asInt() > 0);
    CHECK(state.at("excerpt_only").asBool());
    CHECK_EQ(state.at("request").asStr(), std::string("fix the saved state"));
    return "";
}

TEST(quality_Html_Attributes_Are_Case_And_Whitespace_Tolerant) {
    auto attrs = htmlAttrs("<IMG ALT = '' DATA-alt = 'wrong' src = \"a.png?x=1&amp;y=2\" disabled>");
    CHECK(attrs.count("alt") && attrs.at("alt").empty());
    CHECK(attrs.count("disabled"));
    CHECK_EQ(attrs.at("src"), std::string("a.png?x=1&y=2"));
    CHECK_EQ(htmlAttr("<html LANG = 'en' lang='tr'>", "LANG"), std::string("en"));
    CHECK_EQ(htmlAttr(" width = \"12\" viewBox = \"0 0 1 1\"", "viewbox"), std::string("0 0 1 1"));
    return "";
}

TEST(quality_Detected_Credentials_Stay_Out_Of_Judge_Content) {
    const std::string credential = "# example configuration\npassword = 'hunter2hunter2'\n";
    CHECK(qualityQuestions("settings.py", credential).empty());
    auto state = qualityState("settings.py", "password = os.getenv('PASSWORD')\n", credential,
                              "Use password = 'hunter2hunter2'");
    CHECK(json::stringify(state).find("hunter2hunter2") == std::string::npos);
    CHECK(state.at("before").asStr().find("withheld") != std::string::npos);
    return "";
}
