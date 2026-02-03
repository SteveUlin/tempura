#pragma once

#include <tuple>

#include "symbolic4/core.h"
#include "symbolic4/fraction_ops.h"
#include "symbolic4/interpreter/partial_eval.h"  // For is_ground_v
#include "symbolic4/operators.h"                 // For Op types (AddOp, etc.)
#include "symbolic4/strategy/combinator.h"

// ============================================================================
// partial_eval_exact.h - Exact partial evaluation preserving rational arithmetic
// ============================================================================
//
// Unlike partialEval which evaluates to Constant<double>, this evaluates ground
// rational subexpressions to Fraction, preserving exactness. Uses direct
// recursion instead of cata/fold.
//
// "Exact" means:
//   - Only contains Fractions (or integer Constants)
//   - Only uses +, -, *, / operations (closed over rationals)
//   - No transcendentals (sin, cos, log, exp, sqrt, pow with non-integer exponent)
//
// ============================================================================

namespace tempura::symbolic4 {

// Check if an operator is exact (closed over rationals)
template <typename Op>
constexpr bool is_exact_op_v = isSame<Op, AddOp> || isSame<Op, SubOp> ||
                               isSame<Op, MulOp> || isSame<Op, DivOp> ||
                               isSame<Op, NegOp>;

// ============================================================================
// IsExact - direct recursion implementation
// ============================================================================

namespace exact_detail {

template <Symbolic E>
constexpr bool isExactImpl();

template <typename Op, typename... Args, SizeT... Is>
constexpr bool isExactExpression(Expression<Op, Args...>, IndexSequence<Is...>) {
  if constexpr (!is_exact_op_v<Op>) {
    return false;
  } else {
    return (isExactImpl<Args>() && ...);
  }
}

template <Symbolic E>
constexpr bool isExactImpl() {
  if constexpr (is_fraction_v<E>) {
    return true;
  } else if constexpr (is_constant_v<E>) {
    return std::is_integral_v<decltype(E::value)>;
  } else if constexpr (is_expression_v<E>) {
    return isExactExpression(E{}, MakeIndexSequence<E::arity>{});
  } else {
    return false;
  }
}

// ============================================================================
// ExactEval - direct recursion to Fraction
// ============================================================================

template <Symbolic E>
constexpr auto exactEvalImpl(E);

template <typename Op, typename... Args, SizeT... Is>
constexpr auto exactEvalExpression(Expression<Op, Args...>, IndexSequence<Is...>) {
  if constexpr (sizeof...(Args) == 1) {
    auto a = exactEvalImpl(std::get<0>(std::tuple{Args{}...}));
    if constexpr (isSame<Op, NegOp>) {
      return -a;
    } else {
      static_assert(isSame<Op, NegOp>, "Unknown unary exact operation");
      return a;
    }
  } else if constexpr (sizeof...(Args) == 2) {
    auto a = exactEvalImpl(std::get<0>(std::tuple{Args{}...}));
    auto b = exactEvalImpl(std::get<1>(std::tuple{Args{}...}));
    if constexpr (isSame<Op, AddOp>) {
      return a + b;
    } else if constexpr (isSame<Op, SubOp>) {
      return a - b;
    } else if constexpr (isSame<Op, MulOp>) {
      return a * b;
    } else if constexpr (isSame<Op, DivOp>) {
      return a / b;
    } else {
      static_assert(isSame<Op, AddOp>, "Unknown binary exact operation");
      return a;
    }
  } else {
    static_assert(sizeof...(Args) <= 2, "Exact operations have at most 2 arguments");
    return exactEvalImpl(std::get<0>(std::tuple{Args{}...}));
  }
}

template <Symbolic E>
constexpr auto exactEvalImpl(E) {
  if constexpr (is_fraction_v<E>) {
    return E{};
  } else if constexpr (is_constant_v<E>) {
    // is_exact_v guarantees integral constants only reach here
    return Fraction<static_cast<long long>(E::value), 1>{};
  } else if constexpr (is_expression_v<E>) {
    return exactEvalExpression(E{}, MakeIndexSequence<E::arity>{});
  } else {
    static_assert(is_fraction_v<E>, "ExactEval requires exact (rational) terminals");
    return E{};
  }
}

}  // namespace exact_detail

template <Symbolic E>
constexpr bool isExact(E) {
  return exact_detail::isExactImpl<E>();
}

template <Symbolic E>
constexpr bool is_exact_v = exact_detail::isExactImpl<E>();

// Evaluate an exact expression to a Fraction
template <Symbolic E>
  requires is_exact_v<E>
constexpr auto exactEval(E) {
  return exact_detail::exactEvalImpl(E{});
}

// ============================================================================
// EvalIfGroundExact - Strategy that evaluates exact ground subexpressions
// ============================================================================

struct EvalIfGroundExact {
  template <Symbolic E>
  constexpr auto apply(E e) const {
    if constexpr (!is_ground_v<E>) {
      return e;
    } else if constexpr (is_constant_v<E> || is_fraction_v<E> || is_literal_v<E>) {
      return e;
    } else if constexpr (is_exact_v<E>) {
      return exactEval(E{});
    } else {
      return e;
    }
  }
};

// ============================================================================
// Convenience functions
// ============================================================================

template <Symbolic E>
constexpr auto partialEvalExact(E e) {
  return bottomup(EvalIfGroundExact{}).apply(e);
}

inline constexpr auto partialEvalExactStrategy() {
  return bottomup(EvalIfGroundExact{});
}

}  // namespace tempura::symbolic4
