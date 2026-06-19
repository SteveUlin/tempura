#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>

#include "tape.h"

namespace tempura::autodiff {

// A value handle into a Tape: the current value plus the index of the node that produced
// it. Each operation records one Entry on the shared tape and returns a fresh Var. Thin
// and freely copyable; it BORROWS the tape, which must outlive every Var into it.
template <typename T>
struct Var {
  Tape<T>* tape{};
  std::uint32_t idx{};
  T value{};
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
  return {a.tape, a.tape->binary(a.idx, b.idx, T{1}, T{1}), a.value + b.value};
}
template <typename T>
auto operator-(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.tape && a.tape == b.tape && "Vars must share one tape");
  return {a.tape, a.tape->binary(a.idx, b.idx, T{1}, T{-1}), a.value - b.value};
}
template <typename T>
auto operator*(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.tape && a.tape == b.tape && "Vars must share one tape");
  // ∂(ab)/∂a = b, ∂(ab)/∂b = a
  return {a.tape, a.tape->binary(a.idx, b.idx, b.value, a.value), a.value * b.value};
}
template <typename T>
auto operator/(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.tape && a.tape == b.tape && "Vars must share one tape");
  const T inv = T{1} / b.value;  // ∂(a/b)/∂a = 1/b, ∂/∂b = −a/b²
  return {a.tape, a.tape->binary(a.idx, b.idx, inv, -a.value * inv * inv), a.value * inv};
}
template <typename T>
auto operator-(const Var<T>& a) -> Var<T> {
  return {a.tape, a.tape->unary(a.idx, T{-1}), -a.value};
}

// ── scalar mixed (the scalar is a constant — it adds no node, only folds into value or
// scales the partial). A unary op keeps the chain attached to the variable operand. ──
template <typename T>
auto operator+(const Var<T>& a, const T& c) -> Var<T> {
  return {a.tape, a.tape->unary(a.idx, T{1}), a.value + c};
}
template <typename T>
auto operator+(const T& c, const Var<T>& a) -> Var<T> {
  return a + c;
}
template <typename T>
auto operator-(const Var<T>& a, const T& c) -> Var<T> {
  return {a.tape, a.tape->unary(a.idx, T{1}), a.value - c};
}
template <typename T>
auto operator-(const T& c, const Var<T>& a) -> Var<T> {
  return {a.tape, a.tape->unary(a.idx, T{-1}), c - a.value};
}
template <typename T>
auto operator*(const Var<T>& a, const T& c) -> Var<T> {
  return {a.tape, a.tape->unary(a.idx, c), a.value * c};
}
template <typename T>
auto operator*(const T& c, const Var<T>& a) -> Var<T> {
  return a * c;
}
template <typename T>
auto operator/(const Var<T>& a, const T& c) -> Var<T> {
  return {a.tape, a.tape->unary(a.idx, T{1} / c), a.value / c};
}

// ── elementary functions (ADL: for Var<Dual> these compose with the dual versions) ──
template <typename T>
auto sin(const Var<T>& x) -> Var<T> {
  using std::cos;
  using std::sin;
  return {x.tape, x.tape->unary(x.idx, cos(x.value)), sin(x.value)};
}
template <typename T>
auto cos(const Var<T>& x) -> Var<T> {
  using std::cos;
  using std::sin;
  return {x.tape, x.tape->unary(x.idx, -sin(x.value)), cos(x.value)};
}
template <typename T>
auto exp(const Var<T>& x) -> Var<T> {
  using std::exp;
  const T e = exp(x.value);
  return {x.tape, x.tape->unary(x.idx, e), e};
}
template <typename T>
auto log(const Var<T>& x) -> Var<T> {
  using std::log;
  assert(x.value > T{} && "log domain: value must be > 0");
  return {x.tape, x.tape->unary(x.idx, T{1} / x.value), log(x.value)};
}
template <typename T>
auto sqrt(const Var<T>& x) -> Var<T> {
  using std::sqrt;
  assert(x.value >= T{} && "sqrt domain: value must be ≥ 0");
  const T s = sqrt(x.value);
  return {x.tape, x.tape->unary(x.idx, T{1} / (T{2} * s)), s};
}
template <typename T>
auto tan(const Var<T>& x) -> Var<T> {
  using std::cos;
  using std::tan;
  const T c = cos(x.value);
  return {x.tape, x.tape->unary(x.idx, T{1} / (c * c)), tan(x.value)};
}
template <typename T>
auto pow(const Var<T>& x, const T& n) -> Var<T> {
  using std::pow;
  return {x.tape, x.tape->unary(x.idx, n * pow(x.value, n - T{1})), pow(x.value, n)};
}

}  // namespace tempura::autodiff
