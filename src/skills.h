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

// BM25 search over name/heading/preview, best match first.
std::vector<SkillMeta> skillSearch(const std::vector<SkillMeta>& all,
                                   const std::string& query);

struct SkillSelection {
    std::vector<SkillMeta> skills;
    bool ui = false;
    bool video = false;
};

// Native workflow routing: explicit names, task domains, then bounded project
// manifest evidence. Only installed skills are offered; loaded skills are skipped.
// This does not run a model, execute a workflow, or authorize external delivery.
SkillSelection skillAutoSelect(const std::vector<SkillMeta>& all, const std::string& task,
                               const std::vector<std::string>& alreadyLoaded = {},
                               size_t maxSkills = 4, const std::string& workspace = "");

// Load full SKILL.md (bounded to 256 KiB).
Result<std::string> skillLoad(const std::vector<SkillMeta>& all, const std::string& name);

// Compact one-line rendering for list output.
std::string skillOneLine(const SkillMeta& m);

}  // namespace pocket
