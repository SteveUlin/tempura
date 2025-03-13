#pragma once

#include <cstdint>
#include <random>

namespace tempura::bayes {

namespace detail {

enum class ShiftDirection : bool { Left, Right };

// Random Number Generators
// See Numerical Recipes in C++ 3rd Edition section 7.1
//
// Below each method is a set of known good parameters for the generator.
// The naming is arbitrary and matches the tables in the book.

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

template <uint_fast64_t a1, uint_fast64_t a2, uint_fast64_t a3,
          ShiftDirection direction>
struct XorShift {
  static constexpr auto min() -> uint_fast64_t { return 1; }

  static constexpr auto max() -> uint_fast64_t {
    return std::numeric_limits<uint_fast64_t>::max();
  }

  [[gnu::always_inline]] static constexpr auto step(uint_fast64_t x) {
    if constexpr (direction == ShiftDirection::Left) {
      x ^= x >> a1;
      x ^= x << a2;
      x ^= x >> a3;
    } else {
      x ^= x << a1;
      x ^= x >> a2;
      x ^= x << a3;
    }
    return x;
  }

  constexpr XorShift(uint_fast64_t seed) : state_{seed} {}

  constexpr auto operator()() -> uint_fast64_t {
    state_ = step(state_);
    return state_;
  }

 private:
  uint_fast64_t state_;
};
static_assert(
    std::uniform_random_bit_generator<XorShift<0, 0, 0, ShiftDirection::Left>>);

// Known good parameters

template <ShiftDirection direction = ShiftDirection::Left>
using A1 = XorShift<21, 35, 4, direction>;
template <ShiftDirection direction = ShiftDirection::Left>
using A2 = XorShift<20, 41, 5, direction>;
template <ShiftDirection direction = ShiftDirection::Left>
using A3 = XorShift<17, 31, 8, direction>;
template <ShiftDirection direction = ShiftDirection::Left>
using A4 = XorShift<11, 29, 14, direction>;
template <ShiftDirection direction = ShiftDirection::Left>
using A5 = XorShift<14, 29, 11, direction>;
template <ShiftDirection direction = ShiftDirection::Left>
using A6 = XorShift<30, 35, 13, direction>;
template <ShiftDirection direction = ShiftDirection::Left>
using A7 = XorShift<21, 37, 4, direction>;
template <ShiftDirection direction = ShiftDirection::Left>
using A8 = XorShift<21, 43, 4, direction>;
template <ShiftDirection direction = ShiftDirection::Left>
using A9 = XorShift<23, 41, 18, direction>;

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

template <uint_fast64_t a>
struct MultiplyWithCarry {
  MultiplyWithCarry(uint_fast64_t seed) : state_{seed} {}

  static constexpr auto min() -> uint_fast64_t { return 1; }

  static constexpr auto max() -> uint_fast64_t {
    return std::numeric_limits<uint_fast64_t>::max();
  }

  [[gnu::always_inline]] static constexpr auto step(uint_fast64_t x)
      -> uint_fast64_t {
    x = a * (x & 0xFFFFFFFF) + (x >> 32);
    return x;
  };

  constexpr auto operator()() -> uint_fast64_t {
    state_ = step(state_);
    return state_;
  }

 private:
  uint_fast64_t state_;
};

// Known good parameters

using B1 = MultiplyWithCarry<4294957665>;
using B2 = MultiplyWithCarry<4294963023>;
using B3 = MultiplyWithCarry<4162943475>;
using B4 = MultiplyWithCarry<3947008974>;
using B5 = MultiplyWithCarry<3874257210>;
using B6 = MultiplyWithCarry<2936881968>;
using B7 = MultiplyWithCarry<2811536238>;
using B8 = MultiplyWithCarry<2654432763>;
using B9 = MultiplyWithCarry<1640531364>;

// Linear Congruential Generator
//
// state: x
// initialize: any
// update x = a * x + c mod 2⁶⁴
// period: 2⁶⁴ if c and m are chosen correctly
//
// Not a great generator. The high 32 bits are mostly random
// but the lower 32 bits are not.

template <uint_fast64_t a, uint_fast64_t c>
struct LinearCongruential {
  constexpr LinearCongruential(uint_fast64_t seed) : state_{seed} {}

  static constexpr auto min() -> uint_fast64_t { return 0; }

  static constexpr auto max() -> uint_fast64_t {
    return std::numeric_limits<uint_fast64_t>::max();
  }

  [[gnu::always_inline]] static constexpr auto step(uint_fast64_t x)
      -> uint_fast64_t {
    x = a * x + c;
    return x;
  }

  constexpr auto operator()() -> uint_fast64_t {
    state_ = step(state_);
    return state_;
  }

 private:
  uint_fast64_t state_;
};

using C1 = LinearCongruential<3935559000370003845, 2691343689449507681>;
using C2 = LinearCongruential<3202034522624059733, 4354685564936845319>;
using C3 = LinearCongruential<2862933555777941757, 7046029254386353087>;

// Multiplicative Linear Congruential Generator
//
// state: x
// initialize: x ≠ 0
// update x = a * x mod 2⁶⁴
// period: 2⁶⁴
//
// Top 32 bits are mostly random, bottom 32 bits are not.

template <uint_fast64_t a, uint_fast64_t m = 0>
struct MultiplicativeLinearCongruential {
  constexpr MultiplicativeLinearCongruential(uint_fast64_t seed)
      : state_{seed} {}

  static constexpr auto kMul = a;

  static constexpr auto min() -> uint_fast64_t { return 1; }

  static constexpr auto max() -> uint_fast64_t {
    return std::numeric_limits<uint_fast64_t>::max();
  }

  [[gnu::always_inline]] static constexpr auto step(uint_fast64_t x) {
    if constexpr (m != 0) {
      x = a * x % m;
    } else {
      x = a * x;
    }
    return x;
  }

  constexpr auto operator()() -> uint_fast64_t {
    state_ = step(state_);
    return state_;
  }

 private:
  uint_fast64_t state_;
};

// Known good parameters

using D1 = MultiplicativeLinearCongruential<2685821657736338717>;
using D2 = MultiplicativeLinearCongruential<7664345821815920749>;
using D3 = MultiplicativeLinearCongruential<4768777513237032717>;
using D4 = MultiplicativeLinearCongruential<1181783497276652981>;
using D5 = MultiplicativeLinearCongruential<702098784532940405>;

// Known good parameters for m >> 2³² and m prime.
//
// Only the lower 32 bits are considered to be algorithmically random.
// But you can get ~43 good bits

using E1 = MultiplicativeLinearCongruential<10014146, 549755813881>;
using E2 = MultiplicativeLinearCongruential<30508823, 549755813881>;
using E3 = MultiplicativeLinearCongruential<25708129, 549755813881>;

using E4 = MultiplicativeLinearCongruential<5183781, 2199023255531>;
using E5 = MultiplicativeLinearCongruential<1070739, 2199023255531>;
using E6 = MultiplicativeLinearCongruential<6639568, 2199023255531>;

using E7 = MultiplicativeLinearCongruential<1781978, 4398046511093>;
using E8 = MultiplicativeLinearCongruential<2114307, 4398046511093>;
using E9 = MultiplicativeLinearCongruential<1542852, 4398046511093>;

using E10 = MultiplicativeLinearCongruential<2096259, 8796093022151>;
using E11 = MultiplicativeLinearCongruential<2052163, 8796093022151>;
using E12 = MultiplicativeLinearCongruential<2006881, 8796093022151>;

// Known good parameters for m >> 2³², m prime, and a(m - 1) ≈ 2⁶⁴
//
// The purpose is to make ax reasonably 64 bit random number with
// good enough properties in the high bits to be used in a combined
// generator.
//
// don't use both a and ax. Pick one or the other. a can be accessed with ::mul

// Known good parameters

using F1 = MultiplicativeLinearCongruential<3741260, 4930622455819>;
using F2 = MultiplicativeLinearCongruential<3397916, 5428838662153>;
using F3 = MultiplicativeLinearCongruential<2106408, 8757438316547>;

}  // namespace detail

// Personal Interpretation:
// A*: XorShift
//   Very Fast but the state can get stuck in low entropy modes. To
//   fix this we instead use the output of LinearCongruential as the
//   state. We then take this output and add another XorShift to
//   extend the period.
//
// B*: MultiplyWithCarry
//   As LinearCongruential has bad randomness in the low bits,
//   (and we can't trust XorShift to fix the lower bits), we can xor
//   the output with MultiplyWithCarry to add some randomness to the
//   low bits.
//
// The period of the final output is the LCM of the periods of
// A3, B1, and C3.
//
// This is slightly faster than std::mt19937_64 but has a much shorter period.
// Prefer mt19937_64.
struct Rand {
  // The default seed is arbitrary
  constexpr Rand(uint_fast64_t seed = 129348710293)
      : a3_{seed}, b1_{seed}, c3_{seed} {}

  static constexpr auto min() -> uint_fast64_t { return 0; }

  static constexpr auto max() -> uint_fast64_t {
    return std::numeric_limits<uint_fast64_t>::max();
  }

  constexpr auto operator()() -> uint_fast64_t {
    return (detail::A1<detail::ShiftDirection::Left>::step(c3_()) + a3_()) ^
           b1_();
  }

 private:
  detail::A3<detail::ShiftDirection::Right> a3_;
  detail::B1 b1_;
  detail::C3 c3_;
};

// About 2x faster than std::mt19937_64 but has a much shorter period.
// Do not use for more than 10¹² calls
struct QuickRand1 {
  // The default seed is arbitrary
  constexpr QuickRand1(uint_fast64_t seed = 129348710293) : a1_{seed} {}

  static constexpr auto min() -> uint_fast64_t { return 0; }

  static constexpr auto max() -> uint_fast64_t {
    return std::numeric_limits<uint_fast64_t>::max();
  }

  constexpr auto operator()() -> uint_fast64_t {
    return detail::D1::step(a1_());
  }

 private:
  detail::A1<detail::ShiftDirection::Right> a1_;
};

// About 1.5x faster than std::mt19937_64 but has a much shorter period.
// Do not use for more than 10³⁷ calls
struct QuickRand2 {
  // The default seed is arbitrary
  constexpr QuickRand2(uint_fast64_t seed = 129348710293)
      : a3_{seed}, b1_{seed} {}

  static constexpr auto min() -> uint_fast64_t { return 0; }

  static constexpr auto max() -> uint_fast64_t {
    return std::numeric_limits<uint_fast64_t>::max();
  }

  constexpr auto operator()() -> uint_fast64_t {
    return detail::A1<detail::ShiftDirection::Left>::step(b1_()) ^ a3_();
  }

 private:
  detail::A3<detail::ShiftDirection::Right> a3_;
  detail::B1 b1_;
};

}  // namespace tempura::bayes
