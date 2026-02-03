#pragma once

#include <type_traits>

#include "symbolic4/core.h"

// ============================================================================
// fraction_ops.h - Closed arithmetic over rationals (ℚ)
// ============================================================================
//
// These operators perform exact rational arithmetic at the type level.
// Fraction<N1,D1> + Fraction<N2,D2> → Fraction<...> (not Expression!)
//
// Key property: Arithmetic is closed over ℚ - you never leave the rationals.
// This preserves exactness through arbitrary computations.
//
// These operators are MORE SPECIFIC than the generic Symbolic operators
// in operators.h, so they'll be chosen by overload resolution when both
// operands are Fractions.
//
// Examples:
//   Fraction<1,2> + Fraction<1,3>  → Fraction<5,6>
//   Fraction<2,3> * Fraction<3,4>  → Fraction<1,2>  (auto-reduced)
//   Fraction<1,2> / Fraction<2,1>  → Fraction<1,4>
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Binary Operators: Fraction ⊕ Fraction → Fraction
// ============================================================================

// Addition: a/b + c/d = (ad + bc) / bd
template <long long N1, long long D1, long long N2, long long D2>
constexpr auto operator+(Fraction<N1, D1>, Fraction<N2, D2>) {
  return Fraction<N1 * D2 + N2 * D1, D1 * D2>{};
}

// Subtraction: a/b - c/d = (ad - bc) / bd
template <long long N1, long long D1, long long N2, long long D2>
constexpr auto operator-(Fraction<N1, D1>, Fraction<N2, D2>) {
  return Fraction<N1 * D2 - N2 * D1, D1 * D2>{};
}

// Multiplication: a/b * c/d = ac / bd
template <long long N1, long long D1, long long N2, long long D2>
constexpr auto operator*(Fraction<N1, D1>, Fraction<N2, D2>) {
  return Fraction<N1 * N2, D1 * D2>{};
}

// Division: (a/b) / (c/d) = ad / bc
template <long long N1, long long D1, long long N2, long long D2>
constexpr auto operator/(Fraction<N1, D1>, Fraction<N2, D2>) {
  return Fraction<N1 * D2, D1 * N2>{};
}

// ============================================================================
// Unary Operators
// ============================================================================

// Negation: -(a/b) = -a/b
template <long long N, long long D>
constexpr auto operator-(Fraction<N, D>) {
  return Fraction<-N, D>{};
}

// ============================================================================
// Comparison Operators (for completeness)
// ============================================================================

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator==(Fraction<N1, D1>, Fraction<N2, D2>) {
  // After reduction, equal fractions have equal numerators and denominators
  return Fraction<N1, D1>::numerator == Fraction<N2, D2>::numerator &&
         Fraction<N1, D1>::denominator == Fraction<N2, D2>::denominator;
}

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator<(Fraction<N1, D1>, Fraction<N2, D2>) {
  // Cross-multiply for comparison (both denominators positive after reduction)
  return Fraction<N1, D1>::numerator * Fraction<N2, D2>::denominator <
         Fraction<N2, D2>::numerator * Fraction<N1, D1>::denominator;
}

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator>(Fraction<N1, D1> a, Fraction<N2, D2> b) {
  return b < a;
}

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator<=(Fraction<N1, D1> a, Fraction<N2, D2> b) {
  return !(a > b);
}

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool operator>=(Fraction<N1, D1> a, Fraction<N2, D2> b) {
  return !(a < b);
}

// ============================================================================
// Integer Constant Operations → Fraction
// ============================================================================
//
// When dividing two integer Constants, produce a Fraction (exact) instead
// of an Expression that would evaluate to a float.
//
// This is the key fix: Constant<2> / Constant<3> → Fraction<2,3>, not 0.666...

// Concept for integer-valued Constant
// Includes Symbolic to ensure subsumption (IntegralConstant is more specific than Symbolic)
template <typename T>
concept IntegralConstant = Symbolic<T> && is_constant_v<T> && std::is_integral_v<decltype(T::value)>;

// Integer Constant division → Fraction (EXACT, not float division!)
template <IntegralConstant L, IntegralConstant R>
constexpr auto operator/(L, R) {
  return Fraction<static_cast<long long>(L::value), static_cast<long long>(R::value)>{};
}

// Integer Constant + Fraction → Fraction
template <IntegralConstant C, long long N, long long D>
constexpr auto operator+(C, Fraction<N, D>) {
  return Fraction<C::value * D + N, D>{};
}

template <long long N, long long D, IntegralConstant C>
constexpr auto operator+(Fraction<N, D>, C) {
  return Fraction<N + C::value * D, D>{};
}

// Integer Constant - Fraction → Fraction
template <IntegralConstant C, long long N, long long D>
constexpr auto operator-(C, Fraction<N, D>) {
  return Fraction<C::value * D - N, D>{};
}

template <long long N, long long D, IntegralConstant C>
constexpr auto operator-(Fraction<N, D>, C) {
  return Fraction<N - C::value * D, D>{};
}

// Integer Constant * Fraction → Fraction
template <IntegralConstant C, long long N, long long D>
constexpr auto operator*(C, Fraction<N, D>) {
  return Fraction<C::value * N, D>{};
}

template <long long N, long long D, IntegralConstant C>
constexpr auto operator*(Fraction<N, D>, C) {
  return Fraction<N * C::value, D>{};
}

// Integer Constant / Fraction → Fraction (C / (N/D) = C*D/N)
template <IntegralConstant C, long long N, long long D>
constexpr auto operator/(C, Fraction<N, D>) {
  return Fraction<C::value * D, N>{};
}

// Fraction / Integer Constant → Fraction ((N/D) / C = N/(D*C))
template <long long N, long long D, IntegralConstant C>
constexpr auto operator/(Fraction<N, D>, C) {
  return Fraction<N, D * C::value>{};
}

// ============================================================================
// Type Traits for Exact Arithmetic
// ============================================================================

// Check if a Fraction represents an integer (denominator = 1)
template <typename T>
struct IsInteger : std::false_type {};

template <long long N, long long D>
struct IsInteger<Fraction<N, D>> : std::bool_constant<Fraction<N, D>::denominator == 1> {};

template <typename T>
constexpr bool is_integer_v = IsInteger<T>::value;

// Check if a Fraction represents zero
template <typename T>
struct IsZeroFraction : std::false_type {};

template <long long N, long long D>
struct IsZeroFraction<Fraction<N, D>> : std::bool_constant<Fraction<N, D>::numerator == 0> {};

template <typename T>
constexpr bool is_zero_fraction_v = IsZeroFraction<T>::value;

// Check if a Fraction represents one
template <typename T>
struct IsOneFraction : std::false_type {};

template <long long N, long long D>
struct IsOneFraction<Fraction<N, D>>
    : std::bool_constant<Fraction<N, D>::numerator == 1 && Fraction<N, D>::denominator == 1> {};

template <typename T>
constexpr bool is_one_fraction_v = IsOneFraction<T>::value;

}  // namespace tempura::symbolic4
