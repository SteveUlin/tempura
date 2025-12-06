#include "bayes/student_t.h"

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "StudentT prob at mode (x=0)"_test = [] {
    // t-distribution is symmetric with mode at x = 0
    StudentT dist{5.0};
    auto p_at_0 = dist.prob(0.0);
    expectTrue(p_at_0 > dist.prob(1.0));
    expectTrue(p_at_0 > dist.prob(-1.0));
    expectTrue(dist.prob(1.0) == dist.prob(-1.0));  // Symmetry
  };

  "StudentT prob symmetry"_test = [] {
    StudentT dist{3.0};
    for (double x : {0.5, 1.0, 2.0, 5.0}) {
      expectNear(dist.prob(x), dist.prob(-x));
    }
  };

  "StudentT prob decreases in tails"_test = [] {
    StudentT dist{5.0};
    expectTrue(dist.prob(0.0) > dist.prob(1.0));
    expectTrue(dist.prob(1.0) > dist.prob(2.0));
    expectTrue(dist.prob(2.0) > dist.prob(5.0));
  };

  "StudentT prob heavier tails than normal"_test = [] {
    // t-distribution has heavier tails than standard normal
    StudentT t_dist{3.0};
    // Compare at x = 3 (3 standard deviations)
    // Normal PDF at x=3: (1/sqrt(2π)) exp(-9/2) ≈ 0.0044
    double normal_pdf_at_3 =
        std::exp(-4.5) / std::sqrt(2.0 * std::numbers::pi);
    double t_pdf_at_3 = t_dist.prob(3.0);
    expectTrue(t_pdf_at_3 > normal_pdf_at_3);
  };

  "StudentT prob with different ν"_test = [] {
    StudentT dist1{1.0};   // Cauchy - heaviest tails
    StudentT dist2{5.0};   // Moderate
    StudentT dist3{100.0}; // Nearly normal

    // At x = 3, lower ν should have higher probability (heavier tails)
    expectTrue(dist1.prob(3.0) > dist2.prob(3.0));
    expectTrue(dist2.prob(3.0) > dist3.prob(3.0));
  };

  "StudentT prob Cauchy special case (ν=1)"_test = [] {
    // Cauchy PDF: 1 / (π(1 + x²))
    StudentT dist{1.0};
    for (double x : {0.0, 1.0, 2.0, 5.0}) {
      double expected = 1.0 / (std::numbers::pi * (1.0 + x * x));
      expectNear(expected, dist.prob(x), 1e-10);
    }
  };

  "StudentT logProb consistency with prob"_test = [] {
    StudentT dist{5.0};
    for (double x : {-2.0, -1.0, 0.0, 1.0, 2.0, 5.0}) {
      expectNear(std::log(dist.prob(x)), dist.logProb(x));
    }
  };

  "StudentT logProb avoids underflow"_test = [] {
    StudentT dist{3.0};
    // For large x, prob() may underflow but logProb() stays finite
    auto log_p = dist.logProb(100.0);
    expectTrue(std::isfinite(log_p));
    expectTrue(log_p < -10.0);  // Should be negative
  };

  "StudentT cdf at zero"_test = [] {
    // CDF(0) = 0.5 due to symmetry
    StudentT dist{5.0};
    expectNear(0.5, dist.cdf(0.0), 1e-10);
  };

  "StudentT cdf symmetry"_test = [] {
    // F(-x) = 1 - F(x) due to symmetry
    StudentT dist{5.0};
    for (double x : {0.5, 1.0, 2.0, 5.0}) {
      expectNear(dist.cdf(-x), 1.0 - dist.cdf(x), 1e-8);
    }
  };

  "StudentT cdf approaches limits"_test = [] {
    StudentT dist{5.0};
    expectTrue(dist.cdf(-10.0) < 0.001);
    expectTrue(dist.cdf(10.0) > 0.999);
  };

  "StudentT cdf monotonic"_test = [] {
    StudentT dist{5.0};
    expectTrue(dist.cdf(-2.0) < dist.cdf(-1.0));
    expectTrue(dist.cdf(-1.0) < dist.cdf(0.0));
    expectTrue(dist.cdf(0.0) < dist.cdf(1.0));
    expectTrue(dist.cdf(1.0) < dist.cdf(2.0));
  };

  "StudentT cdf known values ν=1 (Cauchy)"_test = [] {
    // Cauchy CDF: F(x) = 0.5 + arctan(x)/π
    StudentT dist{1.0};
    for (double x : {-1.0, 0.0, 1.0, 2.0}) {
      double expected = 0.5 + std::atan(x) / std::numbers::pi;
      expectNear(expected, dist.cdf(x), 1e-6);
    }
  };

  "StudentT cdf known values ν=2"_test = [] {
    // For ν=2: F(x) = 0.5 + x / (2√(2 + x²))
    StudentT dist{2.0};
    for (double x : {-1.0, 0.0, 1.0, 2.0}) {
      double expected = 0.5 + x / (2.0 * std::sqrt(2.0 + x * x));
      expectNear(expected, dist.cdf(x), 1e-6);
    }
  };

  "StudentT mean formula"_test = [] {
    // Mean = 0 for ν > 1, undefined for ν ≤ 1
    expectEq(0.0, StudentT{2.0}.mean());
    expectEq(0.0, StudentT{5.0}.mean());
    expectEq(0.0, StudentT{100.0}.mean());
  };

  "StudentT mean undefined for ν <= 1"_test = [] {
    // Mean is undefined (NaN) for ν ≤ 1
    expectTrue(std::isnan(StudentT{1.0}.mean()));
    expectTrue(std::isnan(StudentT{0.5}.mean()));
  };

  "StudentT variance formula"_test = [] {
    // Var = ν / (ν - 2) for ν > 2
    expectNear(5.0 / 3.0, StudentT{5.0}.variance());
    expectNear(10.0 / 8.0, StudentT{10.0}.variance());
    expectNear(100.0 / 98.0, StudentT{100.0}.variance());
  };

  "StudentT variance infinite for 1 < ν <= 2"_test = [] {
    expectTrue(std::isinf(StudentT{2.0}.variance()));
    expectTrue(StudentT{2.0}.variance() > 0);
    expectTrue(std::isinf(StudentT{1.5}.variance()));
  };

  "StudentT variance undefined for ν <= 1"_test = [] {
    expectTrue(std::isnan(StudentT{1.0}.variance()));
    expectTrue(std::isnan(StudentT{0.5}.variance()));
  };

  "StudentT variance approaches 1 as ν → ∞"_test = [] {
    // As ν → ∞, variance → 1 (standard normal)
    expectNear(1.0, StudentT{100.0}.variance(), 0.03);
    expectNear(1.0, StudentT{1000.0}.variance(), 0.003);
  };

  "StudentT accessors"_test = [] {
    StudentT dist{7.5};
    expectEq(7.5, dist.nu());
  };

  "StudentT sample with std::mt19937_64"_test = [] {
    std::mt19937_64 gen{42};
    StudentT dist{5.0};

    // Generate samples - all should be finite
    for (int i = 0; i < 1000; ++i) {
      auto x = dist.sample(gen);
      expectTrue(std::isfinite(x));
    }
  };

  "StudentT sample mean approximation"_test = [] {
    std::mt19937_64 gen{123};
    StudentT dist{10.0};  // Use moderate ν for finite variance

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      sum += dist.sample(gen);
    }
    double sample_mean = sum / N;

    // Sample mean should approximate theoretical mean (0)
    expectNear(0.0, sample_mean, 0.1);
  };

  "StudentT sample variance approximation"_test = [] {
    std::mt19937_64 gen{456};
    StudentT dist{10.0};  // Var = 10/8 = 1.25

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

    // Sample variance should approximate theoretical variance (10/8 = 1.25)
    expectNear(1.25, sample_variance, 0.15);
  };

  "StudentT sample Cauchy case (ν=1)"_test = [] {
    std::mt19937_64 gen{789};
    StudentT dist{1.0};

    // Generate samples - all should be finite (though variance is infinite)
    for (int i = 0; i < 1000; ++i) {
      auto x = dist.sample(gen);
      expectTrue(std::isfinite(x));
    }
  };

  "StudentT sample large ν approaches normal"_test = [] {
    std::mt19937_64 gen{321};
    StudentT dist{100.0};  // Nearly standard normal

    const int N = 10'000;
    double sum = 0.0;
    double sum_sq = 0.0;

    for (int i = 0; i < N; ++i) {
      auto x = dist.sample(gen);
      sum += x;
      sum_sq += x * x;
    }

    double sample_mean = sum / N;
    double sample_variance = sum_sq / N - sample_mean * sample_mean;

    // Should be close to N(0,1)
    expectNear(0.0, sample_mean, 0.05);
    expectNear(1.0, sample_variance, 0.1);
  };

  "StudentT sample different parameters"_test = [] {
    std::mt19937_64 g1{100};
    std::mt19937_64 g2{100};
    StudentT dist1{2.0};   // Heavy tails
    StudentT dist2{100.0}; // Nearly normal

    // Different ν affect variance significantly
    double sum_sq1 = 0.0;
    double sum_sq2 = 0.0;
    const int N = 1000;

    for (int i = 0; i < N; ++i) {
      auto x1 = dist1.sample(g1);
      auto x2 = dist2.sample(g2);
      sum_sq1 += x1 * x1;
      sum_sq2 += x2 * x2;
    }

    // ν=2 has infinite variance, ν=100 has variance ≈ 1
    // sum_sq1 should generally be larger
    expectTrue(sum_sq1 > sum_sq2 * 1.5);
  };

  "StudentT constexpr accessors"_test = [] {
    // Note: Full constexpr construction requires constexpr lgamma
    // which is not available in standard library. Test runtime construction.
    StudentT dist{5.0};
    expectEq(5.0, dist.nu());
    expectEq(0.0, dist.mean());
  };

  "StudentT PDF integrates to approximately 1"_test = [] {
    StudentT dist{5.0};

    double integral = 0.0;
    const double dx = 0.01;
    // Integrate from -20 to 20 (captures most mass even for heavy tails)
    for (double x = -20.0; x <= 20.0; x += dx) {
      integral += dist.prob(x) * dx;
    }

    expectNear(1.0, integral, 0.02);
  };

  "StudentT CDF derivative approximates PDF"_test = [] {
    StudentT dist{5.0};

    const double h = 1e-5;
    for (double x : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
      double numerical_deriv = (dist.cdf(x + h) - dist.cdf(x - h)) / (2.0 * h);
      double pdf = dist.prob(x);
      expectNear(pdf, numerical_deriv, 0.001);
    }
  };

  "StudentT ν controls tail weight"_test = [] {
    // Lower ν = heavier tails
    StudentT dist1{1.0};  // Cauchy
    StudentT dist2{5.0};
    StudentT dist3{30.0}; // Nearly normal

    // At x = 4, heavier tails mean more probability
    expectTrue(dist1.prob(4.0) > dist2.prob(4.0));
    expectTrue(dist2.prob(4.0) > dist3.prob(4.0));

    // At x = 0 (center), the relationship reverses (tighter peak for normal)
    expectTrue(dist1.prob(0.0) < dist2.prob(0.0));
    expectTrue(dist2.prob(0.0) < dist3.prob(0.0));
  };

  "StudentT approaches normal as ν increases"_test = [] {
    // For large ν, t-distribution ≈ N(0, 1)
    StudentT t_dist{1000.0};

    // Compare PDF at several points to standard normal
    for (double x : {0.0, 1.0, 2.0}) {
      double normal_pdf =
          std::exp(-x * x / 2.0) / std::sqrt(2.0 * std::numbers::pi);
      double t_pdf = t_dist.prob(x);
      expectNear(normal_pdf, t_pdf, 0.001);
    }
  };

  "StudentT float type"_test = [] {
    StudentT<float> dist{5.0f};
    expectEq(5.0f, dist.nu());
    expectEq(0.0f, dist.mean());
    expectTrue(dist.prob(0.0f) > 0.0f);
  };

  "StudentT long double type"_test = [] {
    StudentT<long double> dist{5.0L};
    expectEq(5.0L, dist.nu());
    expectEq(0.0L, dist.mean());
    expectTrue(dist.prob(0.0L) > 0.0L);
  };

  "StudentT confidence interval property"_test = [] {
    // For ν=10, t₀.₉₇₅ ≈ 2.228 (97.5th percentile)
    // CDF(2.228) should be approximately 0.975
    StudentT dist{10.0};
    double t_975 = 2.228;
    expectNear(0.975, dist.cdf(t_975), 0.002);
  };

  return TestRegistry::result();
}
