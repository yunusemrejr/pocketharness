// PocketHarness tests - runner.
#include "mini.h"

int main() {
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
