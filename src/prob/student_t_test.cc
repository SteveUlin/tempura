#include "prob/student_t.h"

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
    StudentT dist{5.0};  // t with 5 degrees of freedom

    // At mode x=0, density is maximized
    double at_zero = dist.prob(0.0);
    double at_one = dist.prob(1.0);
    expectTrue(at_zero > at_one);

    // logProb should be consistent
    expectNear(std::log(at_zero), dist.logProb(0.0), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    StudentT dist{5.0};
    // -(ν+1)/2 * log(1 + x²/ν) at x=0 should be 0
    expectNear(0.0, dist.unnormalizedLogProb(0.0), 1e-10);
  };

  "sample mean"_test = [] {
    StudentT dist{10.0};  // Mean exists for ν > 1
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    // Mean should be 0
    expectNear(0.0, mean, 0.2);
  };

  "sample median for nu=1 (Cauchy)"_test = [] {
    StudentT dist{1.0};  // t(1) = Cauchy
    std::mt19937_64 rng{42};

    std::vector<double> samples;
    constexpr int n = 10001;
    for (int i = 0; i < n; ++i) {
      samples.push_back(dist.sample(rng));
    }
    std::ranges::sort(samples);
    double median = samples[n / 2];

    // Median should be 0
    expectNear(0.0, median, 0.2);
  };

  "accessors"_test = [] {
    StudentT dist{5.0};
    expectEq(5.0, dist.nu());
    expectEq(0.0, dist.mean());

    // Variance = ν/(ν-2) = 5/3 for ν > 2
    expectNear(5.0 / 3.0, dist.variance(), 1e-10);
  };

  "mean and variance edge cases"_test = [] {
    // ν ≤ 1: mean is undefined (NaN)
    StudentT dist1{1.0};
    expectTrue(std::isnan(dist1.mean()));
    expectTrue(std::isnan(dist1.variance()));

    // 1 < ν ≤ 2: mean exists but variance is infinite
    StudentT dist2{1.5};
    expectEq(0.0, dist2.mean());
    expectTrue(std::isinf(dist2.variance()));
  };

  return TestRegistry::result();
}
