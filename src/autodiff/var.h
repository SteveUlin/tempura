#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <type_traits>

#include "tape.h"

namespace tempura::autodiff {

// A value handle into a Tape: the current value plus the handle of the node that
// produced it. Each operation records one node on the shared tape and returns a fresh
// Var. Thin and freely copyable; it BORROWS the tape, which must outlive every Var.
template <typename T>
struct Var {
  Tape<T>* tape;
  std::size_t id;
  T value;
};

// Create an independent variable on `tape`.
template <typename T>
auto variable(Tape<T>& tape, const T& value) -> Var<T> {
  return {&tape, tape.leaf(), value};
}

// ── binary ops (both operands on the same tape) ──
template <typename T>
auto operator+(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.tape && a.tape == b.tape && "Vars must share one tape");
  return {a.tape, a.tape->binary(a.id, b.id, T{1}, T{1}), a.value + b.value};
}
template <typename T>
auto operator-(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.tape && a.tape == b.tape && "Vars must share one tape");
  return {a.tape, a.tape->binary(a.id, b.id, T{1}, T{-1}), a.value - b.value};
}
template <typename T>
auto operator*(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.tape && a.tape == b.tape && "Vars must share one tape");
  // ∂(ab)/∂a = b, ∂(ab)/∂b = a
  return {a.tape, a.tape->binary(a.id, b.id, b.value, a.value), a.value * b.value};
}
template <typename T>
auto operator/(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.tape && a.tape == b.tape && "Vars must share one tape");
  const T inv = T{1} / b.value;  // ∂(a/b)/∂a = 1/b, ∂/∂b = −a/b²
  return {a.tape, a.tape->binary(a.id, b.id, inv, -a.value * inv * inv), a.value * inv};
}
template <typename T>
auto operator-(const Var<T>& a) -> Var<T> {
  return {a.tape, a.tape->unary(a.id, T{-1}), -a.value};
}

// ── scalar mixed (the scalar is a constant — it adds no node, only folds into value or
// scales the partial). A unary op keeps the chain attached to the variable operand.
// The scalars of Var<T> are exactly what constructs into T (the same gate as Dual): on a
// Tape<Dual<U>> a bare double lifts to a zero-tangent Dual, a lossy mix fails loud. ──
template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator+(const Var<T>& a, const U& s) -> Var<T> {
  return {a.tape, a.tape->unary(a.id, T{1}), a.value + static_cast<T>(s)};
}
template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator+(const U& s, const Var<T>& a) -> Var<T> {
  return a + s;
}
template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator-(const Var<T>& a, const U& s) -> Var<T> {
  return {a.tape, a.tape->unary(a.id, T{1}), a.value - static_cast<T>(s)};
}
template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator-(const U& s, const Var<T>& a) -> Var<T> {
  return {a.tape, a.tape->unary(a.id, T{-1}), static_cast<T>(s) - a.value};
}
template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator*(const Var<T>& a, const U& s) -> Var<T> {
  const T c = static_cast<T>(s);
  return {a.tape, a.tape->unary(a.id, c), a.value * c};
}
template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator*(const U& s, const Var<T>& a) -> Var<T> {
  return a * s;
}
template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator/(const Var<T>& a, const U& s) -> Var<T> {
  const T inv = T{1} / static_cast<T>(s);
  return {a.tape, a.tape->unary(a.id, inv), a.value * inv};
}
template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator/(const U& s, const Var<T>& a) -> Var<T> {
  const T q = static_cast<T>(s) / a.value;
  return {a.tape, a.tape->unary(a.id, -q / a.value), q};  // d(c/x) = −(c/x)/x, no x² forms
}

// ── elementary functions (ADL: for Var<Dual> these compose with the dual versions) ──
template <typename T>
auto sin(const Var<T>& x) -> Var<T> {
  using std::cos;
  using std::sin;
  return {x.tape, x.tape->unary(x.id, cos(x.value)), sin(x.value)};
}
template <typename T>
auto cos(const Var<T>& x) -> Var<T> {
  using std::cos;
  using std::sin;
  return {x.tape, x.tape->unary(x.id, -sin(x.value)), cos(x.value)};
}
template <typename T>
auto exp(const Var<T>& x) -> Var<T> {
  using std::exp;
  const T e = exp(x.value);
  return {x.tape, x.tape->unary(x.id, e), e};
}
template <typename T>
auto log(const Var<T>& x) -> Var<T> {
  using std::log;
  assert(x.value > T{} && "log domain: value must be > 0");
  return {x.tape, x.tape->unary(x.id, T{1} / x.value), log(x.value)};
}
template <typename T>
auto sqrt(const Var<T>& x) -> Var<T> {
  using std::sqrt;
  assert(x.value >= T{} && "sqrt domain: value must be ≥ 0");
  const T s = sqrt(x.value);
  return {x.tape, x.tape->unary(x.id, T{1} / (T{2} * s)), s};
}
template <typename T>
auto tan(const Var<T>& x) -> Var<T> {
  using std::cos;
  using std::tan;
  const T c = cos(x.value);
  return {x.tape, x.tape->unary(x.id, T{1} / (c * c)), tan(x.value)};
}
template <typename T>
auto asin(const Var<T>& x) -> Var<T> {
  using std::asin;
  using std::sqrt;
  return {x.tape, x.tape->unary(x.id, T{1} / sqrt(T{1} - x.value * x.value)), asin(x.value)};
}
template <typename T>
auto acos(const Var<T>& x) -> Var<T> {
  using std::acos;
  using std::sqrt;
  return {x.tape, x.tape->unary(x.id, T{-1} / sqrt(T{1} - x.value * x.value)), acos(x.value)};
}
template <typename T>
auto atan(const Var<T>& x) -> Var<T> {
  using std::atan;
  return {x.tape, x.tape->unary(x.id, T{1} / (T{1} + x.value * x.value)), atan(x.value)};
}
// pow, constant exponent: d/dx xⁿ = n·xⁿ⁻¹. Non-deduced n so pow(x, 2) binds to
// T instead of deducing int and clashing with T=double.
template <typename T>
auto pow(const Var<T>& x, const std::type_identity_t<T>& n) -> Var<T> {
  using std::pow;
  return {x.tape, x.tape->unary(x.id, n * pow(x.value, n - T{1})), pow(x.value, n)};
}
// pow, variable exponent: d(uᵛ) = uᵛ·(v·u′/u + ln u·v′).
template <typename T>
auto pow(const Var<T>& x, const Var<T>& e) -> Var<T> {
  assert(x.tape && x.tape == e.tape && "Vars must share one tape");
  using std::log;
  using std::pow;
  const T uv = pow(x.value, e.value);
  return {x.tape, x.tape->binary(x.id, e.id, uv * e.value / x.value, uv * log(x.value)), uv};
}
// fma: the value keeps std::fma's single rounding; the partials b, a, 1 are exact regardless.
template <typename T>
auto fma(const Var<T>& a, const Var<T>& b, const Var<T>& c) -> Var<T> {
  assert(a.tape && a.tape == b.tape && a.tape == c.tape && "Vars must share one tape");
  using std::fma;
  return {a.tape, a.tape->ternary(a.id, b.id, c.id, b.value, a.value, T{1}),
          fma(a.value, b.value, c.value)};
}
}  // namespace tempura::autodiff
