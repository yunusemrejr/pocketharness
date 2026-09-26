// Small native audio helpers. Returned WAV bytes are owned by the caller.
#pragma once

#include "common.h"

namespace pocket {

struct SoundSpec {
    double duration = 0.25, frequency = 440, endFrequency = 440;
    double gain = 0.3, noise = 0, attack = 0.005, release = 0.03, lowpass = 0;
    double harmonics = 0;  // fraction of damped bell partials, 0..1
    uint32_t seed = 1;
    int rate = 44100;
};

struct AudioStats {
    int rate = 0, channels = 0, bits = 0;
    size_t frames = 0, clippedSamples = 0;
    double peak = 0, rms = 0, pitchHz = 0, silenceSeconds = 0;
};

double noteFreq(const std::string& note);  // strict note or frequency; -1 invalid
Result<SoundSpec> soundPreset(const std::string& name);
// Synthesis is mono PCM16, <=60 seconds, 8000..96000 Hz; no global RNG state.
Result<std::string> synthSound(const SoundSpec& spec);
Result<std::string> synthNotes(const std::string& notes, const std::string& wave = "sine",
                               double bpm = 0, double gain = 0.3, int rate = 44100);
// <=64 MiB RIFF/WAVE; PCM8/16/24/32 or float32, <=32 channels, 8..192 kHz.
// Peak/RMS/silence use channel energy, never a phase-cancelling mono average.
Result<AudioStats> analyzeWav(std::string_view bytes);

int kitWav(const std::vector<std::string>& args);
int kitAudio(const std::vector<std::string>& args);
int kitSfx(const std::vector<std::string>& args);

}  // namespace pocket
