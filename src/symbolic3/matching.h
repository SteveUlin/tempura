#pragma once

#include <type_traits>

#include "symbolic3/core.h"
#include "symbolic3/operators.h"

// Pattern matching for symbolic expressions

namespace tempura::symbolic3 {

// ============================================================================
// Basic Pattern Matching
// ============================================================================

// Match two types
template <typename T1, typename T2>
constexpr bool match() {
  return std::is_same_v<T1, T2>;
}

// Symbol matches Symbol if same type
template <typename U1, typename U2>
constexpr bool match(Symbol<U1>, Symbol<U2>) {
  return std::is_same_v<Symbol<U1>, Symbol<U2>>;
}

// Constant matches Constant if same value
template <auto V1, auto V2>
constexpr bool match(Constant<V1>, Constant<V2>) {
  return V1 == V2;
}

// Never matches Never
constexpr bool match(Never, Never) {
  return false;  // Never never matches
}

// ============================================================================
// Wildcard Matching
// ============================================================================

// AnyArg matches anything
template <Symbolic S>
constexpr bool match(AnyArg, S) {
  return true;
}

template <Symbolic S>
constexpr bool match(S, AnyArg) {
  return true;
}

// AnyExpr matches only expressions
template <typename Op, Symbolic... Args>
  requires(sizeof...(Args) > 0)
constexpr bool match(AnyExpr, Expression<Op, Args...>) {
  return true;
}

// AnyConstant matches only constants
template <auto val>
constexpr bool match(AnyConstant, Constant<val>) {
  return true;
}

// AnySymbol matches only symbols
template <typename unique>
constexpr bool match(AnySymbol, Symbol<unique>) {
  return true;
}

// ============================================================================
// Structural Matching for Expressions
// ============================================================================

// Expression matches Expression - structural recursion with fold expressions
// This avoids the two-pack problem by using fold expressions to pair up args
template <typename Op1, Symbolic... Args1, typename Op2, Symbolic... Args2>
constexpr bool match(Expression<Op1, Args1...>, Expression<Op2, Args2...>) {
  // Operations must match
  if constexpr (!std::is_same_v<Op1, Op2>) {
    return false;
  }
  // Argument count must match
  else if constexpr (sizeof...(Args1) != sizeof...(Args2)) {
    return false;
  }
  // Empty argument lists always match (same operation, zero args)
  else if constexpr (sizeof...(Args1) == 0 && sizeof...(Args2) == 0) {
    return true;
  }
  // Recursively match all arguments pairwise using fold expression
  // The fold expression (match(Args1{}, Args2{}) && ...) automatically
  // pairs up corresponding arguments from both packs element-wise
  else {
    return (match(Args1{}, Args2{}) && ...);
  }
}

// Never matches nothing
template <Symbolic S>
constexpr bool match(Never, S) {
  return false;
}

template <Symbolic S>
constexpr bool match(S, Never) {
  return false;
}

// ============================================================================
// Fallback - Non-matching types return false
// ============================================================================

// Catch-all for any two symbolic types that don't match via other overloads
// This ensures expressions don't match non-expressions, etc.
template <Symbolic S1, Symbolic S2>
constexpr bool match(S1, S2) {
  return false;
}

// ============================================================================
// Operation-Specific Matching
// ============================================================================

// Check if expression matches specific operation
template <typename Op, typename T>
struct matches_op : std::false_type {};

template <typename Op, Symbolic... Args>
struct matches_op<Op, Expression<Op, Args...>> : std::true_type {};

template <typename Op, typename T>
constexpr bool matches_op_v = matches_op<Op, T>::value;

// ============================================================================
// Decomposition Helpers
// ============================================================================

// Note: get_op and get_args are defined in core.h, not duplicated here

// ============================================================================
// Predicates for Strategies
// ============================================================================

// Predicate: is constant
struct is_constant_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context) const {
    return is_constant<S>;
  }
};

// Predicate: is symbol
struct is_symbol_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context) const {
    return is_symbol<S>;
  }
};

// Predicate: is expression
struct is_expression_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context) const {
    return is_expression<S>;
  }
};

// Predicate: is trig function
struct is_trig_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context) const {
    return is_trig_function<S>;
  }
};

// Predicate: has specific operation
template <typename Op>
struct has_op_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context) const {
    return matches_op_v<Op, S>;
  }
};

// Predicate: context has tag
template <typename Tag>
struct has_tag_pred {
  template <Symbolic S, typename Context>
  constexpr bool operator()(S, Context ctx) const {
    return ctx.template has<Tag>();
  }
};

}  // namespace tempura::symbolic3
