#pragma once

#include "symbolic2/pattern_matching.h"
#include "symbolic2/recursive_rewrite.h"

namespace tempura {

// Pure symbolic notation for recursive differentiation rules
//
// Enables: diff_(f_, var_) instead of lambda boilerplate
// Implementation: DiffCall expressions are evaluated after pattern substitution

struct DiffOperator {};

// Placeholder for differentiation variable in rules
struct VarPlaceholder : SymbolicTag {
  static constexpr auto id = kMeta<VarPlaceholder>;
};

inline constexpr VarPlaceholder var_;

// Deferred differentiation call (evaluated after pattern substitution)
template <Symbolic Expr, Symbolic Var>
struct DiffCall : SymbolicTag {
  using expr_type = Expr;
  using var_type = Var;

  constexpr DiffCall() = default;
  constexpr DiffCall(Expr, Var) {}
};

// Helper to detect if a type is a DiffCall
template <typename T>
struct IsDiffCall : std::false_type {};

template <Symbolic E, Symbolic V>
struct IsDiffCall<DiffCall<E, V>> : std::true_type {};

template <typename T>
inline constexpr bool is_diff_call_v = IsDiffCall<T>::value;

// =============================================================================
// DIFF OPERATOR: Creates DiffCall expressions
// =============================================================================

// The diff_ operator that creates symbolic differentiation calls
struct DiffOp {
  // diff_(expr, var) creates a DiffCall<Expr, Var>
  template <Symbolic E, Symbolic V>
  constexpr auto operator()(E, V) const {
    return DiffCall<E, V>{};
  }
};

inline constexpr DiffOp diff_;

// =============================================================================
// SUBSTITUTION: Extend to handle VarPlaceholder and DiffCall
// =============================================================================

namespace detail {

// Substitute in VarPlaceholder - leave unchanged (will be replaced during eval)
template <typename Context>
constexpr auto substitute_impl(VarPlaceholder, Context) {
  return VarPlaceholder{};
}

// Substitute in DiffCall - recursively substitute in both arguments
template <Symbolic E, Symbolic V, typename Context>
constexpr auto substitute_impl(DiffCall<E, V>, Context ctx) {
  return DiffCall<decltype(substitute_impl(E{}, ctx)),
                  decltype(substitute_impl(V{}, ctx))>{};
}

}  // namespace detail

// =============================================================================
// EVALUATION: Walk expression tree and evaluate DiffCall nodes
// =============================================================================

namespace detail {

// Evaluate constants - return unchanged
template <auto V, typename RecursiveFn, typename Var>
constexpr auto evaluateDiffCalls(Constant<V>, RecursiveFn, Var) -> Constant<V> {
  return {};
}

// Evaluate symbols - return unchanged
template <typename U, typename RecursiveFn, typename Var>
constexpr auto evaluateDiffCalls(Symbol<U>, RecursiveFn, Var) -> Symbol<U> {
  return {};
}

// Evaluate pattern variables - return unchanged (should be substituted already)
template <typename U, typename RecursiveFn, typename Var>
constexpr auto evaluateDiffCalls(PatternVar<U>, RecursiveFn, Var)
    -> PatternVar<U> {
  return {};
}

// Evaluate VarPlaceholder - replace with actual var
template <typename RecursiveFn, typename ActualVar>
constexpr auto evaluateDiffCalls(VarPlaceholder, RecursiveFn,
                                 ActualVar actual_var) -> ActualVar {
  return actual_var;
}

// Forward declare DiffCall specialization
template <Symbolic E, Symbolic V, typename RecursiveFn, typename ActualVar>
constexpr auto evaluateDiffCalls(DiffCall<E, V>, RecursiveFn recursive_fn,
                                 ActualVar actual_var);

// Evaluate expressions - recursively evaluate arguments
template <typename Op, Symbolic... Args, typename RecursiveFn, typename Var>
constexpr auto evaluateDiffCalls(Expression<Op, Args...>,
                                 RecursiveFn recursive_fn, Var var) {
  return Expression<Op, decltype(evaluateDiffCalls(Args{}, recursive_fn,
                                                   var))...>{};
}

// Evaluate a DiffCall - this is where the magic happens!
// Replace DiffCall<Expr, Var> with recursive_fn(eval(Expr), eval(Var))
template <Symbolic E, Symbolic V, typename RecursiveFn, typename ActualVar>
constexpr auto evaluateDiffCalls(DiffCall<E, V>, RecursiveFn recursive_fn,
                                 ActualVar actual_var) {
  // First, recursively evaluate any nested DiffCalls in the arguments
  auto evaluated_expr = evaluateDiffCalls(E{}, recursive_fn, actual_var);
  auto evaluated_var = evaluateDiffCalls(V{}, recursive_fn, actual_var);

  // Then call the recursive differentiation function
  return recursive_fn(evaluated_expr, evaluated_var);
}

// Check if an expression contains any DiffCall nodes (for optimization)
template <Symbolic S>
constexpr bool containsDiffCalls();

template <typename U>
constexpr bool containsDiffCalls_impl(PatternVar<U>) {
  return false;
}

template <auto V>
constexpr bool containsDiffCalls_impl(Constant<V>) {
  return false;
}

template <typename U>
constexpr bool containsDiffCalls_impl(Symbol<U>) {
  return false;
}

constexpr bool containsDiffCalls_impl(VarPlaceholder) { return false; }

template <Symbolic E, Symbolic V>
constexpr bool containsDiffCalls_impl(DiffCall<E, V>) {
  return true;
}

template <typename Op, Symbolic... Args>
constexpr bool containsDiffCalls_impl(Expression<Op, Args...>) {
  return (containsDiffCalls_impl(Args{}) || ...);
}

template <Symbolic S>
constexpr bool containsDiffCalls() {
  return containsDiffCalls_impl(S{});
}

}  // namespace detail

// =============================================================================
// EXTENDED RECURSIVE REWRITE: Support pure symbolic notation
// =============================================================================

// Specialization of RecursiveRewrite that handles symbolic replacements
// containing DiffCall nodes
//
// When the replacement is a pure symbolic expression (not a lambda),
// this version performs two-phase evaluation:
// 1. Substitute pattern variables (f_, g_, etc.) with matched expressions
// 2. Evaluate DiffCall nodes by calling recursive_fn

template <typename Pattern, typename Replacement, typename Predicate>
struct SymbolicRecursiveRewrite {
  Pattern pattern;
  Replacement replacement;
  [[no_unique_address]] Predicate predicate = {};

  // Check if pattern matches expression and predicate holds
  template <Symbolic S>
  static constexpr bool matches(S expr) {
    if constexpr (!match(Pattern{}, expr)) {
      return false;
    } else {
      auto bindings_ctx = detail::extractBindings(Pattern{}, expr);
      if constexpr (detail::isBindingFailure<decltype(bindings_ctx)>()) {
        return false;
      } else {
        return Predicate{}(bindings_ctx);
      }
    }
  }

  // Apply with a recursive function
  template <Symbolic S, typename RecursiveFn, typename... Args>
  static constexpr auto apply([[maybe_unused]] S expr, RecursiveFn recursive_fn,
                              [[maybe_unused]] Args... args) {
    static_assert(
        sizeof...(Args) <= 1,
        "SymbolicRecursiveRewrite expects at most one additional arg (var)");

    if constexpr (!match(Pattern{}, expr)) {
      return expr;  // Pattern doesn't match
    } else {
      auto bindings_ctx = detail::extractBindings(Pattern{}, expr);

      if constexpr (detail::isBindingFailure<decltype(bindings_ctx)>()) {
        return expr;  // Binding failed
      } else {
        if constexpr (!Predicate{}(bindings_ctx)) {
          return expr;  // Predicate failed
        } else {
          // Phase 1: Substitute pattern variables
          constexpr auto substituted = substitute(Replacement{}, bindings_ctx);

          // Phase 2: Evaluate DiffCall nodes if present
          if constexpr (detail::containsDiffCalls<decltype(substituted)>()) {
            // Extract var from args (if provided)
            if constexpr (sizeof...(Args) == 1) {
              return detail::evaluateDiffCalls(substituted, recursive_fn,
                                               (args, ...));
            } else {
              // No var provided - just return substituted (shouldn't happen)
              return substituted;
            }
          } else {
            // No DiffCall nodes - just return substituted result
            return substituted;
          }
        }
      }
    }
  }
};

// Deduction guides
template <typename P, typename R>
SymbolicRecursiveRewrite(P, R) -> SymbolicRecursiveRewrite<P, R, NoPredicate>;

template <typename P, typename R, typename Pred>
SymbolicRecursiveRewrite(P, R, Pred) -> SymbolicRecursiveRewrite<P, R, Pred>;

// =============================================================================
// CONVENIENCE: Automatically choose RecursiveRewrite or
// SymbolicRecursiveRewrite
// =============================================================================

// Helper to detect if a replacement is a callable (lambda)
namespace detail {
template <typename R>
concept CallableReplacement = requires(R r) {
  // Has operator() with at least 3 parameters (ctx, diff_fn, var)
  {
    r(std::declval<EmptyContext>(), std::declval<void (*)()>(),
      std::declval<VarPlaceholder>())
  };
};
}  // namespace detail

// Smart constructor that chooses the right rewrite type
template <typename Pattern, typename Replacement,
          typename Predicate = NoPredicate>
constexpr auto makeRecursiveRewrite(Pattern p, Replacement r,
                                    Predicate pred = {}) {
  if constexpr (detail::CallableReplacement<Replacement>) {
    // Use standard RecursiveRewrite for lambdas
    return RecursiveRewrite{p, r, pred};
  } else {
    // Use SymbolicRecursiveRewrite for pure symbolic expressions
    return SymbolicRecursiveRewrite{p, r, pred};
  }
}

}  // namespace tempura
