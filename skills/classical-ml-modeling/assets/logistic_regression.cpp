// C++17, standard library only. A two-feature binary baseline, not an ML library.
// Numeric CSV has no header: feature_1,feature_2,label (label is 0 or 1).
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Row { std::array<double, 2> x; int y; };
struct Model {
    std::array<double, 2> mean{}, scale{1, 1};
    std::array<double, 3> weight{}; // intercept, standardized feature 1, feature 2
    double logit(const Row& row) const {
        double result = weight[0];
        for (int j = 0; j < 2; ++j) result += weight[j + 1] * ((row.x[j] - mean[j]) / scale[j]);
        if (!std::isfinite(result)) throw std::invalid_argument("prediction outside finite numeric range");
        return result;
    }
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void validate(const std::vector<Row>& rows) {
    if (rows.empty() || rows.size() > 100000) throw std::invalid_argument("expected 1..100000 rows");
    for (const auto& row : rows) {
        if (row.y != 0 && row.y != 1) throw std::invalid_argument("label must be 0 or 1");
        for (double x : row.x)
            if (!std::isfinite(x) || std::abs(x) > 1e100)
                throw std::invalid_argument("features must be finite, with magnitude <=1e100");
    }
}

double sigmoid(double x) { // Neither branch overflows exp().
    double e = std::exp(-std::abs(x));
    return x >= 0 ? 1 / (1 + e) : e / (1 + e);
}

Model fit(const std::vector<Row>& train) {
    validate(train);
    double positives = 0;
    Model model;
    std::array<double, 2> m2{};
    size_t count = 0;
    for (const auto& row : train) {
        ++count; positives += row.y;
        for (int j = 0; j < 2; ++j) {
            double delta = row.x[j] - model.mean[j];
            model.mean[j] += delta / count;
            m2[j] += delta * (row.x[j] - model.mean[j]);
        }
    }
    if (positives == 0 || positives == train.size()) throw std::invalid_argument("training needs both classes");
    for (int j = 0; j < 2; ++j) {
        model.scale[j] = std::sqrt(std::max(0.0, m2[j] / train.size()));
        if (model.scale[j] == 0) model.scale[j] = 1; // constant feature
    }
    auto normalized = train; // Learned preprocessing is fit ONLY on train.
    for (auto& row : normalized)
        for (int j = 0; j < 2; ++j) row.x[j] = (row.x[j] - model.mean[j]) / model.scale[j];
    constexpr double learningRate = .25, penalty = .01;
    for (int step = 0; step < 800; ++step) {
        std::array<double, 3> gradient{};
        for (const auto& row : normalized) {
            double residual = sigmoid(model.weight[0] + model.weight[1] * row.x[0] + model.weight[2] * row.x[1]) - row.y;
            gradient[0] += residual;
            for (int j = 0; j < 2; ++j) gradient[j + 1] += residual * row.x[j];
        }
        for (int j = 0; j < 3; ++j) {
            model.weight[j] -= learningRate * (gradient[j] / train.size() + (j ? penalty * model.weight[j] : 0));
            require(std::isfinite(model.weight[j]), "nonfinite training update");
        }
    }
    return model;
}

struct Metrics { double accuracy = 0, logLoss = 0, brier = 0; };
Metrics evaluate(const Model& model, const std::vector<Row>& test) {
    validate(test);
    Metrics metric;
    for (const auto& row : test) {
        double z = model.logit(row), p = sigmoid(z);
        metric.accuracy += (z >= 0) == row.y;
        metric.logLoss += std::max(z, 0.0) - row.y * z + std::log1p(std::exp(-std::abs(z)));
        metric.brier += (p - row.y) * (p - row.y);
    }
    metric.accuracy /= test.size(); metric.logLoss /= test.size(); metric.brier /= test.size();
    require(std::isfinite(metric.logLoss), "nonfinite evaluation loss");
    return metric;
}

void report(const std::vector<Row>& train, const std::vector<Row>& test) {
    Model model = fit(train);
    Metrics result = evaluate(model, test);
    double positives = 0;
    for (const auto& row : train) positives += row.y;
    double prior = (positives + .5) / (train.size() + 1); // training-only constant baseline
    Model baseline;
    baseline.weight[0] = std::log(prior / (1 - prior));
    Metrics base = evaluate(baseline, test);
    std::cout << std::setprecision(8) << "train_rows=" << train.size() << " test_rows=" << test.size()
              << " accuracy=" << result.accuracy << " log_loss=" << result.logLoss << " brier=" << result.brier
              << " baseline_log_loss=" << base.logLoss << '\n';
    std::cout << "mean=" << model.mean[0] << ',' << model.mean[1] << " scale=" << model.scale[0] << ',' << model.scale[1]
              << " weights=" << model.weight[0] << ',' << model.weight[1] << ',' << model.weight[2] << '\n';
}

std::vector<Row> readCsv(const char* path) {
    std::ifstream file(path);
    if (!file) throw std::invalid_argument("cannot open CSV");
    std::vector<Row> rows;
    char buffer[4096];
    while (file.getline(buffer, sizeof(buffer))) {
        std::string line(buffer);
        if (std::count(line.begin(), line.end(), ',') != 2) throw std::invalid_argument("expected exactly three CSV columns");
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream values(line);
        Row row{}; std::string trailing;
        if (!(values >> row.x[0] >> row.x[1] >> row.y) || (values >> trailing)) throw std::invalid_argument("invalid numeric CSV row");
        rows.push_back(row);
        if (rows.size() > 100000) throw std::invalid_argument("CSV exceeds row limit");
    }
    if (!file.eof()) throw std::invalid_argument("CSV read error or row exceeds 4094 bytes");
    validate(rows);
    return rows;
}

void selfTest() {
    std::mt19937 random(1729);
    std::vector<Row> train, test;
    for (int i = 0; i < 900; ++i) {
        double a = (random() % 4001) / 1000.0 - 2, b = (random() % 4001) / 1000.0 - 2;
        (i < 600 ? train : test).push_back({{a, b}, 2 * a - b + .3 >= 0});
    }
    Model model = fit(train);
    Metrics metric = evaluate(model, test);
    require(metric.accuracy > .95 && metric.logLoss < .3, "synthetic holdout regression");
    auto original = model.mean;
    (void)evaluate(model, {{{100, 100}, 0}, {{-100, -100}, 1}});
    require(model.mean == original, "evaluation changed preprocessing");
    auto constant = train;
    for (auto& row : constant) row.x[1] = 5;
    require(fit(constant).scale[1] == 1, "constant feature handling");
    require(sigmoid(1000) == 1 && sigmoid(-1000) == 0, "extreme logits");
    for (auto bad : std::vector<std::vector<Row>>{{}, {{{0, 0}, 2}}, {{{0, 0}, 0}},
             {{{std::numeric_limits<double>::quiet_NaN(), 0}, 0}, {{1, 1}, 1}}}) {
        bool rejected = false;
        try { (void)fit(bad); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid training data accepted");
    }
    report(train, test);
    std::cout << "PASS: synthetic holdout, train-only preprocessing, constant feature, extreme logits, invalid data\n";
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--self-test") { selfTest(); return 0; }
        if (argc != 3) throw std::invalid_argument("usage: logistic-regression --self-test | train.csv heldout.csv");
        report(readCsv(argv[1]), readCsv(argv[2]));
    } catch (const std::exception& error) {
        std::cerr << "logistic-regression: " << error.what() << '\n';
        return 1;
    }
}
