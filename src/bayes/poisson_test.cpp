#include "bayes/poisson.h"

#include <cmath>
#include <random>
#include <vector>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "Poisson prob at mode"_test = [] {
    // Mode = floor(λ) for non-integer λ, or λ-1 and λ for integer λ
    Poisson dist{5.5};
    auto p_at_5 = dist.prob(5);
    auto p_at_6 = dist.prob(6);
    // Mode is at floor(λ) = 5
    expectTrue(p_at_5 > dist.prob(4));
    expectTrue(p_at_5 >= p_at_6);  // 5 and 6 are close
  };

  "Poisson prob at mean"_test = [] {
    Poisson dist{3.0};
    auto p = dist.prob(3);
    expectTrue(p > 0.0);
    expectTrue(std::isfinite(p));
  };

  "Poisson prob decreases in tails"_test = [] {
    Poisson dist{5.0};
    // Right tail
    expectTrue(dist.prob(5) > dist.prob(10));
    expectTrue(dist.prob(10) > dist.prob(15));
    // Left tail
    expectTrue(dist.prob(5) > dist.prob(1));
    expectTrue(dist.prob(1) > dist.prob(0));
  };

  "Poisson prob negative values"_test = [] {
    Poisson dist{3.0};
    expectEq(0.0, dist.prob(-1));
    expectEq(0.0, dist.prob(-100));
  };

  "Poisson prob at zero"_test = [] {
    Poisson dist{2.0};
    // P(X = 0) = e^(-λ)
    auto expected = std::exp(-2.0);
    expectNear(expected, dist.prob(0));
  };

  "Poisson prob with different parameters"_test = [] {
    Poisson dist1{2.0};
    Poisson dist2{5.0};
    // Different λ => different PMF values
    expectTrue(dist1.prob(3) != dist2.prob(3));
  };

  "Poisson prob known values"_test = [] {
    // P(X = k) = λ^k e^(-λ) / k!
    Poisson dist{3.0};
    // k = 0: e^(-3) ≈ 0.0498
    expectNear(std::exp(-3.0), dist.prob(0), 1e-6);
    // k = 1: 3 * e^(-3) ≈ 0.1494
    expectNear(3.0 * std::exp(-3.0), dist.prob(1), 1e-6);
    // k = 2: 9/2 * e^(-3) ≈ 0.224
    expectNear(4.5 * std::exp(-3.0), dist.prob(2), 1e-6);
    // k = 3: 27/6 * e^(-3) ≈ 0.224
    expectNear(4.5 * std::exp(-3.0), dist.prob(3), 1e-6);
  };

  "Poisson logProb consistency with prob"_test = [] {
    Poisson dist{4.0};
    for (int64_t k : {0, 1, 2, 5, 10}) {
      expectNear(std::log(dist.prob(k)), dist.logProb(k));
    }
  };

  "Poisson logProb avoids underflow"_test = [] {
    Poisson dist{1.0};
    // For large k, prob() underflows but logProb() stays finite
    auto log_p = dist.logProb(100);
    expectTrue(std::isfinite(log_p));
    expectTrue(log_p < -100.0);  // Should be very negative
  };

  "Poisson logProb negative values"_test = [] {
    Poisson dist{3.0};
    auto log_p = dist.logProb(-1);
    expectTrue(std::isinf(log_p));
    expectTrue(log_p < 0.0);
  };

  "Poisson cdf at zero"_test = [] {
    Poisson dist{2.0};
    // CDF(0) = P(X = 0) = e^(-λ)
    expectNear(std::exp(-2.0), dist.cdf(0), 1e-6);
  };

  "Poisson cdf approaches 1"_test = [] {
    Poisson dist{5.0};
    expectTrue(dist.cdf(20) > 0.999);
    expectTrue(dist.cdf(30) > 0.9999);
  };

  "Poisson cdf monotonic"_test = [] {
    Poisson dist{3.0};
    expectTrue(dist.cdf(0) < dist.cdf(1));
    expectTrue(dist.cdf(1) < dist.cdf(2));
    expectTrue(dist.cdf(2) < dist.cdf(5));
    expectTrue(dist.cdf(5) < dist.cdf(10));
  };

  "Poisson cdf negative values"_test = [] {
    Poisson dist{3.0};
    expectEq(0.0, dist.cdf(-1));
    expectEq(0.0, dist.cdf(-100));
  };

  "Poisson cdf sum of PMF"_test = [] {
    Poisson dist{3.0};
    // CDF(k) should equal sum of prob(i) for i = 0 to k
    for (int64_t k : {0, 1, 2, 3, 5, 10}) {
      double sum = 0.0;
      for (int64_t i = 0; i <= k; ++i) {
        sum += dist.prob(i);
      }
      expectNear(sum, dist.cdf(k), 1e-6);
    }
  };

  "Poisson mean formula"_test = [] {
    expectEq(2.0, Poisson{2.0}.mean());
    expectEq(5.0, Poisson{5.0}.mean());
    expectEq(10.0, Poisson{10.0}.mean());
  };

  "Poisson variance formula"_test = [] {
    // Mean = Variance for Poisson
    expectEq(2.0, Poisson{2.0}.variance());
    expectEq(5.0, Poisson{5.0}.variance());
    expectEq(10.0, Poisson{10.0}.variance());
  };

  "Poisson mean equals variance"_test = [] {
    for (double lambda : {0.5, 1.0, 2.5, 5.0, 10.0, 50.0}) {
      Poisson dist{lambda};
      expectEq(dist.mean(), dist.variance());
    }
  };

  "Poisson accessors"_test = [] {
    Poisson dist{4.5};
    expectEq(4.5, dist.lambda());
  };

  "Poisson sample with std::mt19937_64"_test = [] {
    std::mt19937_64 gen{42};
    Poisson dist{3.0};

    // Generate samples - all should be non-negative and finite
    for (int i = 0; i < 1000; ++i) {
      auto k = dist.sample(gen);
      expectTrue(k >= 0);
    }
  };

  "Poisson sample mean approximation"_test = [] {
    std::mt19937_64 gen{123};
    Poisson dist{5.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      sum += static_cast<double>(dist.sample(gen));
    }
    double sample_mean = sum / N;

    // Sample mean should approximate theoretical mean (λ = 5.0)
    expectNear(5.0, sample_mean, 0.15);
  };

  "Poisson sample variance approximation"_test = [] {
    std::mt19937_64 gen{456};
    Poisson dist{5.0};

    const int N = 10'000;
    std::vector<int64_t> samples;
    samples.reserve(N);
    double sum = 0.0;

    for (int i = 0; i < N; ++i) {
      auto k = dist.sample(gen);
      samples.push_back(k);
      sum += static_cast<double>(k);
    }

    double mean = sum / N;
    double sum_sq_diff = 0.0;
    for (auto k : samples) {
      auto diff = static_cast<double>(k) - mean;
      sum_sq_diff += diff * diff;
    }
    double sample_variance = sum_sq_diff / (N - 1);

    // Sample variance should approximate theoretical variance (λ = 5.0)
    expectNear(5.0, sample_variance, 0.3);
  };

  "Poisson sample small lambda"_test = [] {
    std::mt19937_64 gen{789};
    Poisson dist{0.5};

    const int N = 5'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      sum += static_cast<double>(dist.sample(gen));
    }
    double sample_mean = sum / N;

    expectNear(0.5, sample_mean, 0.05);
  };

  "Poisson sample large lambda"_test = [] {
    std::mt19937_64 gen{321};
    Poisson dist{50.0};

    const int N = 5'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      sum += static_cast<double>(dist.sample(gen));
    }
    double sample_mean = sum / N;

    expectNear(50.0, sample_mean, 1.0);
  };

  "Poisson sample different parameters"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{100};
    Poisson dist1{2.0};
    Poisson dist2{10.0};

    // Average over many samples should differ
    double sum1 = 0.0;
    double sum2 = 0.0;
    const int N = 1000;
    for (int i = 0; i < N; ++i) {
      sum1 += static_cast<double>(dist1.sample(g1));
      sum2 += static_cast<double>(dist2.sample(g2));
    }

    expectTrue(std::abs(sum1 / N - sum2 / N) > 5.0);
  };

  "Poisson sample different seeds produce different sequences"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{200};
    Poisson dist{5.0};

    // Not all samples will differ, but sequences should diverge
    int same_count = 0;
    for (int i = 0; i < 100; ++i) {
      if (dist.sample(g1) == dist.sample(g2)) {
        ++same_count;
      }
    }
    // Very unlikely to have many matches with different seeds
    expectTrue(same_count < 50);
  };

  "Poisson constexpr construction"_test = [] {
    constexpr Poisson dist{3.0};
    static_assert(dist.lambda() == 3.0);
    static_assert(dist.mean() == 3.0);
    static_assert(dist.variance() == 3.0);
  };

  "Poisson PMF sums to approximately 1"_test = [] {
    Poisson dist{5.0};

    double sum = 0.0;
    // Sum PMF from 0 to a large value (tail is negligible)
    for (int64_t k = 0; k <= 30; ++k) {
      sum += dist.prob(k);
    }

    expectNear(1.0, sum, 1e-8);
  };

  "Poisson additivity property"_test = [] {
    // If X ~ Poisson(λ₁) and Y ~ Poisson(λ₂), then X+Y ~ Poisson(λ₁+λ₂)
    // Test via mean
    std::mt19937_64 g1{888};
    std::mt19937_64 g2{888};

    Poisson dist1{2.0};
    Poisson dist2{3.0};
    Poisson dist_sum{5.0};

    const int N = 5'000;
    double sum_of_samples = 0.0;

    for (int i = 0; i < N; ++i) {
      auto x1 = dist1.sample(g1);
      auto x2 = dist2.sample(g2);
      sum_of_samples += static_cast<double>(x1 + x2);
    }

    double sample_mean = sum_of_samples / N;
    expectNear(dist_sum.mean(), sample_mean, 0.2);
  };

  "Poisson Binomial approximation"_test = [] {
    // Binomial(n, p) ≈ Poisson(np) for large n, small p
    // Compare means and variances conceptually
    double n = 1000.0;
    double p = 0.005;
    double lambda = n * p;  // = 5.0

    Poisson dist{lambda};
    expectEq(lambda, dist.mean());
    // Binomial variance = np(1-p) ≈ np = λ for small p
    expectNear(n * p * (1 - p), dist.variance(), 0.1);
  };

  "Poisson rare event modeling"_test = [] {
    // λ = 0.1 (rare events: ~10% chance of at least one event)
    Poisson dist{0.1};

    // P(X = 0) should be high
    expectTrue(dist.prob(0) > 0.9);
    // P(X >= 1) should be low
    expectTrue(1.0 - dist.cdf(0) < 0.1);
  };

  "Poisson high rate modeling"_test = [] {
    // λ = 100 (many events, approximately normal)
    Poisson dist{100.0};

    // Most mass should be near mean
    expectTrue(dist.cdf(120) - dist.cdf(80) > 0.9);
  };

  "Poisson float type"_test = [] {
    Poisson<float> dist{3.0f};
    expectEq(3.0f, dist.lambda());
    expectEq(3.0f, dist.mean());
    expectTrue(dist.prob(3) > 0.0f);
  };

  "Poisson long double type"_test = [] {
    Poisson<long double> dist{3.0L};
    expectEq(3.0L, dist.lambda());
    expectEq(3.0L, dist.mean());
    expectTrue(dist.prob(3) > 0.0L);
  };

  return TestRegistry::result();
}
