#pragma once

#include "modular/number_theory.h"

// Ratio - A rational number type (numerator/denominator)
//
// Features:
// - Auto-reduces to lowest terms using gcd
// - Sign normalization (denominator always positive)
// - Full arithmetic and comparison operators
// - constexpr throughout

namespace tempura {

template <typename T = long long>
struct Ratio {
  T num;  // numerator (can be negative)
  T den;  // denominator (always positive)

  // Construct and reduce to lowest terms
  constexpr Ratio(T numerator = 0, T denominator = 1)
      : num(numerator), den(denominator) {
    reduce();
  }

  constexpr void reduce() {
    if (den == 0) {
      num = (num > 0) ? 1 : (num < 0) ? -1 : 0;
      return;
    }
    // Normalize sign: denominator always positive
    if (den < 0) {
      num = -num;
      den = -den;
    }
    // Reduce by gcd
    T g = gcd(num < 0 ? -num : num, den);
    if (g > 1) {
      num /= g;
      den /= g;
    }
  }

  // Conversion to floating point
  constexpr explicit operator double() const {
    return static_cast<double>(num) / static_cast<double>(den);
  }

  constexpr explicit operator float() const {
    return static_cast<float>(num) / static_cast<float>(den);
  }

  // Check if zero
  constexpr bool isZero() const { return num == 0; }

  // Check if integer
  constexpr bool isInteger() const { return den == 1; }

  // Absolute value
  constexpr Ratio abs() const { return {num < 0 ? -num : num, den}; }

  // Reciprocal
  constexpr Ratio reciprocal() const { return {den, num}; }
};

// Arithmetic operators
template <typename T>
constexpr Ratio<T> operator+(Ratio<T> a, Ratio<T> b) {
  return {a.num * b.den + b.num * a.den, a.den * b.den};
}

template <typename T>
constexpr Ratio<T> operator-(Ratio<T> a, Ratio<T> b) {
  return {a.num * b.den - b.num * a.den, a.den * b.den};
}

template <typename T>
constexpr Ratio<T> operator*(Ratio<T> a, Ratio<T> b) {
  return {a.num * b.num, a.den * b.den};
}

template <typename T>
constexpr Ratio<T> operator/(Ratio<T> a, Ratio<T> b) {
  return {a.num * b.den, a.den * b.num};
}

template <typename T>
constexpr Ratio<T> operator-(Ratio<T> r) {
  return {-r.num, r.den};
}

// Compound assignment
template <typename T>
constexpr Ratio<T>& operator+=(Ratio<T>& a, Ratio<T> b) {
  return a = a + b;
}

template <typename T>
constexpr Ratio<T>& operator-=(Ratio<T>& a, Ratio<T> b) {
  return a = a - b;
}

template <typename T>
constexpr Ratio<T>& operator*=(Ratio<T>& a, Ratio<T> b) {
  return a = a * b;
}

template <typename T>
constexpr Ratio<T>& operator/=(Ratio<T>& a, Ratio<T> b) {
  return a = a / b;
}

// Comparison operators
template <typename T>
constexpr bool operator==(Ratio<T> a, Ratio<T> b) {
  return a.num == b.num && a.den == b.den;
}

template <typename T>
constexpr bool operator!=(Ratio<T> a, Ratio<T> b) {
  return !(a == b);
}

template <typename T>
constexpr bool operator<(Ratio<T> a, Ratio<T> b) {
  return a.num * b.den < b.num * a.den;
}

template <typename T>
constexpr bool operator>(Ratio<T> a, Ratio<T> b) {
  return b < a;
}

template <typename T>
constexpr bool operator<=(Ratio<T> a, Ratio<T> b) {
  return !(a > b);
}

template <typename T>
constexpr bool operator>=(Ratio<T> a, Ratio<T> b) {
  return !(a < b);
}

// Mixed arithmetic with integers
template <typename T>
constexpr Ratio<T> operator+(Ratio<T> r, T n) {
  return r + Ratio<T>{n};
}

template <typename T>
constexpr Ratio<T> operator+(T n, Ratio<T> r) {
  return Ratio<T>{n} + r;
}

template <typename T>
constexpr Ratio<T> operator-(Ratio<T> r, T n) {
  return r - Ratio<T>{n};
}

template <typename T>
constexpr Ratio<T> operator-(T n, Ratio<T> r) {
  return Ratio<T>{n} - r;
}

template <typename T>
constexpr Ratio<T> operator*(Ratio<T> r, T n) {
  return {r.num * n, r.den};
}

template <typename T>
constexpr Ratio<T> operator*(T n, Ratio<T> r) {
  return {n * r.num, r.den};
}

template <typename T>
constexpr Ratio<T> operator/(Ratio<T> r, T n) {
  return {r.num, r.den * n};
}

template <typename T>
constexpr Ratio<T> operator/(T n, Ratio<T> r) {
  return {n * r.den, r.num};
}

// Static assertions
static_assert(Ratio{1, 2}.num == 1);
static_assert(Ratio{1, 2}.den == 2);
static_assert(Ratio{2, 4}.num == 1);  // Auto-reduces
static_assert(Ratio{2, 4}.den == 2);
static_assert(Ratio{-1, 2}.num == -1);
static_assert(Ratio{1, -2}.num == -1);  // Sign normalization
static_assert(Ratio{1, -2}.den == 2);
static_assert(Ratio{-1, -2}.num == 1);  // Double negative
static_assert(Ratio{6, 9} == Ratio{2, 3});
static_assert(Ratio{1, 2} + Ratio{1, 3} == Ratio{5, 6});
static_assert(Ratio{1, 2} - Ratio{1, 3} == Ratio{1, 6});
static_assert(Ratio{2, 3} * Ratio{3, 4} == Ratio{1, 2});
static_assert(Ratio{1, 2} / Ratio{1, 4} == Ratio{2, 1});
static_assert(Ratio{1, 2} < Ratio{2, 3});
static_assert(Ratio{3, 4} > Ratio{1, 2});

}  // namespace tempura
