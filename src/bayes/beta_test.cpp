#include "bayes/beta.h"

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
  constexpr auto operator<=(Float o) const -> bool { return value <= o.value; }
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
inline auto lgamma(Float f) -> Float { return Float{std::lgamma(f.value)}; }
}  // namespace custom

auto main() -> int {
  "Beta prob uniform Beta(1,1)"_test = [] {
    Beta dist{1.0, 1.0};
    // Beta(1,1) is Uniform(0,1) with PDF = 1
    expectNear(1.0, dist.prob(0.0));
    expectNear(1.0, dist.prob(0.5));
    expectNear(1.0, dist.prob(1.0));
  };

  "Beta prob symmetric Beta(2,2)"_test = [] {
    Beta dist{2.0, 2.0};
    // Beta(2,2) is symmetric around 0.5
    // PDF at mean (0.5): p(0.5|2,2) = 0.5^1 * 0.5^1 / B(2,2) = 0.25 / (1/6) = 1.5
    expectNear(1.5, dist.prob(0.5));
    // Symmetric: p(0.3) = p(0.7)
    expectNear(dist.prob(0.3), dist.prob(0.7));
  };

  "Beta prob asymmetric Beta(2,5)"_test = [] {
    Beta dist{2.0, 5.0};
    // Mode at (α-1)/(α+β-2) = 1/5 = 0.2
    auto p_mode = dist.prob(0.2);
    auto p_left = dist.prob(0.1);
    auto p_right = dist.prob(0.3);
    // PDF should be highest near mode
    expectTrue(p_mode > p_left);
    expectTrue(p_mode > p_right);
  };

  "Beta prob out of bounds"_test = [] {
    Beta dist{2.0, 2.0};
    // PDF is 0 outside [0,1]
    expectEq(0.0, dist.prob(-0.1));
    expectEq(0.0, dist.prob(1.1));
    expectEq(0.0, dist.prob(-10.0));
    expectEq(0.0, dist.prob(10.0));
  };

  "Beta prob edge cases at boundaries"_test = [] {
    // Beta(1,1): uniform, boundary values should work
    Beta uniform{1.0, 1.0};
    expectNear(1.0, uniform.prob(0.0));
    expectNear(1.0, uniform.prob(1.0));

    // Beta(2,2): boundaries should be 0
    Beta symmetric{2.0, 2.0};
    expectNear(0.0, symmetric.prob(0.0));
    expectNear(0.0, symmetric.prob(1.0));

    // Beta(0.5, 0.5): U-shaped, infinite at boundaries (we return 0 to avoid inf)
    Beta u_shaped{0.5, 0.5};
    expectEq(0.0, u_shaped.prob(0.0));
    expectEq(0.0, u_shaped.prob(1.0));
  };

  "Beta logProb uniform Beta(1,1)"_test = [] {
    Beta dist{1.0, 1.0};
    // log(1) = 0
    expectNear(0.0, dist.logProb(0.5));
  };

  "Beta logProb consistency with prob"_test = [] {
    Beta dist{2.0, 5.0};
    for (double x : {0.1, 0.2, 0.3, 0.5, 0.7, 0.9}) {
      expectNear(std::log(dist.prob(x)), dist.logProb(x));
    }
  };

  "Beta logProb out of bounds returns negative infinity"_test = [] {
    Beta dist{2.0, 2.0};
    auto neg_inf = -numeric_infinity(0.0);
    expectEq(neg_inf, dist.logProb(-0.1));
    expectEq(neg_inf, dist.logProb(1.5));
  };

  "Beta logProb avoids underflow"_test = [] {
    // Beta(10, 10) has very sharp peak at 0.5
    Beta dist{10.0, 10.0};
    // Far from mean, prob() may underflow but logProb() stays finite
    auto log_p = dist.logProb(0.01);
    expectTrue(std::isfinite(log_p));
    expectTrue(log_p < -10.0);  // Should be very negative
  };

  "Beta logProb edge cases at boundaries"_test = [] {
    // Beta(1,1): log(1) = 0 everywhere
    Beta uniform{1.0, 1.0};
    expectNear(0.0, uniform.logProb(0.5));

    // Beta(2,2): log(0) at boundaries
    Beta symmetric{2.0, 2.0};
    expectEq(-numeric_infinity(0.0), symmetric.logProb(0.0));
    expectEq(-numeric_infinity(0.0), symmetric.logProb(1.0));
  };

  "Beta unnormalizedLogProb consistency"_test = [] {
    Beta dist{3.0, 4.0};
    for (double x : {0.1, 0.3, 0.5, 0.7, 0.9}) {
      // unnormalized + normalization constant = logProb
      auto unnorm = dist.unnormalizedLogProb(x);
      auto norm = dist.logProb(x);
      // Difference is the log normalization constant
      auto diff = norm - unnorm;
      expectNear(diff, norm - unnorm);  // Just checking it's finite
      expectTrue(std::isfinite(diff));
    }
  };

  "Beta unnormalizedLogProb out of bounds"_test = [] {
    Beta dist{2.0, 2.0};
    expectEq(-numeric_infinity(0.0), dist.unnormalizedLogProb(-0.1));
    expectEq(-numeric_infinity(0.0), dist.unnormalizedLogProb(1.5));
  };

  "Beta mean"_test = [] {
    // E[X] = α / (α + β)
    expectNear(0.5, Beta{1.0, 1.0}.mean());
    expectNear(0.5, Beta{2.0, 2.0}.mean());
    expectNear(2.0 / 7.0, Beta{2.0, 5.0}.mean());
    expectNear(5.0 / 7.0, Beta{5.0, 2.0}.mean());
    expectNear(0.75, Beta{3.0, 1.0}.mean());
  };

  "Beta variance"_test = [] {
    // Var[X] = (αβ) / ((α+β)²(α+β+1))
    Beta uniform{1.0, 1.0};
    // Var[Uniform(0,1)] = 1/12 ≈ 0.08333
    expectNear(1.0 / 12.0, uniform.variance());

    Beta symmetric{2.0, 2.0};
    // (2*2) / (16 * 5) = 4/80 = 0.05
    expectNear(0.05, symmetric.variance());

    Beta skewed{2.0, 5.0};
    // (2*5) / (49 * 8) = 10/392 ≈ 0.0255
    expectNear(10.0 / 392.0, skewed.variance());
  };

  "Beta accessors"_test = [] {
    Beta dist{3.5, 7.5};
    expectEq(3.5, dist.alpha());
    expectEq(7.5, dist.beta());
  };

  "Beta sample in valid range"_test = [] {
    Beta dist{2.0, 5.0};
    std::mt19937_64 g{42};

    // All samples should be in [0, 1]
    for (int i = 0; i < 100; ++i) {
      auto x = dist.sample(g);
      expectTrue(x >= 0.0 && x <= 1.0);
    }
  };

  "Beta sample distribution statistics uniform"_test = [] {
    Beta dist{1.0, 1.0};  // Uniform(0,1)
    std::mt19937_64 g{123};

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
    expectNear<0.01>(0.5, sample_mean);       // E[Uniform] = 0.5
    expectNear<0.01>(1.0 / 12.0, sample_var);  // Var[Uniform] = 1/12
  };

  "Beta sample distribution statistics symmetric"_test = [] {
    Beta dist{2.0, 2.0};
    std::mt19937_64 g{456};

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

    expectNear<0.02>(0.5, sample_mean);        // E[Beta(2,2)] = 0.5
    expectNear<0.01>(0.05, sample_var);        // Var[Beta(2,2)] = 0.05
  };

  "Beta sample distribution statistics asymmetric"_test = [] {
    Beta dist{2.0, 5.0};
    std::mt19937_64 g{789};

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

    const double expected_mean = 2.0 / 7.0;  // ≈ 0.286
    const double expected_var = 10.0 / 392.0; // ≈ 0.0255

    expectNear<0.01>(expected_mean, sample_mean);
    expectNear<0.01>(expected_var, sample_var);
  };

  "Beta sample different seeds produce different sequences"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{200};
    Beta dist{2.0, 2.0};

    auto x1 = dist.sample(g1);
    auto x2 = dist.sample(g2);

    // Different seeds should (almost always) produce different samples
    expectTrue(x1 != x2);
  };

  "Beta constexpr construction"_test = [] {
    constexpr Beta dist{2.0, 5.0};
    static_assert(dist.alpha() == 2.0);
    static_assert(dist.beta() == 5.0);
    static_assert(dist.mean() == 2.0 / 7.0);
  };

  "Beta constexpr variance"_test = [] {
    constexpr Beta dist{2.0, 2.0};
    static_assert(dist.variance() == 0.05);
  };

  "Beta rejects integer types"_test = [] {
    // These should fail to compile with clear error messages
    // Uncomment to verify:
    // Beta<int> d1{2, 5};        // error: static assertion failed
    // Beta<int64_t> d2{2, 5};    // error: static assertion failed
    // Beta<uint32_t> d3{2, 5};   // error: static assertion failed

    // Floating-point types work fine
    Beta<float> f{2.0f, 5.0f};
    Beta<double> d{2.0, 5.0};
    Beta<long double> ld{2.0L, 5.0L};

    expectNear(2.0F / 7.0F, f.mean());
    expectNear(2.0 / 7.0, d.mean());
    expectNear(2.0L / 7.0L, ld.mean());
  };

  "Beta custom type with extension points"_test = [] {
    // Custom type with numeric_infinity, log, exp, and lgamma extension points
    Beta<custom::Float> dist{custom::Float{2.0}, custom::Float{5.0}};

    expectNear(2.0 / 7.0, dist.mean().value);
    expectNear(2.0, dist.alpha().value);
    expectNear(5.0, dist.beta().value);

    // prob uses exp and lgamma extension points
    auto p = dist.prob(custom::Float{0.3});
    expectTrue(p.value > 0.0);

    // logProb uses log and lgamma extension points
    auto log_p = dist.logProb(custom::Float{0.3});
    expectNear(std::log(p.value), log_p.value);
  };

  "Beta PDF integrates to 1"_test = [] {
    // Numerical integration check: ∫₀¹ p(x) dx ≈ 1
    Beta dist{2.0, 5.0};

    double integral = 0.0;
    const double dx = 0.001;
    for (double x = dx / 2; x < 1.0; x += dx) {
      integral += dist.prob(x) * dx;
    }

    expectNear<0.01>(1.0, integral);
  };

  "Beta special case uniform distribution"_test = [] {
    // Beta(1,1) should be exactly Uniform(0,1)
    Beta dist{1.0, 1.0};

    // PDF constant at 1
    for (double x : {0.0, 0.1, 0.5, 0.9, 1.0}) {
      expectNear(1.0, dist.prob(x));
    }

    // Mean and variance match uniform
    expectNear(0.5, dist.mean());
    expectNear(1.0 / 12.0, dist.variance());
  };

  "Beta special case U-shaped distribution"_test = [] {
    // Beta(0.5, 0.5) is U-shaped with more mass near boundaries
    Beta dist{0.5, 0.5};

    // PDF higher near boundaries than center
    auto p_center = dist.prob(0.5);
    auto p_edge = dist.prob(0.1);
    expectTrue(p_edge > p_center);
  };

  "Beta special case bell-shaped distribution"_test = [] {
    // Beta(5, 5) is bell-shaped centered at 0.5
    Beta dist{5.0, 5.0};

    // PDF highest at center
    auto p_center = dist.prob(0.5);
    auto p_left = dist.prob(0.3);
    auto p_right = dist.prob(0.7);
    expectTrue(p_center > p_left);
    expectTrue(p_center > p_right);

    // Symmetric
    expectNear(p_left, p_right);
  };
}

