#include "bayes/logistic.h"

#include <cmath>
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
  constexpr auto operator>=(Float o) const -> bool { return value >= o.value; }
  constexpr auto operator==(Float o) const -> bool { return value == o.value; }
  constexpr auto operator!=(Float o) const -> bool { return value != o.value; }
};

// Extension points (found via ADL)
constexpr auto numeric_infinity(Float) -> Float {
  return Float{std::numeric_limits<double>::infinity()};
}

inline auto log(Float f) -> Float { return Float{std::log(f.value)}; }
inline auto exp(Float f) -> Float { return Float{std::exp(f.value)}; }
}  // namespace custom

auto main() -> int {
  "Logistic prob at mean"_test = [] {
    Logistic dist{0.0, 1.0};
    // At x = μ, PDF = 1/(4s) = 0.25 for s = 1
    expectNear(0.25, dist.prob(0.0));
  };

  "Logistic prob symmetric around mean"_test = [] {
    Logistic dist{5.0, 2.0};
    // PDF should be symmetric: p(μ + δ) = p(μ - δ)
    expectNear(dist.prob(7.0), dist.prob(3.0));
    expectNear(dist.prob(8.0), dist.prob(2.0));
  };

  "Logistic prob with different parameters"_test = [] {
    Logistic dist{0.0, 2.0};
    // At μ, PDF = 1/(4s) = 1/8 = 0.125
    expectNear(0.125, dist.prob(0.0));
  };

  "Logistic prob far from mean"_test = [] {
    Logistic dist{0.0, 1.0};
    // PDF decays exponentially in tails
    auto p_at_5 = dist.prob(5.0);
    auto p_at_10 = dist.prob(10.0);
    expectTrue(p_at_5 > p_at_10);
    expectTrue(p_at_10 > 0.0);
  };

  "Logistic logProb at mean"_test = [] {
    Logistic dist{0.0, 1.0};
    // log(1/4) = -log(4)
    expectNear(-std::log(4.0), dist.logProb(0.0));
  };

  "Logistic logProb consistency with prob"_test = [] {
    Logistic dist{2.0, 3.0};
    for (double x : {-5.0, 0.0, 2.0, 5.0, 10.0}) {
      expectNear(std::log(dist.prob(x)), dist.logProb(x));
    }
  };

  "Logistic logProb avoids underflow"_test = [] {
    Logistic dist{0.0, 1.0};
    // At extreme values, prob() may underflow but logProb() stays finite
    auto log_p = dist.logProb(50.0);
    expectTrue(std::isfinite(log_p));
    expectTrue(log_p < -40.0);  // Should be very negative
  };

  "Logistic cdf at mean"_test = [] {
    Logistic dist{0.0, 1.0};
    // CDF(μ) = 0.5 for any μ and s
    expectNear(0.5, dist.cdf(0.0));
  };

  "Logistic cdf symmetric around mean"_test = [] {
    Logistic dist{5.0, 2.0};
    // CDF(μ - δ) + CDF(μ + δ) = 1 (by symmetry)
    expectNear(1.0, dist.cdf(3.0) + dist.cdf(7.0));
    expectNear(1.0, dist.cdf(2.0) + dist.cdf(8.0));
  };

  "Logistic cdf bounds"_test = [] {
    Logistic dist{0.0, 1.0};
    // CDF approaches 0 as x → -∞
    expectTrue(dist.cdf(-20.0) < 0.001);
    // CDF approaches 1 as x → +∞
    expectTrue(dist.cdf(20.0) > 0.999);
  };

  "Logistic cdf monotonic"_test = [] {
    Logistic dist{0.0, 1.0};
    // CDF is strictly increasing
    expectTrue(dist.cdf(-2.0) < dist.cdf(-1.0));
    expectTrue(dist.cdf(-1.0) < dist.cdf(0.0));
    expectTrue(dist.cdf(0.0) < dist.cdf(1.0));
    expectTrue(dist.cdf(1.0) < dist.cdf(2.0));
  };

  "Logistic mean"_test = [] {
    expectEq(0.0, Logistic{0.0, 1.0}.mean());
    expectEq(5.0, Logistic{5.0, 2.0}.mean());
    expectEq(-3.0, Logistic{-3.0, 0.5}.mean());
  };

  "Logistic variance"_test = [] {
    // Var[Logistic(μ, s)] = (π²s²)/3
    Logistic dist1{0.0, 1.0};
    const double pi_sq_over_3 = std::numbers::pi * std::numbers::pi / 3.0;
    expectNear(pi_sq_over_3, dist1.variance());

    Logistic dist2{0.0, 2.0};
    expectNear(4.0 * pi_sq_over_3, dist2.variance());
  };

  "Logistic accessors"_test = [] {
    Logistic dist{1.5, 3.5};
    expectEq(1.5, dist.mu());
    expectEq(3.5, dist.sigma());
  };

  "Logistic sample with std::mt19937_64"_test = [] {
    std::mt19937_64 g{42};
    Logistic dist{0.0, 1.0};

    // Verify samples have reasonable range (most within ±10 for s=1)
    int in_range = 0;
    for (int i = 0; i < 100; ++i) {
      auto x = dist.sample(g);
      if (x >= -10.0 && x <= 10.0) {
        ++in_range;
      }
    }
    // Most samples should be in reasonable range (tails are rare but possible)
    expectTrue(in_range >= 90);
  };

  "Logistic sample distribution statistics"_test = [] {
    std::mt19937_64 g{123};
    Logistic dist{5.0, 2.0};

    const int N = 10'000;
    double sum = 0.0;
    double sum_sq = 0.0;

    for (int i = 0; i < N; ++i) {
      auto x = dist.sample(g);
      sum += x;
      sum_sq += x * x;
    }

    double sample_mean = sum / N;
    double sample_var = (sum_sq / N) - (sample_mean * sample_mean);

    // Check sample statistics match theoretical values
    const double expected_mean = 5.0;
    const double pi_sq_over_3 = std::numbers::pi * std::numbers::pi / 3.0;
    const double expected_var = 4.0 * pi_sq_over_3;

    expectNear<0.1>(expected_mean, sample_mean);
    expectNear<0.3>(expected_var, sample_var);
  };

  "Logistic sample different seeds produce different sequences"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{200};
    Logistic dist{0.0, 1.0};

    auto x1 = dist.sample(g1);
    auto x2 = dist.sample(g2);

    // Different seeds should (almost always) produce different samples
    expectTrue(x1 != x2);
  };

  "Logistic constexpr construction"_test = [] {
    constexpr Logistic dist{0.0, 1.0};
    static_assert(dist.mean() == 0.0);
    static_assert(dist.mu() == 0.0);
    static_assert(dist.sigma() == 1.0);
  };

  "Logistic constexpr probability"_test = [] {
    constexpr Logistic dist{0.0, 1.0};
    // At μ, PDF = 1/(4s) = 0.25
    static_assert(dist.prob(0.0) == 0.25);
    static_assert(dist.cdf(0.0) == 0.5);
  };

  "Logistic constexpr variance"_test = [] {
    constexpr Logistic dist{0.0, 1.0};
    constexpr double pi_sq_over_3 =
        std::numbers::pi * std::numbers::pi / 3.0;
    static_assert(dist.variance() == pi_sq_over_3);
  };

  "Logistic rejects integer types"_test = [] {
    // These should fail to compile with clear error messages
    // Uncomment to verify:
    // Logistic<int> d1{0, 1};        // error: static assertion failed
    // Logistic<int64_t> d2{0, 1};    // error: static assertion failed
    // Logistic<uint32_t> d3{0, 1};   // error: static assertion failed

    // Floating-point types work fine
    Logistic<float> f{0.0f, 1.0f};
    Logistic<double> d{0.0, 1.0};
    Logistic<long double> ld{0.0L, 1.0L};

    expectEq(0.0F, f.mean());
    expectEq(0.0, d.mean());
    expectEq(0.0L, ld.mean());
  };

  "Logistic custom type with extension points"_test = [] {
    // Custom type with numeric_infinity, log, and exp extension points
    Logistic<custom::Float> dist{custom::Float{0.0}, custom::Float{1.0}};

    expectNear(0.0, dist.mean().value);
    expectNear(0.25, dist.prob(custom::Float{0.0}).value);

    // CDF at mean should be 0.5
    expectNear(0.5, dist.cdf(custom::Float{0.0}).value);

    // logProb uses log and exp extension points
    auto log_prob = dist.logProb(custom::Float{0.0});
    expectNear(-std::log(4.0), log_prob.value);
  };

  "Logistic PDF integrates to 1"_test = [] {
    // Numerical integration check: ∫ p(x) dx ≈ 1
    Logistic dist{0.0, 1.0};

    double integral = 0.0;
    const double dx = 0.01;
    // Integrate from -20 to +20 (captures most of the mass)
    for (double x = -20.0; x <= 20.0; x += dx) {
      integral += dist.prob(x) * dx;
    }

    expectNear<0.01>(1.0, integral);
  };

  "Logistic CDF derivative equals PDF"_test = [] {
    // Numerical derivative check: dF/dx ≈ p(x)
    Logistic dist{0.0, 1.0};

    const double h = 1e-5;
    for (double x : {-5.0, -1.0, 0.0, 1.0, 5.0}) {
      // Numerical derivative of CDF
      double numerical_deriv = (dist.cdf(x + h) - dist.cdf(x - h)) / (2.0 * h);
      double pdf = dist.prob(x);
      expectNear<0.001>(pdf, numerical_deriv);
    }
  };
}
