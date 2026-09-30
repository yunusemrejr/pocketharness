// Music, mixing, loudness, narration scripts and the video-work gate.
#include "mini.h"

#include <cmath>
#include <filesystem>
#include <limits>

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
void putLe(std::string& s, size_t at, uint32_t v, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) s[at + i] = char(v >> (8 * i));
}
std::string monoWav(int rate, double hz, double seconds) {
    const size_t n = size_t(rate * seconds);
    std::string wav(44 + n * 2, '\0');
    wav.replace(0, 4, "RIFF"); wav.replace(8, 8, "WAVEfmt "); wav.replace(36, 4, "data");
    putLe(wav, 4, uint32_t(wav.size() - 8), 4); putLe(wav, 16, 16, 4); putLe(wav, 20, 1, 2);
    putLe(wav, 22, 1, 2); putLe(wav, 24, rate, 4); putLe(wav, 28, rate * 2, 4);
    putLe(wav, 32, 2, 2); putLe(wav, 34, 16, 2); putLe(wav, 40, uint32_t(n * 2), 4);
    for (size_t i = 0; i < n; ++i)
        putLe(wav, 44 + i * 2, uint16_t(int16_t(std::lround(.2 * 32767 * std::sin(2 * M_PI * hz * i / rate)))), 2);
    return wav;
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

TEST(studio_Wav_Rejects_Corruption_Instead_Of_Concealing_It) {
    Dir d;
    const auto valid = monoWav(48000, 997, 1);
    for (int kind = 0; kind < 9; ++kind) {
        std::string wav = valid;
        if (kind == 0) putLe(wav, 32, 99, 2);                     // invalid alignment
        if (kind == 1) putLe(wav, 28, 0, 4);                      // invalid byte rate
        if (kind == 2) putLe(wav, 4, uint32_t(wav.size()), 4);     // truncated RIFF
        if (kind == 3) putLe(wav, 16, 1000000, 4);                // truncated chunk
        if (kind == 4) putLe(wav, 40, 12345, 4);                  // incomplete sample frame
        if (kind == 5) putLe(wav, 22, 0, 2);                      // no channels
        if (kind == 6) {                                         // duplicate fmt
            wav.insert(36, wav.substr(12, 24)); putLe(wav, 4, uint32_t(wav.size() - 8), 4);
        }
        if (kind == 7) {                                         // malformed extensible GUID
            wav.insert(36, 24, '\0'); putLe(wav, 4, uint32_t(wav.size() - 8), 4);
            putLe(wav, 16, 40, 4); putLe(wav, 20, 0xfffe, 2); putLe(wav, 36, 22, 2);
        }
        if (kind == 8) {                                         // non-finite floating point sample
            putLe(wav, 20, 3, 2); putLe(wav, 28, 192000, 4);
            putLe(wav, 32, 4, 2); putLe(wav, 34, 32, 2); putLe(wav, 44, 0x7fc00000u, 4);
        }
        CHECK(atomicWriteFile(d.at("bad.wav"), wav).ok);
        CHECK(!loadAudio(d.at("bad.wav")).ok);
    }
    Stereo invalid = sine(997, .1, .1);
    invalid.r.pop_back();
    CHECK(!saveStereoWav(d.at("out.wav"), invalid).ok);
    invalid.r = invalid.l; invalid.l[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!saveStereoWav(d.at("out.wav"), invalid).ok);
    CHECK(!std::filesystem::exists(d.at("out.wav")));
    return "";
}

TEST(studio_Resampling_Rejects_Ultrasonic_Aliases_And_Preserves_Audible_Pitch) {
    Dir d;
    CHECK(atomicWriteFile(d.at("high.wav"), monoWav(96000, 32000, .5)).ok);
    CHECK(atomicWriteFile(d.at("low.wav"), monoWav(96000, 12000, .5)).ok);
    auto high = loadAudio(d.at("high.wav")), low = loadAudio(d.at("low.wav"));
    CHECK(high.ok && low.ok && high.value.size() == 24000 && low.value.size() == 24000);
    auto rms = [](const Stereo& s) {
        double energy = 0;
        for (size_t i = 100; i < s.size() - 100; ++i) energy += s.l[i] * s.l[i];
        return std::sqrt(energy / (s.size() - 200));
    };
    CHECK(rms(high.value) < .0002);  // >57 dB rejection vs the original .141 RMS alias
    CHECK(std::fabs(rms(low.value) - .2 / std::sqrt(2.0)) < .003);
    return "";
}

TEST(studio_Mix_Covers_All_Tracks_And_Normalises_Short_Cues) {
    Dir d;
    CHECK(saveStereoWav(d.at("music.wav"), sine(997, .1, 2)).ok);
    CHECK(saveStereoWav(d.at("cue.wav"), sine(440, .1, .1)).ok);
    CHECK(kitMix({d.at("out.wav"), "--music", d.at("music.wav"), "--at", "0:" + d.at("cue.wav")}) == 0);
    auto all = loadAudio(d.at("out.wav"));
    CHECK(all.ok && all.value.size() == 2u * kStudioRate);
    CHECK(kitMix({d.at("short.wav"), "--at", "0:" + d.at("cue.wav"), "--fade-out", "0"}) == 0);
    auto cue = loadAudio(d.at("short.wav"));
    CHECK(cue.ok && cue.value.size() == 4800);
    CHECK(std::fabs(integratedLufs(cue.value) + 14) < .1 && peakDb(cue.value) <= -1.49);
    CHECK(kitMix({d.at("trim.wav"), "--music", d.at("music.wav"), "--duration", "1"}) == 0);
    CHECK(loadAudio(d.at("trim.wav")).value.size() == size_t(kStudioRate));
    for (const auto& options : {std::vector<std::string>{"--voice-at", "1e300"}, {"--music-at", "1e300"},
                               {"--music-db", "1000"}, {"--at", "1e300:" + d.at("cue.wav")},
                               {"--at", "0:" + d.at("cue.wav") + ":1000"}}) {
        std::vector<std::string> args = {d.at("bad.wav"), "--music", d.at("music.wav")};
        args.insert(args.end(), options.begin(), options.end());
        CHECK(kitMix(args) != 0);
        CHECK(!std::filesystem::exists(d.at("bad.wav")));
    }
    CHECK(kitMusic({d.at("bad.wav"), "--key", "Cjunk"}) != 0);
    return "";
}

TEST(studio_Mix_Trims_Input_Before_Placement_And_Fades_In) {
    Dir d;
    Stereo source = sine(997, .1, 2);
    // The first half is silent. Trimming selects the actual recording instead of its lead-in.
    std::fill(source.l.begin(), source.l.begin() + kStudioRate, 0);
    std::fill(source.r.begin(), source.r.begin() + kStudioRate, 0);
    CHECK(saveStereoWav(d.at("source.wav"), source).ok);
    CHECK(kitMix({d.at("edit.wav"), "--voice", d.at("source.wav"), "--voice-trim", "1:2", "--voice-at", ".2",
                  "--fade-in", ".3", "--fade-out", "0"}) == 0);
    auto edit = loadAudio(d.at("edit.wav"));
    CHECK(edit.ok && edit.value.size() == 57600);
    for (size_t i = 0; i < 9600; ++i) CHECK(edit.value.l[i] == 0 && edit.value.r[i] == 0);
    CHECK(std::fabs(integratedLufs(edit.value) + 14) < .1);
    CHECK(peakDb(edit.value) <= -1.49);
    CHECK(kitMix({d.at("bad.wav"), "--voice", d.at("source.wav"), "--voice-trim", "2:1"}) != 0);
    CHECK(kitMix({d.at("bad.wav"), "--voice", d.at("source.wav"), "--voice-trim", "0:3"}) != 0);
    CHECK(kitMix({d.at("bad.wav"), "--music", d.at("source.wav"), "--voice-trim", "0:1"}) != 0);
    CHECK(!std::filesystem::exists(d.at("bad.wav")));
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
