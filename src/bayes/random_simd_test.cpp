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
