#include "bayes/random.h"

#include <cassert>
#include <limits>
#include <print>
#include <random>

#include "benchmark.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "XorShift Left"_test = [] {
    auto xs = Generator{12345ULL, XorShift<ShiftDirection::Left>{XorShiftPresets::kAll[0]}};

    auto value = xs();
    std::println("XorShift Left: {}", value);
    assert(value != 0);  // Should produce non-zero output

    auto value2 = xs();
    std::println("XorShift Left: {}", value2);
    assert(value2 != value);  // Should be different
  };

  "XorShift Right"_test = [] {
    auto xs = Generator{12345ULL, XorShift<ShiftDirection::Right>{XorShiftPresets::kAll[2]}};

    auto value = xs();
    std::println("XorShift Right: {}", value);
    assert(value != 0);
  };

  "MultiplyWithCarry"_test = [] {
    auto mwc = Generator{12345ULL, MultiplyWithCarry{MultiplyWithCarryPresets::kAll[0]}};

    auto value = mwc();
    std::println("MultiplyWithCarry: {}", value);
    assert(value != 0);

    auto value2 = mwc();
    std::println("MultiplyWithCarry: {}", value2);
    assert(value2 != value);
  };

  "LinearCongruential"_test = [] {
    auto lcg = Generator{12345ULL, LinearCongruential{LinearCongruentialPresets::kAll[0]}};

    auto value = lcg();
    std::println("LinearCongruential: {}", value);

    auto value2 = lcg();
    std::println("LinearCongruential: {}", value2);
    assert(value2 != value);
  };

  "makeRandom default seed"_test = [] {
    auto rng = makeRandom();

    auto value = rng();
    std::println("Combined RNG (default seed): {}", value);
    assert(value != 0);

    auto value2 = rng();
    std::println("Combined RNG (default seed): {}", value2);
    assert(value2 != value);
  };

  "makeRandom custom seed"_test = [] {
    auto rng1 = makeRandom(42);
    auto rng2 = makeRandom(42);

    // Same seed should produce same sequence
    auto v1 = rng1();
    auto v2 = rng2();
    std::println("RNG1 (seed=42): {}", v1);
    std::println("RNG2 (seed=42): {}", v2);
    assert(v1 == v2);

    // Next values should also match
    v1 = rng1();
    v2 = rng2();
    assert(v1 == v2);
  };

  "Generator min/max"_test = [] {
    auto rng = Generator{0ULL, XorShift<ShiftDirection::Left>{XorShiftPresets::kAll[0]}};

    static_assert(decltype(rng)::min() == 0);
    static_assert(decltype(rng)::max() == std::numeric_limits<uint64_t>::max());

    std::println("Generator::min() = {}", decltype(rng)::min());
    std::println("Generator::max() = {}", decltype(rng)::max());
  };

  "Test all XorShift presets"_test = [] {
    for (std::size_t i = 0; i < XorShiftPresets::kAll.size(); ++i) {
      auto xs = Generator{12345ULL, XorShift<ShiftDirection::Left>{XorShiftPresets::kAll[i]}};
      auto value = xs();
      std::println("XorShift preset[{}]: {}", i, value);
      assert(value != 0);
    }
  };

  "Test all MultiplyWithCarry presets"_test = [] {
    for (std::size_t i = 0; i < MultiplyWithCarryPresets::kAll.size(); ++i) {
      auto mwc = Generator{12345ULL, MultiplyWithCarry{MultiplyWithCarryPresets::kAll[i]}};
      auto value = mwc();
      std::println("MultiplyWithCarry preset[{}]: {}", i, value);
      assert(value != 0);
    }
  };

  "Test all LinearCongruential presets"_test = [] {
    for (std::size_t i = 0; i < LinearCongruentialPresets::kAll.size(); ++i) {
      auto lcg = Generator{12345ULL, LinearCongruential{LinearCongruentialPresets::kAll[i]}};
      auto value = lcg();
      std::println("LinearCongruential preset[{}]: {}", i, value);
    }
  };

  "seed() method"_test = [] {
    auto rng = makeRandom(123);
    auto v1 = rng();

    // Can't directly reseed the lambda, but can test that Generator supports it
    auto gen = Generator{123ULL, XorShift<ShiftDirection::Left>{XorShiftPresets::kAll[0]}};
    auto initial = gen();

    gen.seed(456ULL);
    auto after_reseed = gen();

    std::println("Initial value: {}", initial);
    std::println("After reseed: {}", after_reseed);
    // Different seed should eventually produce different values
  };

  "constexpr Generator"_test = [] {
    // Test that Generator can be used in constexpr context
    constexpr auto test_constexpr = []() constexpr {
      auto gen = Generator{42ULL, XorShift<ShiftDirection::Left>{XorShiftPresets::kAll[0]}};
      auto v1 = gen();
      auto v2 = gen();
      return v1 != v2;
    };

    static_assert(test_constexpr());
    std::println("Generator is constexpr-compatible");
  };

  // Benchmarks
  volatile uint64_t out;

  "std::mt19937_64 bench"_bench.ops(10'000) = [&out, gen = std::mt19937_64{123456}] mutable {
    for (int i = 0; i < 100; ++i) {
      out = gen();
    }
  };

  "makeRandom bench"_bench.ops(10'000) = [&out, rng = makeRandom(123456)] mutable {
    for (int i = 0; i < 100; ++i) {
      out = rng();
    }
  };

  "XorShift bench"_bench.ops(10'000) = [&out, xs = Generator{123456ULL, XorShift<ShiftDirection::Left>{XorShiftPresets::kAll[0]}}] mutable {
    for (int i = 0; i < 100; ++i) {
      out = xs();
    }
  };

  "MultiplyWithCarry bench"_bench.ops(10'000) = [&out, mwc = Generator{123456ULL, MultiplyWithCarry{MultiplyWithCarryPresets::kAll[0]}}] mutable {
    for (int i = 0; i < 100; ++i) {
      out = mwc();
    }
  };

  "LinearCongruential bench"_bench.ops(10'000) = [&out, lcg = Generator{123456ULL, LinearCongruential{LinearCongruentialPresets::kAll[0]}}] mutable {
    for (int i = 0; i < 100; ++i) {
      out = lcg();
    }
  };

  return TestRegistry::result();
}
