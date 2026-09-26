// PocketHarness - skill discovery/search/load implementation.
#include "skills.h"

#include "brain.h"
#include "config.h"

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
            bool inPara = false, front = startsWith(t.value, "---");
            std::string para, desc;
            std::vector<std::string> lines = splitLines(t.value);
            size_t i = 0;
            // YAML-ish frontmatter: only `description:` matters (one line).
            if (front)
                for (i = 1; i < lines.size() && trim(lines[i]) != "---"; ++i)
                    if (startsWith(lines[i], "description:")) desc = trim(lines[i].substr(12));
            if (front) ++i;
            for (; i < lines.size(); ++i) {
                const std::string& line = lines[i];
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
            if (!desc.empty()) para = desc;
            m.preview = para.size() > 300 ? para.substr(0, 300) + "..." : para;
        }
        out.push_back(std::move(m));
    }
}

}  // namespace

std::vector<SkillMeta> skillDiscover(const std::string& workspace) {
    std::vector<SkillMeta> out;
    scanDir(bundledSkillDir(), "bundled", out);
    // Later layers override earlier ones: bundled < global < project.
    for (auto [dir, src] : {std::pair{globalSkillDir(), "global"},
                            std::pair{projectSkillDir(workspace), "project"}}) {
        std::vector<SkillMeta> layer;
        scanDir(dir, src, layer);
        for (auto& pm : layer) {
            out.erase(std::remove_if(out.begin(), out.end(),
                                     [&](const SkillMeta& m) { return m.name == pm.name; }),
                      out.end());
            out.push_back(std::move(pm));
        }
    }
    std::sort(out.begin(), out.end(),
              [](const SkillMeta& a, const SkillMeta& b) { return a.name < b.name; });
    return out;
}

std::vector<SkillMeta> skillSearch(const std::vector<SkillMeta>& all, const std::string& query) {
    if (trim(query).empty()) return all;
    // BM25 over name (x3), heading (x2) and description, plus a fuzzy name
    // bonus so typos and partial names still land on the right skill.
    std::vector<std::string> docs;
    for (const auto& m : all) {
        std::string n = m.name;
        std::replace(n.begin(), n.end(), '-', ' ');
        docs.push_back(n + " " + n + " " + n + " " + m.heading + " " + m.heading + " " + m.preview);
    }
    std::vector<double> score = bm25(docs, query);
    std::vector<std::pair<double, size_t>> ranked;
    for (size_t i = 0; i < all.size(); ++i) {
        double f = fuzzyScore(query, all[i].name);
        double s = score[i] + (f >= 0.8 ? 4 * f : 0);
        if (s > 0) ranked.push_back({s, i});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
    std::vector<SkillMeta> out;
    for (const auto& [s, i] : ranked) out.push_back(all[i]);
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
