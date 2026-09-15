// PocketHarness - skill discovery/search/load implementation.
#include "skills.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>

#include "config.h"

namespace pocket {

namespace {

void scanDir(const std::string& dir, const std::string& source, std::vector<SkillMeta>& out) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct DirCloser {
        DIR* d;
        ~DirCloser() { closedir(d); }
    } closer{d};
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n.empty() || n[0] == '.') continue;
        std::string md = dir + "/" + n + "/SKILL.md";
        struct stat st;
        // lstat: we only need existence/readability here; content is read bounded.
        if (lstat(md.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (access(md.c_str(), R_OK) != 0) continue;
        SkillMeta m;
        m.name = n;
        m.source = source;
        m.path = md;
        auto t = readFileBounded(md, 1 << 18);
        if (t.ok) {
            bool inPara = false;
            std::string para;
            for (const std::string& line : splitLines(t.value)) {
                std::string s = trim(line);
                if (m.heading.empty() && startsWith(s, "#")) {
                    m.heading = trim(s.substr(s.find_first_not_of('#')));
                    continue;
                }
                if (s.empty()) {
                    if (inPara) break;
                    continue;
                }
                inPara = true;
                if (!para.empty()) para += " ";
                para += s;
                if (para.size() > 300) break;
            }
            m.preview = para.size() > 300 ? para.substr(0, 300) + "..." : para;
        }
        out.push_back(std::move(m));
    }
}

}  // namespace

std::vector<SkillMeta> skillDiscover(const std::string& workspace) {
    std::vector<SkillMeta> out;
    scanDir(globalSkillDir(), "global", out);
    // Project skills override globals with the same name.
    std::vector<SkillMeta> proj;
    scanDir(projectSkillDir(workspace), "project", proj);
    for (auto& pm : proj) {
        out.erase(std::remove_if(out.begin(), out.end(),
                                 [&](const SkillMeta& m) { return m.name == pm.name; }),
                  out.end());
        out.push_back(std::move(pm));
    }
    std::sort(out.begin(), out.end(),
              [](const SkillMeta& a, const SkillMeta& b) { return a.name < b.name; });
    return out;
}

std::vector<SkillMeta> skillSearch(const std::vector<SkillMeta>& all, const std::string& query) {
    std::string q = toLower(trim(query));
    if (q.empty()) return all;
    std::vector<SkillMeta> out;
    for (const auto& m : all) {
        std::string hay = toLower(m.name + "\n" + m.heading + "\n" + m.preview);
        if (hay.find(q) != std::string::npos) out.push_back(m);
    }
    return out;
}

Result<std::string> skillLoad(const std::vector<SkillMeta>& all, const std::string& name) {
    std::string n = trim(name);
    for (const auto& m : all) {
        if (m.name == n) {
            auto t = readFileBounded(m.path, 1 << 18);
            if (!t.ok) return Result<std::string>::Err("cannot read skill \"" + n + "\"");
            return Result<std::string>::Ok("# " + m.name + " (" + m.source + ")\n\n" + t.value);
        }
    }
    return Result<std::string>::Err("skill not found: \"" + n + "\" (use action=list)");
}

std::string skillOneLine(const SkillMeta& m) {
    std::string line = "- " + m.name + " [" + m.source + "]";
    if (!m.heading.empty() && m.heading != m.name) line += " — " + m.heading;
    if (!m.preview.empty()) line += ": " + m.preview;
    if (line.size() > 300) line = line.substr(0, 300) + "...";
    return line;
}

}  // namespace pocket
