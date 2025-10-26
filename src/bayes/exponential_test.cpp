#include "bayes/exponential.h"

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
  constexpr auto operator<=(Float o) const -> bool { return value <= o.value; }
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
  "Exponential prob at zero"_test = [] {
    Exponential dist{1.0};
    // At x = 0, PDF = λ
    expectNear(1.0, dist.prob(0.0));
  };

  "Exponential prob at mean"_test = [] {
    Exponential dist{2.0};
    // At x = 1/λ = 0.5, PDF = λ·exp(-1) = 2·exp(-1)
    expectNear(2.0 * std::exp(-1.0), dist.prob(0.5));
  };

  "Exponential prob decays exponentially"_test = [] {
    Exponential dist{1.0};
    // PDF decreases exponentially with x
    auto p0 = dist.prob(0.0);
    auto p1 = dist.prob(1.0);
    auto p2 = dist.prob(2.0);
    expectTrue(p0 > p1);
    expectTrue(p1 > p2);
    expectNear(p1 / p0, std::exp(-1.0));
  };

  "Exponential prob negative values"_test = [] {
    Exponential dist{1.0};
    // PDF is zero for x < 0
    expectEq(0.0, dist.prob(-1.0));
    expectEq(0.0, dist.prob(-100.0));
  };

  "Exponential prob with different rates"_test = [] {
    Exponential dist1{1.0};
    Exponential dist2{2.0};
    // Higher rate => faster decay
    expectTrue(dist2.prob(0.0) > dist1.prob(0.0));
    expectTrue(dist2.prob(1.0) < dist1.prob(1.0));
  };

  "Exponential logProb at zero"_test = [] {
    Exponential dist{1.0};
    // log(λ) at x = 0
    expectNear(0.0, dist.logProb(0.0));
  };

  "Exponential logProb consistency with prob"_test = [] {
    Exponential dist{1.5};
    for (double x : {0.0, 0.5, 1.0, 2.0, 5.0, 10.0}) {
      expectNear(std::log(dist.prob(x)), dist.logProb(x));
    }
  };

  "Exponential logProb avoids underflow"_test = [] {
    Exponential dist{1.0};
    // For large x, prob() underflows but logProb() stays finite
    auto log_p = dist.logProb(1000.0);
    expectTrue(std::isfinite(log_p));
    expectTrue(log_p < -100.0);  // Should be very negative
  };

  "Exponential logProb negative values"_test = [] {
    Exponential dist{1.0};
    // logProb should be -infinity for x < 0
    auto log_p = dist.logProb(-1.0);
    expectTrue(std::isinf(log_p));
    expectTrue(log_p < 0.0);
  };

  "Exponential cdf at zero"_test = [] {
    Exponential dist{1.0};
    // CDF(0) = 0
    expectNear(0.0, dist.cdf(0.0));
  };

  "Exponential cdf at mean"_test = [] {
    Exponential dist{2.0};
    // CDF(1/λ) = 1 - exp(-1) ≈ 0.632
    expectNear(1.0 - std::exp(-1.0), dist.cdf(0.5));
  };

  "Exponential cdf approaches 1"_test = [] {
    Exponential dist{1.0};
    // CDF approaches 1 as x → ∞
    expectTrue(dist.cdf(10.0) > 0.99);
    expectTrue(dist.cdf(100.0) > 0.999999);
  };

  "Exponential cdf monotonic"_test = [] {
    Exponential dist{1.0};
    // CDF is strictly increasing for x ≥ 0
    expectTrue(dist.cdf(0.0) < dist.cdf(0.5));
    expectTrue(dist.cdf(0.5) < dist.cdf(1.0));
    expectTrue(dist.cdf(1.0) < dist.cdf(2.0));
    expectTrue(dist.cdf(2.0) < dist.cdf(10.0));
  };

  "Exponential cdf negative values"_test = [] {
    Exponential dist{1.0};
    // CDF is zero for x < 0
    expectEq(0.0, dist.cdf(-1.0));
    expectEq(0.0, dist.cdf(-100.0));
  };

  "Exponential invCdf at median"_test = [] {
    Exponential dist{1.0};
    // Median = ln(2)/λ = ln(2) for λ = 1
    expectNear(std::log(2.0), dist.invCdf(0.5));
  };

  "Exponential invCdf roundtrip"_test = [] {
    Exponential dist{1.5};
    for (double p : {0.1, 0.25, 0.5, 0.75, 0.9, 0.99}) {
      auto x = dist.invCdf(p);
      expectNear(p, dist.cdf(x));
    }
  };

  "Exponential invCdf at bounds"_test = [] {
    Exponential dist{1.0};
    // invCDF(0) = 0
    expectNear(0.0, dist.invCdf(0.0));
    // invCDF(1) = ∞ (but we can test high probabilities)
    expectTrue(dist.invCdf(0.9999) > 9.0);
  };

  "Exponential mean formula"_test = [] {
    expectNear(1.0, Exponential{1.0}.mean());
    expectNear(0.5, Exponential{2.0}.mean());
    expectNear(2.0, Exponential{0.5}.mean());
  };

  "Exponential variance formula"_test = [] {
    expectNear(1.0, Exponential{1.0}.variance());
    expectNear(0.25, Exponential{2.0}.variance());
    expectNear(4.0, Exponential{0.5}.variance());
  };

  "Exponential memoryless property"_test = [] {
    // P(T > s + t | T > s) = P(T > t)
    // Equivalently: P(T > s + t) / P(T > s) = P(T > t)
    Exponential dist{1.0};
    double s = 2.0;
    double t = 3.0;

    // P(T > x) = 1 - CDF(x) = exp(-λx)
    auto prob_greater = [&](double x) { return 1.0 - dist.cdf(x); };

    double lhs = prob_greater(s + t) / prob_greater(s);
    double rhs = prob_greater(t);
    expectNear(lhs, rhs);
  };

  "Exponential accessor"_test = [] {
    Exponential dist{2.5};
    expectEq(2.5, dist.lambda());
  };

  "Exponential sample with std::mt19937_64"_test = [] {
    std::mt19937_64 gen{42};
    Exponential dist{1.0};

    // Generate samples - all should be non-negative and finite
    for (int i = 0; i < 1000; ++i) {
      auto x = dist.sample(gen);
      expectTrue(x >= 0.0);
      expectTrue(std::isfinite(x));
    }
  };

  "Exponential sample mean approximation"_test = [] {
    std::mt19937_64 gen{123};
    Exponential dist{2.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      sum += dist.sample(gen);
    }
    double sample_mean = sum / N;

    // Sample mean should approximate theoretical mean (1/λ = 0.5)
    expectNear<0.05>(0.5, sample_mean);
  };

  "Exponential sample variance approximation"_test = [] {
    std::mt19937_64 gen{456};
    Exponential dist{1.0};

    const int N = 10'000;
    std::vector<double> samples;
    samples.reserve(N);
    double sum = 0.0;

    for (int i = 0; i < N; ++i) {
      auto x = dist.sample(gen);
      samples.push_back(x);
      sum += x;
    }

    double mean = sum / N;
    double sum_sq_diff = 0.0;
    for (auto x : samples) {
      auto diff = x - mean;
      sum_sq_diff += diff * diff;
    }
    double sample_variance = sum_sq_diff / (N - 1);

    // Sample variance should approximate theoretical variance (1/λ² = 1.0)
    expectNear<0.1>(1.0, sample_variance);
  };

  "Exponential sample different parameters"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{100};
    Exponential dist1{1.0};
    Exponential dist2{5.0};

    auto x1 = dist1.sample(g1);
    auto x2 = dist2.sample(g2);

    // Different parameters should (almost always) produce different samples
    expectTrue(x1 != x2);
  };

  "Exponential sample different seeds produce different sequences"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{200};
    Exponential dist{1.0};

    auto x1 = dist.sample(g1);
    auto x2 = dist.sample(g2);

    // Different seeds should (almost always) produce different samples
    expectTrue(x1 != x2);
  };

  "Exponential constexpr construction"_test = [] {
    constexpr Exponential dist{2.0};
    static_assert(dist.lambda() == 2.0);
    static_assert(dist.mean() == 0.5);
    static_assert(dist.variance() == 0.25);
  };

  "Exponential constexpr probability"_test = [] {
    constexpr Exponential dist{1.0};
    // At x = 0, PDF = λ = 1.0
    static_assert(dist.prob(0.0) == 1.0);
    // CDF(0) = 0
    static_assert(dist.cdf(0.0) == 0.0);
  };

  "Exponential rejects integer types"_test = [] {
    // These should fail to compile with clear error messages
    // Uncomment to verify:
    // Exponential<int> d1{1};        // error: static assertion failed
    // Exponential<int64_t> d2{1};    // error: static assertion failed
    // Exponential<uint32_t> d3{1};   // error: static assertion failed

    // Floating-point types work fine
    Exponential<float> f{1.0f};
    Exponential<double> d{1.0};
    Exponential<long double> ld{1.0L};

    expectEq(1.0F, f.lambda());
    expectEq(1.0, d.lambda());
    expectEq(1.0L, ld.lambda());
  };

  "Exponential custom type with extension points"_test = [] {
    // Custom type with numeric_infinity, log, and exp
    Exponential<custom::Float> dist{custom::Float{1.0}};

    expectNear(1.0, dist.lambda().value);
    expectNear(1.0, dist.mean().value);
    expectNear(1.0, dist.variance().value);

    // prob uses exp extension point
    expectNear(1.0, dist.prob(custom::Float{0.0}).value);

    // logProb uses log and numeric_infinity extension points
    auto log_p = dist.logProb(custom::Float{0.0});
    expectNear(0.0, log_p.value);

    auto log_p_neg = dist.logProb(custom::Float{-1.0});
    expectTrue(std::isinf(log_p_neg.value));

    // cdf uses exp extension point
    expectNear(0.0, dist.cdf(custom::Float{0.0}).value);

    // invCdf uses log extension point
    auto x = dist.invCdf(custom::Float{0.5});
    expectNear(std::log(2.0), x.value);
  };

  "Exponential PDF integrates to 1"_test = [] {
    // Numerical integration check: ∫₀^∞ p(x) dx ≈ 1
    Exponential dist{1.0};

    double integral = 0.0;
    const double dx = 0.01;
    // Integrate from 0 to 10 (captures >99.99% of the mass)
    for (double x = 0.0; x <= 10.0; x += dx) {
      integral += dist.prob(x) * dx;
    }

    expectNear<0.01>(1.0, integral);
  };

  "Exponential CDF derivative equals PDF"_test = [] {
    // Numerical derivative check: dF/dx ≈ p(x)
    Exponential dist{1.0};

    const double h = 1e-5;
    for (double x : {0.1, 0.5, 1.0, 2.0, 5.0}) {
      // Numerical derivative of CDF
      double numerical_deriv = (dist.cdf(x + h) - dist.cdf(x - h)) / (2.0 * h);
      double pdf = dist.prob(x);
      expectNear<0.001>(pdf, numerical_deriv);
    }
  };

  "Exponential median formula"_test = [] {
    // Median = ln(2)/λ
    Exponential dist{1.0};
    double median = dist.invCdf(0.5);
    expectNear(std::log(2.0), median);

    Exponential dist2{2.0};
    double median2 = dist2.invCdf(0.5);
    expectNear(std::log(2.0) / 2.0, median2);
  };

  "Exponential PDF formula verification"_test = [] {
    // Verify PDF formula: p(x|λ) = λ exp(-λx)
    Exponential dist{1.5};

    auto x = 2.0;
    auto λ = 1.5;
    auto expected = λ * std::exp(-λ * x);

    expectNear(expected, dist.prob(x));
  };

  "Exponential hazard rate is constant"_test = [] {
    // Hazard function h(x) = p(x) / (1 - CDF(x)) = λ (constant!)
    Exponential dist{2.0};

    for (double x : {0.1, 0.5, 1.0, 2.0, 5.0}) {
      double hazard = dist.prob(x) / (1.0 - dist.cdf(x));
      expectNear(2.0, hazard);
    }
  };

  "Exponential scale invariance"_test = [] {
    // If X ~ Exp(λ), then cX ~ Exp(λ/c)
    std::mt19937_64 g1{789};
    std::mt19937_64 g2{789};
    Exponential dist1{1.0};
    Exponential dist2{0.5};

    auto x1 = dist1.sample(g1);
    auto x2 = dist2.sample(g2);

    // x2 should be approximately 2 * x1
    expectNear<0.5>(2.0 * x1, x2);
  };
}
