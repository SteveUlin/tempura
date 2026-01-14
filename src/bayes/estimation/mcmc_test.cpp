#include "bayes/estimation/mcmc.h"

#include <cmath>
#include <limits>
#include <random>
#include <ranges>
#include <vector>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

// Helper to extract .value from Sample - used with std::views::transform
constexpr auto getValue = [](const auto& sample) { return sample.value; };

auto main() -> int {
  "chain is an input_range"_test = [] {
    auto log_target = [](double x) { return -x * x / 2; };
    auto c = chain<double>(log_target, GaussianWalk{0.5}, std::mt19937{42}, 0.0);

    static_assert(std::ranges::input_range<decltype(c)>);
    // Chain is NOT a view (not copyable) - that's intentional
  };

  "basic sampling from N(0,1)"_test = [] {
    auto log_target = [](double x) { return -x * x / 2; };
    auto c = chain<double>(log_target, GaussianWalk{1.0}, std::mt19937{42}, 0.0);

    // Burn-in + collect using standard range operations
    auto vec = std::move(c)
        | std::views::drop(1000)
        | std::views::take(10000)
        | std::views::transform(getValue)
        | std::ranges::to<std::vector>();

    expectEq(vec.size(), 10000uz);

    // Compute sample mean and variance
    double sum = 0;
    for (double x : vec) sum += x;
    double mean = sum / static_cast<double>(vec.size());

    double sum_sq = 0;
    for (double x : vec) sum_sq += (x - mean) * (x - mean);
    double variance = sum_sq / static_cast<double>(vec.size() - 1);

    // Should be approximately N(0,1)
    // SE of mean ≈ 1/√10000 = 0.01, tolerance of 0.1 is ~10 SE
    expectNear(mean, 0.0, 0.1);
    expectNear(variance, 1.0, 0.2);
  };

  "stride view reduces samples"_test = [] {
    auto log_target = [](double x) { return -x * x / 2; };
    auto c = chain<double>(log_target, GaussianWalk{1.0}, std::mt19937{42}, 0.0);

    auto vec = std::move(c)
        | std::views::drop(100)
        | std::views::stride(10)
        | std::views::take(100)
        | std::ranges::to<std::vector>();

    expectEq(vec.size(), 100uz);
  };

  "Sample contains metadata"_test = [] {
    auto log_target = [](double x) { return -x * x / 2; };
    auto c = chain<double>(log_target, GaussianWalk{0.5}, std::mt19937{42}, 0.0);

    auto vec = std::move(c)
        | std::views::take(10)
        | std::ranges::to<std::vector>();

    for (const auto& sample : vec) {
      expectTrue(std::isfinite(sample.log_prob));
      // sample.accepted is either true or false - just check it compiles
      [[maybe_unused]] bool acc = sample.accepted;
    }
  };

  "acceptance rate computation"_test = [] {
    auto log_target = [](double x) { return -x * x / 2; };
    auto c = chain<double>(log_target, GaussianWalk{1.0}, std::mt19937{42}, 0.0);

    auto vec = std::move(c)
        | std::views::drop(100)
        | std::views::take(1000)
        | std::ranges::to<std::vector>();

    double rate = acceptanceRate(vec);

    // For N(0,1) with σ=1 proposal, optimal rate ~0.44 for 1D
    // Allow wide tolerance since this is stochastic
    expectTrue(rate > 0.2 && rate < 0.8);
  };

  "shifted normal N(5, 4)"_test = [] {
    // N(μ=5, σ²=4) => log p(x) = -(x-5)²/8 + const
    auto log_target = [](double x) { return -(x - 5) * (x - 5) / 8; };
    auto c = chain<double>(log_target, GaussianWalk{2.0}, std::mt19937{123}, 5.0);

    auto vec = std::move(c)
        | std::views::drop(1000)
        | std::views::take(10000)
        | std::views::transform(getValue)
        | std::ranges::to<std::vector>();

    double sum = 0;
    for (double x : vec) sum += x;
    double mean = sum / static_cast<double>(vec.size());

    double sum_sq = 0;
    for (double x : vec) sum_sq += (x - mean) * (x - mean);
    double variance = sum_sq / static_cast<double>(vec.size() - 1);

    expectNear(mean, 5.0, 0.2);
    expectNear(variance, 4.0, 0.5);
  };

  "bounded support (exponential)"_test = [] {
    // Exponential(λ=1): p(x) = e^(-x) for x > 0
    auto log_target = [](double x) {
      if (x <= 0) return -std::numeric_limits<double>::infinity();
      return -x;
    };
    auto c = chain<double>(log_target, GaussianWalk{1.0}, std::mt19937{456}, 1.0);

    auto vec = std::move(c)
        | std::views::drop(1000)
        | std::views::take(10000)
        | std::views::transform(getValue)
        | std::ranges::to<std::vector>();

    // All samples should be positive
    for (double x : vec) {
      expectTrue(x > 0);
    }

    // Mean should be ~1 for Exp(1)
    double sum = 0;
    for (double x : vec) sum += x;
    double mean = sum / static_cast<double>(vec.size());
    expectNear(mean, 1.0, 0.1);
  };

  "2D sampling with array state"_test = [] {
    using State = std::array<double, 2>;

    // Independent N(0,1) in each dimension
    auto log_target = [](const State& x) {
      return -(x[0] * x[0] + x[1] * x[1]) / 2;
    };

    auto c = chain<State>(log_target, GaussianWalkND<double, 2>{1.0},
                          std::mt19937{789}, State{0.0, 0.0});

    auto vec = std::move(c)
        | std::views::drop(1000)
        | std::views::take(5000)
        | std::views::transform(getValue)
        | std::ranges::to<std::vector>();

    expectEq(vec.size(), 5000uz);

    // Check means
    double sum0 = 0, sum1 = 0;
    for (const auto& x : vec) {
      sum0 += x[0];
      sum1 += x[1];
    }
    double mean0 = sum0 / static_cast<double>(vec.size());
    double mean1 = sum1 / static_cast<double>(vec.size());

    expectNear(mean0, 0.0, 0.1);
    expectNear(mean1, 0.0, 0.1);
  };

  "composability: drop + stride + take + transform"_test = [] {
    auto log_target = [](double x) { return -x * x / 2; };
    auto c = chain<double>(log_target, GaussianWalk{1.0}, std::mt19937{42}, 0.0);

    // Full pipeline composition using standard adaptors
    auto vec = std::move(c)
        | std::views::drop(500)    // burn-in
        | std::views::stride(5)    // thinning
        | std::views::take(200)    // limit
        | std::views::transform(getValue)  // extract values
        | std::ranges::to<std::vector>();

    expectEq(vec.size(), 200uz);
  };

  "reproducibility with same seed"_test = [] {
    auto log_target = [](double x) { return -x * x / 2; };

    auto c1 = chain<double>(log_target, GaussianWalk{1.0}, std::mt19937{42}, 0.0);
    auto c2 = chain<double>(log_target, GaussianWalk{1.0}, std::mt19937{42}, 0.0);

    auto vec1 = std::move(c1)
        | std::views::take(100)
        | std::views::transform(getValue)
        | std::ranges::to<std::vector>();

    auto vec2 = std::move(c2)
        | std::views::take(100)
        | std::views::transform(getValue)
        | std::ranges::to<std::vector>();

    expectEq(vec1.size(), vec2.size());
    for (std::size_t i = 0; i < vec1.size(); ++i) {
      expectEq(vec1[i], vec2[i]);
    }
  };

  "tuple state with TupleWalk"_test = [] {
    using State = std::tuple<double, double>;

    auto log_target = [](const State& s) {
      auto [x, y] = s;
      return -(x * x + y * y) / 2;
    };

    auto c = chain<State>(
        log_target,
        TupleWalk{GaussianWalk{0.5}, GaussianWalk{1.0}},
        std::mt19937{999},
        State{0.0, 0.0});

    auto vec = std::move(c)
        | std::views::drop(500)
        | std::views::take(5000)
        | std::views::transform(getValue)
        | std::ranges::to<std::vector>();

    expectEq(vec.size(), 5000uz);

    // Check means are near zero
    double sum0 = 0, sum1 = 0;
    for (const auto& [x, y] : vec) {
      sum0 += x;
      sum1 += y;
    }
    expectNear(sum0 / 5000.0, 0.0, 0.1);
    expectNear(sum1 / 5000.0, 0.0, 0.1);
  };

  "tuple state with HomogeneousWalk"_test = [] {
    using State = std::tuple<double, double, double>;

    auto log_target = [](const State& s) {
      auto [x, y, z] = s;
      return -(x * x + y * y + z * z) / 2;
    };

    auto c = chain<State>(
        log_target,
        HomogeneousWalk{GaussianWalk{0.8}},
        std::mt19937{888},
        State{0.0, 0.0, 0.0});

    auto vec = std::move(c)
        | std::views::drop(500)
        | std::views::take(3000)
        | std::views::transform(getValue)
        | std::ranges::to<std::vector>();

    expectEq(vec.size(), 3000uz);
  };

  return TestRegistry::result();
}
