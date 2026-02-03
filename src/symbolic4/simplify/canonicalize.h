#pragma once

#include "symbolic4/operators.h"
#include "symbolic4/scheme/cata.h"
#include "symbolic4/simplify/ordering.h"

// ============================================================================
// canonicalize.h - Canonical ordering for commutative operations
// ============================================================================
//
// Reorders children of commutative operations (Add, Mul) so that the smaller
// child (by type ordering) comes first. This produces a canonical form where
// x + y and y + x become the same type.
//
// Usage:
//   auto expr = y + x;           // Expression<AddOp, Y, X>
//   auto canon = canonicalize(expr);  // Expression<AddOp, X, Y>
//
// Runtime data (e.g., Literal values) is preserved through the transformation.
//
// This does NOT flatten nested operations. (a + b) + c stays as-is, it doesn't
// become a ternary add. Each binary operation is independently canonicalized.
//
// ============================================================================

namespace tempura::symbolic4 {

struct Canonicalize {
  // Terminals pass through unchanged (preserves Literal data)
  template <typename T>
  static constexpr auto terminal(T t) {
    return t;
  }

  // Combine: reorder commutative ops, reconstruct others
  template <typename Op, typename... Cs>
  static constexpr auto combine(Cs... cs) {
    return Rule<Op, Cs...>::apply(cs...);
  }

  // Default: reconstruct unchanged
  template <typename Op, typename... Cs>
  struct Rule {
    static constexpr auto apply(Cs... cs) {
      return Expression<Op, Cs...>{cs...};
    }
  };

  // Addition: swap if right < left
  template <typename L, typename R>
  struct Rule<AddOp, L, R> {
    static constexpr auto apply(L l, R r) {
      if constexpr (compare(R{}, L{}) == Ordering::Less) {
        return Expression<AddOp, R, L>{r, l};
      } else {
        return Expression<AddOp, L, R>{l, r};
      }
    }
  };

  // Multiplication: swap if right < left
  template <typename L, typename R>
  struct Rule<MulOp, L, R> {
    static constexpr auto apply(L l, R r) {
      if constexpr (compare(R{}, L{}) == Ordering::Less) {
        return Expression<MulOp, R, L>{r, l};
      } else {
        return Expression<MulOp, L, R>{l, r};
      }
    }
  };
};

// ============================================================================
// Convenience function
// ============================================================================

template <Symbolic E>
constexpr auto canonicalize(E expr) {
  return fold<Canonicalize>(expr);
}

}  // namespace tempura::symbolic4
