#include "mini.h"

#include <bit>
#include <cmath>
#include <limits>

#include "../src/kit_audio.h"

using namespace pocket;
using namespace pocket::test;

namespace {
void setLe(std::string& s, size_t p, uint32_t n, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) s[p + i] = static_cast<char>((n >> (i * 8)) & 255);
}

// Independent fixture encoder: interleaved PCM/float, with an odd padded JUNK chunk.
std::string fixture(unsigned format, unsigned bits, unsigned channels, const std::vector<uint32_t>& samples) {
    const unsigned align = channels * bits / 8;
    std::string out(54 + samples.size() * bits / 8 + ((samples.size() * bits / 8) & 1), '\0');
    out.replace(0, 4, "RIFF"); setLe(out, 4, out.size() - 8, 4); out.replace(8, 8, "WAVEJUNK");
    setLe(out, 16, 1, 4); out[20] = 'x'; out.replace(22, 4, "fmt "); setLe(out, 26, 16, 4);
    setLe(out, 30, format, 2); setLe(out, 32, channels, 2); setLe(out, 34, 44100, 4);
    setLe(out, 38, 44100 * align, 4); setLe(out, 42, align, 2); setLe(out, 44, bits, 2);
    out.replace(46, 4, "data"); setLe(out, 50, samples.size() * bits / 8, 4);
    for (size_t i = 0; i < samples.size(); ++i) setLe(out, 54 + i * bits / 8, samples[i], bits / 8);
    return out;
}
}  // namespace

TEST(audio_Strict_Notes_And_Bounded_Rendering) {
    for (const std::string bad : {"440junk", "A4junk", "A-", "A999999999999999", "A#", "nan", "inf", "0", "-20", "A4.2"})
        CHECK(noteFreq(bad) < 0);
    CHECK(std::fabs(noteFreq("Bb4") - noteFreq("A#4")) < 1e-9);
    for (const std::string bad : {"", "  ", ":.5", "Rabbit:.5", "C4:nan", "C4:inf", "C4:1junk", "C4:-1", "C4:0", "C4:1:2", "30000:.1"})
        CHECK(!synthNotes(bad).ok);
    CHECK(!synthNotes("C4:60 C4:60").ok);
    CHECK(!synthNotes("C4", "unknown").ok);
    CHECK(!synthNotes("C4", "sine", 0, std::numeric_limits<double>::quiet_NaN()).ok);
    CHECK(!synthNotes("C4", "sine", -1).ok);
    std::string many;
    for (int i = 0; i < 4097; ++i) many += "A4:.0001 ";
    CHECK(!synthNotes(many).ok);
    const auto wav = synthNotes("A4:1,\tR:.5\nA4:.5", "sine", 120, .5);
    CHECK(wav.ok && wav.value.size() == 44 + 44100 * 2);
    const auto stats = analyzeWav(wav.value);
    CHECK(stats.ok && stats.value.frames == 44100 && stats.value.channels == 1);
    CHECK(stats.value.clippedSamples == 0 && stats.value.peak > .49 && stats.value.peak < .501);
    CHECK(std::fabs(stats.value.pitchHz - 440) < 1);
    CHECK(wav.value[44] == 0 && wav.value[45] == 0 && wav.value.back() == 0);
    return "";
}

TEST(audio_Presets_Are_Deterministic_And_Quiet_At_Endpoints) {
    CHECK(!soundPreset("missing").ok);
    for (const std::string preset : {"click", "chime", "laser", "whoosh", "impact", "tone", "noise"}) {
        const auto spec = soundPreset(preset);
        CHECK(spec.ok);
        const auto a = synthSound(spec.value), b = synthSound(spec.value);
        CHECK(a.ok && b.ok && a.value == b.value);
        CHECK(a.value[44] == 0 && a.value[45] == 0 && a.value[a.value.size() - 2] == 0 && a.value.back() == 0);
        const auto stats = analyzeWav(a.value);
        CHECK(stats.ok && stats.value.clippedSamples == 0 && stats.value.peak > .01 && stats.value.rms > .001);
        CHECK(stats.value.peak <= spec.value.gain + 1e-4);
    }
    SoundSpec noise = soundPreset("noise").value;
    const auto first = synthSound(noise);
    ++noise.seed;
    CHECK(synthSound(noise).value != first.value);
    noise.seed = 0;
    CHECK(analyzeWav(synthSound(noise).value).value.rms > .01);
    noise.gain = 0;
    const auto silent = analyzeWav(synthSound(noise).value);
    CHECK(silent.ok && silent.value.peak == 0 && silent.value.rms == 0 && silent.value.pitchHz == 0);
    CHECK(std::fabs(silent.value.silenceSeconds - noise.duration) < 1.0 / noise.rate);
    return "";
}

TEST(audio_Synth_Rejects_Nonfinite_And_Excessive_Arguments) {
    SoundSpec s;
    s.duration = std::numeric_limits<double>::infinity(); CHECK(!synthSound(s).ok);
    s.duration = std::numeric_limits<double>::quiet_NaN(); CHECK(!synthSound(s).ok);
    s.duration = 60.01; CHECK(!synthSound(s).ok);
    s.duration = .1; s.rate = 0; CHECK(!synthSound(s).ok);
    s.rate = 96001; CHECK(!synthSound(s).ok);
    s.rate = 44100; s.frequency = 22050; CHECK(!synthSound(s).ok);
    s.frequency = 440; s.endFrequency = -1; CHECK(!synthSound(s).ok);
    s.endFrequency = 440; s.gain = 1.01; CHECK(!synthSound(s).ok);
    s.gain = .3; s.noise = -1; CHECK(!synthSound(s).ok);
    s.noise = 0; s.attack = -1; CHECK(!synthSound(s).ok);
    s.attack = 0; s.lowpass = 1e10; CHECK(!synthSound(s).ok);
    return "";
}

TEST(audio_Filter_And_Pitch_Estimate_Have_Measurable_Effects) {
    SoundSpec noise = soundPreset("noise").value;
    const auto unfiltered = analyzeWav(synthSound(noise).value);
    noise.lowpass = 500;
    const auto filtered = analyzeWav(synthSound(noise).value);
    CHECK(unfiltered.ok && filtered.ok && filtered.value.rms < unfiltered.value.rms * .5);
    for (const int rate : {8000, 44100, 96000}) {
        for (const double hz : {50.0, 110.0, 440.0, 1800.0}) {
            SoundSpec tone; tone.rate = rate; tone.duration = .6; tone.frequency = tone.endFrequency = hz;
            const auto stats = analyzeWav(synthSound(tone).value);
            CHECK(stats.ok && std::fabs(stats.value.pitchHz - hz) < hz * .01);
        }
    }
    return "";
}

TEST(audio_Analyzes_Channel_Energy_Without_Phase_Cancellation) {
    std::vector<uint32_t> stereo;
    for (int i = 0; i < 4410; ++i) {
        const int v = std::lround(16000 * std::sin(2 * 3.14159265358979323846 * 440 * i / 44100));
        stereo.push_back(static_cast<uint16_t>(v)); stereo.push_back(static_cast<uint16_t>(-v));
    }
    const auto stats = analyzeWav(fixture(1, 16, 2, stereo));
    CHECK(stats.ok && stats.value.channels == 2 && stats.value.frames == 4410);
    CHECK(stats.value.peak > .48 && stats.value.rms > .34 && stats.value.silenceSeconds == 0);
    CHECK(std::fabs(stats.value.pitchHz - 440) < 1);
    return "";
}

TEST(audio_PCM_And_Float_Formats_Have_Correct_Sign_And_Scale) {
    for (unsigned bits : {8, 16, 24, 32}) {
        const uint32_t half = uint32_t(1) << (bits - 1);
        std::vector<uint32_t> raw = bits == 8 ? std::vector<uint32_t>{64, 192} : std::vector<uint32_t>{half / 2, half + half / 2};
        const auto stats = analyzeWav(fixture(1, bits, 1, raw));
        CHECK(stats.ok && stats.value.frames == 2 && stats.value.bits == int(bits));
        CHECK(std::fabs(stats.value.peak - .5) < 1e-9 && std::fabs(stats.value.rms - .5) < 1e-9);
    }
    const auto floats = analyzeWav(fixture(3, 32, 1, {std::bit_cast<uint32_t>(.5f), std::bit_cast<uint32_t>(-.5f)}));
    CHECK(floats.ok && floats.value.rms == .5 && floats.value.peak == .5);
    CHECK(!analyzeWav(fixture(3, 32, 1, {0x7fc00000})).ok);
    CHECK(!analyzeWav(fixture(3, 32, 1, {0x7f800000})).ok);
    CHECK(!analyzeWav(fixture(3, 32, 1, {std::bit_cast<uint32_t>(1e9f)})).ok);
    const auto clipped = analyzeWav(fixture(1, 16, 1, {0x7fff, 0x8000, 0}));
    CHECK(clipped.ok && clipped.value.clippedSamples == 2);
    return "";
}

TEST(audio_Rejects_Truncated_Chunks_And_Dangerous_Formats) {
    const std::string good = fixture(1, 16, 1, {0, 1, 2, 3});
    for (size_t n = 0; n < good.size(); ++n) CHECK(!analyzeWav(good.substr(0, n)).ok);
    for (const auto [offset, value] : std::vector<std::pair<size_t, uint32_t>>{
            {4, 0xffffffff}, {16, 0xffffffff}, {26, 0}, {26, 2}, {34, 0}, {34, 1}, {34, 9},
            {34, 0xffffffff}, {38, 0}, {50, 0xffffffff}, {50, 1}}) {
        std::string bad = good; setLe(bad, offset, value, 4); CHECK(!analyzeWav(bad).ok);
    }
    for (const auto [offset, value] : std::vector<std::pair<size_t, uint32_t>>{
            {30, 2}, {30, 3}, {32, 0}, {32, 33}, {42, 0}, {42, 3}, {44, 0}, {44, 12}}) {
        std::string bad = good; setLe(bad, offset, value, 2); CHECK(!analyzeWav(bad).ok);
    }
    std::string fmtOnly = good.substr(0, 46); setLe(fmtOnly, 4, fmtOnly.size() - 8, 4);
    CHECK(!analyzeWav(fmtOnly).ok);
    const auto empty = analyzeWav(fixture(1, 16, 1, {}));
    CHECK(empty.ok && empty.value.frames == 0 && empty.value.rms == 0);
    // A short fmt chunk at end used to cause an out-of-bounds field read.
    std::string shortFmt(20, '\0');
    shortFmt.replace(0, 4, "RIFF"); setLe(shortFmt, 4, 12, 4); shortFmt.replace(8, 8, "WAVEfmt ");
    CHECK(!analyzeWav(shortFmt).ok);
    return "";
}

TEST(audio_Extensible_PCM_Has_Checked_GUID_And_Valid_Bits) {
    std::string wav = fixture(1, 24, 1, {0x400000, 0xc00000});
    std::string extra(24, '\0');
    setLe(extra, 0, 22, 2); setLe(extra, 2, 24, 2); setLe(extra, 8, 1, 2);
    extra[14] = '\x10'; extra[16] = '\x80'; extra[19] = '\xaa'; extra[21] = '\x38'; extra[22] = '\x9b'; extra[23] = '\x71';
    wav.insert(46, extra); setLe(wav, 4, wav.size() - 8, 4); setLe(wav, 26, 40, 4); setLe(wav, 30, 0xfffe, 2);
    const auto stats = analyzeWav(wav);
    CHECK(stats.ok && stats.value.rms == .5 && stats.value.bits == 24);
    std::string bad = wav; bad[69] = 0; CHECK(!analyzeWav(bad).ok);
    bad = wav; setLe(bad, 48, 20, 2); CHECK(!analyzeWav(bad).ok);
    return "";
}

TEST(audio_CLI_Invalid_Options_Do_Not_Overwrite_Output) {
    const std::string dir = makeTempDir("pocket-audio");
    CHECK(!dir.empty());
    struct Cleanup { std::string p; ~Cleanup() { rmRf(p); } } cleanup{dir};
    const std::string file = dir + "/cue.wav";
    CHECK(atomicWriteFile(file, "untouched").ok);
    for (const std::vector<std::string>& args : {
            std::vector<std::string>{file, "chime", "--gain"}, {file, "chime", "--other", "1"},
            {file, "chime", "--gain", "nan"}, {file, "chime", "--seed", "-1"},
            {file, "chime", "--seed", "4294967296"}, {file, "chime", "--rate", "44100.5"},
            {file, "chime", "--duration", "1junk"}}) {
        CHECK(kitSfx(args) != 0); CHECK(readFileBounded(file, 100).value == "untouched");
    }
    CHECK(kitWav({file, "A4", "--gain"}) != 0);
    CHECK(kitWav({file, "A4", "--bpm", "0"}) != 0);
    CHECK(kitWav({file, "A4", "--gain", ".4junk"}) != 0);
    CHECK(kitWav({file, "A4", "--unknown", "1"}) != 0);
    CHECK(readFileBounded(file, 100).value == "untouched");
    CHECK(kitSfx({file, "chime", "--duration", ".2", "--freq", "660", "--seed", "42"}) == 0);
    CHECK(kitAudio({file}) == 0);
    CHECK(kitAudio({file, "extra"}) != 0);
    return "";
}

TEST(audio_Chords_Tracks_And_Drums_Mix_Without_Clipping) {
    // Three tracks of one second each: chord, saw bass, drums. Loud gain
    // would clip when summed; the mix is scaled below full scale instead.
    const auto wav = synthNotes("C4+E4+G4:1 | saw> C2:.5 C2 | K:.25 H S H", "sine", 0, 1);
    CHECK(wav.ok);
    const auto stats = analyzeWav(wav.value);
    CHECK(stats.ok && stats.value.clippedSamples == 0 && stats.value.peak <= .99 && stats.value.peak > .5);
    CHECK(stats.value.frames >= 44100);
    CHECK(!synthNotes("C4+:1").ok);
    CHECK(!synthNotes("buzz> C4").ok);
    std::string tracks;
    for (int i = 0; i < 17; ++i) tracks += "C4|";
    CHECK(!synthNotes(tracks).ok);
    return "";
}

TEST(audio_Track_Wave_Aliases_Are_Accepted) {
    CHECK(synthNotes("sawtooth> C3:.1 | triangle> E4:.1").ok);
    CHECK(!synthNotes("wobble> C3:.1").ok);
    return "";
}
