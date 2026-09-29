// PocketHarness - `pocket kit lint`: one table-driven rule engine for security,
// performance, backend and coding patterns, DRY, UI/accessibility slop and SVG
// hygiene. The same scan runs on every write/edit and on the files a turn
// changed, so agents get the findings without asking for them.
#pragma once

#include <string>
#include <vector>

namespace pocket {

// Findings for one file's content, most severe first: "L12,40: [sec/H] message".
// minSev is 'H', 'M' or 'L' (lowest severity reported). Unknown file types and
// vendored/generated paths yield nothing.
std::vector<std::string> lintScan(const std::string& path, const std::string& content, char minSev = 'M');

// Lint files and directories (recursive, hidden and vendored dirs skipped),
// adding cross-file duplicate-block detection. Empty string when clean.
std::string lintPaths(const std::vector<std::string>& paths, char minSev = 'M');

int kitLint(const std::vector<std::string>& args);

}  // namespace pocket
