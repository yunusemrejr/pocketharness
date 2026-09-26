// Offline C++17 threshold evaluation. No requests, model training or config edits.
// CSV rows: probability,label. Probability -1 denotes a missing/unknown score.
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
struct Item { double p; int label; };
std::vector<Item> load(const char* path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("cannot open score file");
    std::vector<Item> rows;
    char line[256];
    while (file.getline(line, sizeof(line))) {
        std::string text(line);
        if (std::count(text.begin(), text.end(), ',') != 1) throw std::runtime_error("expected probability,label");
        std::replace(text.begin(), text.end(), ',', ' ');
        std::istringstream input(text);
        Item row{}; std::string extra;
        if (!(input >> row.p >> row.label) || (input >> extra) || !std::isfinite(row.p) ||
            (row.p != -1 && (row.p < 0 || row.p > 1)) || (row.label != 0 && row.label != 1))
            throw std::runtime_error("invalid probability or binary label");
        rows.push_back(row);
        if (rows.size() > 100000) throw std::runtime_error("score file exceeds row limit");
    }
    if (!file.eof() || rows.empty()) throw std::runtime_error("empty file, oversized row or read error");
    return rows;
}
struct Metrics {
    int tp = 0, fp = 0, tn = 0, fn = 0, unknown = 0;
    double brier = 0, ece = 0;
    int cost() const { return 2 * fp + fn; } // Example policy: false alarms cost twice a miss.
};
Metrics score(const std::vector<Item>& rows, double threshold) {
    Metrics m;
    std::array<int, 10> count{}, positives{};
    std::array<double, 10> probability{};
    for (const auto& row : rows) {
        if (row.p < 0) { ++m.unknown; continue; }
        bool yes = row.p >= threshold;
        if (yes) row.label ? ++m.tp : ++m.fp;
        else row.label ? ++m.fn : ++m.tn;
        m.brier += (row.p - row.label) * (row.p - row.label);
        int bin = std::min(9, static_cast<int>(row.p * 10));
        ++count[bin]; positives[bin] += row.label; probability[bin] += row.p;
    }
    size_t known = rows.size() - m.unknown;
    if (!known) throw std::runtime_error("no scored examples; unknown is not a negative label");
    m.brier /= known;
    for (int bin = 0; bin < 10; ++bin)
        if (count[bin]) m.ece += std::abs(probability[bin] - positives[bin]) / known;
    return m;
}
void report(const char* name, const std::vector<Item>& rows, double threshold) {
    auto m = score(rows, threshold);
    std::cout << name << " threshold=" << threshold << " tp=" << m.tp << " fp=" << m.fp
              << " tn=" << m.tn << " fn=" << m.fn << " unknown=" << m.unknown
              << " coverage=" << double(rows.size() - m.unknown) / rows.size()
              << " cost=" << m.cost() << " brier=" << m.brier << " ece10=" << m.ece << '\n';
}
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("usage: decision-threshold calibration.csv heldout.csv");
        auto calibration = load(argv[1]);
        bool positive = false, negative = false;
        for (const auto& row : calibration) if (row.p >= 0) { positive |= row.label == 1; negative |= row.label == 0; }
        if (!positive || !negative) throw std::runtime_error("calibration needs scored examples of both classes");
        double selected = .5;
        int best = score(calibration, selected).cost();
        for (int i = 1; i <= 19; ++i) {
            double threshold = i / 20.0;
            int cost = score(calibration, threshold).cost();
            if (cost < best || (cost == best && threshold > selected)) { best = cost; selected = threshold; }
        }
        // Only now read the heldout scores; they never select the threshold.
        auto heldout = load(argv[2]);
        std::cout << std::setprecision(6);
        report("calibration", calibration, selected);
        report("heldout_default", heldout, .5);
        report("heldout_fitted", heldout, selected);
        if (score(heldout, selected).cost() > score(heldout, .5).cost())
            std::cout << "Fitted policy is worse on heldout data; training fit does not justify deployment.\n";
    } catch (const std::exception& error) {
        std::cerr << "decision-threshold: " << error.what() << '\n';
        return 1;
    }
}
