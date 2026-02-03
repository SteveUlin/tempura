#pragma once

#include <tuple>

#include "symbolic4/core.h"
#include "symbolic4/fraction_ops.h"
#include "symbolic4/interpreter/partial_eval.h"  // For is_ground_v
#include "symbolic4/operators.h"                 // For Op types (AddOp, etc.)
#include "symbolic4/scheme/cata.h"
#include "symbolic4/strategy/combinator.h"

// ============================================================================
// partial_eval_exact.h - Exact partial evaluation preserving rational arithmetic
// ============================================================================
//
// Unlike partialEval which evaluates to Constant<double>, this evaluates ground
// rational subexpressions to Fraction, preserving exactness.
//
// "Exact" means:
//   - Only contains Fractions (or integer Constants)
//   - Only uses +, -, *, / operations (closed over rationals)
//   - No transcendentals (sin, cos, log, exp, sqrt, pow with non-integer exponent)
//
// Usage:
//   auto expr = (Fraction<1,2>{} + Fraction<1,3>{}) * x;
//   auto result = partialEvalExact(expr);
//   // result is Expression<MulOp, Fraction<5,6>, Symbol<X>>
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// IsExact - Check if expression uses only exact (rational) operations
// ============================================================================
//
// An expression is "exact" if:
// 1. All terminals are Fractions (includes integer constants as Fraction<N,1>)
// 2. All operations are +, -, *, / (closed over rationals)
//
// Note: Integer Constants are considered exact (convertible to Fraction<N,1>)

// Check if an operator is exact (closed over rationals)
template <typename Op>
constexpr bool is_exact_op_v = isSame<Op, AddOp> || isSame<Op, SubOp> ||
                               isSame<Op, MulOp> || isSame<Op, DivOp> ||
                               isSame<Op, NegOp>;

struct IsExactInterpreter {
  template <typename T>
  static constexpr bool terminal(T) {
    if constexpr (is_fraction_v<T>) {
      return true;  // Fractions are exact
    } else if constexpr (is_constant_v<T>) {
      // Integer constants are exact (treatable as Fraction<N,1>)
      return std::is_integral_v<decltype(T::value)>;
    } else {
      return false;  // Symbols, literals, etc. are not exact constants
    }
  }

  template <typename Op, typename... Children>
  static constexpr bool combine(Children... exact_flags) {
    if constexpr (!is_exact_op_v<Op>) {
      return false;  // Non-exact operation (sin, log, etc.)
    } else {
      return (exact_flags && ...);  // Exact iff all children are exact
    }
  }
};

template <Symbolic E>
constexpr bool isExact(E) {
  return fold<IsExactInterpreter>(E{});
}

template <Symbolic E>
constexpr bool is_exact_v = fold<IsExactInterpreter>(E{});

// ============================================================================
// ExactEval - Type-level evaluation to Fraction
// ============================================================================
//
// This is a catamorphism that evaluates exact expressions to Fraction at
// compile time. Unlike Eval which returns double, this returns Fraction types.

struct ExactEvalInterpreter {
  template <typename T>
  static constexpr auto terminal(T) {
    if constexpr (is_fraction_v<T>) {
      return T{};
    } else if constexpr (is_constant_v<T> && std::is_integral_v<decltype(T::value)>) {
      // Convert integer constant to Fraction<N, 1>
      return Fraction<static_cast<long long>(T::value), 1>{};
    } else {
      // Shouldn't reach here if is_exact_v was checked
      static_assert(is_fraction_v<T>, "ExactEval requires exact (rational) terminals");
      return T{};
    }
  }

  template <typename Op, typename... Rs>
  static constexpr auto combine(Rs... children) {
    if constexpr (sizeof...(Rs) == 1) {
      // Unary: negation
      auto [a] = std::tuple{children...};
      if constexpr (isSame<Op, NegOp>) {
        return -a;  // Uses Fraction's operator-
      } else {
        static_assert(isSame<Op, NegOp>, "Unknown unary exact operation");
        return a;
      }
    } else if constexpr (sizeof...(Rs) == 2) {
      // Binary: +, -, *, /
      auto [a, b] = std::tuple{children...};
      if constexpr (isSame<Op, AddOp>) {
        return a + b;  // Uses Fraction's operator+
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
      static_assert(sizeof...(Rs) <= 2, "Exact operations have at most 2 arguments");
      return std::get<0>(std::tuple{children...});
    }
  }
};

// Evaluate an exact expression to a Fraction
template <Symbolic E>
  requires is_exact_v<E>
constexpr auto exactEval(E) {
  return fold<ExactEvalInterpreter>(E{});
}

// ============================================================================
// EvalIfGroundExact - Strategy that evaluates exact ground subexpressions
// ============================================================================

struct EvalIfGroundExact {
  template <Symbolic E>
  constexpr auto apply(E e) const {
    if constexpr (!is_ground_v<E>) {
      // Contains free symbols - cannot evaluate
      return e;
    } else if constexpr (is_constant_v<E> || is_fraction_v<E> || is_literal_v<E>) {
      // Already a terminal value - no simplification needed
      return e;
    } else if constexpr (is_exact_v<E>) {
      // Ground and exact - evaluate to Fraction
      return exactEval(E{});
    } else {
      // Ground but not exact (contains transcendentals) - leave unchanged
      // Could optionally fall back to partialEval here for non-exact ground exprs
      return e;
    }
  }
};

// ============================================================================
// Convenience functions
// ============================================================================

// Apply exact partial evaluation bottom-up
template <Symbolic E>
constexpr auto partialEvalExact(E e) {
  return bottomup(EvalIfGroundExact{}).apply(e);
}

// Just the strategy, for composition with other strategies
inline constexpr auto partialEvalExactStrategy() {
  return bottomup(EvalIfGroundExact{});
}

}  // namespace tempura::symbolic4
