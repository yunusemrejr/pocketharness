// PocketHarness tests - skills discovery/search/load.
#include "mini.h"

#include "../src/config.h"
#include "../src/skills.h"

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
