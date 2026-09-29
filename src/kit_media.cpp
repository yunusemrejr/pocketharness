#include "kit_media.h"

#include <sched.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <set>

#include "config.h"
#include "json.h"
#include "kit_studio.h"
#include "process.h"

namespace pocket {

std::string voicesDir() { return getenv("POCKET_VOICES") && *getenv("POCKET_VOICES") ? getenv("POCKET_VOICES") : stateDir() + "/voices"; }

namespace {

namespace fs = std::filesystem;
constexpr const char* kUA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140 Safari/537.36";

int fail(const std::string& e) {
    fprintf(stderr, "pocket kit: %s\n", e.c_str());
    return 1;
}
bool number(const std::string& s, double& v) {
    if (s.empty()) return false;
    auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size() && std::isfinite(v);
}
SpawnResult run(std::vector<std::string> argv, long timeoutMs, size_t limit = 8 << 20, std::string workdir = "") {
    SpawnOpts o;
    o.exe = argv[0];
    o.argv = std::move(argv);
    o.timeoutMs = timeoutMs;
    o.outLimit = limit;
    o.workdir = std::move(workdir);
    return spawn(o);
}
std::string curlText(const std::string& url, long sec = 30) {
    SpawnResult r = run({"curl", "--disable", "-sSL", "--compressed", "-A", kUA, "--max-time", std::to_string(sec),
                         "--proto", "=http,https", url}, (sec + 3) * 1000, 32 << 20);
    return r.ok && r.exitCode == 0 ? r.out : "";
}
// Download to a temp name beside `dest`, publish atomically. Returns "" or an error.
std::string download(const std::string& url, const std::string& dest, long sec = 600) {
    if (!startsWith(url, "http://") && !startsWith(url, "https://")) return "not an http(s) URL: " + url;
    std::error_code ec;
    fs::create_directories(fs::path(dest).parent_path(), ec);
    const std::string tmp = dest + ".part";
    SpawnResult r = run({"curl", "--disable", "-fsSL", "-A", kUA, "--connect-timeout", "10", "--max-time", std::to_string(sec),
                         "--max-filesize", "400000000", "--proto", "=http,https", "-o", tmp, url}, (sec + 5) * 1000, 1 << 16);
    struct stat st{};
    if (!r.ok || r.exitCode != 0 || stat(tmp.c_str(), &st) || st.st_size == 0) {
        unlink(tmp.c_str());
        return "download failed (" + url + "): " + trim(r.err.substr(0, 200));
    }
    if (rename(tmp.c_str(), dest.c_str())) return "cannot write " + dest;
    return "";
}
std::string slug(const std::string& s) {
    std::string o;
    for (unsigned char c : s) o += isalnum(c) || c == '.' || c == '_' || c == '-' ? (char)c : '-';
    while (!o.empty() && (o[0] == '.' || o[0] == '-')) o.erase(0, 1);
    return o.empty() ? "asset" : o.substr(0, 80);
}
std::string toUpper(std::string s) { for (char& c : s) c = (char)toupper((unsigned char)c); return s; }
std::string secs(double t) { char b[32]; snprintf(b, sizeof b, "%.3fs", t); return b; }
std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) o += c == '&' ? "&amp;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : std::string(1, c);
    return o;
}

// ---------------------------------------------------------------------------
// say
// ---------------------------------------------------------------------------
int syllables(const std::string& w) {
    int n = 0;
    bool prev = false;
    std::string l = toLower(w);
    for (char c : l) {
        bool v = strchr("aeiouy", c) != nullptr;
        if (v && !prev) ++n;
        if (isdigit((unsigned char)c)) n += 1;
        prev = v;
    }
    std::string t = l;
    while (!t.empty() && !isalpha((unsigned char)t.back())) t.pop_back();
    if (n > 1 && !t.empty() && t.back() == 'e' && !endsWith(t, "le")) --n;
    return std::max(1, n);
}

struct Voice { std::string exe, model, name; bool piper = true; };

Voice findVoice(const std::string& want) {
    Voice v;
    for (const std::string& c : {std::string(getenv("POCKET_PIPER") ? getenv("POCKET_PIPER") : ""), whichExe("piper"),
                                 voicesDir() + "/piper/piper"})
        if (!c.empty() && access(c.c_str(), X_OK) == 0) { v.exe = c; break; }
    std::vector<std::string> models;
    std::error_code ec;
    if (fs::is_directory(voicesDir(), ec))
        for (auto& e : fs::directory_iterator(voicesDir(), ec))
            if (e.path().extension() == ".onnx") models.push_back(e.path().string());
    std::sort(models.begin(), models.end());
    if (!want.empty()) {
        if (want.find('/') != std::string::npos || endsWith(want, ".onnx")) v.model = want;
        else for (auto& m : models) if (fs::path(m).stem() == want) v.model = m;
        if (v.model.empty()) { v.exe.clear(); return v; }
    } else if (!models.empty()) {
        v.model = models[0];
        for (auto& m : models) if (fs::path(m).stem() == "en_US-lessac-medium") v.model = m;
    }
    v.name = fs::path(v.model).stem().string();
    if (v.exe.empty() || v.model.empty()) {
        v.piper = false;
        for (const char* n : {"espeak-ng", "espeak"}) if (!(v.exe = whichExe(n)).empty()) break;
        v.name = "espeak (robotic fallback)";
    }
    return v;
}

int say(const std::vector<std::string>& a);

int sayInstall(const std::string& name) {
    static const std::set<std::string> qualities = {"x_low", "low", "medium", "high"};
    std::vector<std::string> parts;
    for (size_t p = 0; p <= name.size();) {
        size_t q = name.find('-', p);
        if (q == std::string::npos) q = name.size();
        parts.push_back(name.substr(p, q - p));
        p = q + 1;
    }
    if (parts.size() != 3 || !qualities.count(parts[2]) || parts[0].size() < 4)
        return fail("voice names look like en_US-lessac-medium (see https://rhasspy.github.io/piper-samples)");
    struct utsname u{};
    uname(&u);
    const std::string arch = !strcmp(u.machine, "aarch64") ? "aarch64" : "x86_64";
    if (strcmp(u.sysname, "Linux") || (arch == "x86_64" && strcmp(u.machine, "x86_64"))) return fail("automatic setup supports Linux x86_64 and aarch64; install piper yourself and set POCKET_PIPER");
    const std::string dir = voicesDir();
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (access(dir.c_str(), W_OK))
        return fail("cannot write " + dir + ". Installing a voice is a user action outside the sandbox: run `pocket kit say --setup` in your own terminal (in the TUI: ! pocket kit say --setup)");
    if (access((dir + "/piper/piper").c_str(), X_OK)) {
        fprintf(stderr, "downloading the Piper engine (about 25 MB) into %s\n", dir.c_str());
        std::string e = download("https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_linux_" + arch + ".tar.gz", dir + "/piper.tgz", 900);
        if (!e.empty()) return fail(e);
        SpawnResult t = run({"tar", "xzf", dir + "/piper.tgz", "-C", dir}, 120000, 1 << 16);
        unlink((dir + "/piper.tgz").c_str());
        if (!t.ok || t.exitCode != 0) return fail("cannot unpack the Piper engine: " + trim(t.err.substr(0, 200)));
    }
    const std::string base = "https://huggingface.co/rhasspy/piper-voices/resolve/main/" + parts[0].substr(0, 2) + "/" + parts[0] + "/" +
                             parts[1] + "/" + parts[2] + "/" + name + ".onnx";
    for (const char* ext : {".onnx", ".onnx.json"})
        if (access((dir + "/" + name + ext).c_str(), R_OK)) {
            fprintf(stderr, "downloading voice %s%s\n", name.c_str(), ext);
            std::string e = download(base + (strcmp(ext, ".onnx") ? ".json" : ""), dir + "/" + name + ext, 900);
            if (!e.empty()) return fail(e);
        }
    printf("ready: %s (engine + voice in %s)\n", name.c_str(), dir.c_str());
    return 0;
}

bool sentenceEnd(const std::string& d) {
    static const std::set<std::string> abbr = {"e.g.", "i.e.", "vs.", "etc.", "mr.", "mrs.", "ms.", "dr.", "st.", "no."};
    std::string t = d;
    while (!t.empty() && strchr("\"')]}”’", t.back())) t.pop_back();
    if (t.empty() || !strchr(".!?", t.back())) return false;
    return !abbr.count(toLower(t));
}

struct Timed { std::string text; double start = 0, end = 0; };
struct Sentence {
    int beat = 0;
    double start = 0, end = 0;
    std::string text;
    std::vector<Timed> words;
};

std::string hms(double t, char sep) {
    if (t < 0) t = 0;
    long ms = long(std::lround(t * 1000));
    char b[32];
    snprintf(b, sizeof b, "%02ld:%02ld:%02ld%c%03ld", ms / 3600000, ms / 60000 % 60, ms / 1000 % 60, sep, ms % 1000);
    return b;
}

int say(const std::vector<std::string>& a) {
    const char* usage =
        "usage: kit say OUT.wav \"script text\" | --file SCRIPT.txt [--voice NAME] [--speed 1.0] [--lead .2] [--gap .28] [--beat-gap .6] [--caption-words 6]\n"
        "       kit say --setup [VOICE]   download Piper + a neural voice (default en_US-lessac-medium)\n"
        "       kit say --voices          list installed voices\n"
        "Blank lines in the script separate beats. {TTS|tee tee ess} shows the first form and speaks the second.";
    if (a.empty() || a[0] == "--help") { puts(usage); return a.empty() ? 2 : 0; }
    if (a[0] == "--setup") return sayInstall(a.size() > 1 ? a[1] : "en_US-lessac-medium");
    if (a[0] == "--voices") {
        std::error_code ec;
        int n = 0;
        if (fs::is_directory(voicesDir(), ec))
            for (auto& e : fs::directory_iterator(voicesDir(), ec))
                if (e.path().extension() == ".onnx") printf("%s\n", e.path().stem().string().c_str()), ++n;
        if (!n) puts("(none installed: run `pocket kit say --setup`)");
        return 0;
    }
    std::string out = a[0], script, voiceName;
    double speed = 1.0, lead = .2, gap = .28, beatGap = .6, capWords = 6;
    for (size_t i = 1; i < a.size(); ++i) {
        const std::string& k = a[i];
        if (!startsWith(k, "--")) { if (!script.empty()) return fail(usage); script = k; continue; }
        if (i + 1 >= a.size()) return fail("missing value for " + k);
        const std::string& v = a[++i];
        if (k == "--file") { auto f = readFileBounded(v, 1 << 20); if (!f.ok) return fail(f.error); script = f.value; }
        else if (k == "--voice") voiceName = v;
        else {
            double* num = k == "--speed" ? &speed : k == "--lead" ? &lead : k == "--gap" ? &gap : k == "--beat-gap" ? &beatGap : k == "--caption-words" ? &capWords : nullptr;
            if (!num || !number(v, *num)) return fail("unknown option or invalid number: " + k);
        }
    }
    if (!endsWith(toLower(out), ".wav")) return fail("output must end in .wav");
    if (speed < .5 || speed > 2 || lead < 0 || lead > 5 || gap < 0 || gap > 3 || beatGap < 0 || beatGap > 5 || capWords < 2 || capWords > 12)
        return fail("speed .5..2, lead 0..5, gap 0..3, beat-gap 0..5, caption-words 2..12");
    auto beats = scriptSentences(script);
    size_t total = 0;
    for (auto& b : beats) total += b.size();
    if (!total) return fail("the script is empty");
    if (total > 400) return fail("more than 400 sentences: narrate long videos in parts");
    Voice voice = findVoice(voiceName);
    if (voice.exe.empty() || voice.model.empty()) {
        if (!voiceName.empty() && voice.exe.empty() && voice.model.empty()) return fail("no such voice: " + voiceName + " (pocket kit say --voices)");
        return fail("no speech engine installed. Run `pocket kit say --setup` once (downloads Piper, ~90 MB, neural voice, about 0.1x real time on CPU)");
    }
    // Spoken lines: one per sentence.
    std::vector<std::string> lines;
    for (auto& b : beats)
        for (auto& s : b) {
            std::string l;
            for (auto& u : s) l += (l.empty() ? "" : " ") + u.spoken;
            lines.push_back(l);
        }
    std::string tmpl = (getenv("TMPDIR") && *getenv("TMPDIR") ? std::string(getenv("TMPDIR")) : "/tmp") + "/pocket-say-XXXXXX";
    if (!mkdtemp(tmpl.data())) return fail("cannot create a temporary directory");
    struct Cleanup { std::string d; ~Cleanup() { std::error_code e; fs::remove_all(d, e); } } cleanup{tmpl};
    int64_t started = nowMs();
    std::vector<std::string> files;
    if (voice.piper) {
        SpawnOpts o;
        o.exe = voice.exe;
        o.argv = {voice.exe, "--model", voice.model, "--output_dir", tmpl, "--length_scale", std::to_string(1.0 / speed),
                  "--sentence_silence", "0"};
        for (auto& l : lines) o.stdinData += l + "\n";
        o.workdir = fs::path(voice.exe).parent_path().string();
        o.timeoutMs = 900000;
        o.outLimit = 1 << 16;
        // Politeness: at most 4 cores, low priority, so a render or a game beside it stays smooth.
        cpu_set_t all, some;
        CPU_ZERO(&some);
        if (sched_getaffinity(0, sizeof all, &all) == 0)
            for (int c = 0, n = 0; c < CPU_SETSIZE && n < 4; ++c) if (CPU_ISSET(c, &all)) CPU_SET(c, &some), ++n;
        o.childSetup = [some] { sched_setaffinity(0, sizeof some, &some); setpriority(PRIO_PROCESS, 0, 8); };
        SpawnResult r = spawn(o);
        if (!r.ok || r.exitCode != 0) return fail("Piper failed: " + trim(r.err.substr(std::max<size_t>(r.err.size(), 400) - 400)));
        for (auto& e : fs::directory_iterator(tmpl)) if (e.path().extension() == ".wav") files.push_back(e.path().string());
        std::sort(files.begin(), files.end());
    } else {
        for (size_t i = 0; i < lines.size(); ++i) {
            std::string f = tmpl + "/" + std::to_string(1000 + i) + ".wav";
            SpawnResult r = run({voice.exe, "-v", "en-us", "-s", std::to_string(int(165 * speed)), "-w", f, lines[i]}, 60000, 1 << 16);
            if (!r.ok || r.exitCode != 0) return fail("espeak failed: " + trim(r.err.substr(0, 200)));
            files.push_back(f);
        }
    }
    if (files.size() != lines.size()) return fail("the speech engine produced " + std::to_string(files.size()) + " clips for " + std::to_string(lines.size()) + " sentences");
    // Assemble with tight edges and measured times.
    const int R = kStudioRate;
    Stereo mix;
    auto pushSilence = [&](double s) { size_t n = size_t(s * R); mix.l.insert(mix.l.end(), n, 0.f); mix.r.insert(mix.r.end(), n, 0.f); };
    pushSilence(lead);
    std::vector<Sentence> sentences;
    size_t idx = 0;
    for (size_t b = 0; b < beats.size(); ++b)
        for (size_t s = 0; s < beats[b].size(); ++s, ++idx) {
            auto clip = loadAudio(files[idx]);
            if (!clip.ok) return fail(clip.error);
            Stereo& c = clip.value;
            size_t lo = 0, hi = c.size();
            while (lo < hi && std::fabs(c.l[lo]) < .004f) ++lo;
            while (hi > lo && std::fabs(c.l[hi - 1]) < .004f) --hi;
            lo = lo > size_t(.012 * R) ? lo - size_t(.012 * R) : 0;
            hi = std::min(c.size(), hi + size_t(.05 * R));
            Sentence sn;
            sn.beat = int(b) + 1;
            sn.start = double(mix.size()) / R + .012;
            const size_t fade = size_t(.004 * R);
            for (size_t i = lo; i < hi; ++i) {
                float f = std::min({1.f, float(i - lo) / fade, float(hi - 1 - i) / fade});
                mix.l.push_back(c.l[i] * f); mix.r.push_back(c.l[i] * f);
            }
            sn.end = double(mix.size()) / R - .05;
            // Word windows by syllable weight; commas and colons hold the voice a little longer.
            double sum = 0;
            std::vector<double> wt;
            for (auto& u : beats[b][s]) {
                double w = syllables(u.spoken) + (u.display.size() && strchr(",;:", u.display.back()) ? .8 : 0);
                wt.push_back(w); sum += w;
                sn.text += (sn.text.empty() ? "" : " ") + u.display;
            }
            double t = sn.start;
            for (size_t w = 0; w < wt.size(); ++w) {
                double d = (sn.end - sn.start) * wt[w] / sum;
                sn.words.push_back({beats[b][s][w].display, t, t + d});
                t += d;
            }
            sentences.push_back(std::move(sn));
            const bool lastInBeat = s + 1 == beats[b].size();
            pushSilence(lastInBeat ? beatGap : gap);
        }
    mix.l.resize(mix.l.size() - size_t(beatGap * R));
    mix.r.resize(mix.l.size());
    pushSilence(.35);  // room for the last word to land
    const double duration = double(mix.size()) / R;
    if (const double pk = peakDb(mix); pk > -1.5) {  // leave headroom: neural voices touch full scale
        const float g = float(std::pow(10.0, (-1.5 - pk) / 20));
        for (size_t i = 0; i < mix.size(); ++i) mix.l[i] *= g, mix.r[i] *= g;
    }
    auto saved = saveStereoWav(out, mix);
    if (!saved.ok) return fail(saved.error);
    // Sidecars.
    std::string base = out.substr(0, out.size() - 4);
    json::Array js, jb;
    std::vector<std::pair<double, double>> beatSpan(beats.size(), {1e9, 0});
    for (auto& sn : sentences) {
        json::Array jw;
        for (auto& w : sn.words) jw.push_back(json::Object{{"w", w.text}, {"s", std::round(w.start * 1000) / 1000}, {"e", std::round(w.end * 1000) / 1000}});
        js.push_back(json::Object{{"beat", sn.beat}, {"s", std::round(sn.start * 1000) / 1000}, {"e", std::round(sn.end * 1000) / 1000},
                                  {"text", sn.text}, {"words", jw}});
        auto& sp = beatSpan[sn.beat - 1];
        sp.first = std::min(sp.first, sn.start); sp.second = std::max(sp.second, sn.end);
    }
    for (size_t b = 0; b < beatSpan.size(); ++b)
        jb.push_back(json::Object{{"beat", (int)b + 1}, {"s", std::round(beatSpan[b].first * 1000) / 1000}, {"e", std::round(beatSpan[b].second * 1000) / 1000}});
    auto w1 = writeOutputFile(base + ".json", json::stringify(json::Object{
        {"duration", std::round(duration * 1000) / 1000}, {"voice", voice.name}, {"timing", "sentence times are measured; word times are estimated from syllables"},
        {"beats", jb}, {"sentences", js}}, true));
    if (!w1.ok) return fail(w1.error);
    // Caption phrases: short, break at punctuation, never across sentences.
    struct Phrase { std::string text; double s, e; };
    std::vector<Phrase> phrases;
    for (auto& sn : sentences) {
        // Balanced chunks of about 30 characters, preferring breaks after punctuation.
        const size_t chunks = std::max<size_t>(1, (sn.text.size() + 33) / 34);
        const double target = double(sn.text.size()) / chunks;
        Phrase cur{"", 0, 0};
        int count = 0;
        for (size_t w = 0; w < sn.words.size(); ++w) {
            if (!count) cur = {sn.words[w].text, sn.words[w].start, sn.words[w].end};
            else cur.text += " " + sn.words[w].text, cur.e = sn.words[w].end;
            ++count;
            const bool punct = strchr(",;:", sn.words[w].text.back()) != nullptr;
            const size_t remaining = sn.words.size() - w - 1;
            const bool full = cur.text.size() >= target * 1.15 || count >= capWords || (punct && cur.text.size() >= target * .7);
            if (!remaining || (full && remaining >= 2 && chunks > 1)) { phrases.push_back(cur); count = 0; }
        }
    }
    std::string srt, html = "<div class=\"cap\" aria-hidden=\"true\">\n", css = ":root{--voice-end:" + secs(duration) + ";";
    for (size_t i = 0; i < phrases.size(); ++i) {
        double e = phrases[i].e + (i + 1 < phrases.size() ? std::min(.2, std::max(0.0, phrases[i + 1].s - phrases[i].e)) : .2);
        srt += std::to_string(i + 1) + "\n" + hms(phrases[i].s, ',') + " --> " + hms(e, ',') + "\n" + phrases[i].text + "\n\n";
        html += "<span style=\"--s:" + secs(phrases[i].s) + ";--d:" + secs(e - phrases[i].s) + "\">" + esc(phrases[i].text) + "</span>\n";
    }
    html += "</div>\n";
    for (size_t b = 0; b < beatSpan.size(); ++b)
        css += "\n--b" + std::to_string(b + 1) + "-s:" + secs(beatSpan[b].first) + ";--b" + std::to_string(b + 1) + "-e:" + secs(beatSpan[b].second) +
               ";--b" + std::to_string(b + 1) + "-d:" + secs(beatSpan[b].second - beatSpan[b].first) + ";";
    for (size_t i = 0; i < sentences.size(); ++i)
        css += "\n--u" + std::to_string(i + 1) + "-s:" + secs(sentences[i].start) + ";--u" + std::to_string(i + 1) + "-e:" + secs(sentences[i].end) + ";";
    css += "\n}\n";
    for (auto pr : {std::pair<std::string, std::string>{".srt", srt}, {".captions.html", html}, {".cues.css", css}}) {
        auto w = writeOutputFile(base + pr.first, pr.second);
        if (!w.ok) return fail(w.error);
    }
    printf("%s: %.2fs, %zu sentences in %zu beats, voice %s, synthesised in %.1fs\n", out.c_str(), duration, sentences.size(), beats.size(),
           voice.name.c_str(), double(nowMs() - started) / 1000);
    printf("timing: %s.json  %s.srt  %s.captions.html  %s.cues.css (--b1-s .. beat times, --u1-s .. sentence times)\n", base.c_str(), base.c_str(), base.c_str(), base.c_str());
    return 0;
}

// ---------------------------------------------------------------------------
// asset
// ---------------------------------------------------------------------------
void attribute(const std::string& dir, const std::string& line) {
    std::string path = dir + "/ATTRIBUTION.txt";
    auto old = readFileBounded(path, 1 << 20);
    std::string text = old.ok ? old.value : "";
    if (text.find(line) == std::string::npos) atomicWriteFile(path, text + line + "\n");
}

std::string mediaExt(const std::string& url) {
    std::string p = url.substr(0, url.find_first_of("?#"));
    size_t dot = p.rfind('.');
    return dot != std::string::npos && p.size() - dot <= 6 ? p.substr(dot) : "";
}

int assetSearch(const std::vector<std::string>& a) {
    std::string kind = "image", query;
    double count = 8;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] == "--kind" && i + 1 < a.size()) kind = a[++i];
        else if (a[i] == "-n" && i + 1 < a.size()) { if (!number(a[++i], count) || count < 1 || count > 30) return fail("-n must be 1..30"); }
        else query += (query.empty() ? "" : " ") + a[i];
    }
    if (query.empty()) return fail("usage: kit asset search QUERY [--kind image|audio|model|hdri|texture] [-n 8]");
    std::string enc;
    for (unsigned char c : query) { char b[4]; if (isalnum(c)) enc += (char)c; else { snprintf(b, sizeof b, "%%%02X", c); enc += b; } }
    if (kind == "image" || kind == "audio") {
        std::string body = curlText("https://api.openverse.org/v1/" + std::string(kind == "image" ? "images" : "audio") + "/?q=" + enc +
                                    "&page_size=" + std::to_string(int(count) * (kind == "audio" ? 3 : 1)) + "&license_type=commercial&mature=false");
        auto j = json::parse(body);
        if (!j.ok) return fail("Openverse did not answer (rate limit or offline); retry shortly");
        int shown = 0;
        for (const auto& r : j.value.at("results").asArr()) {
            std::string url = r.at("url").isStr() ? r.at("url").asStr() : "";
            if (url.empty() || shown >= count) continue;
            ++shown;
            const std::string title = sanitizeTerminal(r.at("title").asStr()), who = sanitizeTerminal(r.at("creator").isStr() ? r.at("creator").asStr() : "unknown");
            const std::string lic = toUpper(r.at("license").asStr()) + (r.at("license_version").isStr() ? " " + r.at("license_version").asStr() : "");
            const std::string size = kind == "image" ? " " + std::to_string(r.at("width").asInt()) + "x" + std::to_string(r.at("height").asInt())
                                                     : " " + std::to_string(r.at("duration").asInt() / 1000) + "s";
            printf("%d. %s — %s [%s]%s\n   url: %s\n   credit: \"%s by %s, %s, %s\"\n", shown, title.c_str(), who.c_str(), lic.c_str(), size.c_str(), url.c_str(),
                   title.c_str(), who.c_str(), lic.c_str(), r.at("foreign_landing_url").asStr().c_str());
        }
        if (!shown) puts("no results");
        else puts("get one with: kit asset get URL OUT_DIR --credit \"<its credit line>\"   (recorded in ATTRIBUTION.txt; CC BY needs it in the description or end card)");
        return 0;
    }
    if (kind != "model" && kind != "hdri" && kind != "texture") return fail("kind must be image, audio, model, hdri or texture");
    auto j = json::parse(curlText("https://api.polyhaven.com/assets?t=" + std::string(kind == "model" ? "models" : kind == "hdri" ? "hdris" : "textures"), 40));
    if (!j.ok) return fail("Poly Haven did not answer; retry shortly");
    std::vector<std::string> words;
    for (size_t p = 0; p < query.size();) {
        size_t q = query.find(' ', p);
        if (q == std::string::npos) q = query.size();
        if (q > p) words.push_back(toLower(query.substr(p, q - p)));
        p = q + 1;
    }
    std::vector<std::pair<int, std::string>> ranked;
    for (const auto& [id, v] : j.value.asObj()) {
        std::string hay = toLower(id + " " + v.at("name").asStr());
        for (const auto& t : v.at("tags").asArr()) hay += " " + toLower(t.asStr());
        for (const auto& t : v.at("categories").asArr()) hay += " " + toLower(t.asStr());
        int score = 0;
        for (auto& w : words) if (hay.find(w) != std::string::npos) score += toLower(v.at("name").asStr()).find(w) != std::string::npos ? 3 : 1;
        if (score) ranked.push_back({-score, id});
    }
    std::sort(ranked.begin(), ranked.end());
    for (size_t i = 0; i < ranked.size() && i < (size_t)count; ++i) {
        const auto& v = j.value.at(ranked[i].second);
        std::string cats;
        for (const auto& c : v.at("categories").asArr()) cats += (cats.empty() ? "" : ", ") + c.asStr();
        printf("%zu. %s [CC0] %s\n   get: kit asset get polyhaven:%s OUT_DIR\n", i + 1, v.at("name").asStr().c_str(), cats.c_str(), ranked[i].second.c_str());
    }
    if (ranked.empty()) puts("no results");
    return 0;
}

int assetGet(const std::vector<std::string>& a) {
    if (a.size() < 2) return fail("usage: kit asset get URL|polyhaven:ID|three OUT_DIR [--res 1k|2k|4k]");
    std::string res = "1k", credit;
    for (size_t i = 2; i + 1 < a.size(); i += 2) { if (a[i] == "--res") res = a[i + 1]; else if (a[i] == "--credit") credit = a[i + 1]; }
    const std::string dir = a[1];
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (!fs::is_directory(dir, ec)) return fail("cannot create " + dir);
    if (a[0] == "three") {
        const std::string cdn = "https://cdn.jsdelivr.net/npm/three@0.170.0/";
        for (auto [from, to] : std::vector<std::pair<std::string, std::string>>{
                 {"build/three.module.min.js", "three.module.min.js"}, {"examples/jsm/loaders/GLTFLoader.js", "jsm/loaders/GLTFLoader.js"},
                 {"examples/jsm/utils/BufferGeometryUtils.js", "jsm/utils/BufferGeometryUtils.js"},
                 {"examples/jsm/environments/RoomEnvironment.js", "jsm/environments/RoomEnvironment.js"},
                 {"examples/jsm/loaders/RGBELoader.js", "jsm/loaders/RGBELoader.js"}}) {
            std::string e = download(cdn + from, dir + "/" + to, 120);
            if (!e.empty()) return fail(e);
        }
        attribute(dir, "three.js r170 — MIT License — threejs.org");
        printf("three.js r170 saved in %s\nuse: <script type=\"importmap\">{\"imports\":{\"three\":\"./three.module.min.js\",\"three/addons/\":\"./jsm/\"}}</script>\n"
               "then: import * as THREE from 'three'; import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';\n"
               "WebGL renders in software here: keep models under ~100k triangles, set renderer.setPixelRatio(1), and await loading in window.renderReady.\n", dir.c_str());
        return 0;
    }
    if (startsWith(a[0], "polyhaven:")) {
        const std::string id = a[0].substr(10);
        if (id.empty() || id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos) return fail("invalid Poly Haven id");
        auto files = json::parse(curlText("https://api.polyhaven.com/files/" + id));
        if (!files.ok || !files.value.isObj()) return fail("unknown Poly Haven asset " + id);
        const std::string out = dir + "/" + id;
        std::vector<std::pair<std::string, std::string>> want;  // url, relative path
        const json::Value& f = files.value;
        auto pickRes = [&](const json::Value& byRes) -> const json::Value& {
            return byRes.has(res) ? byRes.at(res) : byRes.at("1k");
        };
        if (f.has("gltf")) {
            const auto& g = pickRes(f.at("gltf")).at("gltf");
            want.push_back({g.at("url").asStr(), id + ".gltf"});
            if (g.at("include").isObj()) for (const auto& [rel, v] : g.at("include").asObj()) want.push_back({v.at("url").asStr(), rel});
        } else if (f.has("hdri")) {
            const auto& h = pickRes(f.at("hdri"));
            const auto& pick = h.has("hdr") ? h.at("hdr") : h.at("exr");
            want.push_back({pick.at("url").asStr(), id + "_" + res + (h.has("hdr") ? ".hdr" : ".exr")});
        } else {
            for (const char* map : {"Diffuse", "nor_gl", "Rough", "Displacement", "arm"})
                if (f.has(map)) {
                    const auto& m = pickRes(f.at(map));
                    const auto& pick = m.has("jpg") ? m.at("jpg") : m.at("png");
                    if (pick.at("url").isStr()) want.push_back({pick.at("url").asStr(), std::string(map) + mediaExt(pick.at("url").asStr())});
                }
        }
        if (want.empty()) return fail("no downloadable files for " + id);
        for (auto& [url, rel] : want) {
            if (rel.find("..") != std::string::npos || (!rel.empty() && rel[0] == '/')) return fail("refusing unsafe path in asset listing");
            std::string e = download(url, out + "/" + rel, 300);
            if (!e.empty()) return fail(e);
        }
        attribute(dir, id + ": Poly Haven — CC0 (public domain), https://polyhaven.com/a/" + id);
        printf("%s: %zu file%s in %s (CC0, no credit required)\n", id.c_str(), want.size(), want.size() == 1 ? "" : "s", out.c_str());
        return 0;
    }
    std::string name = slug(fs::path(a[0].substr(0, a[0].find_first_of("?#"))).filename().string());
    std::string ext = mediaExt(a[0]);
    if (ext.empty()) name += ".bin";
    std::string e = download(a[0], dir + "/" + name);
    if (!e.empty()) return fail(e);
    attribute(dir, name + ": " + (credit.empty() ? a[0] + " (check the licence on the source page before publishing)" : credit + " <" + a[0] + ">"));
    printf("%s/%s\n", dir.c_str(), name.c_str());
    return 0;
}

int assetFont(const std::vector<std::string>& a) {
    if (a.size() < 2) return fail("usage: kit asset font \"Family Name\" OUT_DIR [--weights 400,700]");
    std::string family = a[0], weights = "400;700";
    for (size_t i = 2; i + 1 < a.size(); i += 2) if (a[i] == "--weights") { weights = a[i + 1]; std::replace(weights.begin(), weights.end(), ',', ';'); }
    std::string enc;
    for (char c : family) enc += c == ' ' ? '+' : c;
    if (enc.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+") != std::string::npos) return fail("invalid family name");
    // A modern browser UA makes Google serve woff2 split by unicode range.
    auto css = [&](const std::string& spec) {
        SpawnResult r = run({"curl", "--disable", "-sSL", "--max-time", "20", "-A", kUA, "https://fonts.googleapis.com/css2?family=" + enc + spec + "&display=swap"}, 25000);
        return r.ok && r.exitCode == 0 && r.out.find("@font-face") != std::string::npos ? r.out : "";
    };
    std::string text = css(":wght@" + weights);
    if (text.empty()) text = css("");
    if (text.empty()) return fail("Google Fonts has no family \"" + family + "\" (or the weights do not exist)");
    std::error_code ec;
    fs::create_directories(a[1], ec);
    std::string outCss, subset;
    std::set<std::string> done;
    int files = 0;
    for (const auto& line : splitLines(text)) {
        std::string t = trim(line);
        if (startsWith(t, "/*") && endsWith(t, "*/")) { subset = trim(t.substr(2, t.size() - 4)); outCss += "/* " + subset + " */\n"; continue; }
        size_t u = t.find("url(");
        if (u != std::string::npos && (subset == "latin" || subset == "latin-ext")) {
            std::string url = t.substr(u + 4, t.find(')', u) - u - 4);
            std::string name = slug(fs::path(url).stem().string()) + "-" + subset + ".woff2";
            if (!done.count(url)) {
                std::string e = download(url, a[1] + "/" + name, 60);
                if (!e.empty()) return fail(e);
                done.insert(url); ++files;
            }
            outCss += "  src: url(" + name + ") format('woff2');\n";
        } else outCss += line + "\n";
    }
    // Drop blocks of subsets we did not download.
    std::string kept;
    for (size_t p = 0; p < outCss.size();) {
        size_t s = outCss.find("/* ", p);
        if (s == std::string::npos) break;
        size_t e = outCss.find("/* ", s + 3);
        std::string block = outCss.substr(s, e == std::string::npos ? std::string::npos : e - s);
        if (block.find("url(") != std::string::npos) kept += block;
        if (e == std::string::npos) break;
        p = e;
    }
    auto w = atomicWriteFile(a[1] + "/" + slug(family) + ".css", kept);
    if (!w.ok) return fail(w.error);
    attribute(a[1], family + ": Google Fonts — SIL Open Font License 1.1 (free for commercial use)");
    printf("%s: %d font file%s + %s/%s.css (font-family: '%s'); link it with <link rel=\"stylesheet\" href=\"...\">\n", family.c_str(), files,
           files == 1 ? "" : "s", a[1].c_str(), slug(family).c_str(), family.c_str());
    return 0;
}

// ---------------------------------------------------------------------------
// theme
// ---------------------------------------------------------------------------
double srgbEncode(double v) { return v <= .0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - .055; }
double srgbDecode(double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); }

bool oklch(double L, double C, double h, double rgb[3]) {
    double a = C * std::cos(h * M_PI / 180), b = C * std::sin(h * M_PI / 180);
    double l = L + .3963377774 * a + .2158037573 * b, m = L - .1055613458 * a - .0638541728 * b, s = L - .0894841775 * a - 1.2914855480 * b;
    l = l * l * l; m = m * m * m; s = s * s * s;
    double lin[3] = {4.0767416621 * l - 3.3077115913 * m + .2309699292 * s, -1.2684380046 * l + 2.6097574011 * m - .3413193965 * s,
                     -.0041960863 * l - .7034186147 * m + 1.7076147010 * s};
    bool in = true;
    for (int i = 0; i < 3; ++i) { in = in && lin[i] >= -1e-4 && lin[i] <= 1 + 1e-4; rgb[i] = std::clamp(lin[i], 0.0, 1.0); }
    return in;
}

double luminance(const std::string& hex) {
    double c[3];
    for (int i = 0; i < 3; ++i) c[i] = srgbDecode(std::stoi(hex.substr(1 + 2 * i, 2), nullptr, 16) / 255.0);
    return .2126 * c[0] + .7152 * c[1] + .0722 * c[2];
}

struct FontPair { const char *display, *text; };
// Distinctive free families; the Inter / Space Grotesk / Instrument Serif / Geist rotation is deliberately absent.
const FontPair kPairs[] = {
    {"Bricolage Grotesque", "Public Sans"}, {"Unbounded", "Figtree"}, {"Syne", "Karla"}, {"Big Shoulders Display", "Barlow"},
    {"Young Serif", "Hanken Grotesk"}, {"Anybody", "Atkinson Hyperlegible"}, {"Familjen Grotesk", "Literata"}, {"Gloock", "Work Sans"},
    {"Epilogue", "Crimson Pro"}, {"Schibsted Grotesk", "Bitter"}, {"Red Hat Display", "Red Hat Text"}, {"Darker Grotesque", "Spectral"},
    {"Onest", "Newsreader"}, {"Albert Sans", "Libre Caslon Text"},
};

}  // namespace

std::string oklchHex(double L, double C, double hue) {
    double rgb[3];
    while (!oklch(L, C, hue, rgb) && C > 0) C = std::max(0.0, C - .004);
    oklch(L, C, hue, rgb);
    char b[8];
    snprintf(b, sizeof b, "#%02x%02x%02x", int(std::lround(srgbEncode(rgb[0]) * 255)), int(std::lround(srgbEncode(rgb[1]) * 255)),
             int(std::lround(srgbEncode(rgb[2]) * 255)));
    return b;
}
double contrastRatio(const std::string& x, const std::string& y) {
    double a = luminance(x), b = luminance(y);
    return (std::max(a, b) + .05) / (std::min(a, b) + .05);
}

int kitTheme(const std::vector<std::string>& a) {
    const char* usage = "usage: kit theme \"topic words\" [--mode dark|light] [--seed N]";
    if (a.empty() || a[0] == "--help") { puts(usage); return a.empty() ? 2 : 0; }
    std::string topic = a[0], mode;
    double seed = 0;
    for (size_t i = 1; i + 1 < a.size(); i += 2) {
        if (a[i] == "--mode") mode = a[i + 1];
        else if (a[i] == "--seed") { if (!number(a[i + 1], seed)) return fail("invalid seed"); }
        else return fail(usage);
    }
    uint32_t h = 2166136261u;
    for (unsigned char c : toLower(topic) + std::to_string((long)seed)) h = (h ^ c) * 16777619u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    // Accent arcs exclude indigo->magenta (255-345) and the terracotta band (25-70): the two learned defaults.
    struct Arc { double lo, hi, lift; const char* name; };
    static const Arc arcs[] = {{14, 32, -.06, "red"}, {88, 125, .08, "lime"}, {140, 195, 0, "green-teal"}, {205, 250, 0, "cyan-blue"}};
    const int pick = int(h % 4);
    const Arc& arc = arcs[pick];
    auto inArc = [](const Arc& r, uint32_t bits) { return std::fmod(r.lo + (r.hi - r.lo) * (bits % 1000) / 1000.0, 360); };
    const double hue = inArc(arc, h >> 8);
    // Second accent: the allowed arc farthest around the wheel from the first.
    int far = 0;
    double best = -1;
    for (int i = 0; i < 4; ++i) {
        double d = std::fabs(std::fmod(std::fabs(std::fmod((arcs[i].lo + arcs[i].hi) / 2, 360) - hue), 360));
        d = std::min(d, 360 - d);
        if (i != pick && d > best) best = d, far = i;
    }
    const double h2 = inArc(arcs[far], h >> 18);
    if (mode.empty()) mode = (h >> 20) & 1 ? "dark" : "light";
    if (mode != "dark" && mode != "light") return fail(usage);
    const bool dark = mode == "dark";
    // Ground: a tinted neutral (cool-tinted white in light mode, never cream).
    std::string paper = dark ? oklchHex(.19, .022, hue) : oklchHex(.965, .009, 235);
    std::string ink = dark ? oklchHex(.95, .008, hue) : oklchHex(.2, .02, hue);
    std::string muted = dark ? oklchHex(.72, .025, hue) : oklchHex(.45, .025, hue);
    std::string line = dark ? oklchHex(.32, .025, hue) : oklchHex(.86, .012, hue);
    double al = (dark ? .74 : .5) + arc.lift;
    std::string accent = oklchHex(al, .17, hue);
    for (int i = 0; i < 20 && contrastRatio(accent, paper) < 4.5; ++i) accent = oklchHex(al += dark ? .02 : -.02, .17, hue);
    double l2 = (dark ? .72 : .5) + arcs[far].lift;
    std::string accent2 = oklchHex(l2, .14, h2);
    for (int i = 0; i < 20 && contrastRatio(accent2, paper) < 3; ++i) accent2 = oklchHex(l2 += dark ? .02 : -.02, .14, h2);
    const FontPair& fp = kPairs[((h >> 4) * 2654435761u >> 16) % (sizeof kPairs / sizeof *kPairs)];
    printf("/* theme for \"%s\": %s ground, accent hue %.0f° (%s arc; state why it fits the subject, or pass --seed to re-roll) */\n", topic.c_str(),
           mode.c_str(), hue, arc.name);
    printf(":root{--paper:%s;--ink:%s;--muted:%s;--line:%s;--accent:%s;--accent-2:%s;--display:'%s',sans-serif;--text:'%s',sans-serif;}\n",
           paper.c_str(), ink.c_str(), muted.c_str(), line.c_str(), accent.c_str(), accent2.c_str(), fp.display, fp.text);
    printf("/* contrast: ink on paper %.1f:1, accent on paper %.1f:1, muted on paper %.1f:1 */\n", contrastRatio(ink, paper), contrastRatio(accent, paper),
           contrastRatio(muted, paper));
    printf("/* fonts to fetch: kit asset font \"%s\" fonts/ ; kit asset font \"%s\" fonts/ . Derived from the topic words only: the subject's own vocabulary decides the final look. */\n",
           fp.display, fp.text);
    return 0;
}

// ---------------------------------------------------------------------------
// vsheet
// ---------------------------------------------------------------------------
int kitVsheet(const std::vector<std::string>& a) {
    const char* usage = "usage: kit vsheet VIDEO OUT.png [--n 12] [--cols 4] [--width 480]   contact sheet: review a whole video in one image";
    if (a.size() < 2 || a[0] == "--help") { puts(usage); return a.empty() || a[0] != "--help" ? 2 : 0; }
    double n = 12, cols = 4, width = 480;
    for (size_t i = 2; i + 1 < a.size(); i += 2) {
        double* v = a[i] == "--n" ? &n : a[i] == "--cols" ? &cols : a[i] == "--width" ? &width : nullptr;
        if (!v || !number(a[i + 1], *v)) return fail(usage);
    }
    if (n < 2 || n > 48 || cols < 1 || cols > 8 || width < 120 || width > 960 || std::floor(n) != n || std::floor(cols) != cols) return fail("n 2..48, cols 1..8, width 120..960");
    if (!endsWith(toLower(a[1]), ".png")) return fail("output must end in .png");
    const std::string ffprobe = whichExe("ffprobe"), ffmpeg = whichExe("ffmpeg");
    if (ffprobe.empty() || ffmpeg.empty()) return fail("FFmpeg is needed");
    SpawnResult d = run({ffprobe, "-v", "error", "-select_streams", "v:0", "-show_entries", "stream=nb_frames,r_frame_rate:format=duration",
                         "-of", "json", a[0]}, 30000, 1 << 16);
    auto info = json::parse(d.out);
    double dur = info.ok ? atof(info.value.at("format").at("duration").asStr().c_str()) : 0;
    if (!d.ok || d.exitCode != 0 || !info.ok || dur <= 0) return fail("cannot read the video duration: " + trim(d.err.substr(0, 200)));
    const auto& st = info.value.at("streams").at(size_t(0));
    double fps = 30;
    if (std::string rate = st.at("r_frame_rate").asStr(); rate.find('/') != std::string::npos)
        fps = atof(rate.c_str()) / std::max(1.0, atof(rate.c_str() + rate.find('/') + 1));
    long frames = st.at("nb_frames").isStr() ? atol(st.at("nb_frames").asStr().c_str()) : 0;
    if (frames <= 0) frames = std::lround(dur * fps);
    const int rows = int(std::ceil(n / cols));
    // Pick frames by index and stamp each with its own presentation time: no resampling filter to disagree about the clock.
    std::string pick;
    long firstIdx = 0;
    for (int i = 0; i < int(n); ++i) {
        const long k = std::min(frames - 1, long((i + .5) * frames / n));
        if (!i) firstIdx = k;
        pick += (i ? "+eq(n\\," : "eq(n\\,") + std::to_string(k) + ")";
    }
    const double first = double(firstIdx) / fps;
    auto build = [&](bool stamp) {
        return "select='" + pick + "',scale=" + std::to_string(int(width)) + ":-2" +
               (stamp ? ",drawtext=text='%{pts\\:hms}':x=8:y=8:fontsize=h/12:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=4" : "") +
               ",tile=" + std::to_string(int(cols)) + "x" + std::to_string(rows) + ":padding=6:margin=6:color=0x101010";
    };
    SpawnResult r;
    for (bool stamp : {true, false}) {
        r = run({ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y", "-i", a[0], "-vf", build(stamp), "-fps_mode", "passthrough", "-frames:v", "1", a[1]}, 300000, 1 << 16);
        if (r.ok && r.exitCode == 0) break;
    }
    if (!r.ok || r.exitCode != 0) return fail("contact sheet failed: " + trim(r.err.substr(0, 300)));
    printf("%s: %d frames, %d columns, one every %.2fs (first at %.2fs); read it as an image\n", a[1].c_str(), int(n), int(cols), dur / n, first);
    return 0;
}

// ---------------------------------------------------------------------------
// script parsing (shared with tests)
// ---------------------------------------------------------------------------
std::vector<std::vector<std::vector<Utterance>>> scriptSentences(const std::string& script) {
    std::vector<std::vector<std::vector<Utterance>>> beats;
    std::vector<std::vector<Utterance>> sentences;
    std::vector<Utterance> cur;
    auto endSentence = [&] { if (!cur.empty()) sentences.push_back(std::move(cur)); cur.clear(); };
    auto endBeat = [&] { endSentence(); if (!sentences.empty()) beats.push_back(std::move(sentences)); sentences.clear(); };
    std::string text;
    for (char c : script) if (c != '*' && c != '`' && c != '\r') text += c;
    for (size_t p = 0; p < text.size();) {
        if (text[p] == '\n') {
            size_t q = p;
            int newlines = 0;
            while (q < text.size() && (text[q] == '\n' || text[q] == ' ' || text[q] == '\t')) newlines += text[q] == '\n', ++q;
            if (newlines >= 2) endBeat();
            p = q;
            continue;
        }
        if (text[p] == ' ' || text[p] == '\t') { ++p; continue; }
        Utterance u;
        if (text[p] == '{') {
            size_t close = text.find('}', p), bar = text.find('|', p);
            if (close != std::string::npos && bar != std::string::npos && bar < close) {
                u.display = trim(text.substr(p + 1, bar - p - 1));
                u.spoken = trim(text.substr(bar + 1, close - bar - 1));
                p = close + 1;
                while (p < text.size() && !isspace((unsigned char)text[p])) u.display += text[p], u.spoken += text[p], ++p;  // trailing punctuation
                if (u.display.empty()) continue;
                cur.push_back(u);
                if (sentenceEnd(u.display)) endSentence();
                continue;
            }
        }
        size_t q = p;
        while (q < text.size() && !isspace((unsigned char)text[q])) ++q;
        u.display = u.spoken = text.substr(p, q - p);
        p = q;
        cur.push_back(u);
        if (sentenceEnd(u.display)) endSentence();
    }
    endBeat();
    return beats;
}

int kitAsset(const std::vector<std::string>& a) {
    const char* usage =
        "usage: kit asset search QUERY [--kind image|audio|model|hdri|texture] [-n 8]   open-licensed assets (Openverse, Poly Haven CC0)\n"
        "       kit asset get URL|polyhaven:ID|three OUT_DIR [--res 1k] [--credit TXT]  download; writes ATTRIBUTION.txt\n"
        "       kit asset font \"Family\" OUT_DIR [--weights 400,700]                     Google Font as local woff2 + css";
    if (a.empty() || a[0] == "--help") { puts(usage); return a.empty() ? 2 : 0; }
    std::vector<std::string> rest(a.begin() + 1, a.end());
    if (a[0] == "search") return assetSearch(rest);
    if (a[0] == "get") return assetGet(rest);
    if (a[0] == "font") return assetFont(rest);
    return fail(usage);
}

int kitSay(const std::vector<std::string>& args) { return say(args); }

}  // namespace pocket
