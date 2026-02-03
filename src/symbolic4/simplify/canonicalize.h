#pragma once

#include "symbolic4/indexed/reduce_over.h"  // For is_reduce_over_v
#include "symbolic4/operators.h"
#include "symbolic4/simplify/ordering.h"

// ============================================================================
// canonicalize.h - Canonical ordering for commutative operations
// ============================================================================
//
// Reorders children of commutative operations (Add, Mul) so that the smaller
// child (by type ordering) comes first. Uses direct recursion.
//
// ============================================================================

namespace tempura::symbolic4 {

namespace canon_detail {

// Forward declaration
template <Symbolic E>
constexpr auto canonImpl(E expr);

// Canonicalize an Expression by recursing into children then reordering
template <typename Op, typename... Args, SizeT... Is>
constexpr auto canonExpression(Expression<Op, Args...> expr, IndexSequence<Is...>) {
  // Recursively canonicalize each child
  auto canonicalized = Expression<Op, decltype(canonImpl(expr.template arg<Is>()))...>{
      canonImpl(expr.template arg<Is>())...};
  return canonicalized;
}

// Reorder commutative binary ops after canonicalizing children
template <typename Op, typename L, typename R>
constexpr auto canonBinary(L l, R r) {
  if constexpr ((std::is_same_v<Op, AddOp> || std::is_same_v<Op, MulOp>) &&
                compare(R{}, L{}) == Ordering::Less) {
    return Expression<Op, R, L>{r, l};
  } else {
    return Expression<Op, L, R>{l, r};
  }
}

template <Symbolic E>
constexpr auto canonImpl(E expr) {
  if constexpr (is_reduce_over_v<E>) {
    auto inner = canonImpl(expr.expr());
    return E::rebuild(inner);
  } else if constexpr (is_expression_v<E>) {
    if constexpr (E::arity == 2) {
      // Binary: canonicalize children, then maybe reorder
      auto l = canonImpl(expr.template arg<0>());
      auto r = canonImpl(expr.template arg<1>());
      using Op = typename E::op_type;
      return canonBinary<Op>(l, r);
    } else {
      // Non-binary: just canonicalize children
      return canonExpression(expr, MakeIndexSequence<E::arity>{});
    }
  } else {
    // Terminal: pass through unchanged (preserves Literal data)
    return expr;
  }
}

}  // namespace canon_detail

// ============================================================================
// Convenience function
// ============================================================================

template <Symbolic E>
constexpr auto canonicalize(E expr) {
  return canon_detail::canonImpl(expr);
}

}  // namespace tempura::symbolic4
