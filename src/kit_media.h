// `pocket kit say|asset|theme|vsheet`: narration with timing sidecars, licensed
// assets from the open web, a subject-derived palette, and a video contact sheet.
#pragma once

#include <string>
#include <vector>
#include "json.h"

namespace pocket {

std::string voicesDir();  // $POCKET_VOICES (real path inside the sandbox) or ~/.local/share/pocketharness/voices
int kitSay(const std::vector<std::string>& args);
int kitAsset(const std::vector<std::string>& args);
int kitTheme(const std::vector<std::string>& args);
int kitVsheet(const std::vector<std::string>& args);
// Bounded local glTF/GLB preflight: framing, dependency sizes, geometry ranges,
// scene references and decoder requirements. No renderer or network is involved.
Result<json::Value> inspectGltf(const std::string& path);

// Pure helpers (unit-tested).
struct Utterance { std::string display, spoken; };
// Split a script into sentences per paragraph. `{TTS|tee tee ess}` shows the
// first form and speaks the second.
std::vector<std::vector<std::vector<Utterance>>> scriptSentences(const std::string& script);
std::string oklchHex(double L, double C, double hueDeg);  // clipped into sRGB
double contrastRatio(const std::string& hexA, const std::string& hexB);

}  // namespace pocket
