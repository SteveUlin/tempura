#include "bayes/random_simd.h"

#include <print>

#include "benchmark.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "simd::XorShiftSimd"_test = [] {
    const Vec512i64 seed{{7073242132491550564, 1179269353366884230,
                          3941578509859010014, 4437109666059500420,
                          4035966242879597485, 3373052566401125716,
                          1556011196226971778, 1235654174036890696}};
    auto xorShift = RNG{seed, makeXorShiftSimd()};

    auto value = xorShift();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Value[{}]: {}", i, value[i]);
    }
    std::println("---");
    value = xorShift();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Value[{}]: {}", i, value[i]);
    }
  };

  "simd::MultiplyWithCarrySimd"_test = [] {
    const Vec512i64 seed{{7073242132491550564, 1179269353366884230,
                          3941578509859010014, 4437109666059500420,
                          4035966242879597485, 3373052566401125716,
                          1556011196226971778, 1235654174036890696}};
    auto mwc = RNG{seed, makeMultiplyWithCarrySimd()};

    auto value = mwc();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Value[{}]: {}", i, value[i]);
    }
    std::println("---");
    value = mwc();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Value[{}]: {}", i, value[i]);
    }
  };

  "simd::LinearCongruentialSimd"_test = [] {
    const Vec512i64 seed{{7073242132491550564, 1179269353366884230,
                          3941578509859010014, 4437109666059500420,
                          4035966242879597485, 3373052566401125716,
                          1556011196226971778, 1235654174036890696}};
    auto lc = RNG{seed, makeLinearCongruentialSimd()};

    auto value = lc();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Value[{}]: {}", i, value[i]);
    }
    std::println("---");
    value = lc();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Value[{}]: {}", i, value[i]);
    }
  };

  "simd::RandSimd"_test = [] {
    const Vec512i64 seed{{7073242132491550564, 1179269353366884230,
                          3941578509859010014, 4437109666059500420,
                          4035966242879597485, 3373052566401125716,
                          1556011196226971778, 1235654174036890696}};
    auto rand = makeSimdRand();
    auto value = rand();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Value[{}]: {}", i, value[i]);
    }
    std::println("---");
    value = rand();
    for (std::size_t i = 0; i < value.size(); ++i) {
      std::println("Value[{}]: {}", i, value[i]);
    }
  };

  volatile uint_fast64_t out;
  "mt bench"_bench.ops(100) = [&out, mt = std::mt19937{123456}] mutable {
    for (int i = 0; i < 100; ++i) {
      out = mt();
    }
  };

  Vec512i64 out_simd;
  "rand bench"_bench.ops(800) = [&out_simd, rand = makeSimdRand()] mutable {
    for (int i = 0; i < 100; ++i) {
      out_simd = rand();
    }
  };
}
