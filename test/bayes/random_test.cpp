#include "bayes/random.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <print>
#include <random>

#include "benchmark.h"
#include "unit.h"

using namespace tempura;

struct ChiSquaredOptions {
  int nbins = 1'000;
  double min = 0.0;
  double max = 1.0;
};
auto chiSquareTest(std::ranges::range auto&& data,
                   ChiSquaredOptions options = {}) {
  std::vector<int> observed_frequencies(options.nbins, 0);
  for (const double value : data) {
    assert(value >= options.min && value <= options.max);

    int bin = static_cast<int>((value - options.min) /
                               ((options.max - options.min) / options.nbins));
    if (bin >= 0 && bin < options.nbins) {
      ++observed_frequencies[bin];
    }
  }
}

template <int64_t valid_bits = 64, bool pop_front = false>
auto simpleRansomBitTest(auto&& g) {
  std::array<int64_t, (size_t{1} << 8)> counts{};
  for (int64_t i = 0; i < (int64_t{1} << 18); ++i) {
    auto bits = g();
    if constexpr (pop_front) {
      bits >>= 64 - valid_bits;
    }
    for (int64_t offset = 0; offset < valid_bits; offset += 8) {
      ++counts[(bits >> offset) & 0xFF];
    }
  }

  // Check that the counts are within 10% of the expected value
  return std::ranges::all_of(counts, [](auto count) {
    return (count >= 0.90 * (1 << 10) * (valid_bits / 8)) &&
           (count <= 1.10 * (1 << 10) * (valid_bits / 8));
  });
}

auto main() -> int {
  "Xorshift64 A"_test = [] {
    expectTrue(simpleRansomBitTest(bayes::detail::XorShift(bayes::detail::A1)));
    expectTrue(simpleRansomBitTest(bayes::detail::XorShift(bayes::detail::A2)));
    expectTrue(simpleRansomBitTest(bayes::detail::XorShift(bayes::detail::A3)));
    expectTrue(simpleRansomBitTest(bayes::detail::XorShift(bayes::detail::A4)));
    expectTrue(simpleRansomBitTest(bayes::detail::XorShift(bayes::detail::A5)));
    expectTrue(simpleRansomBitTest(bayes::detail::XorShift(bayes::detail::A6)));
    expectTrue(simpleRansomBitTest(bayes::detail::XorShift(bayes::detail::A7)));
    expectTrue(simpleRansomBitTest(bayes::detail::XorShift(bayes::detail::A8)));
    expectTrue(simpleRansomBitTest(bayes::detail::XorShift(bayes::detail::A9)));
  };

  "Multiply With Carry B"_test = [] {
    expectTrue(simpleRansomBitTest<32>(
        bayes::detail::MultiplyWithCarry(bayes::detail::B1)));
    expectTrue(simpleRansomBitTest<32>(
        bayes::detail::MultiplyWithCarry(bayes::detail::B2)));
    expectTrue(simpleRansomBitTest<32>(
        bayes::detail::MultiplyWithCarry(bayes::detail::B3)));
    expectTrue(simpleRansomBitTest<32>(
        bayes::detail::MultiplyWithCarry(bayes::detail::B4)));
    expectTrue(simpleRansomBitTest<32>(
        bayes::detail::MultiplyWithCarry(bayes::detail::B5)));
    expectTrue(simpleRansomBitTest<32>(
        bayes::detail::MultiplyWithCarry(bayes::detail::B6)));
    expectTrue(simpleRansomBitTest<32>(
        bayes::detail::MultiplyWithCarry(bayes::detail::B7)));
    expectTrue(simpleRansomBitTest<32>(
        bayes::detail::MultiplyWithCarry(bayes::detail::B8)));
  };

  "Linear Congruential Generator"_test = [] {
    expectTrue(simpleRansomBitTest<32, true>(
        bayes::detail::LinearCongruential{bayes::detail::C1}));
    expectTrue(simpleRansomBitTest<32, true>(
        bayes::detail::LinearCongruential{bayes::detail::C2}));
    expectTrue(simpleRansomBitTest<32, true>(
        bayes::detail::LinearCongruential{bayes::detail::C3}));
  };

  // "Multiplicative Linear Congruential Generator"_test = [] {
  //   expectTrue(simpleRansomBitTest<32, true>(bayes::detail::D1{123456}));
  //   expectTrue(simpleRansomBitTest<32, true>(bayes::detail::D2{123456}));
  //   expectTrue(simpleRansomBitTest<32, true>(bayes::detail::D3{123456}));
  //   expectTrue(simpleRansomBitTest<32, true>(bayes::detail::D4{123456}));
  //   expectTrue(simpleRansomBitTest<32, true>(bayes::detail::D5{123456}));
  // };

  // "Multiplicative Linear Congruential Generator with modulus"_test = [] {
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E1{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E2{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E3{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E4{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E5{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E6{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E7{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E8{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E9{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E10{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E11{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::E12{123456}));
  // };

  // "Multiplicative Linear Congruential Generator with modulus and ax"_test =
  // [] {
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::F1{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::F2{123456}));
  //   expectTrue(simpleRansomBitTest<32>(bayes::detail::F3{123456}));
  // };

  "Rand"_test = [] {
    expectTrue(simpleRansomBitTest<64>(bayes::Rand{123456}));
    // expectTrue(simpleRansomBitTest<64>(bayes::QuickRand1{123456}));
    // expectTrue(simpleRansomBitTest<64>(bayes::QuickRand2{123456}));
  };

  // "simd xor"_test = [] {
  //   __m512i seed{7073242132491550564,  1179269353366884230,
  //   3941578509859010014,
  //                4437109666059500420,  4035966242879597485,
  //                3373052566401125716, 1556011196226971778,
  //                1235654174036890696};
  //   tempura::bayes::detail::XorShiftAvx512 xorshift{seed};
  //   for (int i = 0; i < 100; ++i) xorshift();

  //   for (int i = 0; i < 100; ++i) {
  //     auto out = xorshift();
  //     std::println("{:0>64b}", reinterpret_cast<uint64_t*>(&out)[4]);
  //   }
  // };

  // Benchmarked on 11th Gen Intel(R) Core(TM) i7-11800H
  // With -O3 -march=native

  volatile uint_fast64_t out;
  volatile __m512i state;
  // Ops per sec: 472,143,531
  "mt bench"_bench.ops(1000) = [&out, mt = std::mt19937{123456}] mutable {
#pragma GCC unroll 1000
    for (int i = 0; i < 1000; ++i) {
      out = mt();
    }
  };

  // Ops per sec: 303,306,035
  "rand bench"_bench.ops(1000) = [&out, rand = bayes::Rand{123456}] mutable {
#pragma GCC unroll 1000
    for (int i = 0; i < 1000; ++i) {
      out = rand();
    }
  };

  "avx rand bench"_bench.ops(8000) = [&state, rand = bayes::detail::XorShiftAvx512{_mm512_set1_epi64(42)}] mutable {
#pragma GCC unroll 1000
    for (int i = 0; i < 1000; ++i) {
      state = rand();
    }
  };
  //
  //   // Ops per sec: 355,871,886
  //   "qrand1 bench"_bench.ops(1000) =
  //       [&out, qrand1 = bayes::QuickRand1{123456}] mutable {
  // #pragma GCC unroll 1000
  //         for (int i = 0; i < 1000; ++i) {
  //           out = qrand1();
  //         }
  //       };
  //
  //   // Ops per sec: 266,808,964
  //   "qrand2 bench"_bench.ops(1000) =
  //       [&out, qrand2 = bayes::QuickRand2{123456}] mutable {
  // #pragma GCC unroll 1000
  //         for (int i = 0; i < 1000; ++i) {
  //           out = qrand2();
  //         }
  //       };

  return TestRegistry::result();
};
