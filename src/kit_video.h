// Deterministic local HTML frames and video. Optional Chrome/FFmpeg executables;
// no browser SDK, network debugging port, Node runtime, or linked media library.
#pragma once
#include <string>
#include <vector>
namespace pocket {
int kitVideo(const std::vector<std::string>& args);
int kitFrame(const std::vector<std::string>& args);
int kitVcheck(const std::vector<std::string>& args);  // mechanical faults in a finished video
}
