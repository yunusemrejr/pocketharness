// PocketHarness - model catalog implementation.
#include "catalog.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#include <mutex>
#include <thread>

#include "json.h"
#include "provider.h"

namespace pocket {

namespace {

std::string catalogPath() { return stateDir() + "/catalog.json"; }
constexpr long kStaleSec = 24 * 3600;

double price(const json::Value& v, bool perToken) {
    double n = v.isStr() ? atof(v.asStr().c_str()) : v.asNum(-1);
    if (!(v.isStr() || v.isNum()) || n < 0) return -1;
    return perToken ? n * 1e6 : n;
}

json::Value toJson(const CatalogModel& m) {
    json::Object o{{"id", m.id}};
    if (m.context > 0) o["ctx"] = m.context;
    if (m.reasoning >= 0) o["think"] = (long)m.reasoning;
    if (!m.floor.empty()) o["floor"] = m.floor;
    if (m.vision) o["vision"] = true;
    if (m.inPrice >= 0) o["in"] = m.inPrice;
    if (m.outPrice >= 0) o["out"] = m.outPrice;
    return o;
}

CatalogModel fromJson(const std::string& provider, const json::Value& v) {
    CatalogModel m;
    m.provider = provider;
    m.id = v.at("id").asStr();
    m.context = v.at("ctx").asInt(-1);
    m.reasoning = (int)v.at("think").asInt(-1);
    m.floor = v.at("floor").asStr();
    m.vision = v.at("vision").asBool();
    m.inPrice = v.at("in").asNum(-1);
    m.outPrice = v.at("out").asNum(-1);
    return m;
}

json::Value readCache() {
    auto t = readFileBounded(catalogPath(), 32 << 20);
    auto v = t.ok ? json::parse(t.value) : Result<json::Value>::Err("");
    return v.ok && v.value.at("providers").isObj() ? v.value
                                                   : json::Value(json::Object{{"providers", json::obj()}});
}

std::mutex g_refreshMu;

}  // namespace

std::vector<CatalogModel> parseCatalog(const std::string& provider, const std::string& body) {
    std::vector<CatalogModel> out;
    auto v = json::parse(body);
    if (!v.ok) return out;
    const json::Value* arr = v.value.isArr() ? &v.value
                             : v.value.at("data").isArr() ? &v.value.at("data")
                             : v.value.at("models").isArr() ? &v.value.at("models") : nullptr;
    if (!arr) return out;
    for (const auto& e : arr->asArr()) {
        if (!e.isObj()) continue;
        CatalogModel m;
        m.provider = provider;
        m.id = e.at("id").asStr();
        if (m.id.empty()) {
            m.id = e.at("name").asStr();
            if (startsWith(m.id, "models/")) m.id = m.id.substr(7);
        }
        if (m.id.empty() || m.id.size() > 200) continue;
        std::string type = toLower(e.at("type").asStr());
        std::string lid = toLower(m.id);
        if (type == "embedding" || type == "embeddings" || type == "image" || type == "audio" ||
            type == "rerank" || type == "moderation" || lid.find("embed") != std::string::npos ||
            lid.find("whisper") != std::string::npos || lid.find("tts") != std::string::npos ||
            lid.find("rerank") != std::string::npos)
            continue;
        for (const char* f : {"context_length", "context_window", "max_context", "max_context_length",
                              "max_model_len", "context", "inputTokenLimit", "max_input_tokens"}) {
            long n = e.at(f).asInt(-1);
            if (n >= 512 && n <= 10000000) { m.context = n; break; }
        }
        if (m.context < 0) {
            long n = e.at("top_provider").at("context_length").asInt(-1);
            if (n >= 512 && n <= 10000000) m.context = n;
        }
        const auto& sp = e.at("supported_parameters");
        if (sp.isArr()) {
            m.reasoning = 0;
            for (const auto& p : sp.asArr())
                if (p.asStr() == "reasoning" || p.asStr() == "include_reasoning" ||
                    p.asStr() == "reasoning_effort")
                    m.reasoning = 1;
        }
        // OpenRouter publishes whether reasoning can be turned off at all.
        const auto& rs = e.at("reasoning");
        if (rs.isObj() && rs.at("mandatory").asBool()) {
            static const char* kEfforts[] = {"minimal", "low", "medium", "high", "xhigh", "max"};
            for (const char* eff : kEfforts) {
                for (const auto& s : rs.at("supported_efforts").asArr())
                    if (s.asStr() == eff) m.floor = eff;
                if (!m.floor.empty()) break;
            }
            if (m.floor.empty()) m.floor = "low";
        }
        for (const auto& mod : e.at("architecture").at("input_modalities").asArr())
            if (mod.asStr() == "image") m.vision = true;
        const auto& pr = e.at("pricing");
        if (pr.isObj()) {
            bool perToken = pr.at("prompt").isStr();  // OpenRouter-style: USD per token
            m.inPrice = price(perToken ? pr.at("prompt") : pr.at("input"), perToken);
            m.outPrice = price(perToken ? pr.at("completion") : pr.at("output"), perToken);
        }
        out.push_back(std::move(m));
    }
    return out;
}

std::vector<CatalogModel> catalogLoad(const Config& cfg) {
    std::vector<CatalogModel> out;
    json::Value cache = readCache();
    for (const auto& [prov, entry] : cache.at("providers").asObj()) {
        bool known = false;  // renamed/removed providers age out of the picker
        for (const auto& p : cfg.providers) known = known || p.name == prov;
        if (!known) continue;
        for (const auto& m : entry.at("models").asArr()) out.push_back(fromJson(prov, m));
    }
    // Configured models are always selectable, even for listing-less endpoints.
    for (const auto& a : cfg.models)
        if (!catalogFind(out, a.provider, a.model)) {
            CatalogModel m;
            m.provider = a.provider;
            m.id = a.model;
            if (a.contextSet) m.context = a.context;
            out.push_back(std::move(m));
        }
    return out;
}

const CatalogModel* catalogFind(const std::vector<CatalogModel>& all, const std::string& provider,
                                const std::string& id) {
    for (const auto& m : all)
        if (m.provider == provider && m.id == id) return &m;
    return nullptr;
}

std::string catalogRefresh(const Config& cfg, std::atomic<bool>* cancel) {
    std::lock_guard<std::mutex> lk(g_refreshMu);
    std::vector<const ProviderCfg*> todo;
    for (const auto& p : cfg.providers) {
        if (cancel && cancel->load()) return "catalog: cancelled before any provider answered";
        auto k = providerApiKey(p);
        // Loopback daemons are skipped unless running: the probe costs 5s otherwise.
        if (k.ok && !isLoopbackHttp(p.baseUrl) && p.protocol != "codex") todo.push_back(&p);
    }
    std::vector<std::vector<CatalogModel>> got(todo.size());
    std::vector<std::thread> threads;
    for (size_t i = 0; i < todo.size(); ++i)
        threads.emplace_back([&, i] {
            got[i] = parseCatalog(todo[i]->name, fetchModelsBody(*todo[i], 12000, cancel));
        });
    for (auto& t : threads) t.join();
    // A partial refresh is worse than none: rewriting the cache now would drop
    // listings for every provider the interrupt cut off.
    if (cancel && cancel->load()) return "catalog: cancelled, cache left unchanged";
    json::Value cache = readCache();
    long now = (long)time(nullptr), models = 0, ok = 0;
    for (size_t i = 0; i < todo.size(); ++i) {
        if (got[i].empty()) continue;  // keep the previous listing on failure
        json::Array arr;
        for (const auto& m : got[i]) arr.push_back(toJson(m));
        cache.asObj()["providers"].asObj()[todo[i]->name] = json::Object{{"ts", now}, {"models", arr}};
        models += (long)got[i].size();
        ++ok;
    }
    cache.asObj()["ts"] = now;
    if (ensureDir(stateDir(), 0700).ok) (void)atomicWriteFile(catalogPath(), json::stringify(cache), 0600);
    return "catalog: " + std::to_string(models) + " models from " + std::to_string(ok) + "/" +
           std::to_string(todo.size()) + " keyed providers";
}

void catalogRefreshIfStale(const Config&) {
    struct stat st;
    std::string path = catalogPath();
    if (stat(path.c_str(), &st) == 0 && time(nullptr) - st.st_mtime < kStaleSec) return;
    // Claim the refresh (mtime) so concurrent sessions don't all fetch.
    if (ensureDir(stateDir(), 0700).ok && stat(path.c_str(), &st) != 0)
        (void)atomicWriteFile(path, "{\"providers\":{}}", 0600);
    utimes(path.c_str(), nullptr);
    // A detached `pocket --refresh-catalog` grandchild: never delays startup,
    // never outlives-or-crashes this process's threads at exit.
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        setsid();
        if (fork() == 0) {
            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0) dup2(devnull, 0), dup2(devnull, 1), dup2(devnull, 2);
            execl("/proc/self/exe", "pocket", "--refresh-catalog", (char*)nullptr);
        }
        _exit(0);
    }
    waitpid(pid, nullptr, 0);
}

std::string catalogLabel(const CatalogModel& m) {
    std::string s = m.provider + ":" + m.id;
    if (m.context > 0) s += "  " + std::to_string(m.context >= 1000000 ? m.context / 1000000 : m.context / 1000) +
                            (m.context >= 1000000 ? "M" : "k");
    if (m.inPrice >= 0 && m.outPrice >= 0) {
        char b[48];
        if (m.inPrice == 0 && m.outPrice == 0) snprintf(b, sizeof b, "  free");
        else snprintf(b, sizeof b, "  $%.2f/$%.2f", m.inPrice, m.outPrice);
        s += b;
    }
    if (m.reasoning == 1) s += "  think";
    if (m.vision) s += "  vision";
    return s;
}

void catalogApplyFrom(const Config& cfg, const std::vector<CatalogModel>& all, ResolvedModel& m,
                      std::atomic<bool>* cancel) {
    const CatalogModel* c = catalogFind(all, m.provider.name, m.model);
    bool pinned = hasExplicitContext(cfg, m.provider.name, m.model);
    if (c && c->reasoning == 0 && m.options.reasoning == "auto") m.options.reasoning = "none";
    if (c && c->reasoning == 1) m.options.thinkOff = c->floor.empty() ? "disable" : c->floor;
    if (c) m.inPrice = c->inPrice, m.outPrice = c->outPrice;
    if (pinned) return;
    if (c && c->context > 0) { m.context = c->context; return; }
    // The catalog did not know this model, so ask the provider directly. That
    // is a blocking network call: honour cancel so a UI can escape it.
    long live = fetchModelContext(m.provider, m.model, cancel);
    if (live > 0) m.context = live;
}

void catalogApply(const Config& cfg, ResolvedModel& m, std::atomic<bool>* cancel) {
    catalogApplyFrom(cfg, catalogLoad(cfg), m, cancel);
}

}  // namespace pocket
