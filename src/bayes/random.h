#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace tempura {

template <typename StateT, typename AlgoT>
class Generator {
public:
  constexpr Generator(StateT state, const AlgoT& algo)
      : state_(state), algo_(algo) {}

  constexpr Generator(const AlgoT& algo)
      : state_(StateT{}), algo_(algo) {}

  constexpr void seed(StateT state) {
    state_ = state;
  }

  constexpr auto operator()() -> StateT {
    state_ = algo_(state_);
    return state_;
  }

  constexpr auto state() const -> StateT {
    return state_;
  }

  static constexpr auto min() -> StateT { return 0; }
  static constexpr auto max() -> StateT {
    return std::numeric_limits<StateT>::max();
  }

  private:
  StateT state_;
  AlgoT algo_;
};

enum class ShiftDirection : bool { Left, Right };

// Random Number Generators
// See Numerical Recipes in C++ 3rd Edition section 7.1
//
// Below each method is a set of known good parameters for the generator.
// Parameter sets are from Numerical Recipes tables.

// 64-bit Xorshift
// state: x
// intialize: x ≠ 0
// update:
//   x ← x ∧ (x >> a₁)
//   x ← x ∧ (x << a₂)
//   x ← x ∧ (x >> a₃)
// period: 2⁶⁴ - 1
//
// Should not be used by itself as numbers with a low number of on bits
// tend to produce more number with low numbers of on bits.

struct XorShiftOptions {
  uint64_t a1;
  uint64_t a2;
  uint64_t a3;
};

// Known good parameter sets from Numerical Recipes
struct XorShiftPresets {
  static constexpr std::array kAll = {
    XorShiftOptions{21, 35, 4},   // 0
    XorShiftOptions{20, 41, 5},   // 1
    XorShiftOptions{17, 31, 8},   // 2
    XorShiftOptions{11, 29, 14},  // 3
    XorShiftOptions{14, 29, 11},  // 4
    XorShiftOptions{30, 35, 13},  // 5
    XorShiftOptions{21, 37, 4},   // 6
    XorShiftOptions{21, 43, 4},   // 7
    XorShiftOptions{23, 41, 18},  // 8
  };
};

template <ShiftDirection dir>
class XorShift {
 public:
  constexpr XorShift(XorShiftOptions options) : options_{options} {}

  [[nodiscard]] constexpr auto operator()(uint64_t x) const -> uint64_t {
    if constexpr (dir == ShiftDirection::Left) {
      x ^= x >> options_.a1;
      x ^= x << options_.a2;
      x ^= x >> options_.a3;
    } else {
      x ^= x << options_.a1;
      x ^= x >> options_.a2;
      x ^= x << options_.a3;
    }
    return x;
  }

 private:
  XorShiftOptions options_;
};

// Multiply With Carry
// state: x
// intialize: x ≠ 0
// update:
//   x ← a (x & [2³² - 1]) + (x >> 32)
// period: (2³²a - 2) / 2  where a is prime
//
// Only the lower 32 bits are considered to be algorithmically random.
// However the upper bits also contain a high degree of randomness and
// could be used when combined with other generators.

// Known good parameter sets from Numerical Recipes
struct MultiplyWithCarryPresets {
  static constexpr std::array kAll = {
    uint64_t{4294957665},  // 0
    uint64_t{4294963023},  // 1
    uint64_t{4162943475},  // 2
    uint64_t{3947008974},  // 3
    uint64_t{3874257210},  // 4
    uint64_t{2936881968},  // 5
    uint64_t{2811536238},  // 6
    uint64_t{2654432763},  // 7
    uint64_t{1640531364},  // 8
  };
};

class MultiplyWithCarry {
 public:
  constexpr MultiplyWithCarry(uint64_t a) : a_{a} {}

  [[nodiscard]] constexpr auto operator()(uint64_t x) const -> uint64_t {
    x = a_ * (x & uint64_t{0xFFFFFFFF}) + (x >> uint64_t{32});
    return x;
  }

 private:
  uint64_t a_;
};
// Linear Congruential Generator
//
// state: x
// initialize: any
// update x = a * x + c mod 2⁶⁴
// period: 2⁶⁴ if c and m are chosen correctly
//
// Not a great generator. The high 32 bits are mostly random
// but the lower 32 bits are not.

struct LinearCongruentialOptions {
  uint64_t a;
  uint64_t c;
};

// Known good parameter sets from Numerical Recipes
struct LinearCongruentialPresets {
  static constexpr std::array kAll = {
    LinearCongruentialOptions{3935559000370003845ULL, 2691343689449507681ULL},  // 0
    LinearCongruentialOptions{3202034522624059733ULL, 4354685564936845319ULL},  // 1
    LinearCongruentialOptions{2862933555777941757ULL, 7046029254386353087ULL},  // 2
  };
};

class LinearCongruential {
 public:
  constexpr LinearCongruential(LinearCongruentialOptions options)
      : a_{options.a}, c_{options.c} {}

  [[nodiscard]] constexpr auto operator()(uint64_t x) const -> uint64_t {
    x = a_ * x + c_;
    return x;
  }

 private:
  uint64_t a_;
  uint64_t c_;
};

// Combined Generator
//
// Combines XorShift, MultiplyWithCarry, and LinearCongruential
// for high-quality random numbers.
//
// Strategy from Numerical Recipes:
// - XorShift (Left): Process the output of LinearCongruential
// - XorShift (Right): Add another layer of scrambling
// - MultiplyWithCarry: XOR to add randomness to lower bits
// - LinearCongruential: Provides the base state evolution
//
// Period is the LCM of the individual generator periods.

constexpr uint64_t kDefaultRandomSeed = 129348710293ULL;

inline auto makeRandom(uint64_t seed = kDefaultRandomSeed) {
  auto left_shift = XorShift<ShiftDirection::Left>{XorShiftPresets::kAll[0]};
  auto mwc_gen = Generator{seed, MultiplyWithCarry{MultiplyWithCarryPresets::kAll[0]}};
  auto lcg_gen = Generator{seed, LinearCongruential{LinearCongruentialPresets::kAll[2]}};
  auto right_shift_gen = Generator{seed, XorShift<ShiftDirection::Right>{XorShiftPresets::kAll[2]}};

  return [=]() mutable -> uint64_t {
    return (left_shift(lcg_gen()) + right_shift_gen()) ^ mwc_gen();
  };
}

// Multiplicative Linear Congruential Generator (commented out - less useful)
//
// state: x
// initialize: x ≠ 0
// update x = a * x mod 2⁶⁴
// period: 2⁶⁴
//
// Top 32 bits are mostly random, bottom 32 bits are not.

// template <uint_fast64_t a, uint_fast64_t m = 0>
// struct MultiplicativeLinearCongruential {
//   constexpr MultiplicativeLinearCongruential(uint_fast64_t seed)
//       : state_{seed} {}
//
//   static constexpr auto kMul = a;
//
//   static constexpr auto min() -> uint_fast64_t { return 1; }
//
//   static constexpr auto max() -> uint_fast64_t {
//     return std::numeric_limits<uint_fast64_t>::max();
//   }
//
//   [[gnu::always_inline]] static constexpr auto step(uint_fast64_t x) {
//     if constexpr (m != 0) {
//       x = a * x % m;
//     } else {
//       x = a * x;
//     }
//     return x;
//   }
//
//   constexpr auto operator()() -> uint_fast64_t {
//     state_ = step(state_);
//     return state_;
//   }
//
//  private:
//   uint_fast64_t state_;
// };
//
// // Known good parameters
//
// using D1 = MultiplicativeLinearCongruential<2685821657736338717>;
// using D2 = MultiplicativeLinearCongruential<7664345821815920749>;
// using D3 = MultiplicativeLinearCongruential<4768777513237032717>;
// using D4 = MultiplicativeLinearCongruential<1181783497276652981>;
// using D5 = MultiplicativeLinearCongruential<702098784532940405>;
//
// // Known good parameters for m >> 2³² and m prime.
// //
// // Only the lower 32 bits are considered to be algorithmically random.
// // But you can get ~43 good bits
//
// using E1 = MultiplicativeLinearCongruential<10014146, 549755813881>;
// using E2 = MultiplicativeLinearCongruential<30508823, 549755813881>;
// using E3 = MultiplicativeLinearCongruential<25708129, 549755813881>;
//
// using E4 = MultiplicativeLinearCongruential<5183781, 2199023255531>;
// using E5 = MultiplicativeLinearCongruential<1070739, 2199023255531>;
// using E6 = MultiplicativeLinearCongruential<6639568, 2199023255531>;
//
// using E7 = MultiplicativeLinearCongruential<1781978, 4398046511093>;
// using E8 = MultiplicativeLinearCongruential<2114307, 4398046511093>;
// using E9 = MultiplicativeLinearCongruential<1542852, 4398046511093>;
//
// using E10 = MultiplicativeLinearCongruential<2096259, 8796093022151>;
// using E11 = MultiplicativeLinearCongruential<2052163, 8796093022151>;
// using E12 = MultiplicativeLinearCongruential<2006881, 8796093022151>;
//
// // Known good parameters for m >> 2³², m prime, and a(m - 1) ≈ 2⁶⁴
// //
// // The purpose is to make ax reasonably 64 bit random number with
// // good enough properties in the high bits to be used in a combined
// // generator.
// //
// // don't use both a and ax. Pick one or the other. a can be accessed with ::mul
//
// // Known good parameters
//
// using F1 = MultiplicativeLinearCongruential<3741260, 4930622455819>;
// using F2 = MultiplicativeLinearCongruential<3397916, 5428838662153>;
// using F3 = MultiplicativeLinearCongruential<2106408, 8757438316547>;

}  // namespace tempura
