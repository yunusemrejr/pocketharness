// PocketHarness - the wisdom book: a compact doctrine that is always in the
// frozen system prompt, plus domain sections retrieved per request (BM25)
// for the planning brief. ~/.config/pocketharness/wisdom.md ("## " sections)
// replaces the built-in book when present.
#pragma once

#include <string>

namespace pocket {

const std::string& wisdomDoctrine();
// The most relevant book sections for a request, within maxBytes.
std::string wisdomFor(const std::string& request, size_t maxBytes);

}  // namespace pocket
