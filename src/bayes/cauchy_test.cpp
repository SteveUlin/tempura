#include "bayes/cauchy.h"

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

constexpr auto numeric_quiet_nan(Float) -> Float {
  return Float{std::numeric_limits<double>::quiet_NaN()};
}

inline auto log(Float f) -> Float { return Float{std::log(f.value)}; }
inline auto atan(Float f) -> Float { return Float{std::atan(f.value)}; }
inline auto tan(Float f) -> Float { return Float{std::tan(f.value)}; }
}  // namespace custom

auto main() -> int {
  "Cauchy prob at median"_test = [] {
    Cauchy dist{0.0, 1.0};
    // At x = μ, PDF = 1/(πσ) for standard Cauchy
    expectNear(1.0 / std::numbers::pi, dist.prob(0.0));
  };

  "Cauchy prob symmetric around median"_test = [] {
    Cauchy dist{5.0, 2.0};
    // PDF should be symmetric: p(μ + δ) = p(μ - δ)
    expectNear(dist.prob(7.0), dist.prob(3.0));
    expectNear(dist.prob(10.0), dist.prob(0.0));
  };

  "Cauchy prob with different scale"_test = [] {
    Cauchy dist{0.0, 2.0};
    // At μ, PDF = 1/(πσ) = 1/(2π)
    expectNear(1.0 / (2.0 * std::numbers::pi), dist.prob(0.0));
  };

  "Cauchy prob heavy tails"_test = [] {
    Cauchy dist{0.0, 1.0};
    // PDF decays as 1/x² in tails (much slower than exponential)
    auto p_at_5 = dist.prob(5.0);
    auto p_at_10 = dist.prob(10.0);
    expectTrue(p_at_5 > p_at_10);
    expectTrue(p_at_10 > 0.0);

    // Compare to expected: p(x) ≈ σ/(πx²) for large |x|
    expectNear<0.01>(1.0 / (std::numbers::pi * 100.0), p_at_10);
  };

  "Cauchy prob at extreme values"_test = [] {
    Cauchy dist{0.0, 1.0};
    // Even at x = 1000, probability should be non-zero (heavy tails)
    auto p = dist.prob(1000.0);
    expectTrue(p > 0.0);
    expectTrue(std::isfinite(p));
  };

  "Cauchy logProb at median"_test = [] {
    Cauchy dist{0.0, 1.0};
    // log(1/π) = -log(π)
    expectNear(-std::log(std::numbers::pi), dist.logProb(0.0));
  };

  "Cauchy logProb consistency with prob"_test = [] {
    Cauchy dist{2.0, 3.0};
    for (double x : {-10.0, -1.0, 0.0, 2.0, 5.0, 20.0}) {
      expectNear(std::log(dist.prob(x)), dist.logProb(x));
    }
  };

  "Cauchy logProb avoids underflow"_test = [] {
    Cauchy dist{0.0, 1.0};
    // At extreme values, prob() may underflow but logProb() stays finite
    auto log_p = dist.logProb(1000.0);
    expectTrue(std::isfinite(log_p));
    expectTrue(log_p < -10.0);  // Should be very negative
  };

  "Cauchy cdf at median"_test = [] {
    Cauchy dist{0.0, 1.0};
    // CDF(μ) = 0.5 for any μ and σ
    expectNear(0.5, dist.cdf(0.0));
  };

  "Cauchy cdf symmetric around median"_test = [] {
    Cauchy dist{5.0, 2.0};
    // CDF(μ - δ) + CDF(μ + δ) = 1 (by symmetry)
    expectNear(1.0, dist.cdf(3.0) + dist.cdf(7.0));
    expectNear(1.0, dist.cdf(0.0) + dist.cdf(10.0));
  };

  "Cauchy cdf bounds"_test = [] {
    Cauchy dist{0.0, 1.0};
    // CDF approaches 0 as x → -∞
    expectTrue(dist.cdf(-100.0) < 0.01);
    // CDF approaches 1 as x → +∞
    expectTrue(dist.cdf(100.0) > 0.99);
  };

  "Cauchy cdf monotonic"_test = [] {
    Cauchy dist{0.0, 1.0};
    // CDF is strictly increasing
    expectTrue(dist.cdf(-10.0) < dist.cdf(-1.0));
    expectTrue(dist.cdf(-1.0) < dist.cdf(0.0));
    expectTrue(dist.cdf(0.0) < dist.cdf(1.0));
    expectTrue(dist.cdf(1.0) < dist.cdf(10.0));
  };

  "Cauchy cdf analytical values"_test = [] {
    Cauchy dist{0.0, 1.0};
    // CDF(μ + σ) = 0.5 + arctan(1)/π = 0.75
    expectNear(0.75, dist.cdf(1.0));
    // CDF(μ - σ) = 0.5 - arctan(1)/π = 0.25
    expectNear(0.25, dist.cdf(-1.0));
  };

  "Cauchy invCdf at median"_test = [] {
    Cauchy dist{0.0, 1.0};
    // invCDF(0.5) = μ
    expectNear(0.0, dist.invCdf(0.5));
  };

  "Cauchy invCdf roundtrip"_test = [] {
    Cauchy dist{5.0, 2.0};
    for (double p : {0.1, 0.25, 0.5, 0.75, 0.9}) {
      auto x = dist.invCdf(p);
      expectNear(p, dist.cdf(x));
    }
  };

  "Cauchy invCdf analytical values"_test = [] {
    Cauchy dist{0.0, 1.0};
    // invCDF(0.75) = μ + σ·tan(π/4) = 1
    expectNear(1.0, dist.invCdf(0.75));
    // invCDF(0.25) = μ + σ·tan(-π/4) = -1
    expectNear(-1.0, dist.invCdf(0.25));
  };

  "Cauchy median"_test = [] {
    expectEq(0.0, Cauchy{0.0, 1.0}.median());
    expectEq(5.0, Cauchy{5.0, 2.0}.median());
    expectEq(-3.0, Cauchy{-3.0, 0.5}.median());
  };

  "Cauchy mean is NaN"_test = [] {
    // Mean does not exist for Cauchy distribution
    Cauchy dist{0.0, 1.0};
    expectTrue(std::isnan(dist.mean()));
  };

  "Cauchy variance is NaN"_test = [] {
    // Variance does not exist for Cauchy distribution
    Cauchy dist{0.0, 1.0};
    expectTrue(std::isnan(dist.variance()));
  };

  "Cauchy accessors"_test = [] {
    Cauchy dist{1.5, 3.5};
    expectEq(1.5, dist.mu());
    expectEq(3.5, dist.sigma());
  };

  "Cauchy sample with std::mt19937_64"_test = [] {
    std::mt19937_64 g{42};
    Cauchy dist{0.0, 1.0};

    // Generate samples - Cauchy has no bounded support, extreme values expected
    int extreme_count = 0;
    for (int i = 0; i < 1000; ++i) {
      auto x = dist.sample(g);
      expectTrue(std::isfinite(x));
      if (std::abs(x) > 100.0) {
        ++extreme_count;
      }
    }
    // Heavy tails mean some extreme values are expected
    expectTrue(extreme_count < 100);  // But not too many
  };

  "Cauchy sample median approximation"_test = [] {
    std::mt19937_64 g{123};
    Cauchy dist{5.0, 2.0};

    const int N = 10'000;
    std::vector<double> samples;
    samples.reserve(N);

    for (int i = 0; i < N; ++i) {
      samples.push_back(dist.sample(g));
    }

    // Sort to find sample median
    std::sort(samples.begin(), samples.end());
    double sample_median = samples[N / 2];

    // Sample median should approximate theoretical median (= μ)
    // Tolerance is larger due to heavy tails
    expectNear<0.5>(5.0, sample_median);
  };

  "Cauchy sample different parameters"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{100};
    Cauchy dist1{0.0, 1.0};
    Cauchy dist2{10.0, 5.0};

    auto x1 = dist1.sample(g1);
    auto x2 = dist2.sample(g2);

    // Different parameters should (almost always) produce different samples
    expectTrue(x1 != x2);
  };

  "Cauchy sample different seeds produce different sequences"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{200};
    Cauchy dist{0.0, 1.0};

    auto x1 = dist.sample(g1);
    auto x2 = dist.sample(g2);

    // Different seeds should (almost always) produce different samples
    expectTrue(x1 != x2);
  };

  "Cauchy constexpr construction"_test = [] {
    constexpr Cauchy dist{0.0, 1.0};
    static_assert(dist.median() == 0.0);
    static_assert(dist.mu() == 0.0);
    static_assert(dist.sigma() == 1.0);
  };

  "Cauchy constexpr probability"_test = [] {
    constexpr Cauchy dist{0.0, 1.0};
    // At μ, PDF = 1/π
    constexpr double expected = 1.0 / std::numbers::pi;
    static_assert(dist.prob(0.0) == expected);
    static_assert(dist.cdf(0.0) == 0.5);
  };

  "Cauchy rejects integer types"_test = [] {
    // These should fail to compile with clear error messages
    // Uncomment to verify:
    // Cauchy<int> d1{0, 1};        // error: static assertion failed
    // Cauchy<int64_t> d2{0, 1};    // error: static assertion failed
    // Cauchy<uint32_t> d3{0, 1};   // error: static assertion failed

    // Floating-point types work fine
    Cauchy<float> f{0.0f, 1.0f};
    Cauchy<double> d{0.0, 1.0};
    Cauchy<long double> ld{0.0L, 1.0L};

    expectEq(0.0F, f.median());
    expectEq(0.0, d.median());
    expectEq(0.0L, ld.median());
  };

  "Cauchy custom type with extension points"_test = [] {
    // Custom type with numeric_infinity, numeric_quiet_nan, log, atan, and tan
    Cauchy<custom::Float> dist{custom::Float{0.0}, custom::Float{1.0}};

    expectNear(0.0, dist.median().value);
    expectNear(1.0 / std::numbers::pi, dist.prob(custom::Float{0.0}).value);

    // CDF at median should be 0.5
    expectNear(0.5, dist.cdf(custom::Float{0.0}).value);

    // logProb uses log extension point
    auto log_prob = dist.logProb(custom::Float{0.0});
    expectNear(-std::log(std::numbers::pi), log_prob.value);

    // invCdf uses tan extension point
    auto x = dist.invCdf(custom::Float{0.75});
    expectNear(1.0, x.value);

    // mean and variance use numeric_quiet_nan extension point
    expectTrue(std::isnan(dist.mean().value));
    expectTrue(std::isnan(dist.variance().value));
  };

  "Cauchy PDF integrates to 1"_test = [] {
    // Numerical integration check: ∫ p(x) dx ≈ 1
    Cauchy dist{0.0, 1.0};

    double integral = 0.0;
    const double dx = 0.1;
    // Integrate from -100 to +100 (captures most of the mass)
    for (double x = -100.0; x <= 100.0; x += dx) {
      integral += dist.prob(x) * dx;
    }

    expectNear<0.05>(1.0, integral);
  };

  "Cauchy CDF derivative equals PDF"_test = [] {
    // Numerical derivative check: dF/dx ≈ p(x)
    Cauchy dist{0.0, 1.0};

    const double h = 1e-5;
    for (double x : {-10.0, -1.0, 0.0, 1.0, 10.0}) {
      // Numerical derivative of CDF
      double numerical_deriv = (dist.cdf(x + h) - dist.cdf(x - h)) / (2.0 * h);
      double pdf = dist.prob(x);
      expectNear<0.001>(pdf, numerical_deriv);
    }
  };

  "Cauchy quartiles"_test = [] {
    Cauchy dist{0.0, 1.0};
    // For standard Cauchy, quartiles are at ±1
    // Q1 = invCDF(0.25) = -1
    // Q3 = invCDF(0.75) = 1
    expectNear(-1.0, dist.invCdf(0.25));
    expectNear(1.0, dist.invCdf(0.75));

    // IQR = Q3 - Q1 = 2 for standard Cauchy
    double iqr = dist.invCdf(0.75) - dist.invCdf(0.25);
    expectNear(2.0, iqr);
  };

  "Cauchy PDF formula verification"_test = [] {
    // Verify PDF formula: p(x|μ,σ) = 1/(πσ(1+((x-μ)/σ)²))
    Cauchy dist{3.0, 2.0};

    auto x = 5.0;
    auto μ = 3.0;
    auto σ = 2.0;
    auto z = (x - μ) / σ;
    auto expected = 1.0 / (std::numbers::pi * σ * (1.0 + z * z));

    expectNear(expected, dist.prob(x));
  };
}
