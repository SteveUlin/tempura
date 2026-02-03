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
using tempura::AbsOp;
using tempura::AcosOp;
using tempura::AddOp;
using tempura::AsinOp;
using tempura::AtanOp;
using tempura::CbrtOp;
using tempura::CeilOp;
using tempura::CosOp;
using tempura::CoshOp;
using tempura::DigammaOp;
using tempura::DivOp;
using tempura::EOp;
using tempura::ErfcOp;
using tempura::ErfOp;
using tempura::Exp2Op;
using tempura::ExpOp;
using tempura::Expm1Op;
using tempura::FloorOp;
using tempura::HypotOp;
using tempura::LgammaOp;
using tempura::Log10Op;
using tempura::Log1pOp;
using tempura::Log2Op;
using tempura::LogOp;
using tempura::MaxOp;
using tempura::MinOp;
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
// SymbolicLike conversion infrastructure
// ============================================================================
//
// Types like RandomVar and IndexedData can be used directly in expressions.
// They're automatically converted to their symbolic forms via asSymbolic().
//
// Conversion priorities:
//   1. Already Symbolic → return as-is
//   2. Has sym() method → call sym() (IndexedData, old-style types)
//   3. Has constrainedExpr() static method → call it (RandomVar)
//
// This enables natural syntax:
//   auto alpha = normal(0, 10);  // RandomVar
//   auto x = data<Obs>();        // IndexedData
//   auto mu = alpha + x;         // Works! Converts automatically

// Check if T has a sym() method returning Symbolic
template <typename T>
concept HasSymMethod = requires(const T& t) {
  { t.sym() } -> Symbolic;
};

// Check if T has a constrainedExpr() method (static or instance) returning Symbolic
template <typename T>
concept HasConstrainedExpr = requires(const T& t) {
  { t.constrainedExpr() } -> Symbolic;
};

// Combined: types that can participate in symbolic expressions
template <typename T>
concept SymbolicLike = Symbolic<T> || HasSymMethod<T> || HasConstrainedExpr<T>;

// Helper to get the symbolic form of a value
// Priority:
//   1. Already Symbolic → return as-is (preserves Sample atoms, Expressions, etc.)
//   2. Has constrainedExpr() → call it (RandomVar types - returns Sample atom)
//   3. Has sym() → call it (IndexedData)
template <typename T>
constexpr auto asSymbolic(const T& x) {
  if constexpr (Symbolic<T>) {
    // Already a valid symbolic expression (Atom, Expression, etc.)
    // This preserves Sample atoms which are needed for auto-discovery
    return x;
  } else if constexpr (HasConstrainedExpr<T>) {
    // RandomVar - returns Sample atom for auto-discovery
    // Constraint transform is applied during evaluation
    return x.constrainedExpr();
  } else if constexpr (HasSymMethod<T>) {
    // IndexedData and similar - convert to their symbol type
    return x.sym();
  } else {
    static_assert(Symbolic<T> || HasConstrainedExpr<T> || HasSymMethod<T>,
                  "Type cannot be converted to Symbolic");
  }
}

// ============================================================================
// Binary Operations
// ============================================================================
//
// Each operator creates an Expression node with the corresponding Op type.
// The expression preserves both operands as template arguments, encoding
// the full tree structure in the type system.
//
// Operators accept any SymbolicLike type and auto-convert via asSymbolic().
// This enables natural syntax like:
//   auto alpha = normal(0, 10);  // RandomVar
//   auto beta = normal(0, 5);    // RandomVar
//   auto x = data<Obs>();        // IndexedData
//   auto mu = alpha + beta * x;  // Works without *alpha, *beta, x.sym()
//
// Associativity: These operators are left-associative by C++ precedence rules.
//   a + b + c parses as (a + b) + c → Expression<AddOp, Expression<AddOp, a, b>, c>

template <SymbolicLike L, SymbolicLike R>
constexpr auto operator+(L lhs, R rhs) {
  auto l = asSymbolic(lhs);
  auto r = asSymbolic(rhs);
  return Expression<AddOp, decltype(l), decltype(r)>{l, r};
}

template <SymbolicLike L, SymbolicLike R>
constexpr auto operator-(L lhs, R rhs) {
  auto l = asSymbolic(lhs);
  auto r = asSymbolic(rhs);
  return Expression<SubOp, decltype(l), decltype(r)>{l, r};
}

template <SymbolicLike L, SymbolicLike R>
constexpr auto operator*(L lhs, R rhs) {
  auto l = asSymbolic(lhs);
  auto r = asSymbolic(rhs);
  return Expression<MulOp, decltype(l), decltype(r)>{l, r};
}

template <SymbolicLike L, SymbolicLike R>
constexpr auto operator/(L lhs, R rhs) {
  auto l = asSymbolic(lhs);
  auto r = asSymbolic(rhs);
  return Expression<DivOp, decltype(l), decltype(r)>{l, r};
}

// ============================================================================
// Unary Operations
// ============================================================================
//
// Unary negation. Note: unary + is intentionally not overloaded (it's a no-op).

template <SymbolicLike S>
constexpr auto operator-(S s) {
  auto sym = asSymbolic(s);
  return Expression<NegOp, decltype(sym)>{sym};
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

template <SymbolicLike S>
constexpr auto sin(S s) {
  auto sym = asSymbolic(s);
  return Expression<SinOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto cos(S s) {
  auto sym = asSymbolic(s);
  return Expression<CosOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto tan(S s) {
  auto sym = asSymbolic(s);
  return Expression<TanOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto asin(S s) {
  auto sym = asSymbolic(s);
  return Expression<AsinOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto acos(S s) {
  auto sym = asSymbolic(s);
  return Expression<AcosOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto atan(S s) {
  auto sym = asSymbolic(s);
  return Expression<AtanOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto sinh(S s) {
  auto sym = asSymbolic(s);
  return Expression<SinhOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto cosh(S s) {
  auto sym = asSymbolic(s);
  return Expression<CoshOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto tanh(S s) {
  auto sym = asSymbolic(s);
  return Expression<TanhOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto exp(S s) {
  auto sym = asSymbolic(s);
  return Expression<ExpOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto log(S s) {
  auto sym = asSymbolic(s);
  return Expression<LogOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto sqrt(S s) {
  auto sym = asSymbolic(s);
  return Expression<SqrtOp, decltype(sym)>{sym};
}

template <SymbolicLike L, SymbolicLike R>
constexpr auto pow(L lhs, R rhs) {
  auto l = asSymbolic(lhs);
  auto r = asSymbolic(rhs);
  return Expression<PowOp, decltype(l), decltype(r)>{l, r};
}

// ============================================================================
// Special Functions (for probability distributions)
// ============================================================================
//
// These functions are essential for computing normalized log-probabilities
// in distributions like Gamma, Beta, Dirichlet, Binomial, etc.

template <SymbolicLike S>
constexpr auto lgamma(S s) {
  auto sym = asSymbolic(s);
  return Expression<LgammaOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto digamma(S s) {
  auto sym = asSymbolic(s);
  return Expression<DigammaOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto erf(S s) {
  auto sym = asSymbolic(s);
  return Expression<ErfOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto erfc(S s) {
  auto sym = asSymbolic(s);
  return Expression<ErfcOp, decltype(sym)>{sym};
}

// ============================================================================
// Numerical Stability Functions
// ============================================================================

template <SymbolicLike S>
constexpr auto log1p(S s) {
  auto sym = asSymbolic(s);
  return Expression<Log1pOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto expm1(S s) {
  auto sym = asSymbolic(s);
  return Expression<Expm1Op, decltype(sym)>{sym};
}

// ============================================================================
// Additional Math Functions
// ============================================================================

template <SymbolicLike S>
constexpr auto abs(S s) {
  auto sym = asSymbolic(s);
  return Expression<AbsOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto floor(S s) {
  auto sym = asSymbolic(s);
  return Expression<FloorOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto ceil(S s) {
  auto sym = asSymbolic(s);
  return Expression<CeilOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto cbrt(S s) {
  auto sym = asSymbolic(s);
  return Expression<CbrtOp, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto log10(S s) {
  auto sym = asSymbolic(s);
  return Expression<Log10Op, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto log2(S s) {
  auto sym = asSymbolic(s);
  return Expression<Log2Op, decltype(sym)>{sym};
}

template <SymbolicLike S>
constexpr auto exp2(S s) {
  auto sym = asSymbolic(s);
  return Expression<Exp2Op, decltype(sym)>{sym};
}

template <SymbolicLike L, SymbolicLike R>
constexpr auto hypot(L lhs, R rhs) {
  auto l = asSymbolic(lhs);
  auto r = asSymbolic(rhs);
  return Expression<HypotOp, decltype(l), decltype(r)>{l, r};
}

template <SymbolicLike L, SymbolicLike R>
constexpr auto max(L lhs, R rhs) {
  auto l = asSymbolic(lhs);
  auto r = asSymbolic(rhs);
  return Expression<MaxOp, decltype(l), decltype(r)>{l, r};
}

template <SymbolicLike L, SymbolicLike R>
constexpr auto min(L lhs, R rhs) {
  auto l = asSymbolic(lhs);
  auto r = asSymbolic(rhs);
  return Expression<MinOp, decltype(l), decltype(r)>{l, r};
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
// The template c<V> creates any compile-time constant: c<42>, c<3.14>, etc.
// Use the _c literal suffix for cleaner syntax: 42_c, 3.14_c

template <auto V>
inline constexpr Constant<V> c{};

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
// Embedded effect. The expression type doesn't encode the value.

template <typename T>
constexpr auto lit(T value) {
  return Literal<T>{Embedded<T>{value}};
}

}  // namespace tempura::symbolic4
