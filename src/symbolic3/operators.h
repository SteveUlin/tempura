#pragma once

#include <cmath>

#include "meta/function_objects.h"
#include "symbolic3/core.h"

// Symbolic expression builder functions and operator overloads
// Operators themselves are defined in meta/function_objects.h
// Display metadata is in operator_display.h

namespace tempura::symbolic3 {

// Import operator types from tempura namespace
using tempura::AddOp;
using tempura::SubOp;
using tempura::MulOp;
using tempura::DivOp;
using tempura::PowOp;
using tempura::NegOp;
using tempura::SinOp;
using tempura::CosOp;
using tempura::TanOp;
using tempura::SinhOp;
using tempura::CoshOp;
using tempura::TanhOp;
using tempura::ExpOp;
using tempura::LogOp;
using tempura::SqrtOp;
using tempura::PiOp;
using tempura::EOp;

// Comparison operations (empty placeholders for now)
struct EqOp {};
struct NeqOp {};
struct LtOp {};
struct LeOp {};
struct GtOp {};
struct GeOp {};

// Logical operations (empty placeholders for now)
struct AndOp {};
struct OrOp {};
struct NotOp {};

// ============================================================================
// Binary Operations - Operator Overloads for Symbolic Expressions
// ============================================================================

// Addition - binary operation (canonical form will be applied by strategies)
template <Symbolic L, Symbolic R>
constexpr auto operator+(L, R) {
  return Expression<AddOp, L, R>{};
}

template <Symbolic L, Symbolic R>
constexpr auto operator-(L, R) {
  return Expression<SubOp, L, R>{};
}

// Multiplication - binary operation (canonical form will be applied by
// strategies)
template <Symbolic L, Symbolic R>
constexpr auto operator*(L, R) {
  return Expression<MulOp, L, R>{};
}

template <Symbolic L, Symbolic R>
constexpr auto operator/(L, R) {
  return Expression<DivOp, L, R>{};
}

// ============================================================================
// Unary Operations
// ============================================================================

template <Symbolic S>
constexpr auto operator-(S) {
  return Expression<NegOp, S>{};
}

// ============================================================================
// Transcendental Functions
// ============================================================================

template <Symbolic S>
constexpr auto sin(S) {
  return Expression<SinOp, S>{};
}

template <Symbolic S>
constexpr auto cos(S) {
  return Expression<CosOp, S>{};
}

template <Symbolic S>
constexpr auto tan(S) {
  return Expression<TanOp, S>{};
}

template <Symbolic S>
constexpr auto sinh(S) {
  return Expression<SinhOp, S>{};
}

template <Symbolic S>
constexpr auto cosh(S) {
  return Expression<CoshOp, S>{};
}

template <Symbolic S>
constexpr auto tanh(S) {
  return Expression<TanhOp, S>{};
}

template <Symbolic S>
constexpr auto exp(S) {
  return Expression<ExpOp, S>{};
}

template <Symbolic S>
constexpr auto log(S) {
  return Expression<LogOp, S>{};
}

template <Symbolic S>
constexpr auto sqrt(S) {
  return Expression<SqrtOp, S>{};
}

template <Symbolic L, Symbolic R>
constexpr auto pow(L, R) {
  return Expression<PowOp, L, R>{};
}

// ============================================================================
// Convenience: Constant Helpers
// ============================================================================

// Use template variables for common constants
template <int64_t val>
constexpr Constant<val> c{};

template <auto val>
constexpr Constant<val> constant{};

// Common mathematical constants
constexpr Constant<0> zero_c{};
constexpr Constant<1> one_c{};
constexpr Constant<2> two_c{};
constexpr Constant<-1> neg_one_c{};

// Mathematical constants (as zero-arg expressions)
constexpr Expression<PiOp> π{};
constexpr Expression<PiOp> pi{};  // ASCII alias
constexpr Expression<EOp> e{};

// ============================================================================
// Derived Constants - useful for probability distributions
// ============================================================================
//
// These are symbolic expressions built from primitives. They simplify
// correctly and evaluate to the expected numeric values.

// log(2) - used in HalfNormal, etc.
inline constexpr auto log2 = log(Constant<2>{});

// log(π) - used in Cauchy, etc.
inline constexpr auto logPi = log(π);

// log(2π) = log(2) + log(π)
inline constexpr auto log2Pi = log(Constant<2>{} * π);

// log(√(2π)) = ½ log(2π)
inline constexpr auto logSqrt2Pi = Fraction<1, 2>{} * log(Constant<2>{} * π);

// ============================================================================
// Special Functions - symbolic placeholders for transcendental functions
// ============================================================================
//
// These functions don't have closed-form symbolic derivatives in general,
// but we can still represent them symbolically and evaluate them numerically.

// Operator types for special functions
struct LogGammaOp {
  // lgamma(x) - log of gamma function
  template <typename T>
  constexpr auto operator()(T x) const {
    return std::lgamma(static_cast<double>(x));
  }
};

struct LogBetaOp {
  // log(B(a,b)) = lgamma(a) + lgamma(b) - lgamma(a+b)
  template <typename T, typename U>
  constexpr auto operator()(T a, U b) const {
    return std::lgamma(static_cast<double>(a)) +
           std::lgamma(static_cast<double>(b)) -
           std::lgamma(static_cast<double>(a) + static_cast<double>(b));
  }
};

// Symbolic log-gamma function: logΓ(x)
template <Symbolic S>
constexpr auto logGammaFn(S) {
  return Expression<LogGammaOp, S>{};
}

// Symbolic log-beta function: log B(α, β)
template <Symbolic A, Symbolic B>
constexpr auto logBetaFn(A, B) {
  return Expression<LogBetaOp, A, B>{};
}

// ============================================================================
// Type Predicates for Operations
// ============================================================================

template <typename T>
constexpr bool is_add = false;

template <Symbolic L, Symbolic R>
constexpr bool is_add<Expression<AddOp, L, R>> = true;

template <typename T>
constexpr bool is_mul = false;

template <Symbolic L, Symbolic R>
constexpr bool is_mul<Expression<MulOp, L, R>> = true;

template <typename T>
constexpr bool is_trig_function = false;

template <Symbolic S>
constexpr bool is_trig_function<Expression<SinOp, S>> = true;

template <Symbolic S>
constexpr bool is_trig_function<Expression<CosOp, S>> = true;

template <Symbolic S>
constexpr bool is_trig_function<Expression<TanOp, S>> = true;

template <typename T>
constexpr bool is_transcendental = is_trig_function<T>;

template <Symbolic S>
constexpr bool is_transcendental<Expression<ExpOp, S>> = true;

template <Symbolic S>
constexpr bool is_transcendental<Expression<LogOp, S>> = true;

}  // namespace tempura::symbolic3
