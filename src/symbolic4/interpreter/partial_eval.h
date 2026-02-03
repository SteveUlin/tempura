#pragma once

#include "symbolic4/core.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/scheme/cata.h"
#include "symbolic4/strategy/combinator.h"

// ============================================================================
// partial_eval.h - Partial evaluation of symbolic expressions
// ============================================================================
//
// Partial evaluation evaluates "ground" subexpressions - those containing
// no free symbols. This is more general than pattern-based constant folding:
//
//   sin(1_c + 2_c) * x
//   → sin(3_c) * x        (1_c + 2_c is ground, eval to 3_c)
//   → 0.14112_lit * x     (sin(3_c) is ground, eval to literal)
//
// Key distinction:
//   - Constants (1_c, 2_c) are compile-time: Constant<N>
//   - Literals (lit(x)) are runtime values embedded in types
//
// When folding:
//   - All Constant inputs → Constant output (stays compile-time)
//   - Any Literal input → Literal output (becomes runtime)
//
// Usage:
//   auto result = partialEval(sin(1_c + 2_c) * x);
//   // result is lit(0.14112...) * x
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// IsGround - Check if expression contains no free symbols
// ============================================================================

struct IsGroundInterpreter {
  template <typename T>
  static constexpr bool terminal(T) {
    if constexpr (is_constant_v<T> || is_fraction_v<T>) {
      return true;  // Compile-time constants are ground
    } else if constexpr (is_literal_v<T>) {
      return true;  // Runtime literals are ground (value is known)
    } else if constexpr (is_atom_v<T>) {
      return false;  // Symbols are free variables - NOT ground
    } else {
      return true;  // Nullary expressions (pi, e) are ground
    }
  }

  template <typename Op, typename... Children>
  static constexpr bool combine(Children... ground_flags) {
    return (ground_flags && ...);  // Ground iff ALL children are ground
  }
};

template <Symbolic E>
constexpr bool isGround(E) {
  return fold<IsGroundInterpreter>(E{});
}

template <Symbolic E>
constexpr bool is_ground_v = fold<IsGroundInterpreter>(E{});

// ============================================================================
// HasLiteral - Check if expression contains any Literal (runtime value)
// ============================================================================

struct HasLiteralInterpreter {
  template <typename T>
  static constexpr bool terminal(T) {
    return is_literal_v<T>;
  }

  template <typename Op, typename... Children>
  static constexpr bool combine(Children... has_literal_flags) {
    return (has_literal_flags || ...);  // Has literal if ANY child has literal
  }
};

template <Symbolic E>
constexpr bool hasLiteral(E) {
  return fold<HasLiteralInterpreter>(E{});
}

template <Symbolic E>
constexpr bool has_literal_v = fold<HasLiteralInterpreter>(E{});

// ============================================================================
// EvalIfGround - Strategy that evaluates ground subexpressions
// ============================================================================

struct EvalIfGround {
  template <Symbolic E>
  constexpr auto apply(E e) const {
    if constexpr (!is_ground_v<E>) {
      // Contains free symbols - cannot evaluate
      return e;
    } else if constexpr (is_constant_v<E> || is_fraction_v<E> || is_literal_v<E>) {
      // Already a terminal value - no simplification needed
      return e;
    } else if constexpr (has_literal_v<E>) {
      // Ground but contains literals - result is a literal (runtime)
      // Must use actual object `e` to preserve literal values
      auto value = evaluate(e);
      return lit(value);
    } else {
      // Ground and all constants - result is a constant (compile-time)
      // Can use default-constructed E{} since all values are in types
      constexpr auto value = evaluate(E{});
      return Constant<value>{};
    }
  }
};

// ============================================================================
// Convenience functions
// ============================================================================

// Apply partial evaluation bottom-up (evaluates innermost ground terms first)
template <Symbolic E>
constexpr auto partialEval(E e) {
  return bottomup(EvalIfGround{}).apply(e);
}

// Just the strategy, for composition with other strategies
constexpr auto partialEvalStrategy() {
  return bottomup(EvalIfGround{});
}

}  // namespace tempura::symbolic4
