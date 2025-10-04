#pragma once

#include "core.h"

// Constant literals and helpers

namespace tempura {

// --- Constant - Helper Literals ---
//
// Convert 1_c to Constant<1> and 3.14_c to Constant<3.14>
//
// Note that prefixes such as + and - in +10_c and -4_c are not parsed
// as part of the number. Instead C++ treats these as unary operators and
// must be handled separately.

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

// Assume that there is exactly one decimal point in the number
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

}  // namespace tempura
