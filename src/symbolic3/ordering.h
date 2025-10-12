#pragma once

#include "meta/containers.h"
#include "meta/type_id.h"
#include "meta/type_list.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"

// Total ordering for symbolic expressions to establish canonical forms.
// This is critical for preventing infinite rewrite loops when using
// ordering-based rules like: x + y → y + x iff y < x
//
// ZERO STL DEPENDENCIES - Pure compile-time metaprogramming

namespace tempura::symbolic3 {

enum class Ordering : char { Less, Equal, Greater };

// ============================================================================
// Operator Precedence Ordering
// ============================================================================

template <typename LhsOp, typename RhsOp>
constexpr auto compareOps(LhsOp, RhsOp) -> Ordering {
  constexpr static MinimalArray kOpOrder{
      kMeta<AddOp>, kMeta<SubOp>,  kMeta<MulOp>, kMeta<DivOp>, kMeta<NegOp>,
      kMeta<PowOp>, kMeta<SqrtOp>, kMeta<ExpOp>, kMeta<LogOp>, kMeta<SinOp>,
      kMeta<CosOp>, kMeta<TanOp>,  kMeta<EOp>,   kMeta<PiOp>,  kMeta<EqOp>,
      kMeta<NeqOp>, kMeta<LtOp>,   kMeta<GtOp>,  kMeta<AndOp>, kMeta<OrOp>,
      kMeta<NotOp>,
  };

  constexpr auto getIndex = [](TypeId id) -> SizeT {
    for (SizeT i = 0; i < kOpOrder.size(); ++i) {
      if (kOpOrder[i] == id) return i;
    }
    return kOpOrder.size();
  };

  constexpr SizeT lhs_idx = getIndex(kMeta<LhsOp>);
  constexpr SizeT rhs_idx = getIndex(kMeta<RhsOp>);

  if constexpr (lhs_idx < rhs_idx)
    return Ordering::Less;
  else if constexpr (lhs_idx > rhs_idx)
    return Ordering::Greater;
  else
    return Ordering::Equal;
}

// ============================================================================
// Forward declaration
// ============================================================================

template <Symbolic Lhs, Symbolic Rhs>
constexpr auto compare(Lhs lhs, Rhs rhs) -> Ordering;

// ============================================================================
// Compare Constants
// ============================================================================

template <auto LhsVal, auto RhsVal>
constexpr auto compareConstants(Constant<LhsVal>, Constant<RhsVal>)
    -> Ordering {
  if constexpr (LhsVal < RhsVal)
    return Ordering::Less;
  else if constexpr (LhsVal > RhsVal)
    return Ordering::Greater;
  else
    return Ordering::Equal;
}

// ============================================================================
// Compare Fractions
// ============================================================================

template <long long LhsN, long long LhsD, long long RhsN, long long RhsD>
constexpr auto compareFractions(Fraction<LhsN, LhsD>, Fraction<RhsN, RhsD>)
    -> Ordering {
  // Compare via cross-multiplication: a/b < c/d iff a*d < c*b
  // (assuming positive denominators after normalization)
  constexpr auto lhs_cross = LhsN * RhsD;
  constexpr auto rhs_cross = RhsN * LhsD;

  if constexpr (lhs_cross < rhs_cross)
    return Ordering::Less;
  else if constexpr (lhs_cross > rhs_cross)
    return Ordering::Greater;
  else
    return Ordering::Equal;
}

// ============================================================================
// Compare Symbols
// ============================================================================

template <typename LhsUnique, typename RhsUnique>
constexpr auto compareSymbols(Symbol<LhsUnique>, Symbol<RhsUnique>)
    -> Ordering {
  constexpr auto lhs_id = kMeta<Symbol<LhsUnique>>;
  constexpr auto rhs_id = kMeta<Symbol<RhsUnique>>;

  if constexpr (lhs_id < rhs_id)
    return Ordering::Less;
  else if constexpr (lhs_id > rhs_id)
    return Ordering::Greater;
  else
    return Ordering::Equal;
}

// ============================================================================
// Compare Expression Arguments (STL-free recursive comparison)
// ============================================================================

namespace detail {
// Base case: empty lists
constexpr auto compareArgLists(TypeList<>, TypeList<>) -> Ordering {
  return Ordering::Equal;
}

// Different sizes
template <typename... LhsArgs>
constexpr auto compareArgLists(TypeList<LhsArgs...>, TypeList<>) -> Ordering {
  return Ordering::Greater;
}

template <typename... RhsArgs>
constexpr auto compareArgLists(TypeList<>, TypeList<RhsArgs...>) -> Ordering {
  return Ordering::Less;
}

// Recursive case: compare first, then rest
template <typename LhsFirst, typename... LhsRest, typename RhsFirst,
          typename... RhsRest>
constexpr auto compareArgLists(TypeList<LhsFirst, LhsRest...>,
                               TypeList<RhsFirst, RhsRest...>) -> Ordering {
  constexpr auto first_cmp = compare(LhsFirst{}, RhsFirst{});
  if constexpr (first_cmp != Ordering::Equal) {
    return first_cmp;
  } else {
    return compareArgLists(TypeList<LhsRest...>{}, TypeList<RhsRest...>{});
  }
}
}  // namespace detail

// ============================================================================
// Helper for Comparing Expressions (Forward Declaration)
// ============================================================================

template <Symbolic Lhs, Symbolic Rhs>
constexpr auto compare(Lhs lhs, Rhs rhs) -> Ordering;

// Helper for comparing expressions (template specialization to extract args)
template <typename LhsExpr, typename RhsExpr>
constexpr auto compareExprs() -> Ordering;

template <typename LhsOp, Symbolic... LhsArgs, typename RhsOp,
          Symbolic... RhsArgs>
constexpr auto compareExprs(Expression<LhsOp, LhsArgs...>*,
                            Expression<RhsOp, RhsArgs...>*) -> Ordering {
  // First compare operators
  constexpr auto op_cmp = compareOps(LhsOp{}, RhsOp{});
  if constexpr (op_cmp != Ordering::Equal) {
    return op_cmp;
  } else {
    // Same operator: compare arguments
    return detail::compareArgLists(TypeList<LhsArgs...>{},
                                   TypeList<RhsArgs...>{});
  }
}

template <typename LhsExpr, typename RhsExpr>
constexpr auto compareExprs() -> Ordering {
  return compareExprs((LhsExpr*)nullptr, (RhsExpr*)nullptr);
}

// ============================================================================
// Main Comparison Function
// ============================================================================

template <Symbolic Lhs, Symbolic Rhs>
constexpr auto compare(Lhs lhs, Rhs rhs) -> Ordering {
  // Category ordering: Expression < Symbol < Fraction < Constant
  // Note: is_constant, is_symbol, is_expression, is_fraction are defined in
  // core.h
  constexpr bool lhs_is_const = is_constant<Lhs>;
  constexpr bool rhs_is_const = is_constant<Rhs>;
  constexpr bool lhs_is_frac = is_fraction<Lhs>;
  constexpr bool rhs_is_frac = is_fraction<Rhs>;
  constexpr bool lhs_is_sym = is_symbol<Lhs>;
  constexpr bool rhs_is_sym = is_symbol<Rhs>;
  constexpr bool lhs_is_expr = is_expression<Lhs>;
  constexpr bool rhs_is_expr = is_expression<Rhs>;

  // Different categories
  if constexpr (lhs_is_expr && !rhs_is_expr) {
    return Ordering::Less;
  } else if constexpr (!lhs_is_expr && rhs_is_expr) {
    return Ordering::Greater;
  } else if constexpr (lhs_is_sym && (rhs_is_frac || rhs_is_const)) {
    return Ordering::Less;
  } else if constexpr ((lhs_is_frac || lhs_is_const) && rhs_is_sym) {
    return Ordering::Greater;
  } else if constexpr (lhs_is_frac && rhs_is_const) {
    return Ordering::Less;
  } else if constexpr (lhs_is_const && rhs_is_frac) {
    return Ordering::Greater;
  }
  // Same category: compare within category
  else if constexpr (lhs_is_const && rhs_is_const) {
    return compareConstants(lhs, rhs);
  } else if constexpr (lhs_is_frac && rhs_is_frac) {
    return compareFractions(lhs, rhs);
  } else if constexpr (lhs_is_sym && rhs_is_sym) {
    return compareSymbols(lhs, rhs);
  } else if constexpr (lhs_is_expr && rhs_is_expr) {
    // Both expressions: compare operator first
    return compareExprs<Lhs, Rhs>();
  } else {
    return Ordering::Equal;
  }
}

// ============================================================================
// Convenience Predicates
// ============================================================================

template <Symbolic Lhs, Symbolic Rhs>
constexpr bool lessThan(Lhs lhs, Rhs rhs) {
  return compare(lhs, rhs) == Ordering::Less;
}

template <Symbolic Lhs, Symbolic Rhs>
constexpr bool greaterThan(Lhs lhs, Rhs rhs) {
  return compare(lhs, rhs) == Ordering::Greater;
}

template <Symbolic Lhs, Symbolic Rhs>
constexpr bool symbolicEqual(Lhs lhs, Rhs rhs) {
  return compare(lhs, rhs) == Ordering::Equal;
}

}  // namespace tempura::symbolic3
