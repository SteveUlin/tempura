#include "bayes/beta.h"

#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "Beta prob"_test = [] {
    Beta b{1.0, 1.0};
    expectNear(1.0, b.prob(0.5));
  };

  "Beta sample"_test = [] {
    Beta b{2.0, 2.0};
    std::mt19937 g{0};
    for (int i = 0; i < 100; ++i) {
      auto x = b.sample(g);
      expectGreaterThan(x, 0.0);
      expectLessThan(x, 1.0);
    }
  };

  "Beta logProb"_test = [] {
    Beta b{1.0, 1.0};
    expectNear(0., b.logProb(0.5));
  };
}

