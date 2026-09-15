// PocketHarness - skills: plain Markdown directories, progressive disclosure.
#pragma once

#include <string>
#include <vector>

#include "common.h"

namespace pocket {

struct SkillMeta {
    std::string name;      // directory name
    std::string source;    // "project" or "global"
    std::string heading;   // first "# ..." line
    std::string preview;   // first paragraph
    std::string path;      // full path to SKILL.md
};

// Discover skills. Project skills win on name collision.
std::vector<SkillMeta> skillDiscover(const std::string& workspace);

// Case-insensitive substring search over name/heading/preview.
std::vector<SkillMeta> skillSearch(const std::vector<SkillMeta>& all,
                                   const std::string& query);

// Load full SKILL.md (bounded to 256 KiB).
Result<std::string> skillLoad(const std::vector<SkillMeta>& all, const std::string& name);

// Compact one-line rendering for list output.
std::string skillOneLine(const SkillMeta& m);

}  // namespace pocket
