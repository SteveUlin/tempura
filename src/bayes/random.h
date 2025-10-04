#pragma once

namespace tempura {

template <typename StateT, typename AlgoT>
class RNG {
public:
  constexpr RNG(StateT state, const AlgoT& algo)
      : state_(state), algo_(algo) {}

  constexpr RNG(const AlgoT& algo)
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

  private:
  StateT state_;
  AlgoT algo_;
};

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

template <typename T>
struct XorShiftOptions {
  T a1;
  T a2;
  T a3;
};

template <typename T, ShiftDirection dir>
class XorShiftFn {
 public:
  using Options = XorShiftOptions<T>;

  constexpr XorShiftFn(Options options) : options_{options} {}

  constexpr auto operator()(T x) const {
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
  Options options_;
};

// Known good parameters

// constexpr XorShiftOptions<uint_fast64_t> A1 = {
//     .a1 = 21, .a2 = 35, .a3 = 4};
// constexpr XorShiftOptions<uint_fast64_t> A2 = {
//     .a1 = 20, .a2 = 41, .a3 = 5};
// constexpr XorShiftOptions<uint_fast64_t> A3 = {
//     .a1 = 17, .a2 = 31, .a3 = 8};
// constexpr XorShiftOptions<uint_fast64_t> A4 = {
//     .a1 = 11, .a2 = 29, .a3 = 14};
// constexpr XorShiftOptions<uint_fast64_t> A5 = {
//     .a1 = 14, .a2 = 29, .a3 = 11};
// constexpr XorShiftOptions<uint_fast64_t> A6 = {
//     .a1 = 30, .a2 = 35, .a3 = 13};
// constexpr XorShiftOptions<uint_fast64_t> A7 = {
//     .a1 = 21, .a2 = 37, .a3 = 4};
// constexpr XorShiftOptions<uint_fast64_t> A8 = {
//     .a1 = 21, .a2 = 43, .a3 = 4};
// constexpr XorShiftOptions<uint_fast64_t> A9 = {
//     .a1 = 23, .a2 = 41, .a3 = 18};

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

template <typename T>
struct MultiplyWithCarryFn {
  MultiplyWithCarryFn(T a) : a_{a} {}

  constexpr auto operator()(T x) const -> T {
    x = a_ * (x & T{0xFFFFFFFF}) + (x >> T{32});
    return x;
  };

 private:
  T a_;
};

// // Known good parameters
//
// constexpr MultiplyWithCarry::Options B1 = {.a = 4294957665};
// constexpr MultiplyWithCarry::Options B2 = {.a = 4294963023};
// constexpr MultiplyWithCarry::Options B3 = {.a = 4162943475};
// constexpr MultiplyWithCarry::Options B4 = {.a = 3947008974};
// constexpr MultiplyWithCarry::Options B5 = {.a = 3874257210};
// constexpr MultiplyWithCarry::Options B6 = {.a = 2936881968};
// constexpr MultiplyWithCarry::Options B7 = {.a = 2811536238};
// constexpr MultiplyWithCarry::Options B8 = {.a = 2654432763};
// constexpr MultiplyWithCarry::Options B9 = {.a = 1640531364};
//
// Linear Congruential Generator
//
// state: x
// initialize: any
// update x = a * x + c mod 2⁶⁴
// period: 2⁶⁴ if c and m are chosen correctly
//
// Not a great generator. The high 32 bits are mostly random
// but the lower 32 bits are not.

template <typename T>
struct LinearCongruentialOptions {
  T a;
  T c;
};

template <typename T>
struct LinearCongruentialFn {
  constexpr LinearCongruentialFn(LinearCongruentialOptions<T> options)
      : a_{options.a}, c_{options.c} {}

  constexpr auto operator()(T x) const -> T {
    x = a_ * x + c_;
    return x;
  }

 private:
  T a_;
  T c_;
};


// constexpr LinearCongruential::Options C1 = {.a = 3935559000370003845,
//                                             .c = 2691343689449507681};
// constexpr LinearCongruential::Options C2 = {.a = 3202034522624059733,
//                                             .c = 4354685564936845319};
// constexpr LinearCongruential::Options C3 = {.a = 2862933555777941757,
//                                             .c = 7046029254386353087};

// Multiplicative Linear Congruential Generator
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
// // don't use both a and ax. Pick one or the other. a can be accessed with
// ::mul
//
// // Known good parameters
//
// using F1 = MultiplicativeLinearCongruential<3741260, 4930622455819>;
// using F2 = MultiplicativeLinearCongruential<3397916, 5428838662153>;
// using F3 = MultiplicativeLinearCongruential<2106408, 8757438316547>;
//
// }  // namespace detail
//
// // All of these implementations below are slower than std::mt19937_64
// // when -O3 is enabled. Some are slightly faster than std::mt19937_64
// // without optimizations.
// //
// // TLDR: Use std::mt19937_64
//
// // Personal Interpretation:
// // A*: XorShift
// //   Very Fast but the state can get stuck in low entropy modes. To
// //   fix this we instead use the output of LinearCongruential as the
// //   state. We then take this output and add another XorShift to
// //   extend the period.
// //
// // B*: MultiplyWithCarry
// //   As LinearCongruential has bad randomness in the low bits,
// //   (and we can't trust XorShift to fix the lower bits), we can xor
// //   the output with MultiplyWithCarry to add some randomness to the
// //   low bits.
// //
// // The period of the final output is the LCM of the periods of
// // A3, B1, and C3.
// struct Rand {
//   // The default seed is arbitrary
//   constexpr Rand(uint_fast64_t seed = 129348710293) {
//     a3_.seed(seed);
//     b1_.seed(seed);
//     c3_.seed(seed);
//   }
//
//   Rand(const Rand &) = default;
//   Rand(Rand &&) = default;
//   auto operator=(const Rand &) -> Rand & = default;
//   auto operator=(Rand &&) -> Rand & = default;
//
//   static constexpr auto min() -> uint_fast64_t { return 0; }
//
//   static constexpr auto max() -> uint_fast64_t {
//     return std::numeric_limits<uint_fast64_t>::max();
//   }
//
//   constexpr auto operator()() -> uint_fast64_t {
//     return (a1_.step(c3_()) + a3_()) ^ b1_();
//   }
//
//  private:
//   detail::XorShift<uint_fast64_t> a1_ = [] {
//     auto options = detail::A1;
//     options.dir = detail::ShiftDirection::Left;
//     return detail::XorShift<uint_fast64_t>{options};
//   }();
//   detail::XorShift<uint_fast64_t> a3_{detail::A3};
//   detail::MultiplyWithCarry b1_{detail::B1};
//   detail::LinearCongruential c3_{detail::C3};
// };
// //
// // // Do not use for more than 10¹² calls
// // struct QuickRand1 {
// //   // The default seed is arbitrary
// //   constexpr QuickRand1(uint_fast64_t seed = 129348710293) : a1_{seed} {}
// //
// //   static constexpr auto min() -> uint_fast64_t { return 0; }
// //
// //   static constexpr auto max() -> uint_fast64_t {
// //     return std::numeric_limits<uint_fast64_t>::max();
// //   }
// //
// //   constexpr auto operator()() -> uint_fast64_t {
// //     return detail::D1::step(a1_());
// //   }
// //
// //  private:
// //   detail::A1<detail::ShiftDirection::Right> a1_;
// // };
// //
// // // Do not use for more than 10³⁷ calls
// // struct QuickRand2 {
// //   // The default seed is arbitrary
// //   constexpr QuickRand2(uint_fast64_t seed = 129348710293)
// //       : a3_{seed}, b1_{seed} {}
// //
// //   static constexpr auto min() -> uint_fast64_t { return 0; }
// //
// //   static constexpr auto max() -> uint_fast64_t {
// //     return std::numeric_limits<uint_fast64_t>::max();
// //   }
// //
// //   constexpr auto operator()() -> uint_fast64_t {
// //     return detail::A1<detail::ShiftDirection::Left>::step(b1_()) ^ a3_();
// //   }
// //
// //  private:
// //   detail::A3<detail::ShiftDirection::Right> a3_;
// //   detail::B1 b1_;
// // };

}  // namespace tempura
