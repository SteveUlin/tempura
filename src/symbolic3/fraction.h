#pragma once

#include "symbolic3/core.h"
#include "symbolic3/operators.h"

// Fraction arithmetic and literal suffixes for exact rational computation

namespace tempura::symbolic3 {

// ============================================================================
// Fraction Arithmetic - Compile-time operations
// ============================================================================

// Addition: a/b + c/d = (a*d + b*c) / (b*d), then reduce
template <long long N1, long long D1, long long N2, long long D2>
constexpr auto operator+(Fraction<N1, D1>, Fraction<N2, D2>) {
  constexpr auto num = N1 * D2 + N2 * D1;
  constexpr auto den = D1 * D2;
  return Fraction<num, den>{};
}

// Subtraction: a/b - c/d = (a*d - b*c) / (b*d), then reduce
template <long long N1, long long D1, long long N2, long long D2>
constexpr auto operator-(Fraction<N1, D1>, Fraction<N2, D2>) {
  constexpr auto num = N1 * D2 - N2 * D1;
  constexpr auto den = D1 * D2;
  return Fraction<num, den>{};
}

// Multiplication: a/b * c/d = (a*c) / (b*d), then reduce
template <long long N1, long long D1, long long N2, long long D2>
constexpr auto operator*(Fraction<N1, D1>, Fraction<N2, D2>) {
  constexpr auto num = N1 * N2;
  constexpr auto den = D1 * D2;
  return Fraction<num, den>{};
}

// Division: (a/b) / (c/d) = (a*d) / (b*c), then reduce
template <long long N1, long long D1, long long N2, long long D2>
constexpr auto operator/(Fraction<N1, D1>, Fraction<N2, D2>) {
  static_assert(N2 != 0, "Division by zero");
  constexpr auto num = N1 * D2;
  constexpr auto den = D1 * N2;
  return Fraction<num, den>{};
}

// Negation: -(a/b) = (-a)/b
template <long long N, long long D>
constexpr auto operator-(Fraction<N, D>) {
  return Fraction<-N, D>{};
}

// ============================================================================
// Mixed arithmetic with integers - promote to fractions
// ============================================================================

// Fraction + Integer (constexpr integer types only)
template <long long N, long long D, auto i>
constexpr auto operator+(Fraction<N, D>, Constant<i>) {
  return Fraction<N, D>{} + Fraction<i, 1>{};
}

template <auto i, long long N, long long D>
constexpr auto operator+(Constant<i>, Fraction<N, D>) {
  return Fraction<i, 1>{} + Fraction<N, D>{};
}

// Fraction - Integer
template <long long N, long long D, auto i>
constexpr auto operator-(Fraction<N, D>, Constant<i>) {
  return Fraction<N, D>{} - Fraction<i, 1>{};
}

template <auto i, long long N, long long D>
constexpr auto operator-(Constant<i>, Fraction<N, D>) {
  return Fraction<i, 1>{} - Fraction<N, D>{};
}

// Fraction * Integer
template <long long N, long long D, auto i>
constexpr auto operator*(Fraction<N, D>, Constant<i>) {
  return Fraction<N, D>{} * Fraction<i, 1>{};
}

template <auto i, long long N, long long D>
constexpr auto operator*(Constant<i>, Fraction<N, D>) {
  return Fraction<i, 1>{} * Fraction<N, D>{};
}

// Fraction / Integer
template <long long N, long long D, auto i>
constexpr auto operator/(Fraction<N, D>, Constant<i>) {
  static_assert(i != 0, "Division by zero");
  return Fraction<N, D>{} / Fraction<i, 1>{};
}

template <auto i, long long N, long long D>
constexpr auto operator/(Constant<i>, Fraction<N, D>) {
  return Fraction<i, 1>{} / Fraction<N, D>{};
}

// ============================================================================
// Comparison operators
// ============================================================================

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator==(Fraction<N1, D1>, Fraction<N2, D2>) {
  // After GCD reduction, equal fractions have same N and D
  return Fraction<N1, D1>::numerator == Fraction<N2, D2>::numerator &&
         Fraction<N1, D1>::denominator == Fraction<N2, D2>::denominator;
}

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator!=(Fraction<N1, D1> f1, Fraction<N2, D2> f2) {
  return !(f1 == f2);
}

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator<(Fraction<N1, D1>, Fraction<N2, D2>) {
  // a/b < c/d iff a*d < c*b (assuming positive denominators after
  // normalization)
  return N1 * D2 < N2 * D1;
}

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator>(Fraction<N1, D1> f1, Fraction<N2, D2> f2) {
  return f2 < f1;
}

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator<=(Fraction<N1, D1> f1, Fraction<N2, D2> f2) {
  return !(f1 > f2);
}

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator>=(Fraction<N1, D1> f1, Fraction<N2, D2> f2) {
  return !(f1 < f2);
}

// ============================================================================
// Literal suffix for fractions
// ============================================================================

// Helper to parse integer from char pack at compile-time
template <char... chars>
constexpr long long parse_frac_int() {
  long long value = 0;
  auto process = [&value](char c) {
    if ('0' <= c && c <= '9') {
      value = value * 10 + (c - '0');
    }
  };
  (process(chars), ...);
  return value;
}

// Literal suffix: 3_frac creates Fraction<3, 1>
// Usage: auto half = 1_frac / 2_frac;  // Creates Fraction<1,2>
template <char... chars>
constexpr auto operator""_frac() {
  return Fraction<parse_frac_int<chars...>(), 1>{};
}

// Convenience constants for common fractions
inline constexpr Fraction<1, 2> half{};
inline constexpr Fraction<1, 3> third{};
inline constexpr Fraction<1, 4> quarter{};
inline constexpr Fraction<2, 3> two_thirds{};
inline constexpr Fraction<3, 4> three_quarters{};

}  // namespace tempura::symbolic3
