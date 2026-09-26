// C++17, standard library only. Directed nonnegative sparse shortest paths.
// Input: vertex_count edge_count source, then edge_count lines: from to weight.
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Distance = std::uint64_t;
constexpr Distance inf = std::numeric_limits<Distance>::max();
constexpr std::int64_t maxWeight = 1000000000000LL;
struct Edge { int from, to; std::int64_t weight; };

std::vector<Distance> shortestPaths(int vertices, const std::vector<Edge>& edges, int source) {
    if (vertices < 1 || vertices > 100000 || source < 0 || source >= vertices || edges.size() > 1000000)
        throw std::invalid_argument("expected 1..100000 vertices, <=1000000 edges, valid source");
    std::vector<std::vector<std::pair<int, Distance>>> graph(vertices);
    for (const auto& e : edges) {
        if (e.from < 0 || e.to < 0 || e.from >= vertices || e.to >= vertices || e.weight < 0 || e.weight > maxWeight)
            throw std::invalid_argument("edge endpoint or weight outside contract");
        graph[e.from].push_back({e.to, static_cast<Distance>(e.weight)});
    }
    // Every shortest path can be simple: (vertices-1)*maxWeight < inf.
    std::vector<Distance> distance(vertices, inf);
    using Item = std::pair<Distance, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
    distance[source] = 0;
    queue.push({0, source});
    while (!queue.empty()) {
        auto [cost, from] = queue.top(); queue.pop();
        if (cost != distance[from]) continue; // stale heap entry
        for (auto [to, weight] : graph[from]) {
            if (cost + weight >= distance[to]) continue;
            distance[to] = cost + weight;
            queue.push({distance[to], to});
        }
    }
    return distance;
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void selfTest() {
    require(shortestPaths(3, {{0, 1, 0}, {0, 1, 9}, {1, 1, 0}}, 0) ==
            std::vector<Distance>({0, 0, inf}), "zero, duplicate or disconnected case");
    require(shortestPaths(3, {{0, 1, maxWeight}, {1, 2, maxWeight}}, 0)[2] ==
            2 * static_cast<Distance>(maxWeight), "large distance case");
    for (const auto& bad : std::vector<Edge>{{0, 1, -1}, {0, 2, 1}, {0, 1, maxWeight + 1}}) {
        bool rejected = false;
        try { (void)shortestPaths(2, {bad}, 0); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid edge accepted");
    }
    bool rejected = false;
    try { (void)shortestPaths(0, {}, 0); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "empty graph accepted");
    // Independent O(V^3) oracle; generated inputs include cycles and parallel edges.
    std::mt19937 random(1729);
    for (int trial = 0; trial < 200; ++trial) {
        int n = 1 + random() % 18;
        std::vector<Edge> edges;
        std::vector<std::vector<Distance>> reference(n, std::vector<Distance>(n, inf));
        for (int i = 0; i < n; ++i) reference[i][i] = 0;
        for (int i = 0; i < n * 3; ++i) {
            int a = random() % n, b = random() % n;
            Distance w = random() % 100;
            edges.push_back({a, b, static_cast<std::int64_t>(w)});
            reference[a][b] = std::min(reference[a][b], w);
        }
        for (int k = 0; k < n; ++k)
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    if (reference[i][k] != inf && reference[k][j] != inf)
                        reference[i][j] = std::min(reference[i][j], reference[i][k] + reference[k][j]);
        for (int source = 0; source < n; ++source)
            require(shortestPaths(n, edges, source) == reference[source], "differential check failed");
    }
    std::cout << "PASS: 200 generated graphs, all sources, plus boundary cases\n";
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--self-test") { selfTest(); return 0; }
        if (argc != 2) throw std::invalid_argument("usage: shortest-path --self-test | graph.txt");
        std::ifstream input(argv[1]);
        int n, count, source;
        if (!(input >> n >> count >> source) || count < 0 || count > 1000000)
            throw std::invalid_argument("invalid graph header");
        std::vector<Edge> edges;
        for (int i = 0; i < count; ++i) {
            Edge edge{};
            if (!(input >> edge.from >> edge.to >> edge.weight)) throw std::invalid_argument("invalid edge row");
            edges.push_back(edge);
        }
        std::string extra;
        if (input >> extra) throw std::invalid_argument("unexpected data after edges");
        auto distance = shortestPaths(n, edges, source);
        for (int i = 0; i < n; ++i)
            std::cout << i << ' ' << (distance[i] == inf ? "unreachable" : std::to_string(distance[i])) << '\n';
    } catch (const std::exception& error) {
        std::cerr << "shortest-path: " << error.what() << '\n';
        return 1;
    }
}
