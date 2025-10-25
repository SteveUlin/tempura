#include "bayes/uniform.h"

#include <numbers>
#include <random>

#include "bayes/numeric_traits.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

// Example custom numeric type with extension point
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

// Extension point for numeric_infinity (found via ADL)
constexpr auto numeric_infinity(Float) -> Float {
  return Float{std::numeric_limits<double>::infinity()};
}

// Extension point for log (found via ADL)
inline auto log(Float f) -> Float { return Float{std::log(f.value)}; }
}  // namespace custom

auto main() -> int {
  "Uniform prob in range"_test = [] {
    Uniform u{2.0, 5.0};
    expectNear(1.0 / 3.0, u.prob(2.5));
    expectNear(1.0 / 3.0, u.prob(4.0));
  };

  "Uniform prob outside range"_test = [] {
    Uniform u{2.0, 5.0};
    expectEq(0.0, u.prob(1.0));
    expectEq(0.0, u.prob(6.0));
  };

  "Uniform logProb in range"_test = [] {
    Uniform u{1.0, 4.0};
    expectNear(-std::log(3.0), u.logProb(2.0));
  };

  "Uniform logProb outside range"_test = [] {
    Uniform u{1.0, 4.0};
    expectEq(-std::numeric_limits<double>::infinity(), u.logProb(0.5));
    expectEq(-std::numeric_limits<double>::infinity(), u.logProb(5.0));
  };

  "Uniform cdf lower bound"_test = [] {
    Uniform u{0.0, 10.0};
    expectEq(0.0, u.cdf(-1.0));
    expectEq(0.0, u.cdf(0.0));
  };

  "Uniform cdf upper bound"_test = [] {
    Uniform u{0.0, 10.0};
    expectEq(1.0, u.cdf(10.0));
    expectEq(1.0, u.cdf(11.0));
  };

  "Uniform cdf mid-range"_test = [] {
    Uniform u{0.0, 10.0};
    expectNear(0.25, u.cdf(2.5));
    expectNear(0.5, u.cdf(5.0));
    expectNear(0.75, u.cdf(7.5));
  };

  "Uniform mean"_test = [] {
    expectEq(2.5, Uniform{0.0, 5.0}.mean());
    expectEq(5.0, Uniform{2.0, 8.0}.mean());
  };

  "Uniform variance"_test = [] {
    // Variance of U(a,b) = (b-a)^2/12
    expectNear(25.0 / 12.0, Uniform{0.0, 5.0}.variance());
    expectNear(36.0 / 12.0, Uniform{2.0, 8.0}.variance());
  };

  "Uniform accessors"_test = [] {
    Uniform u{1.5, 7.5};
    expectEq(1.5, u.lower());
    expectEq(7.5, u.upper());
  };

  "Uniform sample with std::mt19937_64"_test = [] {
    std::mt19937_64 g{42};
    Uniform u{0.0, 10.0};

    // Verify samples are in range
    for (int i = 0; i < 100; ++i) {
      auto x = u.sample(g);
      expectTrue(x >= 0.0 && x <= 10.0);
    }
  };

  "Uniform sample distribution"_test = [] {
    std::mt19937_64 g{123};
    Uniform u{0.0, 1.0};

    const int N = 10'000;
    double sum = 0.0;
    double sum_sq = 0.0;

    for (int i = 0; i < N; ++i) {
      auto x = u.sample(g);
      sum += x;
      sum_sq += x * x;
    }

    double sample_mean = sum / N;
    double sample_var = (sum_sq / N) - (sample_mean * sample_mean);

    // Check sample statistics match theoretical values within tolerance
    expectNear<0.1>(0.5, sample_mean);        // E[U(0,1)] = 0.5
    expectNear<0.1>(1.0 / 12.0, sample_var);  // Var[U(0,1)] = 1/12
  };

  "Uniform constexpr construction"_test = [] {
    constexpr Uniform u{0.0, 1.0};
    static_assert(u.mean() == 0.5);
    static_assert(u.lower() == 0.0);
    static_assert(u.upper() == 1.0);
  };

  "Uniform constexpr probability"_test = [] {
    constexpr Uniform u{0.0, 2.0};
    static_assert(u.prob(1.0) == 0.5);
    static_assert(u.prob(-1.0) == 0.0);
    static_assert(u.cdf(1.0) == 0.5);
  };

  "Uniform rejects integer types"_test = [] {
    // These should fail to compile with clear error messages
    // Uncomment to verify:
    // Uniform<int> u1{0, 10};        // error: static assertion failed
    // Uniform<int64_t> u2{0, 100};   // error: static assertion failed
    // Uniform<uint32_t> u3{0, 50};   // error: static assertion failed

    // Floating-point types work fine
    Uniform<float> f{0.0f, 1.0f};
    Uniform<double> d{0.0, 1.0};
    Uniform<long double> ld{0.0L, 1.0L};

    expectEq(0.5F, f.mean());
    expectEq(0.5, d.mean());
    expectEq(0.5L, ld.mean());
  };

  "Uniform custom type with extension point"_test = [] {
    // Custom type with numeric_infinity and log extension points
    Uniform<custom::Float> u{custom::Float{0.0}, custom::Float{10.0}};

    expectNear(5.0, u.mean().value);
    expectNear(0.1, u.prob(custom::Float{5.0}).value);

    // logProb uses numeric_infinity extension point for out-of-range
    auto log_prob_out = u.logProb(custom::Float{-1.0});
    expectTrue(std::isinf(log_prob_out.value));
    expectTrue(log_prob_out.value < 0);

    // logProb in range uses log extension point
    auto log_prob_in = u.logProb(custom::Float{5.0});
    expectNear(-std::numbers::ln10, log_prob_in.value);
  };
}
