// PocketHarness tests - skills discovery/search/load.
#include "mini.h"

#include "../src/config.h"
#include "../src/skills.h"

#include <filesystem>

using namespace pocket;
using namespace pocket::test;

TEST(skills_Discover_Precedence) {
    std::string home = makeTempDir("pocket-skill");
    CHECK(!home.empty());
    HomeGuard hg(home);
    std::string ws = home + "/ws";
    CHECK(ensureDir(globalSkillDir() + "/alpha", 0755).ok);
    CHECK(atomicWriteFile(globalSkillDir() + "/alpha/SKILL.md",
                          "# Alpha Global\n\nDoes global things.\n", 0644)
              .ok);
    CHECK(ensureDir(globalSkillDir() + "/beta", 0755).ok);
    CHECK(atomicWriteFile(globalSkillDir() + "/beta/SKILL.md", "no heading here\n", 0644).ok);
    CHECK(ensureDir(projectSkillDir(ws) + "/alpha", 0755).ok);
    CHECK(atomicWriteFile(projectSkillDir(ws) + "/alpha/SKILL.md",
                          "# Alpha Project\n\nProject override.\n", 0644)
              .ok);
    auto all = skillDiscover(ws);
    CHECK_EQ(all.size(), (size_t)2);
    bool projAlpha = false, globalBeta = false;
    for (const auto& m : all) {
        if (m.name == "alpha" && m.source == "project") projAlpha = true;
        if (m.name == "beta" && m.source == "global") globalBeta = true;
    }
    CHECK(projAlpha && globalBeta);
    auto hits = skillSearch(all, "override");
    CHECK_EQ(hits.size(), (size_t)1);
    hits = skillSearch(all, "ALPHA");
    CHECK_EQ(hits.size(), (size_t)1);  // case-insensitive
    auto loaded = skillLoad(all, "alpha");
    CHECK(loaded.ok && loaded.value.find("Project override") != std::string::npos);
    auto missing = skillLoad(all, "zzz");
    CHECK(!missing.ok);
    rmRf(home);
    return "";
}

TEST(skills_Search_Ranks_Name_Heading_Preview) {
    SkillMeta webby{"webby", "global", "Do stuff", "fetch pages and search the web", ""};
    SkillMeta fetch{"fetch", "global", "Other thing", "unrelated words here", ""};
    SkillMeta heady{"heady", "global", "How to fetch things", "nothing relevant", ""};
    std::vector<SkillMeta> all{webby, fetch, heady};
    auto hits = skillSearch(all, "fetch");
    CHECK_EQ(hits.size(), (size_t)3);
    CHECK_EQ(hits[0].name, std::string("fetch"));  // name hit first
    CHECK_EQ(hits[1].name, std::string("heady"));  // then heading hit
    CHECK_EQ(hits[2].name, std::string("webby"));  // then preview hit
    // Multi-token query: tokens union across fields, totals rank, ties break alpha.
    hits = skillSearch(all, "search unrelated");
    CHECK_EQ(hits.size(), (size_t)2);
    CHECK_EQ(hits[0].name, std::string("fetch"));
    CHECK_EQ(hits[1].name, std::string("webby"));
    CHECK(skillSearch(all, "").size() == 3);  // empty query returns all
    CHECK(skillSearch(all, "zzz-no-match").empty());
    return "";
}

TEST(skills_Bundled_Local_Markdown_Links_Resolve) {
    // The installed skill tree keeps relative assets/references. A missing
    // target otherwise sends a model to a script or example it cannot use.
    namespace fs = std::filesystem;
    CHECK(fs::is_directory("skills"));
    for (const auto& entry : fs::recursive_directory_iterator("skills")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".md") continue;
        auto data = readFileBounded(entry.path().string(), 1 << 20);
        CHECK(data.ok);
        for (size_t at = 0; (at = data.value.find("](", at)) != std::string::npos;) {
            size_t end = data.value.find(')', at + 2);
            if (end == std::string::npos) break;
            std::string target = data.value.substr(at + 2, end - at - 2);
            at = end + 1;
            if (target.empty() || target[0] == '#' || target.find(":") != std::string::npos) continue;
            target = target.substr(0, target.find_first_of("# \t\n"));
            if (target.empty()) continue;
            if (!fs::exists(entry.path().parent_path() / target))
                return entry.path().string() + ": missing linked file " + target;
        }
    }
    return "";
}
