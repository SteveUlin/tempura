#pragma once

#include "meta/utility.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"

// ============================================================================
// PATTERN MATCHING FOR SYMBOLIC EXPRESSIONS
// ============================================================================
//
// This header implements the core pattern matching system for symbolic
// expressions. Pattern matching is the foundation for:
// - Rewrite rules (x + 0 → x)
// - Simplification strategies
// - Expression transformation pipelines
//
// MATCHING STRATEGIES:
// --------------------
// 1. Exact Matching - Structural identity (Symbol, Constant, Fraction)
// 2. Wildcard Matching - Category-based (AnyArg, AnyExpr, etc.)
// 3. Pattern Variable Matching - Capture and bind (PatternVar, x_, y_)
// 4. Structural Matching - Recursive through expression trees
//
// DESIGN PHILOSOPHY:
// ------------------
// - Type-based overload resolution for matching semantics
// - Stateless matching (no side effects - pure type-level computation)
// - Compile-time evaluation (constexpr by default)
// - Overload resolution precedence matches semantic precedence

namespace tempura::symbolic3 {

// ============================================================================
// STRATEGY 1: EXACT MATCHING
// ============================================================================
// Match by structural identity - types and values must be identical
// Used for: Symbol, Constant, Fraction

// Generic type-level matching (meta-programming utility)
template <typename T1, typename T2>
constexpr bool match() {
  return isSame<T1, T2>;
}

// Symbol matches Symbol if same unique type identity
// Example: Symbol<λ1>{} matches Symbol<λ1>{}, but not Symbol<λ2>{}
template <typename U1, typename U2>
constexpr bool match(Symbol<U1>, Symbol<U2>) {
  return isSame<Symbol<U1>, Symbol<U2>>;
}

// Constant matches Constant if same value
// Example: Constant<5>{} matches Constant<5>{}, but not Constant<3>{}
template <auto V1, auto V2>
constexpr bool match(Constant<V1>, Constant<V2>) {
  return V1 == V2;
}

// Fraction matches Fraction if same reduced form
// Example: Fraction<1,2>{} matches Fraction<2,4>{} (both reduce to 1/2)
template <long long N1, long long D1, long long N2, long long D2>
constexpr bool match(Fraction<N1, D1>, Fraction<N2, D2>) {
  // Fractions are always reduced at construction, so just compare numerators
  // and denominators
  return Fraction<N1, D1>::numerator == Fraction<N2, D2>::numerator &&
         Fraction<N1, D1>::denominator == Fraction<N2, D2>::denominator;
}

// Never matches Never - always fails (Never is a non-matching sentinel)
constexpr bool match(Never, Never) {
  return false;  // Never never matches (intentionally paradoxical)
}

// ============================================================================
// STRATEGY 2: WILDCARD MATCHING
// ============================================================================
// Match based on expression category - structural constraints without identity
// Used for: AnyArg (𝐚𝐧𝐲), AnyExpr (𝐚𝐧𝐲𝐞𝐱𝐩𝐫), AnyConstant (𝐜), AnySymbol

// AnyArg (𝐚𝐧𝐲) matches any symbolic expression (universal wildcard)
// Example: AnyArg{} matches 5, x, sin(x), x+y, etc.
template <Symbolic S>
constexpr bool match(AnyArg, S) {
  return true;
}

// Symmetric: expression matches AnyArg
template <Symbolic S>
constexpr bool match(S, AnyArg) {
  return true;
}

// AnyExpr (𝐚𝐧𝐲𝐞𝐱𝐩𝐫) matches only compound expressions (not atoms)
// Example: AnyExpr{} matches sin(x), x+y, but NOT x or 5
template <typename Op, Symbolic... Args>
  requires(sizeof...(Args) > 0)
constexpr bool match(AnyExpr, Expression<Op, Args...>) {
  return true;
}

// AnyConstant (𝐜) matches only numeric constants
// Example: AnyConstant{} matches 5, -3, but NOT x or x+y
template <auto val>
constexpr bool match(AnyConstant, Constant<val>) {
  return true;
}

// AnyConstant also matches fractions (exact rationals are constants)
template <long long N, long long D>
constexpr bool match(AnyConstant, Fraction<N, D>) {
  return true;
}

// AnySymbol matches only symbolic variables
// Example: AnySymbol{} matches x, y, but NOT 5 or x+y
template <typename unique>
constexpr bool match(AnySymbol, Symbol<unique>) {
  return true;
}

// ============================================================================
// STRATEGY 3: STRUCTURAL MATCHING
// ============================================================================
// Match compound expressions by recursively matching operation and arguments
// Used for: Expression<Op, Args...>
//
// ALGORITHM:
// 1. Check if operations match (Op1 == Op2)
// 2. Check if argument counts match (sizeof...(Args1) == sizeof...(Args2))
// 3. Recursively match all argument pairs (Args1[i] with Args2[i])
// 4. All checks must pass for expressions to match
//
// EXAMPLE:
//   Expression<AddOp, Symbol<λ1>, Constant<5>>  (represents: x + 5)
//   matches
//   Expression<AddOp, Symbol<λ1>, Constant<5>>  (same expression)
//   but NOT
//   Expression<AddOp, Symbol<λ2>, Constant<5>>  (different symbol)

// Expression matches Expression - structural recursion with fold expressions
// This avoids the two-pack problem by using fold expressions to pair up args
template <typename Op1, Symbolic... Args1, typename Op2, Symbolic... Args2>
constexpr bool match(Expression<Op1, Args1...>, Expression<Op2, Args2...>) {
  // Step 1: Operations must match
  if constexpr (!isSame<Op1, Op2>) {
    return false;
  }
  // Step 2: Argument count must match
  else if constexpr (sizeof...(Args1) != sizeof...(Args2)) {
    return false;
  }
  // Step 3a: Empty argument lists always match (same operation, zero args)
  else if constexpr (sizeof...(Args1) == 0 && sizeof...(Args2) == 0) {
    return true;
  }
  // Step 3b: Recursively match all arguments pairwise using fold expression
  // The fold expression (match(Args1{}, Args2{}) && ...) automatically
  // pairs up corresponding arguments from both packs element-wise
  // This is the key insight: C++20 fold expressions handle parameter pack
  // pairing without explicit indexing or recursion
  else {
    return (match(Args1{}, Args2{}) && ...);
  }
}

// ============================================================================
// STRATEGY 4: NEVER MATCHING
// ============================================================================
// Never is a sentinel type that never matches anything (not even itself)
// Used for: Control flow in pattern matching, marking failed matches

// Never matches nothing - always false
// This includes Never matching Never (handled above) and Never matching
// anything else
template <Symbolic S>
  requires(!isSame<S, Never>)
constexpr bool match(Never, S) {
  return false;
}

// Symmetric: nothing matches Never
template <Symbolic S>
  requires(!isSame<S, Never>)
constexpr bool match(S, Never) {
  return false;
}

// ============================================================================
// FALLBACK MATCHING
// ============================================================================
// Catch-all for type mismatches - ensures all unhandled cases return false
// This overload has lowest precedence due to lack of constraints
//
// EXAMPLES OF WHAT THIS CATCHES:
// - Symbol matching Constant: match(Symbol<λ>{}, Constant<5>{}) → false
// - Expression matching Symbol: match(Expression<...>{}, Symbol<λ>{}) → false
// - Expression matching Constant: match(Expression<...>{}, Constant<5>{}) → false
//
// This ensures the type system prevents nonsensical matches at compile-time

// Catch-all for any two symbolic types that don't match via other overloads
// This ensures expressions don't match non-expressions, atoms don't match
// compounds, etc.
template <Symbolic S1, Symbolic S2>
constexpr bool match(S1, S2) {
  return false;
}

// ============================================================================
// OPERATION-SPECIFIC MATCHING UTILITIES
// ============================================================================
// Helper traits for checking if an expression uses a specific operation
// Used in: Strategy predicates, simplification rules, pattern guards

// Check if expression matches specific operation type
// Example: matches_op_v<AddOp, decltype(x + y)> → true
template <typename Op, typename T>
struct matches_op {
  static constexpr bool value = false;
};

template <typename Op, Symbolic... Args>
struct matches_op<Op, Expression<Op, Args...>> {
  static constexpr bool value = true;
};

template <typename Op, typename T>
constexpr bool matches_op_v = matches_op<Op, T>::value;

// ============================================================================
// DECOMPOSITION HELPERS
// ============================================================================
// Extract operation and arguments from expressions
// Note: get_op and get_args are defined in core.h, not duplicated here

// ============================================================================
// EXPRESSION CLASSIFICATION PREDICATES
// ============================================================================
// Predicates for use with strategy combinators (when, |, >>)
// These predicates work with both the expression and a context parameter
//
// USAGE:
//   auto strat = when(is_constant_pred{}, simplify_constant);
//   auto result = strat.apply(expr, ctx);

// Predicate: check if expression is a constant
struct is_constant_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context) const {
    return is_constant<S>;
  }
};

// Predicate: check if expression is a symbol
struct is_symbol_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context) const {
    return is_symbol<S>;
  }
};

// Predicate: check if expression is a compound expression
struct is_expression_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context) const {
    return is_expression<S>;
  }
};

// Predicate: check if expression is a trigonometric function
struct is_trig_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context) const {
    return is_trig_function<S>;
  }
};

// Predicate: check if expression uses specific operation
// Example: has_op_pred<AddOp>{} checks if expression is an addition
template <typename Op>
struct has_op_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context) const {
    return matches_op_v<Op, S>;
  }
};

// Predicate: check if context has a specific tag
// Used for: Controlling simplification strategies based on context flags
template <typename Tag>
struct has_tag_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context ctx) const {
    return ctx.template has<Tag>();
  }
};

}  // namespace tempura::symbolic3
