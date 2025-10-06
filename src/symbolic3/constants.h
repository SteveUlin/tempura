#pragma once

#include "symbolic3/core.h"

// User-defined literal suffix for compile-time numeric constants.
//
// Usage:
//   42_c → Constant<42>
//   3.14_c → Constant<3.14>
//
// Important note about negation:
//   -4_c is parsed as -(4_c) due to C++ operator precedence.
//   This creates Expression<Neg, Constant<4>> at parse time.
//
//   In contrast, Constant<-4>{} is a single compile-time constant.
//   For pattern matching and simplification, these are different:
//     - Constant<-4>{} is an atomic constant with value -4
//     - -4_c is a negation expression of constant 4
//
//   When writing rewrite rules:
//     Constant<-1>{} matches only atomic constant -1
//     -1_c matches Neg(Constant<1>), not Constant<-1>
//
//   Best practice: Use Constant<N>{} in rewrite patterns for specificity,
//   use N_c for building expressions in replacements for readability.

namespace tempura::symbolic3 {

template <char... chars>
  requires(sizeof...(chars) > 0)
constexpr auto toInt() {
  return [] constexpr {
    long long value = 0;
    const auto process_char = [&](char c) {
      if ('0' <= c && c <= '9') {
        value = value * 10 + (c - '0');
      }
    };

    (process_char(chars), ...);
    return static_cast<int>(value);
  }();
}

template <char... chars>
constexpr auto toDouble() {
  double value = 0.;
  double fraction = 1.;
  bool is_fraction = false;
  const auto process_char = [&](char c) {
    if ('0' <= c && c <= '9') {
      if (is_fraction) {
        fraction /= 10.;
      }
      value = value * 10. + (c - '0');
    } else if (c == '.') {
      is_fraction = true;
    }
  };
  (process_char(chars), ...);
  return value * fraction;
}

template <char... chars>
constexpr auto count(char c) {
  return ((c == chars) + ... + 0);
}

template <char... chars>
constexpr auto operator""_c() {
  constexpr int dot_count = count<chars...>('.');
  if constexpr (dot_count == 0) {
    return Constant<toInt<chars...>()>{};
  } else if constexpr (dot_count == 1) {
    return Constant<toDouble<chars...>()>{};
  } else {
    static_assert(dot_count < 2, "Invalid floating point constant");
  }
}

}  // namespace tempura::symbolic3
