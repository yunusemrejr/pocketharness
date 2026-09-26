// PocketHarness - self-maintaining model catalog. Every keyed provider's
// GET {base}/models is merged into stateDir()/catalog.json and refreshed in
// the background once a day: new model slugs, context windows, reasoning
// support, vision and prices appear without anyone editing a list.
#pragma once

#include <string>
#include <vector>

#include "config.h"

namespace pocket {

struct CatalogModel {
    std::string provider, id;
    long context = -1;   // -1 unpublished
    int reasoning = -1;  // -1 unknown, 0 no, 1 yes (published supported_parameters)
    bool vision = false;
    double inPrice = -1, outPrice = -1;  // USD per 1M tokens, -1 unknown
};

// Parse one /models body (OpenAI {"data"}, Gemini {"models"}, bare arrays).
// Non-chat entries (embeddings, images, audio, rerank) are dropped.
std::vector<CatalogModel> parseCatalog(const std::string& provider, const std::string& body);

// Cached catalog plus configured aliases (never touches the network).
std::vector<CatalogModel> catalogLoad(const Config& cfg);
const CatalogModel* catalogFind(const std::vector<CatalogModel>& all, const std::string& provider,
                                const std::string& id);

// Fetch every provider whose key is present (parallel, bounded) and rewrite
// the cache. Providers that fail keep their previous entries. Returns a
// one-line summary. The async variant (a detached `pocket --refresh-catalog`
// process) runs only when the cache is >24h old.
std::string catalogRefresh(const Config& cfg);
void catalogRefreshIfStale(const Config& cfg);

// "provider:id  256k  $0.30/$1.20  think vision" for pickers.
std::string catalogLabel(const CatalogModel& m);

// Refine a resolved model from the catalog: live context (unless pinned in
// config) and reasoning=none when the provider says the model can't think.
void catalogApply(const Config& cfg, ResolvedModel& m);

}  // namespace pocket
