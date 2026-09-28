// Music, mixing, loudness, narration scripts and the video-work gate.
#include "mini.h"

#include <cmath>
#include <filesystem>

#include "../src/kit_media.h"
#include "../src/kit_studio.h"

using namespace pocket;
using namespace pocket::test;

namespace {
struct Dir {
    std::string path;
    Dir() {
        char t[] = "/tmp/pocket-studio-XXXXXX";
        path = mkdtemp(t) ? t : "";
    }
    ~Dir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    std::string at(const std::string& n) const { return path + "/" + n; }
};
Stereo sine(double hz, double amp, double seconds) {
    Stereo s;
    const size_t n = size_t(seconds * kStudioRate);
    s.l.resize(n); s.r.resize(n);
    for (size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = float(amp * std::sin(2 * M_PI * hz * double(i) / kStudioRate));
    return s;
}
}  // namespace

TEST(studio_Loudness_Follows_BS1770_Calibration) {
    // A stereo 997 Hz sine at -20 dBFS peak is -20 LUFS (both channels sum), and silence has no loudness.
    CHECK(std::fabs(integratedLufs(sine(997, .1, 6)) + 20.0) < .3);
    Stereo quiet = sine(997, 0, 3);
    CHECK(integratedLufs(quiet) < -900);
    return "";
}

TEST(studio_Limiter_Holds_The_Ceiling_And_Spares_Quiet_Audio) {
    Stereo loud = sine(220, 1.8, 1);
    limitPeak(loud, -1.0);
    CHECK(peakDb(loud) <= -0.99);
    Stereo soft = sine(220, .3, 1), copy = soft;
    limitPeak(soft, -1.0);
    CHECK(soft.l == copy.l);
    return "";
}

TEST(studio_Music_Is_Deterministic_Loudness_Normalised_And_Validated) {
    Dir d;
    CHECK(kitMusic({d.at("a.wav"), "--style", "lofi", "--duration", "12", "--seed", "7"}) == 0);
    CHECK(kitMusic({d.at("b.wav"), "--style", "lofi", "--duration", "12", "--seed", "7"}) == 0);
    CHECK(kitMusic({d.at("c.wav"), "--style", "lofi", "--duration", "12", "--seed", "8"}) == 0);
    auto a = readFileBounded(d.at("a.wav"), 1 << 26), b = readFileBounded(d.at("b.wav"), 1 << 26), c = readFileBounded(d.at("c.wav"), 1 << 26);
    CHECK(a.ok && b.ok && c.ok && a.value == b.value && a.value != c.value);
    auto song = loadAudio(d.at("a.wav"));
    CHECK(song.ok && song.value.size() == 12u * kStudioRate);
    CHECK(std::fabs(integratedLufs(song.value) + 16.0) < .6 && peakDb(song.value) <= -0.9);
    for (const char* style : {"ambient", "corporate", "cinematic", "tech", "upbeat"})
        CHECK(kitMusic({d.at("s.wav"), "--style", style, "--duration", "8"}) == 0);
    CHECK(kitMusic({d.at("x.wav"), "--style", "polka"}) != 0);
    CHECK(kitMusic({d.at("x.wav"), "--duration", "2"}) != 0);
    CHECK(kitMusic({d.at("x.wav"), "--key", "H"}) != 0);
    CHECK(!std::filesystem::exists(d.at("x.wav")));
    return "";
}

TEST(studio_Mix_Reaches_Target_Loudness_Under_The_Ceiling) {
    Dir d;
    CHECK(saveStereoWav(d.at("voice.wav"), sine(300, .2, 4)).ok);
    CHECK(kitMusic({d.at("music.wav"), "--style", "corporate", "--duration", "6", "--seed", "3"}) == 0);
    CHECK(saveStereoWav(d.at("hit.wav"), sine(1200, .5, .1)).ok);
    CHECK(kitMix({d.at("out.wav"), "--voice", d.at("voice.wav"), "--music", d.at("music.wav"), "--at", "1.0:" + d.at("hit.wav") + ":-3",
                  "--duration", "6", "--lufs", "-14", "--ceiling", "-1.5"}) == 0);
    auto mix = loadAudio(d.at("out.wav"));
    CHECK(mix.ok && mix.value.size() == 6u * kStudioRate);
    CHECK(std::fabs(integratedLufs(mix.value) + 14.0) < .5);
    CHECK(peakDb(mix.value) <= -1.45);
    CHECK(kitMix({d.at("o2.wav")}) != 0);                                              // nothing to mix
    CHECK(kitMix({d.at("o2.wav"), "--voice", d.at("nope.wav")}) != 0);                 // missing input
    CHECK(kitMix({d.at("o2.wav"), "--voice", d.at("voice.wav"), "--lufs", "0"}) != 0); // out of range
    CHECK(!std::filesystem::exists(d.at("o2.wav")));
    return "";
}

TEST(studio_Script_Splits_Beats_Sentences_And_Pronunciation) {
    auto beats = scriptSentences("Hello there. Is this e.g. fine?\n\nSecond {TTS|tee tee ess} beat.\nStill second, Dr. Who arrives!");
    CHECK(beats.size() == 2);
    CHECK(beats[0].size() == 2 && beats[0][0].size() == 2);          // "Hello there."
    CHECK(beats[0][1].size() == 4);                                 // "Is this e.g. fine?" is one sentence
    CHECK(beats[1].size() == 2);
    CHECK(beats[1][0][1].display == "TTS" && beats[1][0][1].spoken == "tee tee ess");
    CHECK(beats[1][1].size() == 5);                                 // "Dr." does not end a sentence
    CHECK(scriptSentences("  \n\n ").empty());
    return "";
}

TEST(studio_Colour_Maths_Matches_Known_Values) {
    CHECK(oklchHex(1, 0, 0) == "#ffffff" && oklchHex(0, 0, 0) == "#000000");
    CHECK(std::fabs(contrastRatio("#000000", "#ffffff") - 21) < .01);
    CHECK(contrastRatio("#777777", "#ffffff") < 4.6 && contrastRatio("#767676", "#ffffff") >= 4.5);
    std::string vivid = oklchHex(.7, .5, 145);  // out of gamut: chroma is reduced, never garbage
    CHECK(vivid.size() == 7 && vivid[0] == '#');
    return "";
}
