#include "probabilistic/hyperloglog.h"

#include <cstdint>
#include <functional>
#include <random>
#include <unordered_set>

#include "unit.h"

using namespace tempura;

// Simple hash function for testing - std::hash is sufficient for these tests
// In production, use a proper hash like xxHash or MurmurHash
auto hashValue(std::uint64_t x) -> std::uint64_t {
  // Splitmix64 - fast and well-distributed
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}

auto main() -> int {
  "empty estimator returns near zero"_test = [] {
    HyperLogLog<14> hll;
    // Empty estimator should return 0 or very close to it
    expectLT(hll.count(), 1.0);
  };

  "single element"_test = [] {
    HyperLogLog<14> hll;
    hll.add(hashValue(42));
    // Should estimate ~1 distinct element
    double estimate = hll.count();
    expectGT(estimate, 0.5);
    expectLT(estimate, 3.0);
  };

  "duplicate elements counted once"_test = [] {
    HyperLogLog<14> hll;
    // Add same element many times
    for (int i = 0; i < 1000; ++i) {
      hll.add(hashValue(42));
    }
    // Should still estimate ~1 distinct element
    double estimate = hll.count();
    expectGT(estimate, 0.5);
    expectLT(estimate, 3.0);
  };

  "known cardinality 100"_test = [] {
    HyperLogLog<14> hll;

    for (std::uint64_t i = 0; i < 100; ++i) {
      hll.add(hashValue(i));
    }

    double estimate = hll.count();
    double true_count = 100.0;

    // Theoretical error for P=14: 1.04/sqrt(16384) ≈ 0.81%
    // Allow 10% tolerance to account for random variation
    // This is ~12 standard errors, so effectively never fails
    double tolerance = true_count * 0.10;
    expectNear(estimate, true_count, tolerance);
  };

  "known cardinality 1000"_test = [] {
    HyperLogLog<14> hll;

    for (std::uint64_t i = 0; i < 1000; ++i) {
      hll.add(hashValue(i));
    }

    double estimate = hll.count();
    double true_count = 1000.0;

    // Standard error ≈ 0.81% of true count = 8.1
    // Tolerance of 5% = 50, about 6 standard errors
    double tolerance = true_count * 0.05;
    expectNear(estimate, true_count, tolerance);
  };

  "known cardinality 10000"_test = [] {
    HyperLogLog<14> hll;

    for (std::uint64_t i = 0; i < 10000; ++i) {
      hll.add(hashValue(i));
    }

    double estimate = hll.count();
    double true_count = 10000.0;

    // Standard error ≈ 0.81% of true count = 81
    // Tolerance of 3% = 300, about 3.7 standard errors
    double tolerance = true_count * 0.03;
    expectNear(estimate, true_count, tolerance);
  };

  "known cardinality 100000"_test = [] {
    HyperLogLog<14> hll;

    for (std::uint64_t i = 0; i < 100000; ++i) {
      hll.add(hashValue(i));
    }

    double estimate = hll.count();
    double true_count = 100000.0;

    // Standard error ≈ 0.81% of true count = 810
    // Tolerance of 2% = 2000, about 2.5 standard errors
    double tolerance = true_count * 0.02;
    expectNear(estimate, true_count, tolerance);
  };

  "merge combines estimates"_test = [] {
    HyperLogLog<14> hll1;
    HyperLogLog<14> hll2;

    // First HLL gets 0-499
    for (std::uint64_t i = 0; i < 500; ++i) {
      hll1.add(hashValue(i));
    }

    // Second HLL gets 500-999
    for (std::uint64_t i = 500; i < 1000; ++i) {
      hll2.add(hashValue(i));
    }

    // Verify individual estimates are reasonable
    expectNear(hll1.count(), 500.0, 100.0);
    expectNear(hll2.count(), 500.0, 100.0);

    // Merge should give union cardinality
    hll1.merge(hll2);
    expectNear(hll1.count(), 1000.0, 100.0);
  };

  "merge with overlap"_test = [] {
    HyperLogLog<14> hll1;
    HyperLogLog<14> hll2;

    // First HLL gets 0-999
    for (std::uint64_t i = 0; i < 1000; ++i) {
      hll1.add(hashValue(i));
    }

    // Second HLL gets 500-1499 (50% overlap)
    for (std::uint64_t i = 500; i < 1500; ++i) {
      hll2.add(hashValue(i));
    }

    // Merge gives union: 0-1499 = 1500 distinct
    hll1.merge(hll2);
    double estimate = hll1.count();
    double true_count = 1500.0;

    // Standard error ≈ 0.81% of 1500 = 12.15
    // Tolerance of 5% = 75, about 6 standard errors
    expectNear(estimate, true_count, true_count * 0.05);
  };

  "clear resets estimator"_test = [] {
    HyperLogLog<14> hll;

    for (std::uint64_t i = 0; i < 1000; ++i) {
      hll.add(hashValue(i));
    }

    expectGT(hll.count(), 500.0);

    hll.clear();
    expectLT(hll.count(), 1.0);

    // Can add more after clear
    for (std::uint64_t i = 0; i < 100; ++i) {
      hll.add(hashValue(i + 10000));  // Different values
    }
    expectNear(hll.count(), 100.0, 20.0);
  };

  "different precisions"_test = [] {
    HyperLogLog<10> hll_small;  // 1024 registers, ~3.25% error
    HyperLogLog<14> hll_med;    // 16384 registers, ~0.81% error
    HyperLogLog<16> hll_large;  // 65536 registers, ~0.41% error

    for (std::uint64_t i = 0; i < 10000; ++i) {
      std::uint64_t h = hashValue(i);
      hll_small.add(h);
      hll_med.add(h);
      hll_large.add(h);
    }

    // All should estimate ~10000, but with different accuracy
    // P=10: error ~3.25%, tolerance 10%
    expectNear(hll_small.count(), 10000.0, 1000.0);
    // P=14: error ~0.81%, tolerance 3%
    expectNear(hll_med.count(), 10000.0, 300.0);
    // P=16: error ~0.41%, tolerance 2%
    expectNear(hll_large.count(), 10000.0, 200.0);
  };

  "theoretical error computation"_test = [] {
    // Verify theoretical error formula: 1.04 / sqrt(m)
    // P=14: m=16384, error = 1.04/128 ≈ 0.008125
    double error_p14 = HyperLogLog<14>::theoreticalError();
    expectNear(error_p14, 0.008125, 0.0001);

    // P=10: m=1024, error = 1.04/32 = 0.0325
    double error_p10 = HyperLogLog<10>::theoreticalError();
    expectNear(error_p10, 0.0325, 0.0001);
  };

  "register count matches precision"_test = [] {
    expectEq(HyperLogLog<4>::numRegisters(), 16uz);
    expectEq(HyperLogLog<10>::numRegisters(), 1024uz);
    expectEq(HyperLogLog<14>::numRegisters(), 16384uz);
    expectEq(HyperLogLog<16>::numRegisters(), 65536uz);
  };

  return TestRegistry::result();
}
