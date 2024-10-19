#include "bayes/normal.h"
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "Normal prob"_test = [] {
    Normal n{0.0, 1.0};
    expectNear(1. / sqrt(2 * std::numbers::pi), n.prob(0.));
  };

  "Normal logProb"_test = [] {
    Normal n{0.0, 1.0};
    expectNear(-0.5 * log(2 * std::numbers::pi), n.logProb(0.));
  };

  "Normal cdf"_test = [] {
    Normal n{0.0, 1.0};
    expectNear(0.5, n.cdf(0.));
  };

  "Normal mean"_test = [] {
    Normal n{0.0, 1.0};
    expectEq(0.0, n.mean());
  };

  "Normal stddev"_test = [] {
    Normal n{0.0, 1.0};
    expectEq(1.0, n.stddev());
  };

  "Normal variance"_test = [] {
    Normal n{0.0, 1.0};
    expectEq(1.0, n.variance());
  };

  "Normal sample"_test = [] {
    Normal n{0.0, 1.0};
    std::mt19937_64 g;
    g.seed(0);
    expectNeq(0.0, n.sample(g));
  };
}

