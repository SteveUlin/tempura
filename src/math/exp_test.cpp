#include "math/exp.h"

#include <random>

#include "benchmark.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "exp function"_test = [] {
    double x = 0.0;
    for (int i = 0; i <= 10; ++i) {
      const double std_exp = std::exp(x);
      const double tempura_exp = tempura::exp(x);
      std::println("x = {}, std::exp(x) = {}, tempura::exp(x) = {}, diff = {}",
                    x, std_exp, tempura_exp, std::abs(std_exp - tempura_exp)/std_exp);
      x += 0.1;
    }
  };

  return TestRegistry::result();
}
