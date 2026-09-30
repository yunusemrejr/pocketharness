// PocketHarness - `pocket kit lint`: one table-driven rule engine for security,
// performance, backend and coding patterns, DRY, UI/accessibility slop and SVG
// hygiene. The same scan runs on every write/edit and on the files a turn
// changed, so agents get the findings without asking for them.
#pragma once

#include <string>
#include <vector>
#include "oversee.h"

namespace pocket {

// Findings for one file's content, most severe first: "L12,40: [sec/H] message".
// minSev is 'H', 'M' or 'L' (lowest severity reported). Unknown file types and
// vendored/generated paths yield nothing.
std::vector<std::string> lintScan(const std::string& path, const std::string& content, char minSev = 'M');

// Lint files and directories (recursive, hidden and vendored dirs skipped),
// adding cross-file duplicate-block detection. Empty string when clean.
std::string lintPaths(const std::vector<std::string>& paths, char minSev = 'M');

int kitLint(const std::vector<std::string>& args);

// Targeted content questions, shared by write/edit and `kit quality`.
// Empty means native checks suffice; probabilities are advisory findings.
std::vector<Question> qualityQuestions(const std::string& path, const std::string& content,
                                     const std::vector<std::string>* findings = nullptr);
json::Value qualityState(const std::string& path, const std::string& content,
                         const std::string& before, const std::string& intent);
std::string qualityAdvice(const std::map<std::string, double>& answers);
int kitQuality(const std::vector<std::string>& args);

}  // namespace pocket
