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

TEST(skills_Discover_Empty_Markdown_Heading) {
    std::string home = makeTempDir("pocket-skill-heading");
    CHECK(!home.empty());
    HomeGuard hg(home);
    const std::string ws = home + "/ws";
    CHECK(ensureDir(projectSkillDir(ws) + "/empty-heading", 0755).ok);
    CHECK(atomicWriteFile(projectSkillDir(ws) + "/empty-heading/SKILL.md",
                          "#\n##\n# Actual heading\n\nA valid preview.\n", 0644).ok);
    auto all = skillDiscover(ws);
    CHECK_EQ(all.size(), size_t(1));
    CHECK_EQ(all[0].heading, std::string("Actual heading"));
    CHECK_EQ(all[0].preview, std::string("A valid preview."));
    rmRf(home);
    return "";
}

namespace {

std::vector<SkillMeta> workflowCatalog() {
    std::vector<SkillMeta> all;
    for (const char* name : {"project-workflows", "ai-design-slop", "video-studio", "shared-hosting-deployment",
                             "php-application-engineering", "node-runtime-engineering", "modern-frontend-frameworks",
                             "browser-javascript-engineering", "go-service-engineering", "rust-systems-engineering",
                             "java-platform-engineering", "python-software-engineering", "bash-workflows",
                             "cpp-performance-engineering", "c-systems-engineering", "linux-desktop-ui-ux",
                             "local-webapp-workflows", "algorithm-design", "ml-engineering", "llm-fine-tuning",
                             "google-colab-training", "music-composition", "audio-processing", "browser-animation-engineering",
                             "search-discoverability", "natural-editorial-writing", "anti-ai-slop", "linux-network-engineering",
                             "network-traffic-analysis", "packet-trace-analysis", "local-network-analysis", "linux", "ubuntu-operations",
                             "threejs", "threejs-animation-engineering", "blender-production"})
        all.push_back({name, "bundled", name, "", "skills/" + std::string(name) + "/SKILL.md"});
    return all;
}

bool selected(const SkillSelection& result, const std::string& name) {
    for (const auto& skill : result.skills) if (skill.name == name) return true;
    return false;
}

}  // namespace

TEST(skills_AutoSelect_Exact_Domains_And_No_Noise) {
    auto all = workflowCatalog();
    CHECK(skillAutoSelect(all, "hello").skills.empty());
    CHECK(skillAutoSelect(all, "Please go to the logo description").skills.empty());
    CHECK(skillAutoSelect(all, "Compose an email").skills.empty());
    auto js = skillAutoSelect(all, "Fix the JavaScript browser request race");
    CHECK(selected(js, "browser-javascript-engineering"));
    CHECK(!selected(js, "java-platform-engineering"));
    CHECK(!selected(js, "go-service-engineering"));
    for (const auto& item : std::vector<std::pair<std::string, std::string>>{
             {"Build a PHP8+ API", "php-application-engineering"},
             {"Fix the Node.js CLI", "node-runtime-engineering"},
             {"Create a React CDN component", "modern-frontend-frameworks"},
             {"Optimize the Go service", "go-service-engineering"},
             {"Implement a Rust parser", "rust-systems-engineering"},
             {"Fix Java shutdown", "java-platform-engineering"},
             {"Build a Flask API", "python-software-engineering"},
             {"Fix the Bash launcher", "bash-workflows"},
             {"Optimize a C++ renderer", "cpp-performance-engineering"},
             {"Fix memory ownership in C", "c-systems-engineering"},
             {"Build a GTK Linux application", "linux-desktop-ui-ux"},
             {"Debug a local webapp", "local-webapp-workflows"},
             {"Implement a shortest path algorithm", "algorithm-design"},
             {"Build a machine learning classifier", "ml-engineering"}}) {
        auto result = skillAutoSelect(all, item.first);
        if (!selected(result, item.second)) return "missing workflow for " + item.first;
        CHECK(selected(result, "project-workflows"));
        CHECK(result.skills.size() <= 4);
    }
    auto colab = skillAutoSelect(all, "Fine-tune with QLoRA in Google Colab");
    CHECK(selected(colab, "google-colab-training"));
    CHECK(selected(colab, "llm-fine-tuning"));
    CHECK(!selected(colab, "go-service-engineering"));
    return "";
}

TEST(skills_AutoSelect_Bounded_Deduplicated_And_Installed) {
    auto all = workflowCatalog();
    auto ui = skillAutoSelect(all, "Build a PHP website deployed on Namecheap through GitHub SSH");
    CHECK(ui.ui && !ui.video);
    CHECK(selected(ui, "ai-design-slop"));
    CHECK(selected(ui, "shared-hosting-deployment"));
    CHECK(selected(ui, "php-application-engineering"));
    CHECK_EQ(ui.skills.size(), size_t(4));
    auto resumed = skillAutoSelect(all, "Build a PHP website deployed on Namecheap through GitHub SSH",
                                   {"ai-design-slop", "project-workflows", "php-application-engineering"});
    CHECK(resumed.ui);
    CHECK_EQ(resumed.skills.size(), size_t(1));
    CHECK_EQ(resumed.skills[0].name, std::string("shared-hosting-deployment"));
    auto named = skillAutoSelect(all, "Use rust-systems-engineering to create a Rust webapp", {}, 1);
    CHECK_EQ(named.skills.size(), size_t(1));
    CHECK_EQ(named.skills[0].name, std::string("rust-systems-engineering"));
    auto unavailable = skillAutoSelect({all[0]}, "Build a PHP website on GoDaddy");
    CHECK(unavailable.ui);
    CHECK_EQ(unavailable.skills.size(), size_t(1));
    CHECK_EQ(unavailable.skills[0].name, std::string("project-workflows"));
    CHECK(skillAutoSelect(all, "Create a video", {}, 0).skills.empty());
    return "";
}

TEST(skills_AutoSelect_Audio_Is_Independent_Of_Video) {
    auto all = workflowCatalog();
    auto music = skillAutoSelect(all, "Compose a music track and edit its audio loudness");
    CHECK(!music.video);
    CHECK(selected(music, "music-composition"));
    CHECK(selected(music, "audio-processing"));
    CHECK(!selected(music, "video-studio"));
    auto video = skillAutoSelect(all, "Create a video with animated diagrams and narration");
    CHECK(video.video);
    CHECK(selected(video, "video-studio"));
    auto gif = skillAutoSelect(all, "Create a GIF animation");
    CHECK(!gif.video);
    CHECK(selected(gif, "browser-animation-engineering"));
    auto sound = skillAutoSelect(all, "Compose music and edit the sound");
    CHECK(selected(sound, "music-composition") && selected(sound, "audio-processing"));
    CHECK(!sound.video && !selected(sound, "video-studio"));
    CHECK(skillAutoSelect(all, "That argument sounds good").skills.empty());
    CHECK(skillAutoSelect(all, "Sound judgment matters").skills.empty());
    return "";
}

TEST(skills_AutoSelect_Concrete_3D_Assets_And_Animation) {
    const auto all = workflowCatalog();
    for (const auto* task : {"Preview a glTF 3D asset with Three.js", "Inspect model.GLB", "Render a 3D model viewer"}) {
        const auto result = skillAutoSelect(all, task);
        CHECK(selected(result, "threejs"));
        CHECK(!selected(result, "blender-production"));
        CHECK(result.skills.size() <= 4);
    }
    auto motion = skillAutoSelect(all, "Create a Three.js 3D model animation");
    CHECK(selected(motion, "threejs") && selected(motion, "threejs-animation-engineering"));
    CHECK(!selected(motion, "browser-animation-engineering"));
    auto generic3D = skillAutoSelect(all, "Create a 3D animation");
    CHECK(selected(generic3D, "threejs") && selected(generic3D, "threejs-animation-engineering"));
    CHECK(!selected(generic3D, "browser-animation-engineering"));
    auto uiVideo = skillAutoSelect(all, "Create a website video with a Three.js 3D model animation");
    CHECK(uiVideo.ui && uiVideo.video);
    CHECK(selected(uiVideo, "threejs") && selected(uiVideo, "threejs-animation-engineering"));
    CHECK_EQ(uiVideo.skills.size(), size_t(4));
    CHECK(!selected(uiVideo, "browser-animation-engineering"));
    auto loaded = skillAutoSelect(all, "Preview a GLB with Three.js", {"threejs", "project-workflows"});
    CHECK(loaded.skills.empty());
    auto namedLoaded = skillAutoSelect(all, "Use threejs and threejs-animation-engineering for the 3D model animation",
                                       {"threejs", "threejs-animation-engineering", "project-workflows"});
    CHECK(namedLoaded.skills.empty());
    auto capped = skillAutoSelect(all, "Create a Three.js 3D model animation", {}, 1);
    CHECK_EQ(capped.skills.size(), size_t(1));
    CHECK_EQ(capped.skills[0].name, std::string("threejs"));
    CHECK(skillAutoSelect(all, "Preview a GLB with Three.js", {}, 0).skills.empty());
    auto modeling = skillAutoSelect(all, "Model and rig a creature in Blender");
    CHECK(selected(modeling, "blender-production"));
    CHECK(!selected(modeling, "threejs"));
    auto unavailable = skillAutoSelect({all[0]}, "Preview a GLB with Three.js");
    CHECK_EQ(unavailable.skills.size(), size_t(1));
    CHECK_EQ(unavailable.skills[0].name, std::string("project-workflows"));
    CHECK(skillAutoSelect(all, "A 3D printer service needs attention").skills.empty());
    CHECK(skillAutoSelect(all, "The gltfactory description is available").skills.empty());
    return "";
}

TEST(skills_AutoSelect_Project_Manifest_And_Explicit_Runtime) {
    std::string ws = makeTempDir("pocket-workflow");
    CHECK(!ws.empty());
    auto all = workflowCatalog();
    CHECK(atomicWriteFile(ws + "/package.json", "{\"dependencies\":{\"react\":\"19.0.0\"}}", 0644).ok);
    auto result = skillAutoSelect(all, "Fix the failing test", {}, 4, ws);
    CHECK(selected(result, "modern-frontend-frameworks"));
    CHECK(selected(result, "project-workflows"));
    CHECK(skillAutoSelect(all, "hello", {}, 4, ws).skills.empty());
    CHECK(skillAutoSelect(all, "Fix the title in README.md", {}, 4, ws).skills.empty());
    CHECK(skillAutoSelect(all, "Fix a typo in the documentation", {}, 4, ws).skills.empty());
    auto explicitRuntime = skillAutoSelect(all, "Write a Python script", {}, 4, ws);
    CHECK(selected(explicitRuntime, "python-software-engineering"));
    CHECK(!selected(explicitRuntime, "modern-frontend-frameworks"));
    auto asset = skillAutoSelect(all, "Fix the glTF 3D asset preview", {}, 4, ws);
    CHECK(selected(asset, "threejs"));
    CHECK(!selected(asset, "modern-frontend-frameworks"));
    auto audio = skillAutoSelect(all, "Improve the music and edit the sound", {}, 4, ws);
    CHECK(selected(audio, "music-composition") && selected(audio, "audio-processing"));
    CHECK(!selected(audio, "modern-frontend-frameworks") && !selected(audio, "project-workflows"));
    auto resumed = skillAutoSelect(all, "Fix the failing test", {"modern-frontend-frameworks", "project-workflows"}, 4, ws);
    CHECK(resumed.skills.empty());
    rmRf(ws);
    return "";
}

TEST(skills_AutoSelect_Precise_Network_Linux_SEO_And_Writing) {
    const auto all = workflowCatalog();
    auto dns = skillAutoSelect(all, "Diagnose DNS and slow TCP connectivity");
    CHECK(selected(dns, "linux-network-engineering"));
    CHECK(!selected(dns, "project-workflows"));
    auto packet = skillAutoSelect(all, "Inspect a pcap packet trace handshake timeline");
    CHECK(selected(packet, "network-traffic-analysis") && selected(packet, "packet-trace-analysis"));
    CHECK(!selected(packet, "linux-network-engineering"));
    auto sys = skillAutoSelect(all, "Troubleshoot an Ubuntu systemd service with out of memory errors");
    CHECK(selected(sys, "linux") && selected(sys, "ubuntu-operations"));
    auto seo = skillAutoSelect(all, "Audit SEO metadata on the website");
    CHECK(selected(seo, "search-discoverability") && !seo.ui);
    CHECK(!selected(seo, "ai-design-slop") && !selected(seo, "project-workflows"));
    auto design = skillAutoSelect(all, "Design an SEO website layout");
    CHECK(design.ui && selected(design, "ai-design-slop") && selected(design, "search-discoverability"));
    auto writing = skillAutoSelect(all, "Rewrite the report and edit its prose with a quality review");
    CHECK(selected(writing, "natural-editorial-writing") && selected(writing, "anti-ai-slop"));
    CHECK(!selected(writing, "project-workflows"));
    auto code = skillAutoSelect(all, "Write a Bash script that prints a report");
    CHECK(selected(code, "bash-workflows"));
    CHECK(!selected(code, "natural-editorial-writing"));
    CHECK(skillAutoSelect(all, "The geological description is available").skills.empty());
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
