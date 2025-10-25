#include "bayes/random_simd.h"

#include <print>
#include <random>

#include "benchmark.h"
#include "simd/simd.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "simd::XorShiftSimd"_test = [] {
    auto xorShift = Generator{kDefaultSimdRandomSeed, makeXorShiftSimd()};

    auto value = xorShift();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("XorShift Value[{}]: {}", i, value[i]);
    }
    std::println("---");
    value = xorShift();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("XorShift Value[{}]: {}", i, value[i]);
    }
  };

  "simd::MultiplyWithCarrySimd"_test = [] {
    auto mwc = Generator{kDefaultSimdRandomSeed, makeMultiplyWithCarrySimd()};

    auto value = mwc();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("MWC Value[{}]: {}", i, value[i]);
    }
    std::println("---");
    value = mwc();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("MWC Value[{}]: {}", i, value[i]);
    }
  };

  "simd::LinearCongruentialSimd"_test = [] {
    auto lcg = Generator{kDefaultSimdRandomSeed, makeLinearCongruentialSimd()};

    auto value = lcg();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("LCG Value[{}]: {}", i, value[i]);
    }
    std::println("---");
    value = lcg();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("LCG Value[{}]: {}", i, value[i]);
    }
  };

  "simd::SimdRandom"_test = [] {
    auto rand = makeSimdRandom();
    auto value = rand();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Combined Value[{}]: {}", i, value[i]);
    }
    std::println("---");
    value = rand();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Combined Value[{}]: {}", i, value[i]);
    }
  };

  "simd::CustomSeedSimdRandom"_test = [] {
    Vec8i64 custom_seed{1, 2, 3, 4, 5, 6, 7, 8};
    auto rand = makeSimdRandom(custom_seed);
    auto value = rand();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Custom Seed Value[{}]: {}", i, value[i]);
    }
  };

  "simd::toUniform"_test = [] {
    auto rand = makeSimdRandom();
    auto int_vals = rand();
    auto uniform_vals = toUniform(int_vals);

    std::println("Testing toUniform conversion:");
    for (std::size_t i = 0; i < 8; ++i) {
      double val = uniform_vals[i];
      std::println("  Lane[{}]: {:.10f}", i, val);

      // Verify values are in [0, 1)
      assert(val >= 0.0 && "Uniform value should be >= 0.0");
      assert(val < 1.0 && "Uniform value should be < 1.0");
    }
  };

  "simd::toUniformRange scalar"_test = [] {
    auto rand = makeSimdRandom();
    auto int_vals = rand();
    auto uniform_vals = toUniformRange(int_vals, -5.0, 5.0);

    std::println("Testing toUniformRange (scalar) [-5, 5):");
    for (std::size_t i = 0; i < 8; ++i) {
      double val = uniform_vals[i];
      std::println("  Lane[{}]: {:.10f}", i, val);

      // Verify values are in [-5, 5)
      assert(val >= -5.0 && "Uniform value should be >= -5.0");
      assert(val < 5.0 && "Uniform value should be < 5.0");
    }
  };

  "simd::toUniformRange vector"_test = [] {
    auto rand = makeSimdRandom();
    auto int_vals = rand();

    // Different range for each lane
    Vec8d lower{0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    Vec8d upper{10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0};
    auto uniform_vals = toUniformRange(int_vals, lower, upper);

    std::println("Testing toUniformRange (vector) with per-lane ranges:");
    for (std::size_t i = 0; i < 8; ++i) {
      double val = uniform_vals[i];
      double lo = lower[i];
      double hi = upper[i];
      std::println("  Lane[{}]: {:.10f} (expected in [{:.1f}, {:.1f}))", i, val,
                   lo, hi);

      // Verify values are in the correct range for each lane
      assert(val >= lo && "Uniform value should be >= lower bound");
      assert(val < hi && "Uniform value should be < upper bound");
    }
  };

  "simd::uniformDistributionTest"_test = [] {
    auto rand = makeSimdRandom();

    // Generate many samples and verify they're well distributed
    constexpr int kNumSamples = 1000;
    std::array<int, 10> histogram{};

    for (int iter = 0; iter < kNumSamples / 8; ++iter) {
      auto int_vals = rand();
      auto uniform_vals = toUniform(int_vals);

      for (std::size_t i = 0; i < 8; ++i) {
        double val = uniform_vals[i];
        int bucket = static_cast<int>(val * 10.0);
        if (bucket >= 0 && bucket < 10) {
          histogram[bucket]++;
        }
      }
    }

    std::println("Uniform distribution histogram (1000 samples, 10 buckets):");
    for (std::size_t i = 0; i < histogram.size(); ++i) {
      std::println("  Bucket[{:1d}]: {:4d}", i, histogram[i]);

      // Each bucket should have approximately 100 samples
      // Allow for some statistical variation (±50%)
      assert(histogram[i] >= 50 &&
             "Bucket should have at least 50 samples (expected ~100)");
      assert(histogram[i] <= 150 &&
             "Bucket should have at most 150 samples (expected ~100)");
    }
  };

  "simd::boxMuller"_test = [] {
    auto rand = makeSimdRandom();
    auto rand1 = rand();
    auto rand2 = rand();
    auto [z0, z1] = boxMuller(rand1, rand2);

    std::println("Testing Box-Muller N(0,1):");
    std::println("  z0 values:");
    for (std::size_t i = 0; i < 8; ++i) {
      std::println("    Lane[{}]: {:.6f}", i, z0[i]);
    }
    std::println("  z1 values:");
    for (std::size_t i = 0; i < 8; ++i) {
      std::println("    Lane[{}]: {:.6f}", i, z1[i]);
    }

    // Verify values are reasonable (within ±5 sigma should be ~99.9999% of values)
    for (std::size_t i = 0; i < 8; ++i) {
      assert(z0[i] >= -5.0 && z0[i] <= 5.0 &&
             "Normal value should be within ±5 sigma");
      assert(z1[i] >= -5.0 && z1[i] <= 5.0 &&
             "Normal value should be within ±5 sigma");
    }
  };

  "simd::boxMuller custom params"_test = [] {
    auto rand = makeSimdRandom();
    auto rand1 = rand();
    auto rand2 = rand();
    double mean = 100.0;
    double stddev = 15.0;
    auto [z0, z1] = boxMuller(rand1, rand2, mean, stddev);

    std::println("Testing Box-Muller N(100, 15):");
    std::println("  z0 values:");
    for (std::size_t i = 0; i < 8; ++i) {
      double val = z0[i];
      std::println("    Lane[{}]: {:.6f}", i, val);
      // Values should be roughly within mean ± 3*stddev (99.7% of values)
      assert(val >= mean - 5 * stddev && val <= mean + 5 * stddev);
    }
  };

  "simd::boxMuller per-lane params"_test = [] {
    auto rand = makeSimdRandom();
    auto rand1 = rand();
    auto rand2 = rand();

    Vec8d mean{0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0};
    Vec8d stddev{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    auto [z0, z1] = boxMuller(rand1, rand2, mean, stddev);

    std::println("Testing Box-Muller with per-lane parameters:");
    for (std::size_t i = 0; i < 8; ++i) {
      double val = z0[i];
      double μ = mean[i];
      double σ = stddev[i];
      std::println("  Lane[{}]: {:.6f} (N({:.1f}, {:.1f}))", i, val, μ, σ);

      // Values should be roughly within mean ± 5*stddev
      assert(val >= μ - 5 * σ && val <= μ + 5 * σ);
    }
  };

  "simd::normalDistributionTest"_test = [] {
    auto rand = makeSimdRandom();

    // Generate many samples and verify mean and variance
    constexpr int kNumPairs = 500;  // 500 pairs = 1000 samples per lane
    double sum = 0.0;
    double sum_sq = 0.0;

    for (int iter = 0; iter < kNumPairs; ++iter) {
      auto rand1 = rand();
      auto rand2 = rand();
      auto [z0, z1] = boxMuller(rand1, rand2);

      for (std::size_t i = 0; i < 8; ++i) {
        sum += z0[i];
        sum += z1[i];
        sum_sq += z0[i] * z0[i];
        sum_sq += z1[i] * z1[i];
      }
    }

    int total_samples = kNumPairs * 16;  // 500 pairs * 2 vectors * 8 lanes
    double mean = sum / total_samples;
    double variance = (sum_sq / total_samples) - (mean * mean);
    double stddev = std::sqrt(variance);

    std::println("Normal distribution statistics ({} samples):", total_samples);
    std::println("  Mean: {:.6f} (expected 0.0)", mean);
    std::println("  Variance: {:.6f} (expected 1.0)", variance);
    std::println("  Std dev: {:.6f} (expected 1.0)", stddev);

    // Verify mean is close to 0 (within ±0.1 for large sample)
    assert(mean >= -0.1 && mean <= 0.1 && "Mean should be close to 0");

    // Verify variance is close to 1 (within ±0.2 for statistical variation)
    assert(variance >= 0.8 && variance <= 1.2 &&
           "Variance should be close to 1");
  };

  volatile uint64_t out;
  "mt19937 bench"_bench.ops(10'000) = [&out, mt = std::mt19937_64{123456}] mutable {
    for (int i = 0; i < 100; ++i) {
      out = mt();
    }
  };

  Vec8i64 out_simd{};
  "simd random bench"_bench.ops(80'000) = [&out_simd, rand = makeSimdRandom()] mutable {
    for (int i = 0; i < 100; ++i) {
      out_simd = rand();
    }
  };

  return TestRegistry::result();
}
