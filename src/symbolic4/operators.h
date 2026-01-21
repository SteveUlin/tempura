#pragma once

#include "meta/function_objects.h"
#include "symbolic4/core.h"

// ============================================================================
// operators.h - Building symbolic expressions with natural syntax
// ============================================================================
//
// This file provides the user-facing API for constructing symbolic expressions.
// It overloads standard operators (+, -, *, /) and provides named functions
// (sin, cos, exp, log) so you can write expressions naturally:
//
//   Symbol<struct X> x;
//   auto expr = sin(x * x) + exp(-x);  // Just like math notation
//
// Key design decisions:
//
// 1. Operators return Expression<Op, Args...>, not simplified results.
//    We don't simplify x + 0 to x here - that's the job of an interpreter.
//    This keeps the operators simple and makes the AST structure predictable.
//
// 2. Op types come from meta/function_objects.h (AddOp, MulOp, etc.)
//    These are empty tag types that also serve as callable functors.
//    This allows the same Op type to be used for both AST construction
//    and runtime evaluation (op(a, b) computes the result).
//
// 3. The Symbolic concept constrains all operators.
//    Only types derived from SymbolicTag can be combined. This prevents
//    accidental mixing of symbolic and non-symbolic types without explicit
//    conversion (use lit(value) or Constant<V>{} to lift scalars).
//
// 4. Mathematical constants (π, e) are nullary expressions.
//    Expression<PiOp> is the symbolic representation of π. During evaluation,
//    PiOp{}() returns M_PI. This keeps constants in the AST for symbolic
//    manipulation while providing their numeric values when needed.
//
// ============================================================================

namespace tempura::symbolic4 {

// Import operator types from tempura namespace
using tempura::AddOp;
using tempura::AcosOp;
using tempura::AsinOp;
using tempura::AtanOp;
using tempura::CosOp;
using tempura::CoshOp;
using tempura::DivOp;
using tempura::EOp;
using tempura::ExpOp;
using tempura::LogOp;
using tempura::MulOp;
using tempura::NegOp;
using tempura::PiOp;
using tempura::PowOp;
using tempura::SinOp;
using tempura::SinhOp;
using tempura::SqrtOp;
using tempura::SubOp;
using tempura::TanOp;
using tempura::TanhOp;

// ============================================================================
// Binary Operations
// ============================================================================
//
// Each operator creates an Expression node with the corresponding Op type.
// The expression preserves both operands as template arguments, encoding
// the full tree structure in the type system.
//
// Associativity: These operators are left-associative by C++ precedence rules.
//   a + b + c parses as (a + b) + c → Expression<AddOp, Expression<AddOp, a, b>, c>

template <Symbolic L, Symbolic R>
constexpr auto operator+(L lhs, R rhs) {
  return Expression<AddOp, L, R>{lhs, rhs};
}

template <Symbolic L, Symbolic R>
constexpr auto operator-(L lhs, R rhs) {
  return Expression<SubOp, L, R>{lhs, rhs};
}

template <Symbolic L, Symbolic R>
constexpr auto operator*(L lhs, R rhs) {
  return Expression<MulOp, L, R>{lhs, rhs};
}

template <Symbolic L, Symbolic R>
constexpr auto operator/(L lhs, R rhs) {
  return Expression<DivOp, L, R>{lhs, rhs};
}

// ============================================================================
// Unary Operations
// ============================================================================
//
// Unary negation. Note: unary + is intentionally not overloaded (it's a no-op).

template <Symbolic S>
constexpr auto operator-(S s) {
  return Expression<NegOp, S>{s};
}

// ============================================================================
// Transcendental Functions
// ============================================================================
//
// Named functions for transcendental operations. These are in the symbolic4
// namespace, so they don't conflict with <cmath> functions. When you write
// sin(x) where x is Symbolic, ADL finds this overload.
//
// Categories:
//   Trigonometric: sin, cos, tan
//   Hyperbolic:    sinh, cosh, tanh
//   Exponential:   exp, log, sqrt
//   Power:         pow(base, exponent)

template <Symbolic S>
constexpr auto sin(S s) {
  return Expression<SinOp, S>{s};
}

template <Symbolic S>
constexpr auto cos(S s) {
  return Expression<CosOp, S>{s};
}

template <Symbolic S>
constexpr auto tan(S s) {
  return Expression<TanOp, S>{s};
}

template <Symbolic S>
constexpr auto sinh(S s) {
  return Expression<SinhOp, S>{s};
}

template <Symbolic S>
constexpr auto cosh(S s) {
  return Expression<CoshOp, S>{s};
}

template <Symbolic S>
constexpr auto tanh(S s) {
  return Expression<TanhOp, S>{s};
}

template <Symbolic S>
constexpr auto exp(S s) {
  return Expression<ExpOp, S>{s};
}

template <Symbolic S>
constexpr auto log(S s) {
  return Expression<LogOp, S>{s};
}

template <Symbolic S>
constexpr auto sqrt(S s) {
  return Expression<SqrtOp, S>{s};
}

template <Symbolic L, Symbolic R>
constexpr auto pow(L lhs, R rhs) {
  return Expression<PowOp, L, R>{lhs, rhs};
}

// ============================================================================
// Mathematical Constants
// ============================================================================
//
// π and e are represented as nullary expressions (zero children).
// They're symbolic values that can be manipulated and differentiated.
// During evaluation, PiOp{}() and EOp{}() return their numeric values.

inline constexpr Expression<PiOp> pi{};
inline constexpr Expression<EOp> e{};

// ============================================================================
// Convenience Constant Helpers
// ============================================================================
//
// Pre-defined symbolic constants for common values.
// Use these instead of writing Constant<0>{} or Constant<1>{} everywhere.
//
// The template c<V> creates any compile-time constant: c<42>, c<3.14>, etc.

template <auto V>
inline constexpr Constant<V> c{};

inline constexpr Constant<0> zero{};
inline constexpr Constant<1> one{};
inline constexpr Constant<2> two{};
inline constexpr Constant<-1> neg_one{};

// ============================================================================
// Literal Constructor
// ============================================================================
//
// lit(value) creates a Literal from a runtime value.
// Use this when you have a value that isn't known at compile time.
//
// Example:
//   double runtime_value = read_from_file();
//   auto expr = x + lit(runtime_value);  // Embeds runtime_value in the expression
//
// Note: Unlike Constant<V>, Literal carries its value at runtime in the
// Intrinsic strategy. The expression type doesn't encode the value.

template <typename T>
constexpr auto lit(T value) {
  return Literal<T>{Intrinsic<T>{value}};
}

}  // namespace tempura::symbolic4
