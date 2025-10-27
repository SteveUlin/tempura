#include "bayes/binomial.h"

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
inline auto exp(Float f) -> Float { return Float{std::exp(f.value)}; }
inline auto lgamma(Float f) -> Float { return Float{std::lgamma(f.value)}; }
}  // namespace custom

auto main() -> int {
  "Binomial prob for k=0"_test = [] {
    // Binomial(10, 0.5): P(X=0) = C(10,0) * 0.5^0 * 0.5^10 = 1/1024
    Binomial dist{10, 0.5};
    expectNear(1.0 / 1024.0, dist.prob(0));
  };

  "Binomial prob for k=n"_test = [] {
    // Binomial(10, 0.5): P(X=10) = C(10,10) * 0.5^10 * 0.5^0 = 1/1024
    Binomial dist{10, 0.5};
    expectNear(1.0 / 1024.0, dist.prob(10));
  };

  "Binomial prob for k in middle"_test = [] {
    // Binomial(10, 0.5): P(X=5) = C(10,5) * 0.5^5 * 0.5^5
    // C(10,5) = 252
    Binomial dist{10, 0.5};
    expectNear(252.0 / 1024.0, dist.prob(5));
  };

  "Binomial prob out of range negative"_test = [] {
    Binomial dist{10, 0.5};
    expectNear(0.0, dist.prob(-1));
    expectNear(0.0, dist.prob(-100));
  };

  "Binomial prob out of range above n"_test = [] {
    Binomial dist{10, 0.5};
    expectNear(0.0, dist.prob(11));
    expectNear(0.0, dist.prob(100));
  };

  "Binomial prob with asymmetric p"_test = [] {
    // Binomial(5, 0.3): P(X=2) = C(5,2) * 0.3^2 * 0.7^3
    // C(5,2) = 10
    Binomial dist{5, 0.3};
    double expected = 10.0 * std::pow(0.3, 2) * std::pow(0.7, 3);
    expectNear(expected, dist.prob(2));
  };

  "Binomial prob edge case p=0"_test = [] {
    // P(X=0|n,p=0) = 1, P(X=k|n,p=0) = 0 for k>0
    Binomial dist{10, 0.0};
    expectNear(1.0, dist.prob(0));
    expectNear(0.0, dist.prob(1));
    expectNear(0.0, dist.prob(5));
    expectNear(0.0, dist.prob(10));
  };

  "Binomial prob edge case p=1"_test = [] {
    // P(X=n|n,p=1) = 1, P(X=k|n,p=1) = 0 for k<n
    Binomial dist{10, 1.0};
    expectNear(0.0, dist.prob(0));
    expectNear(0.0, dist.prob(5));
    expectNear(0.0, dist.prob(9));
    expectNear(1.0, dist.prob(10));
  };

  "Binomial prob for n=1 matches Bernoulli"_test = [] {
    // Binomial(1, p) = Bernoulli(p)
    Binomial dist{1, 0.7};
    expectNear(0.3, dist.prob(0));  // 1-p
    expectNear(0.7, dist.prob(1));  // p
  };

  "Binomial logProb for k in range"_test = [] {
    Binomial dist{10, 0.5};
    double prob_5 = dist.prob(5);
    expectNear(std::log(prob_5), dist.logProb(5));
  };

  "Binomial logProb consistency with prob"_test = [] {
    Binomial dist{8, 0.4};
    for (int64_t k = 0; k <= 8; ++k) {
      double prob = dist.prob(k);
      if (prob > 0.0) {
        expectNear(std::log(prob), dist.logProb(k), 1e-9);
      }
    }
  };

  "Binomial logProb out of range"_test = [] {
    Binomial dist{10, 0.5};
    expectTrue(std::isinf(dist.logProb(-1)));
    expectTrue(dist.logProb(-1) < 0.0);
    expectTrue(std::isinf(dist.logProb(11)));
    expectTrue(dist.logProb(11) < 0.0);
  };

  "Binomial logProb edge case p=0 for k=0"_test = [] {
    // P(X=0|n,p=0) = 1 => log P = 0
    Binomial dist{10, 0.0};
    expectNear(0.0, dist.logProb(0));
  };

  "Binomial logProb edge case p=0 for k>0"_test = [] {
    // P(X=k|n,p=0) = 0 for k>0 => log P = -
    Binomial dist{10, 0.0};
    expectTrue(std::isinf(dist.logProb(1)));
    expectTrue(dist.logProb(1) < 0.0);
    expectTrue(std::isinf(dist.logProb(10)));
    expectTrue(dist.logProb(10) < 0.0);
  };

  "Binomial logProb edge case p=1 for k=n"_test = [] {
    // P(X=n|n,p=1) = 1 => log P = 0
    Binomial dist{10, 1.0};
    expectNear(0.0, dist.logProb(10));
  };

  "Binomial logProb edge case p=1 for k<n"_test = [] {
    // P(X=k|n,p=1) = 0 for k<n => log P = -
    Binomial dist{10, 1.0};
    expectTrue(std::isinf(dist.logProb(0)));
    expectTrue(dist.logProb(0) < 0.0);
    expectTrue(std::isinf(dist.logProb(9)));
    expectTrue(dist.logProb(9) < 0.0);
  };

  "Binomial cdf at lower bound"_test = [] {
    // CDF(-1) = 0
    Binomial dist{10, 0.5};
    expectNear(0.0, dist.cdf(-1));
    expectNear(0.0, dist.cdf(-100));
  };

  "Binomial cdf at upper bound"_test = [] {
    // CDF(n) = 1, CDF(k>n) = 1
    Binomial dist{10, 0.5};
    expectNear(1.0, dist.cdf(10));
    expectNear(1.0, dist.cdf(100));
  };

  "Binomial cdf at k=0"_test = [] {
    // CDF(0) = P(X=0)
    Binomial dist{10, 0.5};
    expectNear(dist.prob(0), dist.cdf(0));
  };

  "Binomial cdf is non-decreasing"_test = [] {
    Binomial dist{10, 0.5};
    for (int64_t k = 0; k < 10; ++k) {
      expectTrue(dist.cdf(k) <= dist.cdf(k + 1));
    }
  };

  "Binomial cdf via summation"_test = [] {
    Binomial dist{5, 0.3};
    // CDF(2) = P(X=0) + P(X=1) + P(X=2)
    double expected = dist.prob(0) + dist.prob(1) + dist.prob(2);
    expectNear(expected, dist.cdf(2), 1e-9);
  };

  "Binomial mean"_test = [] {
    // E[X] = np
    expectNear(0.0, Binomial{10, 0.0}.mean());
    expectNear(3.0, Binomial{10, 0.3}.mean());
    expectNear(5.0, Binomial{10, 0.5}.mean());
    expectNear(7.0, Binomial{10, 0.7}.mean());
    expectNear(10.0, Binomial{10, 1.0}.mean());
  };

  "Binomial mean for various n"_test = [] {
    expectNear(0.0, Binomial{0, 0.5}.mean());
    expectNear(0.5, Binomial{1, 0.5}.mean());
    expectNear(5.0, Binomial{10, 0.5}.mean());
    expectNear(50.0, Binomial{100, 0.5}.mean());
  };

  "Binomial variance"_test = [] {
    // Var[X] = np(1-p)
    // Maximum variance at p=0.5: n/4
    expectNear(0.0, Binomial{10, 0.0}.variance());
    expectNear(2.1, Binomial{10, 0.3}.variance());
    expectNear(2.5, Binomial{10, 0.5}.variance());
    expectNear(2.1, Binomial{10, 0.7}.variance());
    expectNear(0.0, Binomial{10, 1.0}.variance());
  };

  "Binomial variance is symmetric in p"_test = [] {
    // Var[Binomial(n, p)] = Var[Binomial(n, 1-p)]
    expectNear(Binomial{10, 0.3}.variance(), Binomial{10, 0.7}.variance());
    expectNear(Binomial{10, 0.2}.variance(), Binomial{10, 0.8}.variance());
  };

  "Binomial variance for n=1 matches Bernoulli"_test = [] {
    // Binomial(1, p) variance = p(1-p) = Bernoulli(p) variance
    expectNear(0.25, Binomial{1, 0.5}.variance());
    expectNear(0.21, Binomial{1, 0.3}.variance());
  };

  "Binomial parameter accessors"_test = [] {
    Binomial dist{20, 0.42};
    expectEq(20, dist.n());
    expectNear(0.42, dist.p());
  };

  "Binomial sample with std::mt19937"_test = [] {
    std::mt19937 g{42};
    Binomial dist{100, 0.3};

    // Sample many times and check empirical mean
    const int N_samples = 10'000;
    int64_t sum = 0;
    for (int i = 0; i < N_samples; ++i) {
      int64_t sample = dist.sample(g);
      sum += sample;
      // Verify sample is in valid range
      expectTrue(sample >= 0 && sample <= 100);
    }

    double empirical_mean = static_cast<double>(sum) / N_samples;
    // E[X] = np = 100 * 0.3 = 30
    expectNear(30.0, empirical_mean, 1.0);
  };

  "Binomial sample fair coin"_test = [] {
    std::mt19937 g{0};
    Binomial dist{50, 0.5};

    const int N_samples = 10'000;
    int64_t sum = 0;
    for (int i = 0; i < N_samples; ++i) {
      int64_t sample = dist.sample(g);
      sum += sample;
      expectTrue(sample >= 0 && sample <= 50);
    }

    double empirical_mean = static_cast<double>(sum) / N_samples;
    // E[X] = np = 50 * 0.5 = 25
    expectNear(25.0, empirical_mean, 1.0);
  };

  "Binomial sample edge case p=0"_test = [] {
    std::mt19937 g{0};
    Binomial dist{10, 0.0};

    // All samples should be 0
    for (int i = 0; i < 100; ++i) {
      expectEq(0, dist.sample(g));
    }
  };

  "Binomial sample edge case p=1"_test = [] {
    std::mt19937 g{0};
    Binomial dist{10, 1.0};

    // All samples should be n
    for (int i = 0; i < 100; ++i) {
      expectEq(10, dist.sample(g));
    }
  };

  "Binomial sample with std::mt19937_64"_test = [] {
    std::mt19937_64 g{123};
    Binomial dist{20, 0.7};

    const int N_samples = 10'000;
    int64_t sum = 0;
    for (int i = 0; i < N_samples; ++i) {
      int64_t sample = dist.sample(g);
      sum += sample;
      expectTrue(sample >= 0 && sample <= 20);
    }

    double empirical_mean = static_cast<double>(sum) / N_samples;
    // E[X] = np = 20 * 0.7 = 14
    expectNear(14.0, empirical_mean, 1.0);
  };

  "Binomial sample different seeds produce different sequences"_test = [] {
    std::mt19937 g1{100};
    std::mt19937 g2{200};
    Binomial dist{10, 0.5};

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

  "Binomial constexpr construction"_test = [] {
    constexpr Binomial dist{10, 0.3};
    static_assert(dist.n() == 10);
    static_assert(dist.p() == 0.3);
    static_assert(dist.mean() == 3.0);
  };

  "Binomial constexpr mean and variance"_test = [] {
    constexpr Binomial dist{20, 0.5};
    static_assert(dist.mean() == 10.0);
    static_assert(dist.variance() == 5.0);
  };

  "Binomial rejects integer types"_test = [] {
    // These should fail to compile with clear error messages
    // Uncomment to verify:
    // Binomial<int> d1{10, 0};          // error: static assertion failed
    // Binomial<int64_t> d2{10, 0};      // error: static assertion failed
    // Binomial<uint32_t> d3{10, 0};     // error: static assertion failed

    // Floating-point types work fine
    Binomial<float> f{10, 0.5f};
    Binomial<double> d{10, 0.5};
    Binomial<long double> ld{10, 0.5L};

    expectNear(5.0f, f.mean());
    expectNear(5.0, d.mean());
    expectNear(5.0L, ld.mean());
  };

  "Binomial custom type with extension points"_test = [] {
    // Custom type with numeric_infinity, log, exp, lgamma extension points
    Binomial<custom::Float> dist{5, custom::Float{0.6}};

    expectNear(3.0, dist.mean().value);
    expectNear(1.2, dist.variance().value);

    // prob uses exp and logProb
    auto prob_2 = dist.prob(2);
    expectTrue(prob_2.value > 0.0);

    // logProb uses log and lgamma extension points
    auto log_prob_2 = dist.logProb(2);
    expectNear(std::log(prob_2.value), log_prob_2.value, 1e-9);
  };

  "Binomial PMF sums to 1"_test = [] {
    Binomial dist{10, 0.4};
    double sum = 0.0;
    for (int64_t k = 0; k <= 10; ++k) {
      sum += dist.prob(k);
    }
    expectNear(1.0, sum, 1e-9);
  };

  "Binomial expected value from PMF"_test = [] {
    Binomial dist{8, 0.3};
    // E[X] = � k�P(X=k)
    double expected = 0.0;
    for (int64_t k = 0; k <= 8; ++k) {
      expected += k * dist.prob(k);
    }
    expectNear(dist.mean(), expected, 1e-9);
  };

  "Binomial variance from PMF"_test = [] {
    Binomial dist{6, 0.5};
    // E[X�] = � k��P(X=k)
    double e_x_sq = 0.0;
    for (int64_t k = 0; k <= 6; ++k) {
      e_x_sq += k * k * dist.prob(k);
    }
    // Var[X] = E[X�] - E[X]�
    double variance = e_x_sq - dist.mean() * dist.mean();
    expectNear(dist.variance(), variance, 1e-9);
  };

  "Binomial large n with small p"_test = [] {
    // For large n and small p, np remains moderate
    // Binomial(1000, 0.01) has mean=10, variance=9.9
    Binomial dist{1000, 0.01};
    expectNear(10.0, dist.mean());
    expectNear(9.9, dist.variance());

    // PMF should work without overflow
    double prob_10 = dist.prob(10);
    expectTrue(prob_10 > 0.0);
    expectTrue(std::isfinite(prob_10));
  };

  "Binomial large n with large p"_test = [] {
    // Binomial(1000, 0.99) has mean=990, variance=9.9
    Binomial dist{1000, 0.99};
    expectNear(990.0, dist.mean());
    expectNear(9.9, dist.variance());

    // PMF should work without overflow
    double prob_990 = dist.prob(990);
    expectTrue(prob_990 > 0.0);
    expectTrue(std::isfinite(prob_990));
  };

  "Binomial with int32_t count type"_test = [] {
    Binomial<double, int32_t> dist{10, 0.5};
    expectEq(10, dist.n());
    expectNear(5.0, dist.mean());
    expectNear(2.5, dist.variance());

    // PMF should work
    double prob_5 = dist.prob(5);
    expectTrue(prob_5 > 0.0);

    // Sampling should work
    std::mt19937 g{42};
    int32_t sample = dist.sample(g);
    expectTrue(sample >= 0 && sample <= 10);
  };

  "Binomial with int count type"_test = [] {
    Binomial<double, int> dist{20, 0.3};
    expectEq(20, dist.n());
    expectNear(6.0, dist.mean());

    // Test all methods with int type
    expectTrue(dist.prob(6) > 0.0);
    expectTrue(std::isfinite(dist.logProb(6)));
    expectTrue(dist.cdf(10) > 0.0 && dist.cdf(10) < 1.0);

    // Sampling
    std::mt19937 g{0};
    int sample = dist.sample(g);
    expectTrue(sample >= 0 && sample <= 20);
  };

  "Binomial with uint32_t count type"_test = [] {
    Binomial<double, uint32_t> dist{100u, 0.1};
    expectEq(100u, dist.n());
    expectNear(10.0, dist.mean());

    // Test probability functions
    double prob_10 = dist.prob(10u);
    expectTrue(prob_10 > 0.0);

    // Test sampling
    std::mt19937 g{123};
    uint32_t sample = dist.sample(g);
    expectTrue(sample <= 100u);
  };

  "Binomial rejects non-integer count types"_test = [] {
    // These should fail to compile with clear error messages
    // Uncomment to verify:
    // Binomial<double, double> d1{10.0, 0.5};     // error: static assertion failed
    // Binomial<double, float> d2{10.0f, 0.5};     // error: static assertion failed

    // Integer types work fine
    Binomial<double, int> i{10, 0.5};
    Binomial<double, int32_t> i32{10, 0.5};
    Binomial<double, int64_t> i64{10, 0.5};
    Binomial<double, uint32_t> u32{10u, 0.5};

    expectNear(5.0, i.mean());
    expectNear(5.0, i32.mean());
    expectNear(5.0, i64.mean());
    expectNear(5.0, u32.mean());
  };

  "Binomial with different T and IntT combinations"_test = [] {
    // float probability, int32_t count
    Binomial<float, int32_t> f_i32{50, 0.5f};
    expectNear(25.0f, f_i32.mean());

    // long double probability, int count
    Binomial<long double, int> ld_i{20, 0.7L};
    expectNear(14.0L, ld_i.mean());

    // double probability, uint32_t count
    Binomial<double, uint32_t> d_u32{100u, 0.4};
    expectNear(40.0, d_u32.mean());
  };
}
