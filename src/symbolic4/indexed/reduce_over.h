#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "symbolic4/core.h"
#include "symbolic4/operators.h"

// ============================================================================
// reduce_over.h - Generalized symbolic reduction over a dimension
// ============================================================================
//
// ReduceOver<ReduceOp, DimTag, Expr> represents a monoidal fold: ⊕ᵢ expr[i]
// where i ranges over DimTag and ⊕ is the reduction operation.
//
// Each ReduceOp defines the monoid: identity(), combine(a, b), symbol().
// The algebra guarantees associativity, enabling parallel/tree reductions.
//
// SumOver<D, E> is a backwards-compatible alias for ReduceOver<SumReduce, D, E>.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Reduction operation traits
// ============================================================================

struct SumReduce {
  static constexpr double identity() { return 0.0; }
  static constexpr double combine(double a, double b) { return a + b; }
  static constexpr const char* symbol() { return "Σ"; }
};

struct ProdReduce {
  static constexpr double identity() { return 1.0; }
  static constexpr double combine(double a, double b) { return a * b; }
  static constexpr const char* symbol() { return "Π"; }
};

struct MaxReduce {
  static constexpr double identity() { return -std::numeric_limits<double>::infinity(); }
  static constexpr double combine(double a, double b) { return std::max(a, b); }
  static constexpr const char* symbol() { return "Max"; }
};

struct LogSumExpReduce {
  static constexpr double identity() { return -std::numeric_limits<double>::infinity(); }
  static constexpr double combine(double a, double b) {
    double m = std::max(a, b);
    return m + std::log(std::exp(a - m) + std::exp(b - m));
  }
  static constexpr const char* symbol() { return "LogSumExp"; }
};

// ============================================================================
// ReduceOver - Unified reduction node
// ============================================================================

template <typename ReduceOp, typename DimTag, Symbolic Expr>
struct ReduceOver : SymbolicTag {
  using reduce_op = ReduceOp;
  using dim_tag = DimTag;
  using expr_type = Expr;

  [[no_unique_address]] Expr expr_;

  constexpr ReduceOver() = default;
  constexpr explicit ReduceOver(Expr e) : expr_{e} {}

  constexpr auto expr() const { return expr_; }

  template <Symbolic NewExpr>
  static constexpr auto rebuild(NewExpr new_expr) {
    return ReduceOver<ReduceOp, DimTag, NewExpr>{new_expr};
  }
};

// ============================================================================
// Type traits
// ============================================================================

template <typename T>
constexpr bool is_reduce_over_v = core_traits_detail::isSpecOf<T, ReduceOver>();

template <typename T> requires is_reduce_over_v<T>
using reduce_over_op_t = [:std::meta::template_arguments_of(^^T)[0]:];

template <typename T> requires is_reduce_over_v<T>
using reduce_over_dim_tag_t = [:std::meta::template_arguments_of(^^T)[1]:];

// ============================================================================
// Backwards-compatible aliases
// ============================================================================

template <typename DimTag, Symbolic Expr>
using SumOver = ReduceOver<SumReduce, DimTag, Expr>;

// Backwards-compatible is_sum_over_v — matches SumOver (= ReduceOver<SumReduce, ...>)
template <typename T>
constexpr bool is_sum_over_v = [] consteval {
    if constexpr (!is_reduce_over_v<T>) return false;
    else return std::is_same_v<reduce_over_op_t<T>, SumReduce>;
}();

// Extract dimension tag from SumOver (backwards compat)
template <typename T> requires is_sum_over_v<T>
using sum_over_dim_tag_t = reduce_over_dim_tag_t<T>;

// ============================================================================
// Named reduction op instances for reduce(expr, dim, op) syntax
// ============================================================================

inline constexpr SumReduce sum{};
inline constexpr ProdReduce prod{};
inline constexpr MaxReduce max_reduce{};
inline constexpr LogSumExpReduce log_sum_exp{};

// ============================================================================
// Value-based reduction factories
// ============================================================================
//
// Unified: reduce(expr, dim, op) — works with any ReduceOp and any dim type
//   auto total = reduce(x * x, obs, sum);
//   auto product = reduce(p, obs, prod);
//
// Convenience: sumOver(expr, dim), prodOver(expr, dim), etc.
//   auto lp = sumOver(logProb, obs);

template <Symbolic Expr, typename D, typename ROp>
constexpr auto reduce(Expr expr, D, ROp) {
  return ReduceOver<ROp, D, Expr>{expr};
}

template <Symbolic Expr, typename D>
constexpr auto sumOver(Expr expr, D) {
  return ReduceOver<SumReduce, D, Expr>{expr};
}

template <Symbolic Expr, typename D>
constexpr auto prodOver(Expr expr, D) {
  return ReduceOver<ProdReduce, D, Expr>{expr};
}

template <Symbolic Expr, typename D>
constexpr auto maxOver(Expr expr, D) {
  return ReduceOver<MaxReduce, D, Expr>{expr};
}

template <Symbolic Expr, typename D>
constexpr auto logSumExpOver(Expr expr, D) {
  return ReduceOver<LogSumExpReduce, D, Expr>{expr};
}

// ============================================================================
// Arithmetic with ReduceOver
// ============================================================================
//
// ReduceOver can be combined with other expressions or other ReduceOvers.
// Generalizes the operator overloads that sum_over.h only had for +.

// ReduceOver op Symbolic
template <typename ROp, typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator+(ReduceOver<ROp, DimTag, E1> red, E2 other) {
  return Expression<AddOp, ReduceOver<ROp, DimTag, E1>, E2>{red, other};
}

template <typename ROp, typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator+(E1 other, ReduceOver<ROp, DimTag, E2> red) {
  return Expression<AddOp, E1, ReduceOver<ROp, DimTag, E2>>{other, red};
}

template <typename ROp, typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator-(ReduceOver<ROp, DimTag, E1> red, E2 other) {
  return Expression<SubOp, ReduceOver<ROp, DimTag, E1>, E2>{red, other};
}

template <typename ROp, typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator-(E1 other, ReduceOver<ROp, DimTag, E2> red) {
  return Expression<SubOp, E1, ReduceOver<ROp, DimTag, E2>>{other, red};
}

template <typename ROp, typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator*(ReduceOver<ROp, DimTag, E1> red, E2 other) {
  return Expression<MulOp, ReduceOver<ROp, DimTag, E1>, E2>{red, other};
}

template <typename ROp, typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator*(E1 other, ReduceOver<ROp, DimTag, E2> red) {
  return Expression<MulOp, E1, ReduceOver<ROp, DimTag, E2>>{other, red};
}

template <typename ROp, typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator/(ReduceOver<ROp, DimTag, E1> red, E2 other) {
  return Expression<DivOp, ReduceOver<ROp, DimTag, E1>, E2>{red, other};
}

template <typename ROp, typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator/(E1 other, ReduceOver<ROp, DimTag, E2> red) {
  return Expression<DivOp, E1, ReduceOver<ROp, DimTag, E2>>{other, red};
}

// ReduceOver op ReduceOver
template <typename ROp1, typename D1, Symbolic E1, typename ROp2, typename D2, Symbolic E2>
constexpr auto operator+(ReduceOver<ROp1, D1, E1> lhs, ReduceOver<ROp2, D2, E2> rhs) {
  return Expression<AddOp, ReduceOver<ROp1, D1, E1>, ReduceOver<ROp2, D2, E2>>{lhs, rhs};
}

template <typename ROp1, typename D1, Symbolic E1, typename ROp2, typename D2, Symbolic E2>
constexpr auto operator-(ReduceOver<ROp1, D1, E1> lhs, ReduceOver<ROp2, D2, E2> rhs) {
  return Expression<SubOp, ReduceOver<ROp1, D1, E1>, ReduceOver<ROp2, D2, E2>>{lhs, rhs};
}

template <typename ROp1, typename D1, Symbolic E1, typename ROp2, typename D2, Symbolic E2>
constexpr auto operator*(ReduceOver<ROp1, D1, E1> lhs, ReduceOver<ROp2, D2, E2> rhs) {
  return Expression<MulOp, ReduceOver<ROp1, D1, E1>, ReduceOver<ROp2, D2, E2>>{lhs, rhs};
}

template <typename ROp1, typename D1, Symbolic E1, typename ROp2, typename D2, Symbolic E2>
constexpr auto operator/(ReduceOver<ROp1, D1, E1> lhs, ReduceOver<ROp2, D2, E2> rhs) {
  return Expression<DivOp, ReduceOver<ROp1, D1, E1>, ReduceOver<ROp2, D2, E2>>{lhs, rhs};
}

// Unary negation
template <typename ROp, typename DimTag, Symbolic E>
constexpr auto operator-(ReduceOver<ROp, DimTag, E> red) {
  return Expression<NegOp, ReduceOver<ROp, DimTag, E>>{red};
}

}  // namespace tempura::symbolic4
