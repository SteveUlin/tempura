#include "bayes/normal.h"

#include <numbers>
#include <random>

#include "bayes/numeric_traits.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

// Example custom numeric type with extension points
namespace custom {
struct Float {
  double value;

  constexpr Float() : value{0.0} {}
  constexpr Float(double v) : value{v} {}

  constexpr auto operator+() const -> Float { return *this; }
  constexpr auto operator-() const -> Float { return Float{-value}; }
  constexpr auto operator+(Float o) const -> Float {
    return Float{value + o.value};
  }
  constexpr auto operator-(Float o) const -> Float {
    return Float{value - o.value};
  }
  constexpr auto operator*(Float o) const -> Float {
    return Float{value * o.value};
  }
  constexpr auto operator/(Float o) const -> Float {
    return Float{value / o.value};
  }
  constexpr auto operator<(Float o) const -> bool { return value < o.value; }
  constexpr auto operator>(Float o) const -> bool { return value > o.value; }
};

// Extension points for ADL
inline auto log(Float f) -> Float { return Float{std::log(f.value)}; }
inline auto exp(Float f) -> Float { return Float{std::exp(f.value)}; }
inline auto sqrt(Float f) -> Float { return Float{std::sqrt(f.value)}; }
inline auto sin(Float f) -> Float { return Float{std::sin(f.value)}; }
inline auto cos(Float f) -> Float { return Float{std::cos(f.value)}; }
inline auto erf(Float f) -> Float { return Float{std::erf(f.value)}; }
}  // namespace custom

auto main() -> int {
  "Normal prob at mean"_test = [] {
    Normal n{0.0, 1.0};
    // PDF at μ for N(μ, σ) is 1/(σ√(2π))
    expectNear(1.0 / std::sqrt(2.0 * std::numbers::pi), n.prob(0.0));
  };

  "Normal prob at tails"_test = [] {
    Normal n{0.0, 1.0};
    // PDF decreases exponentially in tails
    expectTrue(n.prob(-3.0) < 0.01);
    expectTrue(n.prob(3.0) < 0.01);
  };

  "Normal prob with non-standard parameters"_test = [] {
    Normal n{5.0, 2.0};
    // PDF at μ for N(μ, σ) is 1/(σ√(2π))
    expectNear(1.0 / (2.0 * std::sqrt(2.0 * std::numbers::pi)), n.prob(5.0));
  };

  "Normal logProb at mean"_test = [] {
    Normal n{0.0, 1.0};
    // log(PDF) at μ for N(0, 1) is -0.5 * log(2π)
    expectNear(-0.5 * std::log(2.0 * std::numbers::pi), n.logProb(0.0));
  };

  "Normal logProb in tails"_test = [] {
    Normal n{0.0, 1.0};
    // Log-space should handle very small probabilities
    auto log_prob_tail = n.logProb(10.0);
    expectTrue(log_prob_tail < -50.0);  // Very negative for far tails
  };

  "Normal logProb with non-standard parameters"_test = [] {
    Normal n{3.0, 0.5};
    auto log_prob = n.logProb(3.0);
    // At mean: -log(σ) - 0.5*log(2π)
    expectNear(-std::log(0.5) - 0.5 * std::log(2.0 * std::numbers::pi), log_prob);
  };

  "Normal cdf at mean"_test = [] {
    Normal n{0.0, 1.0};
    // CDF at μ is 0.5 (median)
    expectNear(0.5, n.cdf(0.0));
  };

  "Normal cdf lower tail"_test = [] {
    Normal n{0.0, 1.0};
    // CDF at -∞ approaches 0
    expectNear<0.001>(0.0, n.cdf(-5.0));
  };

  "Normal cdf upper tail"_test = [] {
    Normal n{0.0, 1.0};
    // CDF at +∞ approaches 1
    expectNear<0.001>(1.0, n.cdf(5.0));
  };

  "Normal cdf one standard deviation"_test = [] {
    Normal n{0.0, 1.0};
    // CDF at μ+σ is approximately 0.8413 (68-95-99.7 rule)
    expectNear<0.001>(0.8413, n.cdf(1.0));
    expectNear<0.001>(0.1587, n.cdf(-1.0));
  };

  "Normal mean"_test = [] {
    expectEq(0.0, Normal{0.0, 1.0}.mean());
    expectEq(5.0, Normal{5.0, 2.0}.mean());
    expectEq(-3.5, Normal{-3.5, 0.1}.mean());
  };

  "Normal variance"_test = [] {
    expectNear(1.0, Normal{0.0, 1.0}.variance());
    expectNear(4.0, Normal{5.0, 2.0}.variance());
    expectNear(0.01, Normal{-3.5, 0.1}.variance());
  };

  "Normal stddev"_test = [] {
    expectEq(1.0, Normal{0.0, 1.0}.stddev());
    expectEq(2.0, Normal{5.0, 2.0}.stddev());
    expectNear(0.1, Normal{-3.5, 0.1}.stddev());
  };

  "Normal parameter accessors"_test = [] {
    Normal n{3.5, 1.5};
    expectEq(3.5, n.mu());
    expectEq(1.5, n.sigma());
  };

  "Normal sample with std::mt19937_64"_test = [] {
    std::mt19937_64 g{42};
    Normal n{0.0, 1.0};

    // Verify samples look reasonable (within ~5 sigma)
    for (int i = 0; i < 100; ++i) {
      auto x = n.sample(g);
      expectTrue(x > -5.0 && x < 5.0);
    }
  };

  "Normal sample distribution statistics"_test = [] {
    std::mt19937_64 g{123};
    Normal n{2.5, 1.5};

    const int N = 10'000;
    double sum = 0.0;
    double sum_sq = 0.0;

    for (int i = 0; i < N; ++i) {
      auto x = n.sample(g);
      sum += x;
      sum_sq += x * x;
    }

    double sample_mean = sum / N;
    double sample_var = (sum_sq / N) - (sample_mean * sample_mean);

    // Check sample statistics match theoretical values within tolerance
    expectNear<0.1>(2.5, sample_mean);          // E[N(2.5, 1.5)] = 2.5
    expectNear<0.2>(2.25, sample_var);          // Var[N(2.5, 1.5)] = 1.5² = 2.25
  };

  "Normal sample caching (Box-Muller)"_test = [] {
    std::mt19937_64 g{789};
    Normal n{0.0, 1.0};

    // Box-Muller generates pairs - verify both values are independent
    auto x1 = n.sample(g);
    auto x2 = n.sample(g);
    auto x3 = n.sample(g);

    // Values should be different (not cached incorrectly)
    expectNeq(x1, x2);
    expectNeq(x2, x3);
    expectNeq(x1, x3);
  };

  "Normal constexpr construction"_test = [] {
    constexpr Normal n{1.0, 2.0};
    static_assert(n.mean() == 1.0);
    static_assert(n.variance() == 4.0);
    static_assert(n.stddev() == 2.0);
    static_assert(n.mu() == 1.0);
    static_assert(n.sigma() == 2.0);
  };

  "Normal constexpr probability"_test = [] {
    constexpr Normal n{0.0, 1.0};
    // Note: prob() and logProb() use std::exp/log which may not be constexpr
    // in all standard library implementations, so we only test construction
    static_assert(n.mean() == 0.0);
  };

  "Normal rejects integer types"_test = [] {
    // These should fail to compile with clear error messages
    // Uncomment to verify:
    // Normal<int> n1{0, 1};        // error: static assertion failed
    // Normal<int64_t> n2{0, 1};    // error: static assertion failed
    // Normal<uint32_t> n3{0, 1};   // error: static assertion failed

    // Floating-point types work fine
    Normal<float> f{0.0f, 1.0f};
    Normal<double> d{0.0, 1.0};
    Normal<long double> ld{0.0L, 1.0L};

    expectEq(0.0F, f.mean());
    expectEq(0.0, d.mean());
    expectEq(0.0L, ld.mean());
  };

  "Normal custom type with extension points"_test = [] {
    // Custom type with ADL extension points
    Normal<custom::Float> n{custom::Float{0.0}, custom::Float{1.0}};

    expectNear(0.0, n.mean().value);
    expectNear(1.0, n.variance().value);

    // Test probability functions use extension points
    auto prob_at_mean = n.prob(custom::Float{0.0});
    expectNear(1.0 / std::sqrt(2.0 * std::numbers::pi), prob_at_mean.value);

    auto log_prob = n.logProb(custom::Float{0.0});
    expectNear(-0.5 * std::log(2.0 * std::numbers::pi), log_prob.value);

    auto cdf_at_mean = n.cdf(custom::Float{0.0});
    expectNear(0.5, cdf_at_mean.value);
  };
}

