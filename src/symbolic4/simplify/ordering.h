#pragma once

#include "meta/type_id.h"
#include "symbolic4/core.h"
#include "symbolic4/operators.h"

// ============================================================================
// ordering.h - Total ordering for symbolic expressions
// ============================================================================
//
// Establishes a strict total order on symbolic expression TYPES. This enables:
//   - Canonical forms for commutative operations
//   - Detecting equivalent subexpressions
//   - Deterministic simplification behavior
//
// IMPORTANT: This is type-level ordering only. Constants with distinct values
// get distinct types (e.g., Constant<1.0> vs Constant<2.0>), so type-level
// ordering correctly distinguishes them.
//
// Ordering (from "smallest" to "largest"):
//   1. Expressions (by operator precedence, then lexicographic on args)
//   2. Atoms/Symbols (by TypeId)
//   3. Fractions (by numeric value)
//   4. Constants (by numeric value)
//
// ============================================================================

namespace tempura::symbolic4 {

enum class Ordering { Less, Equal, Greater };

// ============================================================================
// Forward declaration
// ============================================================================

template <Symbolic L, Symbolic R>
constexpr auto compare(L, R) -> Ordering;

// ============================================================================
// Operator Precedence
// ============================================================================

namespace detail {

template <typename Op>
constexpr auto opIndex() -> int {
  if constexpr (std::is_same_v<Op, AddOp>) return 0;
  else if constexpr (std::is_same_v<Op, SubOp>) return 1;
  else if constexpr (std::is_same_v<Op, MulOp>) return 2;
  else if constexpr (std::is_same_v<Op, DivOp>) return 3;
  else if constexpr (std::is_same_v<Op, NegOp>) return 4;
  else if constexpr (std::is_same_v<Op, PowOp>) return 5;
  else if constexpr (std::is_same_v<Op, SqrtOp>) return 10;
  else if constexpr (std::is_same_v<Op, ExpOp>) return 11;
  else if constexpr (std::is_same_v<Op, LogOp>) return 12;
  else if constexpr (std::is_same_v<Op, SinOp>) return 20;
  else if constexpr (std::is_same_v<Op, CosOp>) return 21;
  else if constexpr (std::is_same_v<Op, TanOp>) return 22;
  else if constexpr (std::is_same_v<Op, AsinOp>) return 23;
  else if constexpr (std::is_same_v<Op, AcosOp>) return 24;
  else if constexpr (std::is_same_v<Op, AtanOp>) return 25;
  else if constexpr (std::is_same_v<Op, SinhOp>) return 30;
  else if constexpr (std::is_same_v<Op, CoshOp>) return 31;
  else if constexpr (std::is_same_v<Op, TanhOp>) return 32;
  else if constexpr (std::is_same_v<Op, PiOp>) return 40;
  else if constexpr (std::is_same_v<Op, EOp>) return 41;
  else return 100 + static_cast<int>(kMeta<Op>);
}

}  // namespace detail

template <typename LhsOp, typename RhsOp>
constexpr auto compareOps() -> Ordering {
  constexpr int lhs = detail::opIndex<LhsOp>();
  constexpr int rhs = detail::opIndex<RhsOp>();
  if constexpr (lhs < rhs) return Ordering::Less;
  else if constexpr (lhs > rhs) return Ordering::Greater;
  else return Ordering::Equal;
}

// ============================================================================
// Compare Constants (compile-time values)
// ============================================================================

template <auto LhsVal, auto RhsVal>
constexpr auto compareConstants(Constant<LhsVal>, Constant<RhsVal>) -> Ordering {
  if constexpr (LhsVal < RhsVal) return Ordering::Less;
  else if constexpr (LhsVal > RhsVal) return Ordering::Greater;
  else return Ordering::Equal;
}

// ============================================================================
// Compare Fractions (exact via cross-multiplication)
// ============================================================================

template <long long LN, long long LD, long long RN, long long RD>
constexpr auto compareFractions(Fraction<LN, LD>, Fraction<RN, RD>) -> Ordering {
  constexpr auto lhs_cross = LN * RD;
  constexpr auto rhs_cross = RN * LD;
  if constexpr (lhs_cross < rhs_cross) return Ordering::Less;
  else if constexpr (lhs_cross > rhs_cross) return Ordering::Greater;
  else return Ordering::Equal;
}

// ============================================================================
// Compare Atoms (by TypeId only - runtime values ignored)
// ============================================================================

template <typename LhsId, typename LhsEffect, typename RhsId, typename RhsEffect>
constexpr auto compareAtoms(Atom<LhsId, LhsEffect>, Atom<RhsId, RhsEffect>) -> Ordering {
  // Compare by full type TypeId (includes both Id and Effect).
  // P2996's std::meta::info only supports ==, not <. Use display_name_of
  // for a stable lexicographic ordering (sufficient for canonicalization).
  constexpr auto lhs_id = kMeta<Atom<LhsId, LhsEffect>>;
  constexpr auto rhs_id = kMeta<Atom<RhsId, RhsEffect>>;
  if constexpr (lhs_id == rhs_id) return Ordering::Equal;
  else {
    constexpr auto lhs_name = std::meta::display_string_of(lhs_id);
    constexpr auto rhs_name = std::meta::display_string_of(rhs_id);
    if constexpr (lhs_name < rhs_name) return Ordering::Less;
    else return Ordering::Greater;
  }
}

// ============================================================================
// Compare Expression Arguments (lexicographic)
// ============================================================================

namespace detail {

// Lexicographic comparison of two argument packs, carried as TypeLists.
// TypeList here is just a pack carrier (defined in meta/tags.h, not type_list.h).
constexpr auto compareArgs(TypeList<>, TypeList<>) -> Ordering {
  return Ordering::Equal;
}

template <typename... Ls>
constexpr auto compareArgs(TypeList<Ls...>, TypeList<>) -> Ordering {
  return Ordering::Greater;
}

template <typename... Rs>
constexpr auto compareArgs(TypeList<>, TypeList<Rs...>) -> Ordering {
  return Ordering::Less;
}

template <typename L0, typename... Ls, typename R0, typename... Rs>
constexpr auto compareArgs(TypeList<L0, Ls...>, TypeList<R0, Rs...>) -> Ordering {
  constexpr auto first = compare(L0{}, R0{});
  if constexpr (first != Ordering::Equal) return first;
  else return compareArgs(TypeList<Ls...>{}, TypeList<Rs...>{});
}

}  // namespace detail

// ============================================================================
// Compare Expressions
// ============================================================================

template <typename LhsOp, Symbolic... LhsArgs, typename RhsOp, Symbolic... RhsArgs>
constexpr auto compareExpressions(Expression<LhsOp, LhsArgs...>,
                                   Expression<RhsOp, RhsArgs...>) -> Ordering {
  constexpr auto op_cmp = compareOps<LhsOp, RhsOp>();
  if constexpr (op_cmp != Ordering::Equal) return op_cmp;
  else return detail::compareArgs(TypeList<LhsArgs...>{}, TypeList<RhsArgs...>{});
}

// ============================================================================
// Main Comparison: Expression < Atom < Fraction < Constant
// ============================================================================

template <Symbolic L, Symbolic R>
constexpr auto compare(L, R) -> Ordering {
  constexpr bool l_expr = is_expression_v<L>;
  constexpr bool r_expr = is_expression_v<R>;
  constexpr bool l_const = is_constant_v<L>;
  constexpr bool r_const = is_constant_v<R>;
  constexpr bool l_frac = is_fraction_v<L>;
  constexpr bool r_frac = is_fraction_v<R>;
  constexpr bool l_atom = is_atom_v<L>;
  constexpr bool r_atom = is_atom_v<R>;

  if constexpr (l_expr && !r_expr) return Ordering::Less;
  else if constexpr (!l_expr && r_expr) return Ordering::Greater;
  else if constexpr (l_atom && (r_frac || r_const)) return Ordering::Less;
  else if constexpr ((l_frac || l_const) && r_atom) return Ordering::Greater;
  else if constexpr (l_frac && r_const) return Ordering::Less;
  else if constexpr (l_const && r_frac) return Ordering::Greater;
  else if constexpr (l_expr && r_expr) return compareExpressions(L{}, R{});
  else if constexpr (l_atom && r_atom) return compareAtoms(L{}, R{});
  else if constexpr (l_frac && r_frac) return compareFractions(L{}, R{});
  else if constexpr (l_const && r_const) return compareConstants(L{}, R{});
  else return Ordering::Equal;
}

// ============================================================================
// Convenience Predicates
// ============================================================================

template <Symbolic L, Symbolic R>
constexpr bool lessThan(L, R) {
  return compare(L{}, R{}) == Ordering::Less;
}

template <Symbolic L, Symbolic R>
constexpr bool greaterThan(L, R) {
  return compare(L{}, R{}) == Ordering::Greater;
}

// ============================================================================
// Comparator for type sorting
// ============================================================================

struct SymbolicComparator {
  template <Symbolic A, Symbolic B>
  constexpr bool operator()(A, B) const {
    return compare(A{}, B{}) == Ordering::Less;
  }
};

}  // namespace tempura::symbolic4
