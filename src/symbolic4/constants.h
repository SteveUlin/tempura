#pragma once

#include <cstddef>

#include "symbolic4/core.h"

// ============================================================================
// constants.h - User-defined literal suffix for compile-time constants
// ============================================================================
//
// Usage:
//   42_c   → Constant<42.0>
//   3.14_c → Constant<3.14>
//
// All constants are doubles to avoid integer arithmetic surprises.
//
// Important note about negation:
//   -4_c is parsed as -(4_c) due to C++ operator precedence.
//   This creates Expression<NegOp, Constant<4.0>> at parse time.
//
// ============================================================================

namespace tempura::symbolic4 {

namespace detail {

template <char... chars>
constexpr auto toDouble() {
  double value = 0.;
  double fraction = 1.;
  bool is_fraction = false;
  bool negative = false;
  bool first = true;
  const auto process_char = [&](char c) {
    if (first && c == '-') {
      negative = true;
    } else if ('0' <= c && c <= '9') {
      if (is_fraction) {
        fraction /= 10.;
      }
      value = value * 10. + (c - '0');
    } else if (c == '.') {
      is_fraction = true;
    }
    first = false;
  };
  (process_char(chars), ...);
  return (negative ? -1. : 1.) * value * fraction;
}

template <char... chars>
constexpr auto toInteger() {
  long long value = 0;
  bool negative = false;
  bool first = true;
  const auto process_char = [&](char c) {
    if (first && c == '-') {
      negative = true;
    } else if ('0' <= c && c <= '9') {
      value = value * 10 + (c - '0');
    }
    first = false;
  };
  (process_char(chars), ...);
  return negative ? -value : value;
}

}  // namespace detail

// _c suffix: Creates Constant<double> for floating-point constants
// Usage: 3.14_c → Constant<3.14>, 42_c → Constant<42.0>
template <char... chars>
constexpr auto operator""_c() {
  return Constant<detail::toDouble<chars...>()>{};
}

// _z suffix: Creates Fraction<N, 1> for exact integer arithmetic (ℤ)
// Usage: 5_z → Fraction<5, 1>, 0_z → Fraction<0, 1>
// These stay exact through rational arithmetic operations.
template <char... chars>
constexpr auto operator""_z() {
  return Fraction<detail::toInteger<chars...>(), 1>{};
}

// ============================================================================
// _f suffix: Create fractions from string literals
// ============================================================================
//
// Usage:
//   "1/2"_f  → Fraction<1, 2>
//   "3/4"_f  → Fraction<3, 4>
//   "-5/6"_f → Fraction<-5, 6>
//   "7"_f    → Fraction<7, 1> (denominator defaults to 1)
//
// Uses C++20 structural types to enable string-to-type conversion at compile
// time. The string is parsed at compile time and the resulting Fraction type
// encodes the numerator and denominator as template parameters.
//
// ============================================================================

namespace detail {

// Structural type that can be used as a non-type template parameter (C++20)
template <std::size_t N>
struct FixedString {
  char data[N]{};

  consteval FixedString(const char (&str)[N]) {
    for (std::size_t i = 0; i < N; ++i) {
      data[i] = str[i];
    }
  }
};

// Result of parsing a fraction string
struct ParsedFraction {
  long long num;
  long long den;
};

// Parse "num/den" or "num" string at compile time
template <FixedString S>
consteval auto parseFraction() -> ParsedFraction {
  long long num = 0;
  long long den = 0;
  bool negative = false;
  std::size_t i = 0;

  // Handle optional negative sign
  if (S.data[i] == '-') {
    negative = true;
    ++i;
  }

  // Parse numerator
  while (S.data[i] != '/' && S.data[i] != '\0') {
    num = num * 10 + (S.data[i] - '0');
    ++i;
  }

  // Check for denominator
  if (S.data[i] == '/') {
    ++i;
    // Parse denominator
    while (S.data[i] != '\0') {
      den = den * 10 + (S.data[i] - '0');
      ++i;
    }
  } else {
    den = 1;  // No slash means integer (denominator = 1)
  }

  if (negative) num = -num;

  return ParsedFraction{num, den};
}

}  // namespace detail

// _f suffix: Creates Fraction from string "numerator/denominator"
template <detail::FixedString S>
consteval auto operator""_f() {
  constexpr auto parsed = detail::parseFraction<S>();
  return Fraction<parsed.num, parsed.den>{};
}

}  // namespace tempura::symbolic4
