#include "probabilistic/t_digest.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty digest"_test = [] {
    TDigest<> digest;
    expectEq(digest.count(), 0.0);
    expectEq(digest.centroidCount(), 0uz);
    expectTrue(std::isnan(digest.quantile(0.5)));
    expectTrue(std::isnan(digest.cdf(0.0)));
  };

  "single value"_test = [] {
    TDigest<> digest;
    digest.add(42.0);

    expectEq(digest.count(), 1.0);
    expectEq(digest.quantile(0.0), 42.0);
    expectEq(digest.quantile(0.5), 42.0);
    expectEq(digest.quantile(1.0), 42.0);

    // CDF for single value
    expectEq(digest.cdf(41.0), 0.0);
    expectEq(digest.cdf(42.0), 0.5);
    expectEq(digest.cdf(43.0), 1.0);
  };

  "exact values for small dataset"_test = [] {
    TDigest<> digest;

    // Add values 1 through 10
    for (int i = 1; i <= 10; ++i) {
      digest.add(static_cast<double>(i));
    }

    expectEq(digest.count(), 10.0);

    // With small datasets, we should get reasonable estimates
    // Median should be between 5 and 6 (5.5 for continuous, but we have discrete data)
    double median = digest.quantile(0.5);
    expectNear(median, 5.5, 1.0);

    // Min and max quantiles
    expectNear(digest.quantile(0.0), 1.0, 0.5);
    expectNear(digest.quantile(1.0), 10.0, 0.5);
  };

  "weighted values"_test = [] {
    TDigest<> digest;

    // Add 10 with weight 1, 20 with weight 3
    // Total weight = 4, so median (q=0.5) is at cumulative weight 2.0
    // Since first centroid has weight 1, median falls in the gap or second centroid
    digest.add(10.0, 1.0);
    digest.add(20.0, 3.0);

    expectEq(digest.count(), 4.0);

    // Median should be interpolated between 10 and 20
    // With weights [1, 3], median at cumulative 2.0 should be between centroids
    double median = digest.quantile(0.5);
    expectGE(median, 10.0);
    expectLE(median, 20.0);

    // Higher quantiles should be closer to 20
    double q75 = digest.quantile(0.75);
    expectGT(q75, median);
  };

  "median estimation accuracy"_test = [] {
    TDigest<100> digest;

    // Add 1000 values uniformly distributed
    for (int i = 1; i <= 1000; ++i) {
      digest.add(static_cast<double>(i));
    }

    // True median is 500.5
    double estimated_median = digest.quantile(0.5);

    // Tolerance: with compression=100, typical error ~3%, so ~15 for values around 500
    // Using 25 to be safe with randomization in compression
    expectNear(estimated_median, 500.5, 25.0);

    // Check quartiles too
    double q25 = digest.quantile(0.25);
    double q75 = digest.quantile(0.75);

    // True Q1 = 250.5, Q3 = 750.5
    expectNear(q25, 250.5, 25.0);
    expectNear(q75, 750.5, 25.0);
  };

  "tail quantiles p99 accuracy"_test = [] {
    // T-Digest is specifically designed for tail accuracy
    TDigest<100> digest;

    // Create a known distribution: 10000 values from 0 to 9999
    for (int i = 0; i < 10000; ++i) {
      digest.add(static_cast<double>(i));
    }

    // True p99 = 9900 (the 99th percentile of 0-9999)
    double p99 = digest.quantile(0.99);

    // T-Digest should be very accurate at tails
    // Relative error should be < 1% for p99
    // Tolerance: 1% of 9900 = 99, but arcsin scale gives better accuracy
    expectNear(p99, 9900.0, 150.0);

    // p1 should also be accurate
    double p01 = digest.quantile(0.01);
    expectNear(p01, 100.0, 150.0);
  };

  "normal distribution quantiles"_test = [] {
    TDigest<200> digest;
    std::mt19937 rng{42};  // Fixed seed for reproducibility
    std::normal_distribution<double> dist{100.0, 15.0};  // Mean 100, std 15

    constexpr int kSamples = 10000;
    std::vector<double> values;
    values.reserve(kSamples);

    for (int i = 0; i < kSamples; ++i) {
      double v = dist(rng);
      values.push_back(v);
      digest.add(v);
    }

    // Sort values to get true quantiles
    std::sort(values.begin(), values.end());

    // Compare estimated vs true median
    double true_median = values[kSamples / 2];
    double estimated_median = digest.quantile(0.5);

    // Standard error for median of normal: ~1.25 * sigma / sqrt(n) ≈ 0.19
    // Use tolerance of 3 (about 15 standard errors)
    expectNear(estimated_median, true_median, 3.0);

    // Compare p99
    double true_p99 = values[static_cast<std::size_t>(0.99 * kSamples)];
    double estimated_p99 = digest.quantile(0.99);
    expectNear(estimated_p99, true_p99, 5.0);
  };

  "cdf basic"_test = [] {
    TDigest<> digest;

    for (int i = 1; i <= 100; ++i) {
      digest.add(static_cast<double>(i));
    }

    // CDF at 50 should be approximately 0.5
    double cdf_50 = digest.cdf(50.0);
    expectNear(cdf_50, 0.5, 0.1);

    // CDF at 25 should be approximately 0.25
    double cdf_25 = digest.cdf(25.0);
    expectNear(cdf_25, 0.25, 0.1);

    // CDF at extremes
    expectEq(digest.cdf(0.0), 0.0);
    expectEq(digest.cdf(101.0), 1.0);
  };

  "merge combines digests"_test = [] {
    TDigest<100> digest1;
    TDigest<100> digest2;

    // First digest: values 1-50
    for (int i = 1; i <= 50; ++i) {
      digest1.add(static_cast<double>(i));
    }

    // Second digest: values 51-100
    for (int i = 51; i <= 100; ++i) {
      digest2.add(static_cast<double>(i));
    }

    expectEq(digest1.count(), 50.0);
    expectEq(digest2.count(), 50.0);

    // Merge
    digest1.merge(digest2);

    expectEq(digest1.count(), 100.0);

    // After merge, should have full range
    double median = digest1.quantile(0.5);
    expectNear(median, 50.5, 5.0);

    double q25 = digest1.quantile(0.25);
    double q75 = digest1.quantile(0.75);
    expectNear(q25, 25.5, 5.0);
    expectNear(q75, 75.5, 5.0);
  };

  "merge empty digest"_test = [] {
    TDigest<> digest1;
    TDigest<> digest2;

    digest1.add(10.0);
    digest1.add(20.0);

    // Merge empty into non-empty
    digest1.merge(digest2);
    expectEq(digest1.count(), 2.0);

    // Merge non-empty into empty
    TDigest<> digest3;
    digest3.merge(digest1);
    expectEq(digest3.count(), 2.0);
  };

  "clear resets digest"_test = [] {
    TDigest<> digest;

    for (int i = 0; i < 100; ++i) {
      digest.add(static_cast<double>(i));
    }

    expectEq(digest.count(), 100.0);
    expectGT(digest.centroidCount(), 0uz);

    digest.clear();

    expectEq(digest.count(), 0.0);
    expectEq(digest.centroidCount(), 0uz);
    expectTrue(std::isnan(digest.quantile(0.5)));
  };

  "compression keeps centroid count bounded"_test = [] {
    TDigest<50> digest;  // Lower compression = fewer centroids

    // Add many values
    for (int i = 0; i < 10000; ++i) {
      digest.add(static_cast<double>(i));
    }

    // Centroid count should be bounded by ~2*Compression
    expectLE(digest.centroidCount(), 150uz);
  };

  "different compression levels"_test = [] {
    TDigest<25> low_compression;
    TDigest<200> high_compression;

    std::mt19937 rng{123};
    std::uniform_real_distribution<double> dist{0.0, 1000.0};

    for (int i = 0; i < 5000; ++i) {
      double v = dist(rng);
      low_compression.add(v);
      high_compression.add(v);
    }

    // Higher compression should have more centroids (up to a point)
    expectLE(low_compression.centroidCount(), high_compression.centroidCount());

    // Both should give reasonable median estimates
    // (can't compare directly since they saw same data in different order)
    double low_median = low_compression.quantile(0.5);
    double high_median = high_compression.quantile(0.5);

    // Both should be near 500 (middle of uniform 0-1000)
    expectNear(low_median, 500.0, 100.0);
    expectNear(high_median, 500.0, 100.0);
  };

  "quantile monotonicity"_test = [] {
    TDigest<100> digest;

    std::mt19937 rng{456};
    std::uniform_real_distribution<double> dist{0.0, 100.0};

    for (int i = 0; i < 1000; ++i) {
      digest.add(dist(rng));
    }

    // Quantile function should be monotonically non-decreasing
    double prev = digest.quantile(0.0);
    for (double q = 0.01; q <= 1.0; q += 0.01) {
      double current = digest.quantile(q);
      expectGE(current, prev);
      prev = current;
    }
  };

  return TestRegistry::result();
}
