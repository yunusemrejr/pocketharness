// PocketHarness - `pocket kit`: native superpowers as BusyBox-style
// subcommands. The model reaches them through the bash tool (the session
// stages this binary on the sandbox PATH), so the tool surface stays at five
// schemas while web research, browser capture, sound, motion, SVG, image and
// quality checks run as small native code with no dependencies.
#pragma once

#include <string>
#include <map>
#include <string_view>
#include <vector>

namespace pocket {

int kitMain(int argc, char** argv);  // argv[0] = "kit"

// Pure helpers (unit-tested).
std::string htmlToText(std::string_view html, std::vector<std::string>* links = nullptr);
std::string urlDecode(std::string_view s);
std::map<std::string, std::string> htmlAttrs(std::string_view tag);  // case-insensitive keys, decoded values
std::string htmlAttr(std::string_view tag, const std::string& name);  // attribute value, "" if absent
double noteFreq(const std::string& note);  // "A4" -> 440, "C#5", "Eb3"; -1 invalid
// Damped spring sampled to CSS linear(); empty if invalid or outside bounded
// settling/sampling limits. durationMs is zero on failure.
std::string springEasing(double stiffness, double damping, double mass, double* durationMs);
// Anti-slop findings for one file's content: "line N: reason" entries.
std::vector<std::string> slopScan(const std::string& path, const std::string& content);
// Host awareness summary (OS, CPU, memory, disk, GPU, toolchain).
std::string hostProbe(const std::string& dir);

}  // namespace pocket
