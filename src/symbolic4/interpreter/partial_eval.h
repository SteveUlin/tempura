#pragma once

#include "symbolic4/core.h"
#include "symbolic4/indexed/reduce_over.h"  // For is_reduce_over_v
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/strategy/combinator.h"

// ============================================================================
// partial_eval.h - Partial evaluation of symbolic expressions
// ============================================================================
//
// Partial evaluation evaluates "ground" subexpressions - those containing
// no free symbols. Uses direct recursion instead of cata/fold.
//
//   sin(1_c + 2_c) * x
//   -> sin(3_c) * x        (1_c + 2_c is ground, eval to 3_c)
//   -> 0.14112_lit * x     (sin(3_c) is ground, eval to literal)
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// IsGround - Check if expression contains no free symbols
// ============================================================================

namespace partial_eval_detail {

template <Symbolic E>
constexpr bool isGroundImpl();

template <typename Op, typename... Args, SizeT... Is>
constexpr bool isGroundExpression(Expression<Op, Args...>, IndexSequence<Is...>) {
  return (isGroundImpl<Args>() && ...);
}

template <Symbolic E>
constexpr bool isGroundImpl() {
  if constexpr (is_reduce_over_v<E>) {
    return isGroundImpl<typename E::expr_type>();
  } else if constexpr (is_constant_v<E> || is_fraction_v<E>) {
    return true;
  } else if constexpr (is_literal_v<E>) {
    return true;
  } else if constexpr (is_atom_v<E>) {
    return false;  // Free variable
  } else if constexpr (is_expression_v<E>) {
    return isGroundExpression(E{}, MakeIndexSequence<E::arity>{});
  } else {
    return true;  // Nullary expressions (pi, e) are ground
  }
}

// HasLiteral - does expression contain any Literal node?
template <Symbolic E>
constexpr bool hasLiteralImpl();

template <typename Op, typename... Args, SizeT... Is>
constexpr bool hasLiteralExpression(Expression<Op, Args...>, IndexSequence<Is...>) {
  return (hasLiteralImpl<Args>() || ...);
}

template <Symbolic E>
constexpr bool hasLiteralImpl() {
  if constexpr (is_reduce_over_v<E>) {
    return hasLiteralImpl<typename E::expr_type>();
  } else if constexpr (is_literal_v<E>) {
    return true;
  } else if constexpr (is_expression_v<E>) {
    return hasLiteralExpression(E{}, MakeIndexSequence<E::arity>{});
  } else {
    return false;
  }
}

}  // namespace partial_eval_detail

template <Symbolic E>
constexpr bool isGround(E) {
  return partial_eval_detail::isGroundImpl<E>();
}

template <Symbolic E>
constexpr bool is_ground_v = partial_eval_detail::isGroundImpl<E>();

template <Symbolic E>
constexpr bool hasLiteral(E) {
  return partial_eval_detail::hasLiteralImpl<E>();
}

template <Symbolic E>
constexpr bool has_literal_v = partial_eval_detail::hasLiteralImpl<E>();

// ============================================================================
// EvalIfGround - Strategy that evaluates ground subexpressions
// ============================================================================

struct EvalIfGround {
  template <Symbolic E>
  constexpr auto apply(E e) const {
    if constexpr (!is_ground_v<E>) {
      return e;
    } else if constexpr (is_constant_v<E> || is_fraction_v<E> || is_literal_v<E>) {
      return e;
    } else if constexpr (has_literal_v<E>) {
      auto value = evaluate(e);
      return lit(value);
    } else {
      constexpr auto value = evaluate(E{});
      return Constant<value>{};
    }
  }
};

// ============================================================================
// Convenience functions
// ============================================================================

template <Symbolic E>
constexpr auto partialEval(E e) {
  return bottomup(EvalIfGround{}).apply(e);
}

constexpr auto partialEvalStrategy() {
  return bottomup(EvalIfGround{});
}

}  // namespace tempura::symbolic4
