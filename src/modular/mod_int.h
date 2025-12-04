#pragma once

#include "meta/utility.h"

// Modular arithmetic with compile-time modulus
//
// Design goals:
// 1. Type-safe: Different moduli are different types (can't accidentally mix)
// 2. constexpr-by-default: All operations work at compile time
// 3. Type-agnostic: Works with unsigned long long, __uint128_t, UInt<N>, etc.
//
// The internal type T must support:
//   - Arithmetic: +, -, *, /, %
//   - Comparison: <, ==
//   - Construction from small integers
//
// Example usage:
//   using Mint = ModInt<unsigned long long, 998244353>;
//   constexpr Mint a(3);
//   constexpr Mint b = a.pow(1000000);
//   static_assert(b * b.inv() == Mint(1));

namespace tempura {

// =============================================================================
// ModInt<T, Mod> - Modular integer with static (compile-time) modulus
// =============================================================================

template <typename T, T Mod>
  requires(Mod > T{1})
struct ModInt {
  using ValueType = T;
  static constexpr T kMod = Mod;

  T value{};

  // --- Constructors ---

  constexpr ModInt() = default;

  // Construct from value, reducing mod Mod
  constexpr explicit ModInt(T v) : value{reduce(v)} {}

  // Raw construction - no reduction performed
  // PRECONDITION: v < Mod
  static constexpr auto raw(T v) -> ModInt {
    ModInt result;
    result.value = v;
    return result;
  }

  // --- Comparison ---

  constexpr auto operator==(const ModInt& other) const -> bool = default;

  // --- Arithmetic operators ---

  constexpr auto operator+(const ModInt& other) const -> ModInt {
    T sum = value + other.value;
    // If sum >= Mod, subtract Mod (branchless-friendly)
    if (sum >= kMod) {
      sum = sum - kMod;
    }
    return raw(sum);
  }

  constexpr auto operator+=(const ModInt& other) -> ModInt& {
    *this = *this + other;
    return *this;
  }

  constexpr auto operator-(const ModInt& other) const -> ModInt {
    T diff = value;
    if (value < other.value) {
      diff = diff + kMod;
    }
    diff = diff - other.value;
    return raw(diff);
  }

  constexpr auto operator-=(const ModInt& other) -> ModInt& {
    *this = *this - other;
    return *this;
  }

  // Unary minus (negation)
  constexpr auto operator-() const -> ModInt {
    if (value == T{0}) {
      return raw(T{0});
    }
    return raw(kMod - value);
  }

  // Multiplication - uses widening strategy for overflow safety
  // For unsigned long long: uses __uint128_t
  // For larger types: relies on the type's own multiplication behavior
  constexpr auto operator*(const ModInt& other) const -> ModInt {
    if constexpr (SameAs<T, unsigned long long>) {
      // Use 128-bit intermediate to avoid overflow
      __uint128_t product = static_cast<__uint128_t>(value) * other.value;
      return ModInt(static_cast<T>(product % kMod));
    } else if constexpr (SameAs<T, unsigned int>) {
      // Use 64-bit intermediate for 32-bit types
      unsigned long long product =
          static_cast<unsigned long long>(value) * other.value;
      return ModInt(static_cast<T>(product % kMod));
    } else {
      // For other types (e.g., UInt<N>), assume they handle overflow
      // or the user has chosen appropriate bit widths
      return ModInt(value * other.value);
    }
  }

  constexpr auto operator*=(const ModInt& other) -> ModInt& {
    *this = *this * other;
    return *this;
  }

  // Division via modular inverse
  // PRECONDITION: other.value != 0 and gcd(other.value, Mod) == 1
  constexpr auto operator/(const ModInt& other) const -> ModInt {
    return *this * other.inv();
  }

  constexpr auto operator/=(const ModInt& other) -> ModInt& {
    *this = *this / other;
    return *this;
  }

  // --- Modular exponentiation (binary method) ---
  //
  // Computes this^exp mod Mod in O(log exp) multiplications
  //
  // Algorithm: Square-and-multiply
  //   result = 1
  //   while exp > 0:
  //     if exp is odd: result *= base
  //     base *= base
  //     exp >>= 1

  constexpr auto pow(T exp) const -> ModInt {
    ModInt result = raw(T{1});
    ModInt base = *this;

    while (exp > T{0}) {
      if ((exp & T{1}) != T{0}) {
        result = result * base;
      }
      base = base * base;
      exp = exp >> 1;
    }

    return result;
  }

  // --- Modular inverse ---
  //
  // For prime Mod: use Fermat's Little Theorem
  //   a^(-1) = a^(p-2) mod p
  //
  // For general Mod: use extended Euclidean algorithm
  //   Find x such that ax + my = gcd(a, m)
  //   If gcd(a, m) = 1, then ax = 1 (mod m), so x is the inverse
  //
  // We provide both methods. For known prime moduli, pow() is simpler.
  // For unknown or composite moduli, use extendedGcdInv().

  // Inverse assuming Mod is prime (Fermat's method)
  // PRECONDITION: Mod is prime, value != 0
  constexpr auto inv() const -> ModInt {
    // a^(-1) = a^(p-2) mod p
    return pow(kMod - T{2});
  }

  // Inverse using extended Euclidean algorithm (works for any Mod)
  // PRECONDITION: gcd(value, Mod) == 1
  // Returns inverse, or raw(0) if no inverse exists
  constexpr auto invExtGcd() const -> ModInt {
    // Extended Euclidean algorithm
    // We want to find x such that value * x = 1 (mod Mod)
    // This means value * x + Mod * y = gcd(value, Mod) = 1

    // Using signed arithmetic internally for the algorithm
    // We track: a = old_r, b = r (the remainders)
    //          old_s, s (the coefficients for value)

    if (value == T{0}) {
      return raw(T{0});  // No inverse for 0
    }

    // Work with copies
    T old_r = value;
    T r = kMod;

    // For the coefficient of 'value', we need signed tracking
    // old_s * value + old_t * Mod = old_r
    // s * value + t * Mod = r
    // Initially: 1 * value + 0 * Mod = value, 0 * value + 1 * Mod = Mod

    // We only need to track s (coefficient of value)
    // Using two values and tracking sign separately
    T old_s = T{1};
    T s = T{0};
    bool old_s_neg = false;
    bool s_neg = false;

    while (r != T{0}) {
      T quotient = old_r / r;

      // Update r: old_r, r = r, old_r - quotient * r
      T temp_r = old_r - quotient * r;
      old_r = r;
      r = temp_r;

      // Update s: old_s, s = s, old_s - quotient * s
      // This requires careful signed arithmetic
      T qs = quotient * s;

      // new_s = old_s - quotient * s
      // Handle signs: old_s has sign old_s_neg, s has sign s_neg
      T new_s;
      bool new_s_neg;

      if (old_s_neg == s_neg) {
        // Same sign: old_s - qs, result has opposite sign if |qs| > |old_s|
        if (old_s >= qs) {
          new_s = old_s - qs;
          new_s_neg = old_s_neg;
        } else {
          new_s = qs - old_s;
          new_s_neg = !old_s_neg;
        }
      } else {
        // Different signs: old_s - qs = old_s + |qs| or -|old_s| - qs
        // If old_s positive, s negative: old_s - (-|s|*q) = old_s + qs
        // If old_s negative, s positive: -|old_s| - qs
        new_s = old_s + qs;
        new_s_neg = old_s_neg;
      }

      old_s = s;
      old_s_neg = s_neg;
      s = new_s;
      s_neg = new_s_neg;
    }

    // old_r is now gcd(value, Mod)
    if (old_r != T{1}) {
      return raw(T{0});  // No inverse exists
    }

    // old_s is the coefficient, with sign old_s_neg
    // We need old_s mod Mod
    if (old_s_neg) {
      // Negative coefficient: add Mod to make positive
      return raw(kMod - (old_s % kMod));
    } else {
      return raw(old_s % kMod);
    }
  }

  // --- Utility ---

  // Get the underlying value
  constexpr auto val() const -> T { return value; }

  // Check if zero
  constexpr auto isZero() const -> bool { return value == T{0}; }

 private:
  // Reduce a value mod Mod
  static constexpr auto reduce(T v) -> T {
    // For types where v might be >= Mod
    return v % kMod;
  }
};

// =============================================================================
// Common type aliases
// =============================================================================

// Popular moduli in competitive programming
using Mod998244353 = ModInt<unsigned long long, 998244353>;
using Mod1000000007 = ModInt<unsigned long long, 1000000007>;

}  // namespace tempura
