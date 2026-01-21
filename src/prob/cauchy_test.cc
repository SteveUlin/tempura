#include "prob/cauchy.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    Cauchy dist{0.0, 1.0};

    // At mode: p(0) = 1/(πγ) = 1/π
    expectNear(1.0 / std::numbers::pi, dist.prob(0.0), 1e-10);

    // log(p(0)) = -log(π)
    expectNear(-std::log(std::numbers::pi), dist.logProb(0.0), 1e-10);

    // At x=1: p(1) = 1/(π·2) = 1/(2π)
    expectNear(1.0 / (2.0 * std::numbers::pi), dist.prob(1.0), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    Cauchy dist{0.0, 1.0};
    expectNear(0.0, dist.unnormalizedLogProb(0.0), 1e-10);
    expectNear(-std::log(2.0), dist.unnormalizedLogProb(1.0), 1e-10);
  };

  "sample median"_test = [] {
    Cauchy dist{5.0, 1.0};
    std::mt19937_64 rng{42};

    std::vector<double> samples;
    constexpr int n = 10001;
    for (int i = 0; i < n; ++i) {
      samples.push_back(dist.sample(rng));
    }
    std::ranges::sort(samples);
    double median = samples[n / 2];

    // Median should be close to location parameter
    expectNear(5.0, median, 0.2);
  };

  "cdf and invCdf"_test = [] {
    Cauchy dist{0.0, 1.0};
    expectNear(0.5, dist.cdf(0.0), 1e-10);
    expectNear(0.75, dist.cdf(1.0), 1e-10);

    expectNear(0.0, dist.invCdf(0.5), 1e-10);
    expectNear(1.0, dist.invCdf(0.75), 1e-10);
  };

  "accessors"_test = [] {
    Cauchy dist{3.0, 2.0};
    expectEq(3.0, dist.location());
    expectEq(2.0, dist.scale());
    expectEq(3.0, dist.median());
  };

  return TestRegistry::result();
}
