#pragma once

#include "symbolic4/core.h"
#include "symbolic4/operators.h"

// ============================================================================
// sum_over.h - Symbolic summation over a dimension
// ============================================================================
//
// SumOver<DimTag, Expr> represents a symbolic sum: Σᵢ expr[i]
// where i ranges over the dimension DimTag.
//
// The sum is deferred until evaluation - this is just a symbolic node.
// This enables:
//   1. Differentiation to push through: diff(SumOver<D, E>, x) → SumOver<D, diff(E, x)>
//   2. Combined evaluation of indexed + scalar expressions
//
// Example:
//   auto lp = sumOver<Observations>(logBeta(theta, alpha, beta));
//   // lp represents Σᵢ log Beta(θ[i] | α, β)
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// SumOver - Symbolic sum over a dimension
// ============================================================================

template <typename DimTag, Symbolic Expr>
struct SumOver : SymbolicTag {
  using dim_tag = DimTag;
  using expr_type = Expr;

  [[no_unique_address]] Expr expr_;

  constexpr SumOver() = default;
  constexpr explicit SumOver(Expr e) : expr_{e} {}

  constexpr auto expr() const { return expr_; }

  // Rebuild with a new expression type (used by diff.h for differentiation)
  template <Symbolic NewExpr>
  static constexpr auto rebuild(NewExpr new_expr) {
    return SumOver<DimTag, NewExpr>{new_expr};
  }
};

// Factory function
template <typename DimTag, Symbolic Expr>
constexpr auto sumOver(Expr expr) {
  return SumOver<DimTag, Expr>{expr};
}

// Type traits
template <typename T>
struct IsSumOver : std::false_type {};

template <typename DimTag, typename Expr>
struct IsSumOver<SumOver<DimTag, Expr>> : std::true_type {};

template <typename T>
constexpr bool is_sum_over_v = IsSumOver<T>::value;

// Extract dimension tag from SumOver
template <typename T>
struct SumOverDimTag;

template <typename DimTag, typename Expr>
struct SumOverDimTag<SumOver<DimTag, Expr>> {
  using type = DimTag;
};

template <typename T>
using sum_over_dim_tag_t = typename SumOverDimTag<T>::type;

// ============================================================================
// Arithmetic with SumOver
// ============================================================================
//
// SumOver can be combined with other expressions using standard operators.
// This produces Expression nodes that will be evaluated appropriately.

// SumOver + Symbolic → Expression
template <typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator+(SumOver<DimTag, E1> sum, E2 other) {
  return Expression<AddOp, SumOver<DimTag, E1>, E2>{sum, other};
}

template <typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator+(E1 other, SumOver<DimTag, E2> sum) {
  return Expression<AddOp, E1, SumOver<DimTag, E2>>{other, sum};
}

// SumOver + SumOver → Expression
template <typename DimTag1, Symbolic E1, typename DimTag2, Symbolic E2>
constexpr auto operator+(SumOver<DimTag1, E1> lhs, SumOver<DimTag2, E2> rhs) {
  return Expression<AddOp, SumOver<DimTag1, E1>, SumOver<DimTag2, E2>>{lhs, rhs};
}

}  // namespace tempura::symbolic4
