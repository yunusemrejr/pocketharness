// PocketHarness - skill discovery/search/load implementation.
#include "skills.h"

#include "brain.h"
#include "config.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <initializer_list>

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
                    const size_t begin = s.find_first_not_of('#');
                    if (begin != std::string::npos) m.heading = trim(s.substr(begin));
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

SkillSelection skillAutoSelect(const std::vector<SkillMeta>& all, const std::string& task,
                               const std::vector<std::string>& alreadyLoaded,
                               size_t maxSkills, const std::string& workspace) {
    SkillSelection result;
    // Word boundaries avoid routes such as Java for JavaScript, Go for "logo",
    // UI for "build", and music for a filename containing "musical".
    auto normalized = [](const std::string& value) {
        std::string out = " ";
        for (unsigned char c : value) {
            if (std::isalnum(c) || c == '+') out += static_cast<char>(std::tolower(c));
            else if (out.back() != ' ') out += ' ';
        }
        if (out.back() != ' ') out += ' ';
        return out;
    };
    const std::string text = normalized(task.substr(0, 32768));
    auto has = [&](std::initializer_list<const char*> words) {
        for (const auto* word : words)
            if (text.find(normalized(word)) != std::string::npos) return true;
        return false;
    };
    auto add = [&](const std::string& name) {
        if (result.skills.size() >= maxSkills ||
            std::find(alreadyLoaded.begin(), alreadyLoaded.end(), name) != alreadyLoaded.end()) return;
        for (const auto& skill : result.skills) if (skill.name == name) return;
        for (const auto& skill : all)
            if (skill.name == name) { result.skills.push_back(skill); return; }
    };

    result.ui = has({"ui", "ux", "gui", "user interface", "landing page", "website", "web page", "webapp",
                     "web app", "frontend", "front end", "dashboard", "mockup", "wireframe", "css", "html",
                     "stylesheet", "tailwind", "navbar", "hero section", "figma", "tui", "terminal interface",
                     "desktop app", "desktop application", "gtk", "qt", "fltk"});
    const bool animation = has({"animation", "animated", "motion graphics", "gif"});
    result.video = has({"video", "youtube", "shorts", "reels", "voiceover", "voice over", "narration", "storyboard",
                       "mp4", "motion graphics"}) || (animation && has({"audio", "sound", "music"}));
    const bool delivery = has({"deploy", "deployment", "hosting", "production website", "namecheap", "godaddy",
                              "cPanel", "ssh deployment"});
    const bool php = has({"php", "php8", "php8+", "composer", "laravel", "symfony"});
    const bool node = has({"node", "nodejs", "node js", "npm", "express", "fastify"});
    const bool react = has({"react", "reactdom", "jsx", "next js"});
    const bool browser = has({"vanilla js", "vanilla javascript", "browser javascript", "dom", "javascript", "html", "css"});
    const bool go = has({"golang", "go service", "go module", "go cli", "go mod", "go program", "go app",
                         "go implementation", "go test", "go code", "go programming", "in go", "using go", "with go"});
    const bool rust = has({"rust", "cargo", "rustc"});
    const bool java = has({"java", "jvm", "gradle", "maven", "spring boot"});
    const bool python = has({"python", "flask", "django", "pyproject", "pip"});
    const bool shell = has({"bash", "shell script", "shell scripting", "sh script"});
    const bool cpp = has({"c++", "cpp", "cxx", "cmake"});
    const bool c = has({"c language", "c code", "c programming", "in c", "using c", "with c", "gcc"});
    const bool desktop = has({"linux app", "linux application", "native app", "native application", "desktop app",
                              "desktop application", "gtk", "qt", "fltk"});
    const bool localWeb = has({"local webapp", "local web app", "localhost", "local flask", "local server"});
    const bool algorithm = has({"algorithm", "data structure", "shortest path", "scheduling algorithm", "graph traversal"});
    const bool tuning = has({"fine tuning", "finetuning", "fine tune", "finetune", "lora", "qlora", "peft", "sft"});
    const bool colab = has({"colab", "google colab"});
    const bool ml = has({"machine learning", "ml", "model training", "classification", "regression model", "neural network"});
    const bool seo = has({"seo", "search discoverability", "search engine", "crawlability", "canonical", "robots txt",
                         "sitemap", "structured data", "indexability", "meta description", "title tags", "schema org"});
    const bool proseArtifact = has({"article", "blog post", "essay", "email", "proposal", "copy", "prose", "paragraph",
                                   "readme", "documentation", "markdown", "story"}) ||
                               (has({"report"}) && !has({"script", "code", "bash", "python", "javascript", "java", "rust", "go", "shell"}));
    const bool writing = has({"writing", "copywriting", "technical writing", "blogging", "proofread", "proofreading"}) ||
                         (has({"write", "rewrite", "edit", "draft", "polish"}) &&
                          proseArtifact);
    const bool quality = has({"quality review", "code quality", "prose quality", "anti slop", "desloppify", "design slop"});
    const bool network = has({"network troubleshooting", "network diagnosis", "dns", "resolver", "dhcp", "firewall",
                             "connection refused", "connectivity", "wifi", "wi fi", "ethernet"}) ||
                         (has({"network", "networking", "tcp", "tls", "routing", "ip address"}) &&
                          has({"fix", "debug", "diagnose", "troubleshoot", "failure", "fails", "broken", "slow", "unreachable", "connect", "connection"}));
    const bool packet = has({"pcap", "packet capture", "capture forensics", "packet trace", "tcpdump", "wireshark"});
    const bool lan = has({"lan discovery", "discover devices", "unknown devices", "local network inventory"});
    const bool linux = has({"linux troubleshooting", "linux system", "linux service", "systemd", "journalctl", "sysadmin",
                           "out of memory", "disk full", "permission denied", "process stuck", "slow disk", "it troubleshooting"});
    if (seo && !has({"design", "redesign", "restyle", "layout", "css", "ui", "ux", "gui", "visual", "build", "create"}))
        result.ui = false;  // a source metadata audit is not a visual redesign
    const bool work = php || node || react || browser || go || rust || java || python || shell || cpp || c || desktop ||
                      localWeb || algorithm || tuning || colab || ml || result.ui || result.video || animation || delivery;

    // Preserve named skills first, then mandatory craft and actual delivery needs.
    for (const auto& skill : all)
        if (text.find(normalized(skill.name)) != std::string::npos) add(skill.name);
    if (result.ui) add("ai-design-slop");
    if (result.video) add("video-studio");
    if (seo) add("search-discoverability");
    if (writing) add("natural-editorial-writing");
    if (quality) add("anti-ai-slop");
    if (packet) add("network-traffic-analysis");
    if (packet && has({"timeline", "handshake", "retransmission", "packet trace"})) add("packet-trace-analysis");
    if (lan) add("local-network-analysis");
    if (network && !packet && !lan) add("linux-network-engineering");
    if (linux) add("linux");
    if (linux && has({"ubuntu"})) add("ubuntu-operations");
    if (delivery) add("shared-hosting-deployment");
    if (work) add("project-workflows");
    if (colab) add("google-colab-training");
    if (tuning) add("llm-fine-tuning");
    if (react) add("modern-frontend-frameworks");
    if (php) add("php-application-engineering");
    if (node) add("node-runtime-engineering");
    if (browser && !react && !node) add("browser-javascript-engineering");
    if (go) add("go-service-engineering");
    if (rust) add("rust-systems-engineering");
    if (java) add("java-platform-engineering");
    if (python) add("python-software-engineering");
    if (shell) add("bash-workflows");
    if (cpp) add("cpp-performance-engineering");
    if (c && !cpp) add("c-systems-engineering");
    if (desktop) add("linux-desktop-ui-ux");
    if (localWeb) add("local-webapp-workflows");
    if (algorithm) add("algorithm-design");
    if (ml && !tuning) add("ml-engineering");
    if (animation && !result.video) add("browser-animation-engineering");
    if (has({"music", "compose a song", "compose song", "songwriting", "melody", "midi", "soundtrack"})) add("music-composition");
    if (has({"audio", "sound editing", "sound edit", "noise reduction", "denoise", "loudness", "mix audio"}))
        add("audio-processing");

    // A generic implementation request can still route from actual manifests.
    // Explicit task runtimes win; project detection never runs commands or scans
    // the entire tree, and a casual greeting does not inspect the workspace.
    const bool explicitRuntime = php || node || react || browser || go || rust || java || python || shell || cpp || c;
    const bool documentationOnly = has({"readme", "markdown", "documentation", "docs", "changelog", "typo", "spelling", "grammar"}) &&
                                   !has({"implement", "debug", "code", "function", "parser", "test", "tests", "crash"});
    if (!workspace.empty() && !explicitRuntime && !documentationOnly && !seo && !writing && !network && !packet && !lan && !linux &&
        has({"implement", "fix", "debug", "refactor", "optimize", "improve", "build", "test", "tests", "bug", "failure"})) {
        auto exists = [&](const char* file) {
            struct stat st;
            return stat((workspace + "/" + file).c_str(), &st) == 0 && S_ISREG(st.st_mode);
        };
        std::vector<std::string> detected;
        if (exists("composer.json")) detected.push_back("php-application-engineering");
        if (exists("package.json")) {
            auto package = readFileBounded(workspace + "/package.json", 1 << 15);
            if (package.ok && package.value.find("\"react\"") != std::string::npos)
                detected.push_back("modern-frontend-frameworks");
            else detected.push_back("node-runtime-engineering");
        }
        if (exists("go.mod")) detected.push_back("go-service-engineering");
        if (exists("Cargo.toml")) detected.push_back("rust-systems-engineering");
        if (exists("pom.xml") || exists("build.gradle") || exists("build.gradle.kts")) detected.push_back("java-platform-engineering");
        if (exists("pyproject.toml") || exists("requirements.txt") || exists("setup.py")) detected.push_back("python-software-engineering");
        if (exists("CMakeLists.txt")) detected.push_back("c-cpp-multiplatform");
        if (detected.empty() && exists("Makefile")) {
            auto makefile = readFileBounded(workspace + "/Makefile", 1 << 15);
            if (makefile.ok && (makefile.value.find("CXX") != std::string::npos || makefile.value.find(".cpp") != std::string::npos))
                detected.push_back("cpp-performance-engineering");
            else if (makefile.ok && makefile.value.find("CC") != std::string::npos)
                detected.push_back("c-systems-engineering");
        }
        if (!detected.empty()) add("project-workflows");
        for (const auto& name : detected) add(name);
    }
    return result;
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
