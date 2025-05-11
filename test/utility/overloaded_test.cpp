#include "utility/overloaded.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Overloaded"_test = [] {
    auto overloaded = Overloaded{
        [](int i) { return i + 1; },
        [](double d) { return d + 1.0; },
        [](const std::string& s) { return s + "a"; },
    };

    static_assert(overloaded(1) == 2);
    static_assert(overloaded(1.0) == 2.0);
    static_assert(overloaded("a") == "aa");
  };
}
