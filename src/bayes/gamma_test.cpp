#include "bayes/gamma.h"

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
  constexpr auto operator+=(Float o) -> Float& {
    value += o.value;
    return *this;
  }
  constexpr auto operator*=(Float o) -> Float& {
    value *= o.value;
    return *this;
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

inline auto abs(Float f) -> Float { return Float{std::abs(f.value)}; }
inline auto log(Float f) -> Float { return Float{std::log(f.value)}; }
inline auto exp(Float f) -> Float { return Float{std::exp(f.value)}; }
inline auto pow(Float f, Float p) -> Float {
  return Float{std::pow(f.value, p.value)};
}
inline auto sqrt(Float f) -> Float { return Float{std::sqrt(f.value)}; }
inline auto lgamma(Float f) -> Float { return Float{std::lgamma(f.value)}; }
}  // namespace custom

auto main() -> int {
  "Gamma prob at mode for α > 1"_test = [] {
    // Mode = (α - 1) / β for α ≥ 1
    Gamma dist{2.0, 1.0};
    auto mode = (2.0 - 1.0) / 1.0;  // = 1.0
    auto p = dist.prob(mode);
    expectTrue(p > 0.0);
    expectTrue(std::isfinite(p));
  };

  "Gamma prob at mean"_test = [] {
    Gamma dist{2.0, 1.0};
    auto mean = 2.0 / 1.0;  // = 2.0
    auto p = dist.prob(mean);
    expectTrue(p > 0.0);
    expectTrue(std::isfinite(p));
  };

  "Gamma prob decreases for large x"_test = [] {
    Gamma dist{2.0, 1.0};
    auto p1 = dist.prob(1.0);
    auto p2 = dist.prob(2.0);
    auto p3 = dist.prob(5.0);
    expectTrue(p1 > p2);
    expectTrue(p2 > p3);
  };

  "Gamma prob negative values"_test = [] {
    Gamma dist{2.0, 1.0};
    expectEq(0.0, dist.prob(-1.0));
    expectEq(0.0, dist.prob(-100.0));
  };

  "Gamma prob at zero"_test = [] {
    Gamma dist{2.0, 1.0};
    expectEq(0.0, dist.prob(0.0));
  };

  "Gamma prob with different parameters"_test = [] {
    Gamma dist1{2.0, 1.0};
    Gamma dist2{3.0, 2.0};
    // Different parameters => different PDF values
    expectTrue(dist1.prob(1.0) != dist2.prob(1.0));
  };

  "Gamma prob Exponential special case"_test = [] {
    // Gamma(1, λ) = Exponential(λ)
    // PDF at x: λ exp(-λx)
    Gamma dist{1.0, 2.0};
    auto x = 1.0;
    auto expected = 2.0 * std::exp(-2.0 * x);
    expectNear(expected, dist.prob(x));
  };

  "Gamma logProb consistency with prob"_test = [] {
    Gamma dist{2.5, 1.5};
    for (double x : {0.1, 0.5, 1.0, 2.0, 5.0}) {
      expectNear(std::log(dist.prob(x)), dist.logProb(x));
    }
  };

  "Gamma logProb avoids underflow"_test = [] {
    Gamma dist{2.0, 1.0};
    // For large x, prob() underflows but logProb() stays finite
    auto log_p = dist.logProb(100.0);
    expectTrue(std::isfinite(log_p));
    expectTrue(log_p < -50.0);  // Should be very negative
  };

  "Gamma logProb negative values"_test = [] {
    Gamma dist{2.0, 1.0};
    auto log_p = dist.logProb(-1.0);
    expectTrue(std::isinf(log_p));
    expectTrue(log_p < 0.0);
  };

  "Gamma logProb at zero"_test = [] {
    Gamma dist{2.0, 1.0};
    auto log_p = dist.logProb(0.0);
    expectTrue(std::isinf(log_p));
    expectTrue(log_p < 0.0);
  };

  "Gamma cdf at zero"_test = [] {
    Gamma dist{2.0, 1.0};
    expectNear(0.0, dist.cdf(0.0));
  };

  "Gamma cdf approaches 1"_test = [] {
    Gamma dist{2.0, 1.0};
    expectTrue(dist.cdf(20.0) > 0.99);
  };

  "Gamma cdf monotonic"_test = [] {
    Gamma dist{2.0, 1.0};
    expectTrue(dist.cdf(0.0) < dist.cdf(1.0));
    expectTrue(dist.cdf(1.0) < dist.cdf(2.0));
    expectTrue(dist.cdf(2.0) < dist.cdf(5.0));
  };

  "Gamma cdf negative values"_test = [] {
    Gamma dist{2.0, 1.0};
    expectEq(0.0, dist.cdf(-1.0));
    expectEq(0.0, dist.cdf(-100.0));
  };

  "Gamma cdf Exponential special case"_test = [] {
    // Gamma(1, λ) = Exponential(λ)
    // CDF: 1 - exp(-λx)
    Gamma dist{1.0, 2.0};
    auto x = 1.0;
    auto expected = 1.0 - std::exp(-2.0 * x);
    expectNear(expected, dist.cdf(x));
  };

  "Gamma mean formula"_test = [] {
    expectNear(2.0, Gamma{2.0, 1.0}.mean());
    expectNear(1.0, Gamma{2.0, 2.0}.mean());
    expectNear(4.0, Gamma{2.0, 0.5}.mean());
  };

  "Gamma variance formula"_test = [] {
    expectNear(2.0, Gamma{2.0, 1.0}.variance());
    expectNear(0.5, Gamma{2.0, 2.0}.variance());
    expectNear(8.0, Gamma{2.0, 0.5}.variance());
  };

  "Gamma Exponential mean special case"_test = [] {
    // Gamma(1, λ) mean = 1/λ
    expectNear(0.5, Gamma{1.0, 2.0}.mean());
    expectNear(2.0, Gamma{1.0, 0.5}.mean());
  };

  "Gamma Exponential variance special case"_test = [] {
    // Gamma(1, λ) variance = 1/λ²
    expectNear(0.25, Gamma{1.0, 2.0}.variance());
    expectNear(4.0, Gamma{1.0, 0.5}.variance());
  };

  "Gamma accessors"_test = [] {
    Gamma dist{2.5, 1.5};
    expectEq(2.5, dist.alpha());
    expectEq(1.5, dist.beta());
  };

  "Gamma sample with std::mt19937_64"_test = [] {
    std::mt19937_64 gen{42};
    Gamma dist{2.0, 1.0};

    // Generate samples - all should be positive and finite
    for (int i = 0; i < 1000; ++i) {
      auto x = dist.sample(gen);
      expectTrue(x > 0.0);
      expectTrue(std::isfinite(x));
    }
  };

  "Gamma sample mean approximation"_test = [] {
    std::mt19937_64 gen{123};
    Gamma dist{2.0, 1.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      sum += dist.sample(gen);
    }
    double sample_mean = sum / N;

    // Sample mean should approximate theoretical mean (α/β = 2.0)
    expectNear(2.0, sample_mean, 0.1);
  };

  "Gamma sample variance approximation"_test = [] {
    std::mt19937_64 gen{456};
    Gamma dist{2.0, 1.0};

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

    // Sample variance should approximate theoretical variance (α/β² = 2.0)
    expectNear(2.0, sample_variance, 0.2);
  };

  "Gamma sample with α < 1"_test = [] {
    std::mt19937_64 gen{789};
    Gamma dist{0.5, 1.0};

    // Generate samples - all should be positive and finite
    for (int i = 0; i < 1000; ++i) {
      auto x = dist.sample(gen);
      expectTrue(x > 0.0);
      expectTrue(std::isfinite(x));
    }
  };

  "Gamma sample mean with α < 1"_test = [] {
    std::mt19937_64 gen{321};
    Gamma dist{0.5, 2.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      sum += dist.sample(gen);
    }
    double sample_mean = sum / N;

    // Sample mean should approximate theoretical mean (α/β = 0.25)
    expectNear(0.25, sample_mean, 0.05);
  };

  "Gamma sample different parameters"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{100};
    Gamma dist1{2.0, 1.0};
    Gamma dist2{3.0, 2.0};

    auto x1 = dist1.sample(g1);
    auto x2 = dist2.sample(g2);

    // Different parameters should (almost always) produce different samples
    expectTrue(x1 != x2);
  };

  "Gamma sample different seeds produce different sequences"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{200};
    Gamma dist{2.0, 1.0};

    auto x1 = dist.sample(g1);
    auto x2 = dist.sample(g2);

    // Different seeds should (almost always) produce different samples
    expectTrue(x1 != x2);
  };

  "Gamma constexpr construction"_test = [] {
    constexpr Gamma dist{2.0, 1.0};
    static_assert(dist.alpha() == 2.0);
    static_assert(dist.beta() == 1.0);
    static_assert(dist.mean() == 2.0);
    static_assert(dist.variance() == 2.0);
  };

  "Gamma constexpr probability"_test = [] {
    constexpr Gamma dist{1.0, 1.0};
    // Gamma(1, 1) at x=0 is 0
    static_assert(dist.prob(0.0) == 0.0);
    static_assert(dist.cdf(0.0) == 0.0);
  };

  "Gamma rejects integer types"_test = [] {
    // These should fail to compile with clear error messages
    // Uncomment to verify:
    // Gamma<int> d1{2, 1};           // error: static assertion failed
    // Gamma<int64_t> d2{2, 1};       // error: static assertion failed
    // Gamma<uint32_t> d3{2, 1};      // error: static assertion failed

    // Floating-point types work fine
    Gamma<float> f{2.0f, 1.0f};
    Gamma<double> d{2.0, 1.0};
    Gamma<long double> ld{2.0L, 1.0L};

    expectEq(2.0F, f.alpha());
    expectEq(2.0, d.alpha());
    expectEq(2.0L, ld.alpha());
  };

  "Gamma custom type with extension points"_test = [] {
    // Custom type with numeric_infinity, log, exp, pow, sqrt, lgamma
    Gamma<custom::Float> dist{custom::Float{2.0}, custom::Float{1.0}};

    expectNear(2.0, dist.alpha().value);
    expectNear(1.0, dist.beta().value);
    expectNear(2.0, dist.mean().value);
    expectNear(2.0, dist.variance().value);

    // prob uses exp, log, pow, lgamma extension points
    expectTrue(dist.prob(custom::Float{1.0}).value > 0.0);

    // logProb uses log, lgamma, and numeric_infinity extension points
    auto log_p = dist.logProb(custom::Float{1.0});
    expectTrue(std::isfinite(log_p.value));

    auto log_p_neg = dist.logProb(custom::Float{-1.0});
    expectTrue(std::isinf(log_p_neg.value));

    // cdf uses incomplete gamma (exp, log, lgamma)
    expectNear(0.0, dist.cdf(custom::Float{0.0}).value);
  };

  "Gamma PDF integrates to approximately 1"_test = [] {
    // Numerical integration check: ∫₀^∞ p(x) dx ≈ 1
    Gamma dist{2.0, 1.0};

    double integral = 0.0;
    const double dx = 0.01;
    // Integrate from 0 to 20 (captures most of the mass)
    for (double x = 0.0; x <= 20.0; x += dx) {
      integral += dist.prob(x) * dx;
    }

    expectNear(1.0, integral, 0.02);
  };

  "Gamma CDF derivative approximates PDF"_test = [] {
    // Numerical derivative check: dF/dx ≈ p(x)
    Gamma dist{2.0, 1.0};

    const double h = 1e-5;
    for (double x : {0.5, 1.0, 2.0, 3.0, 5.0}) {
      double numerical_deriv = (dist.cdf(x + h) - dist.cdf(x - h)) / (2.0 * h);
      double pdf = dist.prob(x);
      expectNear(pdf, numerical_deriv, 0.01);
    }
  };

  "Gamma shape α controls distribution shape"_test = [] {
    // α < 1: decreasing PDF from infinity
    // α = 1: exponential (constant hazard)
    // α > 1: unimodal with mode at (α-1)/β

    Gamma dist1{0.5, 1.0};  // α < 1
    Gamma dist2{1.0, 1.0};  // α = 1 (exponential)
    Gamma dist3{3.0, 1.0};  // α > 1

    // For α < 1, PDF decreases monotonically
    expectTrue(dist1.prob(0.1) > dist1.prob(0.5));
    expectTrue(dist1.prob(0.5) > dist1.prob(1.0));

    // For α = 1 (exponential), PDF decreases monotonically
    expectTrue(dist2.prob(0.0) > dist2.prob(1.0));
    expectTrue(dist2.prob(1.0) > dist2.prob(2.0));

    // For α > 1, PDF has a mode at (α-1)/β = 2.0
    auto mode = 2.0;
    expectTrue(dist3.prob(mode) > dist3.prob(mode - 1.0));
    expectTrue(dist3.prob(mode) > dist3.prob(mode + 1.0));
  };

  "Gamma rate β controls scale"_test = [] {
    // Higher β => more concentrated near 0, faster decay
    Gamma dist1{2.0, 0.5};  // Low rate
    Gamma dist2{2.0, 2.0};  // High rate

    // Low rate has larger mean
    expectTrue(dist1.mean() > dist2.mean());

    // Low rate has larger variance
    expectTrue(dist1.variance() > dist2.variance());

    // High rate is more peaked near 0
    expectTrue(dist2.prob(0.5) > dist1.prob(0.5));
  };

  "Gamma scaling property"_test = [] {
    // If X ~ Gamma(α, β), then cX ~ Gamma(α, β/c)
    std::mt19937_64 g1{999};
    std::mt19937_64 g2{999};

    Gamma dist1{2.0, 1.0};
    Gamma dist2{2.0, 0.5};  // β/c where c=2

    auto x1 = dist1.sample(g1);
    auto x2 = dist2.sample(g2);

    // x2 should be approximately 2 * x1
    expectNear(2.0 * x1, x2, 1.0);
  };

  "Gamma sum property"_test = [] {
    // If X ~ Gamma(α₁, β) and Y ~ Gamma(α₂, β), then X+Y ~ Gamma(α₁+α₂, β)
    // Test via mean: E[X+Y] = (α₁+α₂)/β
    std::mt19937_64 g1{888};
    std::mt19937_64 g2{888};

    Gamma dist1{1.0, 2.0};
    Gamma dist2{2.0, 2.0};
    Gamma dist_sum{3.0, 2.0};

    const int N = 5'000;
    double sum_of_samples = 0.0;

    for (int i = 0; i < N; ++i) {
      auto x1 = dist1.sample(g1);
      auto x2 = dist2.sample(g2);
      sum_of_samples += (x1 + x2);
    }

    double sample_mean = sum_of_samples / N;
    expectNear(dist_sum.mean(), sample_mean, 0.1);
  };

  "Gamma mode formula for α ≥ 1"_test = [] {
    // Mode = (α - 1) / β for α ≥ 1
    Gamma dist{3.0, 2.0};
    auto mode = (3.0 - 1.0) / 2.0;  // = 1.0

    // Check that PDF is maximized near the mode
    expectTrue(dist.prob(mode) > dist.prob(mode - 0.5));
    expectTrue(dist.prob(mode) > dist.prob(mode + 0.5));
  };

  "Gamma chi-squared special case"_test = [] {
    // Chi-squared(k) = Gamma(k/2, 1/2)
    // Mean: k, Variance: 2k
    Gamma chi_sq{2.0, 0.5};  // k = 4

    expectNear(4.0, chi_sq.mean());
    expectNear(8.0, chi_sq.variance());
  };
}
