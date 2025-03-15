#include "bayes/normal.h"

#include <algorithm>
#include <limits>
#include <print>
#include <random>

#include "bayes/random.h"
#include "benchmark.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  auto dist = bayes::Normal(4.0, 8.0);
  auto std_dist = std::normal_distribution<double>(4.0, 8.0);
  volatile double out;
  "std normal"_bench.ops(1000) = [&std_dist, &out,
                                  gen = std::mt19937_64{123456}] mutable {
#pragma GCC unroll 1000
    for (int i = 0; i < 1000; ++i) {
      out = std_dist(gen);
    }
  };

  // Ops per sec: 214,822,771
  "box muller bench"_bench.ops(1000) = [&dist, &out,
                                        gen = std::mt19937_64{123456}] mutable {
#pragma GCC unroll 1000
    for (int i = 0; i < 1000; ++i) {
      out = dist.sample(gen);
    }
  };

  // Ops per sec: 144,529,556
  "qrand2 bench"_bench.ops(1000) = [&dist, &out,
                                    gen = std::mt19937_64{123456}] mutable {
#pragma GCC unroll 1000
    for (int i = 0; i < 1000; ++i) {
      out = dist.ratioOfUniforms(gen);
    }
  };

  return TestRegistry::result();
};
