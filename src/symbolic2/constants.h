#pragma once

#include "core.h"

// User-defined literal suffix for numeric constants
// Usage: 42_c → Constant<42>, 3.14_c → Constant<3.14>
// Note: -4_c is parsed as -(4_c) since negation is a unary operator

namespace tempura {

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

}  // namespace tempura
