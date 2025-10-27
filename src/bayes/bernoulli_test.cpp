#include "bayes/bernoulli.h"

#include <cmath>
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
}  // namespace custom

auto main() -> int {
  "Bernoulli prob for true"_test = [] {
    Bernoulli dist{0.8};
    expectNear(0.8, dist.prob(true));
  };

  "Bernoulli prob for false"_test = [] {
    Bernoulli dist{0.8};
    expectNear(0.2, dist.prob(false));
  };

  "Bernoulli prob for fair coin"_test = [] {
    Bernoulli dist{0.5};
    expectNear(0.5, dist.prob(true));
    expectNear(0.5, dist.prob(false));
  };

  "Bernoulli prob edge case p=0"_test = [] {
    Bernoulli dist{0.0};
    expectNear(0.0, dist.prob(true));
    expectNear(1.0, dist.prob(false));
  };

  "Bernoulli prob edge case p=1"_test = [] {
    Bernoulli dist{1.0};
    expectNear(1.0, dist.prob(true));
    expectNear(0.0, dist.prob(false));
  };

  "Bernoulli logProb for true"_test = [] {
    Bernoulli dist{0.8};
    expectNear(std::log(0.8), dist.logProb(true));
  };

  "Bernoulli logProb for false"_test = [] {
    Bernoulli dist{0.8};
    expectNear(std::log(0.2), dist.logProb(false));
  };

  "Bernoulli logProb consistency with prob"_test = [] {
    Bernoulli dist{0.3};
    expectNear(std::log(dist.prob(true)), dist.logProb(true));
    expectNear(std::log(dist.prob(false)), dist.logProb(false));
  };

  "Bernoulli logProb edge case p=0"_test = [] {
    Bernoulli dist{0.0};
    // log P(X=1|p=0) = -∞
    expectTrue(std::isinf(dist.logProb(true)));
    expectTrue(dist.logProb(true) < 0.0);
    // log P(X=0|p=0) = log(1) = 0
    expectNear(0.0, dist.logProb(false));
  };

  "Bernoulli logProb edge case p=1"_test = [] {
    Bernoulli dist{1.0};
    // log P(X=1|p=1) = log(1) = 0
    expectNear(0.0, dist.logProb(true));
    // log P(X=0|p=1) = -∞
    expectTrue(std::isinf(dist.logProb(false)));
    expectTrue(dist.logProb(false) < 0.0);
  };

  "Bernoulli cdf for false"_test = [] {
    Bernoulli dist{0.8};
    // P(X ≤ 0) = P(X = 0) = 1 - p = 0.2
    expectNear(0.2, dist.cdf(false));
  };

  "Bernoulli cdf for true"_test = [] {
    Bernoulli dist{0.8};
    // P(X ≤ 1) = 1
    expectNear(1.0, dist.cdf(true));
  };

  "Bernoulli cdf properties"_test = [] {
    Bernoulli dist{0.3};
    // CDF is monotonic: F(0) ≤ F(1)
    expectTrue(dist.cdf(false) <= dist.cdf(true));
    // CDF(1) = 1
    expectNear(1.0, dist.cdf(true));
  };

  "Bernoulli mean"_test = [] {
    expectNear(0.0, Bernoulli{0.0}.mean());
    expectNear(0.3, Bernoulli{0.3}.mean());
    expectNear(0.5, Bernoulli{0.5}.mean());
    expectNear(0.7, Bernoulli{0.7}.mean());
    expectNear(1.0, Bernoulli{1.0}.mean());
  };

  "Bernoulli variance"_test = [] {
    // Var[X] = p(1-p)
    // Maximum variance at p=0.5: 0.25
    expectNear(0.0, Bernoulli{0.0}.variance());
    expectNear(0.21, Bernoulli{0.3}.variance());
    expectNear(0.25, Bernoulli{0.5}.variance());
    expectNear(0.21, Bernoulli{0.7}.variance());
    expectNear(0.0, Bernoulli{1.0}.variance());
  };

  "Bernoulli variance is symmetric"_test = [] {
    // Var[Bernoulli(p)] = Var[Bernoulli(1-p)]
    expectNear(Bernoulli{0.3}.variance(), Bernoulli{0.7}.variance());
    expectNear(Bernoulli{0.2}.variance(), Bernoulli{0.8}.variance());
  };

  "Bernoulli accessor"_test = [] {
    Bernoulli dist{0.42};
    expectNear(0.42, dist.p());
  };

  "Bernoulli sample with std::mt19937"_test = [] {
    std::mt19937 g{42};
    Bernoulli dist{0.7};

    // Sample many times and check that roughly 70% are true
    const int N = 10'000;
    int count = 0;
    for (int i = 0; i < N; ++i) {
      if (dist.sample(g)) {
        ++count;
      }
    }

    double empirical_p = static_cast<double>(count) / N;
    expectNear(0.7, empirical_p, 0.1);
  };

  "Bernoulli sample fair coin"_test = [] {
    std::mt19937 g{0};
    Bernoulli dist{0.5};
    const int N = 10'000;
    int count = 0;
    for (int i = 0; i < N; ++i) {
      if (dist.sample(g)) {
        ++count;
      }
    }
    expectNear(0.5, static_cast<double>(count), 0.1 / N);
  };

  "Bernoulli sample with std::mt19937_64"_test = [] {
    std::mt19937_64 g{123};
    Bernoulli dist{0.2};

    const int N = 10'000;
    int count = 0;
    for (int i = 0; i < N; ++i) {
      if (dist.sample(g)) {
        ++count;
      }
    }

    double empirical_p = static_cast<double>(count) / N;
    expectNear(0.2, empirical_p, 0.1);
  };

  "Bernoulli sample edge case p=0"_test = [] {
    std::mt19937 g{0};
    Bernoulli dist{0.0};

    // Should always return false
    for (int i = 0; i < 100; ++i) {
      expectTrue(!dist.sample(g));
    }
  };

  "Bernoulli sample edge case p=1"_test = [] {
    std::mt19937 g{0};
    Bernoulli dist{1.0};

    // Should always return true
    for (int i = 0; i < 100; ++i) {
      expectTrue(dist.sample(g));
    }
  };

  "Bernoulli sample different seeds produce different sequences"_test = [] {
    std::mt19937 g1{100};
    std::mt19937 g2{200};
    Bernoulli dist{0.5};

    // Count number of differences in first 100 samples
    int differences = 0;
    for (int i = 0; i < 100; ++i) {
      if (dist.sample(g1) != dist.sample(g2)) {
        ++differences;
      }
    }

    // Should have some differences (with overwhelming probability)
    expectTrue(differences > 10);
  };

  "Bernoulli constexpr construction"_test = [] {
    constexpr Bernoulli dist{0.3};
    static_assert(dist.p() == 0.3);
    static_assert(dist.mean() == 0.3);
  };

  "Bernoulli constexpr probability"_test = [] {
    constexpr Bernoulli dist{0.6};
    static_assert(dist.prob(true) == 0.6);
    static_assert(dist.prob(false) == 0.4);
  };

  "Bernoulli constexpr cdf"_test = [] {
    constexpr Bernoulli dist{0.8};
    // Runtime check instead of static_assert due to floating-point precision
    expectNear(0.2, dist.cdf(false));
    expectNear(1.0, dist.cdf(true));
  };

  "Bernoulli constexpr variance"_test = [] {
    constexpr Bernoulli dist{0.5};
    static_assert(dist.variance() == 0.25);
  };

  "Bernoulli rejects integer types"_test = [] {
    // These should fail to compile with clear error messages
    // Uncomment to verify:
    // Bernoulli<int> d1{0};          // error: static assertion failed
    // Bernoulli<int64_t> d2{0};      // error: static assertion failed
    // Bernoulli<uint32_t> d3{0};     // error: static assertion failed

    // Floating-point types work fine
    Bernoulli<float> f{0.5f};
    Bernoulli<double> d{0.5};
    Bernoulli<long double> ld{0.5L};

    expectNear(0.5f, f.mean());
    expectNear(0.5, d.mean());
    expectNear(0.5L, ld.mean());
  };

  "Bernoulli custom type with extension points"_test = [] {
    // Custom type with numeric_infinity and log extension points
    Bernoulli<custom::Float> dist{custom::Float{0.8}};

    expectNear(0.8, dist.mean().value);
    expectNear(0.8, dist.prob(true).value);
    expectNear(0.2, dist.prob(false).value);

    // logProb uses log extension point
    auto log_prob_true = dist.logProb(true);
    expectNear(std::log(0.8), log_prob_true.value);

    auto log_prob_false = dist.logProb(false);
    expectNear(std::log(0.2), log_prob_false.value);

    // CDF
    expectNear(0.2, dist.cdf(false).value);
    expectNear(1.0, dist.cdf(true).value);
  };

  "Bernoulli PMF sums to 1"_test = [] {
    Bernoulli dist{0.6};
    // P(X=0) + P(X=1) = 1
    double sum = dist.prob(false) + dist.prob(true);
    expectNear(1.0, sum);
  };

  "Bernoulli expected value from PMF"_test = [] {
    Bernoulli dist{0.7};
    // E[X] = 0·P(X=0) + 1·P(X=1) = P(X=1) = p
    double expected = 0.0 * dist.prob(false) + 1.0 * dist.prob(true);
    expectNear(dist.mean(), expected);
  };

  "Bernoulli variance from PMF"_test = [] {
    Bernoulli dist{0.4};
    // E[X²] = 0²·P(X=0) + 1²·P(X=1) = p
    // Var[X] = E[X²] - E[X]² = p - p² = p(1-p)
    double e_x_sq = 0.0 + dist.prob(true);
    double variance = e_x_sq - dist.mean() * dist.mean();
    expectNear(dist.variance(), variance);
  };
}
