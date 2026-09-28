// `pocket kit music|mix`: generative music and a cue-sheet mixer for videos.
// Everything is native float DSP at 48 kHz stereo; no libraries, no samples.
#pragma once

#include <string>
#include <vector>

#include "common.h"

namespace pocket {

constexpr int kStudioRate = 48000;
struct Stereo {
    std::vector<float> l, r;
    size_t size() const { return l.size(); }
};

// WAV (PCM8..32, float32) is decoded natively; any other container goes through
// ffmpeg. The result is always 48 kHz stereo.
Result<Stereo> loadAudio(const std::string& path);
Result<void> saveStereoWav(const std::string& path, const Stereo& s);
// ITU-R BS.1770-4 integrated loudness with both gates; -1000 for silence.
double integratedLufs(const Stereo& s);
// Look-ahead peak limiter: no sample exceeds ceilingDb afterwards.
void limitPeak(Stereo& s, double ceilingDb);
// Sample peak in dBFS.
double peakDb(const Stereo& s);

int kitMusic(const std::vector<std::string>& args);
int kitMix(const std::vector<std::string>& args);

}  // namespace pocket
