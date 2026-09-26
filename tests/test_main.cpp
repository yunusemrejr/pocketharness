// PocketHarness tests - runner.
#include "mini.h"

int videoTestFixture(int argc, char** argv);

int main(int argc, char** argv) {
    int fixture = videoTestFixture(argc, argv);
    if (fixture >= 0) return fixture;
    const std::string scratch = pocket::test::makeTempDir("pocket-tests-home");
    if (scratch.empty()) return 1;
    struct Cleanup { std::string path; ~Cleanup() { pocket::test::rmRf(path); } } cleanup{scratch};
    pocket::test::HomeGuard isolated(scratch);
    for (const auto& c : pocket::test::registry()) {
        std::string err = c.fn();
        if (err.empty()) {
            pocket::test::passes++;
            std::cout << "PASS " << c.name << "\n";
        } else {
            pocket::test::failures++;
            std::cout << "FAIL " << c.name << "\n  " << err << "\n";
        }
    }
    std::cout << pocket::test::passes << " passed, " << pocket::test::failures << " failed\n";
    return pocket::test::failures ? 1 : 0;
}
