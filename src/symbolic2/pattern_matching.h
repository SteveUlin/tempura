#pragma once

#include "core.h"
#include "matching.h"
#include "meta/tags.h"

// Advanced pattern matching with variable binding for symbolic expressions
//
// Extends the basic matching system with pattern variables that can capture
// and bind subexpressions. Supports:
// - Pattern variables (x_, y_, n_, etc.) for capturing subexpressions
// - Value-level matching (not just type-level)
// - Substitution (replacing variables in expressions with bindings)
// - Rewrite rules: Pattern → Replacement
// - Rewrite systems: Multiple rules applied sequentially
//
// Current Implementation Status:
// ✓ PatternVar<ID> creation and predefined variables (x_, y_, z_, etc.)
// ✓ Integration with existing match() system via ranked overloads (Rank3)
// ✓ Pattern variables match any expression
// ✓ Substitution for simple cases (substitute(x_, binding))
// ✓ Substitution in expressions (substitute(x_ + y_, binding1, binding2))
// ✓ Rewrite rules with constant replacements (pow(x_, 0) → 1)
// ✓ RewriteSystem for sequential rule application
//
// Limitations & Future Work:
// ⚠ Rewrite::apply returns Replacement{} without extracting bindings
//   - Works for constant replacements like 1_c
//   - Doesn't work for replacements with PatternVars like x_
//   - Need: extractBindings(pattern, expr) → tuple of matched values
// ⚠ No support for repeated variables (x_ * x_ matching only when both same)
// ⚠ No commutative matching (a + b should match b + a)
// ⚠ No associative matching or flattening
// ⚠ No pattern guards or conditions
//
// Example:
//   using PowerZero = Rewrite<decltype(pow(x_, 0_c)), decltype(1_c)>;
//   constexpr auto expr = pow(a, 0_c);
//   constexpr auto result = PowerZero::apply(expr);  // Returns 1_c

namespace tempura {

// =============================================================================
// PATTERN VARIABLES - Capture subexpressions during matching
// =============================================================================

// A pattern variable that matches any subexpression
// Uses the lambda trick to generate unique IDs automatically
template <typename unique = decltype([] {})>
struct PatternVar : SymbolicTag {
  // Use TypeId for ordering - each PatternVar{} gets a unique ID
  static constexpr auto id = kMeta<PatternVar<unique>>;

  constexpr PatternVar() {
    // Force type ID generation to preserve declaration order
    (void)id;
  }
};

// Pre-defined pattern variables for convenient use
// Each gets a unique type via the lambda trick
inline constexpr PatternVar x_;
inline constexpr PatternVar y_;
inline constexpr PatternVar z_;
inline constexpr PatternVar a_;
inline constexpr PatternVar b_;
inline constexpr PatternVar c_;
inline constexpr PatternVar f_;  // For functions in derivative rules
inline constexpr PatternVar g_;  // For functions in derivative rules
inline constexpr PatternVar n_;
inline constexpr PatternVar m_;
inline constexpr PatternVar p_;
inline constexpr PatternVar q_;

// =============================================================================
// HELPER: with_vars - Automatic pattern variable extraction
// =============================================================================

// Helper to reduce boilerplate in RecursiveRewrite lambdas by automatically
// extracting pattern variables from the context.
//
// Instead of writing:
//   [](auto ctx, auto diff_fn, auto var) {
//     constexpr auto f = get(ctx, f_);
//     constexpr auto g = get(ctx, g_);
//     return diff_fn(f, var) + diff_fn(g, var);
//   }
//
// You can write:
//   with_vars<f_, g_>([](auto f, auto g, auto diff_fn, auto var) {
//     return diff_fn(f, var) + diff_fn(g, var);
//   })
//
// The pattern variables are extracted in the order specified in the template
// arguments, then passed to your lambda along with diff_fn and var.

template <typename Lambda, auto... PatternVars>
struct WithVarsHelper {
  Lambda lambda;

  constexpr auto operator()(auto ctx, auto diff_fn, auto var) const {
    // Extract each pattern variable from the context in order
    return lambda(get(ctx, PatternVars)..., diff_fn, var);
  }
};

template <auto... PatternVars>
constexpr auto with_vars(auto lambda) {
  return WithVarsHelper<decltype(lambda), PatternVars...>{lambda};
}

// =============================================================================
// WILDCARD PATTERNS - Match categories of expressions
// =============================================================================

// Predefined wildcard patterns for matching categories
// Using Unicode symbols for visual clarity and mathematical style

// Match any expression - the universal wildcard
inline constexpr AnyArg 𝐚𝐧𝐲{};

// Match any compound expression (not constants/symbols)
inline constexpr AnyExpr 𝐞𝐱𝐩𝐫{};

// Match any constant (numeric literals like 1, 2.5, etc.)
inline constexpr AnyConstant 𝐜{};

// Match any symbol (x, y, z, etc.)
inline constexpr AnySymbol 𝐬{};

// =============================================================================
// PATTERN MATCHING - Integrate with existing match() system
// =============================================================================

// Pattern variables match anything (like AnyArg but with an ID)
// We extend the existing matching system to handle PatternVar

// Add PatternVar matching to the ranked overload system
template <typename Unique, Symbolic S>
constexpr auto matchImpl(Rank3, PatternVar<Unique>, S) -> bool {
  return true;  // Pattern variables match anything
}

template <typename Unique, Symbolic S>
constexpr auto matchImpl(Rank3, S, PatternVar<Unique>) -> bool {
  return true;  // Pattern variables match anything (symmetric)
}

// =============================================================================
// SUBSTITUTION - Replace pattern variables in expressions
// =============================================================================

namespace detail {

// =============================================================================
// COMPILE-TIME BINDING CONTEXT
// =============================================================================

// A binding entry: maps a PatternVar TypeId to a bound expression
template <TypeId VarId, typename BoundExpr>
struct BindingEntry {
  static constexpr TypeId var_id = VarId;
  using bound_expr = BoundExpr;
};

// A compile-time binding context (heterogeneous map from TypeId to expressions)
template <typename... Entries>
struct BindingContext {
  static constexpr std::size_t size = sizeof...(Entries);

  // Add a new binding to the context
  template <TypeId VarId, typename BoundExpr>
  constexpr auto bind(PatternVar<decltype([] {})>, BoundExpr) const {
    return BindingContext<Entries..., BindingEntry<VarId, BoundExpr>>{};
  }

  // Lookup a binding by TypeId
  template <TypeId VarId>
  static constexpr auto lookup() {
    return lookupImpl<VarId, Entries...>();
  }

  // Check if a variable is already bound
  template <TypeId VarId>
  static constexpr bool isBound() {
    return isBoundImpl<VarId, Entries...>();
  }

 private:
  template <TypeId VarId, typename Entry, typename... Rest>
  static constexpr auto lookupImpl() {
    if constexpr (Entry::var_id == VarId) {
      return typename Entry::bound_expr{};
    } else if constexpr (sizeof...(Rest) > 0) {
      return lookupImpl<VarId, Rest...>();
    } else {
      // Not found - return the PatternVar itself (shouldn't happen if bindings
      // are correct)
      return PatternVar<TypeOf<VarId>>{};
    }
  }

  template <TypeId VarId>
  static constexpr auto lookupImpl() {
    // Empty context - return PatternVar
    return PatternVar<TypeOf<VarId>>{};
  }

  template <TypeId VarId, typename Entry, typename... Rest>
  static constexpr bool isBoundImpl() {
    if constexpr (Entry::var_id == VarId) {
      return true;
    } else if constexpr (sizeof...(Rest) > 0) {
      return isBoundImpl<VarId, Rest...>();
    } else {
      return false;
    }
  }

  template <TypeId VarId>
  static constexpr bool isBoundImpl() {
    return false;
  }
};

// Empty context for initial state
using EmptyContext = BindingContext<>;

}  // namespace detail

// Helper function to get binding value for a pattern variable from context
// Usage: get(ctx, x_) returns the expression bound to x_
template <typename Unique, typename... Entries>
constexpr auto get(detail::BindingContext<Entries...> ctx, PatternVar<Unique>) {
  return ctx.template lookup<PatternVar<Unique>::id>();
}

// Substitute pattern variables with bound values using a context
// Usage: substitute(expr, context)
//
// This is a compile-time operation that builds a new expression
// with pattern variables replaced by concrete values from the context

namespace detail {

// Substitute a pattern variable - lookup in context
template <typename Unique, typename... Entries>
constexpr auto substitute_impl([[maybe_unused]] PatternVar<Unique> var,
                               BindingContext<Entries...> ctx) {
  return ctx.template lookup<PatternVar<Unique>::id>();
}

// Substitute in a constant - return unchanged
template <auto V, typename Context>
constexpr auto substitute_impl(Constant<V>, Context) {
  return Constant<V>{};
}

// Substitute in a symbol - return unchanged
template <typename U, typename Context>
constexpr auto substitute_impl(Symbol<U>, Context) {
  return Symbol<U>{};
}

// Substitute in an expression - recursively substitute arguments
template <typename Op, Symbolic... Args, typename Context>
constexpr auto substitute_impl(Expression<Op, Args...>, Context ctx) {
  return Expression<Op, decltype(substitute_impl(Args{}, ctx))...>{};
}

// Substitute in wildcards - return unchanged
template <typename Context>
constexpr auto substitute_impl(AnyArg, Context) {
  return AnyArg{};
}

template <typename Context>
constexpr auto substitute_impl(AnyExpr, Context) {
  return AnyExpr{};
}

}  // namespace detail

// Public substitution API with context
template <Symbolic Expr, typename Context>
constexpr auto substitute(Expr expr, Context ctx) {
  return detail::substitute_impl(expr, ctx);
}

// Helper to build a binding context from PatternVar/value pairs
// Usage: makeBindings(x_, val1, y_, val2, ...)
namespace detail {
template <typename... Pairs>
constexpr auto makeBindingsImpl() {
  return EmptyContext{};
}

template <typename VarUnique, Symbolic BoundExpr, typename... Rest>
constexpr auto makeBindingsImpl([[maybe_unused]] PatternVar<VarUnique> var,
                                [[maybe_unused]] BoundExpr expr, Rest... rest) {
  auto ctx =
      BindingContext<BindingEntry<PatternVar<VarUnique>::id, BoundExpr>>{};
  if constexpr (sizeof...(Rest) > 0) {
    // Merge with remaining bindings
    return mergeContexts(ctx, makeBindingsImpl(rest...));
  } else {
    return ctx;
  }
}

// Merge two contexts
template <typename... Entries1, typename... Entries2>
constexpr auto mergeContexts(BindingContext<Entries1...>,
                             BindingContext<Entries2...>) {
  return BindingContext<Entries1..., Entries2...>{};
}
}  // namespace detail

template <typename... Pairs>
constexpr auto makeBindings(Pairs... pairs) {
  return detail::makeBindingsImpl(pairs...);
}

// Convenience overload for simple substitution with explicit var/value pairs
// Usage: substitute(expr, x_, val1, y_, val2)
template <Symbolic Expr, typename... Pairs>
  requires(sizeof...(Pairs) % 2 == 0)
constexpr auto substitute(Expr expr, Pairs... pairs) {
  auto ctx = makeBindings(pairs...);
  return detail::substitute_impl(expr, ctx);
}

// =============================================================================
// BINDING EXTRACTION - Extract matched values from pattern variables
// =============================================================================

namespace detail {

// =============================================================================
// BINDING EXTRACTION - Recursively walk pattern and expression
// =============================================================================

// Marker type to indicate binding failure (when repeated PatternVar matches
// different expressions)
struct BindingFailure {};

// Check if a type is BindingFailure
template <typename T>
constexpr bool isBindingFailure() {
  return std::is_same_v<T, BindingFailure>;
}

// Walk pattern and expression together, collecting bindings in a context
// Returns a BindingContext with all matched PatternVars bound to their
// expressions Returns BindingFailure if a PatternVar appears multiple times
// with different bindings

// PatternVar: bind it to the expression in the context
// If already bound, check that the new binding matches the existing one
template <typename Unique, Symbolic S, typename... Entries>
constexpr auto extractBindingsImpl([[maybe_unused]] PatternVar<Unique> var,
                                   S expr, BindingContext<Entries...> ctx) {
  constexpr TypeId varId = PatternVar<Unique>::id;

  // Check if this variable is already bound
  if constexpr (ctx.template isBound<varId>()) {
    // Get the existing binding
    auto existingBinding = ctx.template lookup<varId>();

    // Check if the new expression matches the existing binding
    if constexpr (match(existingBinding, expr)) {
      // Same binding, just return the existing context
      return ctx;
    } else {
      // Different binding for the same variable - pattern match fails!
      return BindingFailure{};
    }
  } else {
    // First time binding this variable
    return BindingContext<Entries..., BindingEntry<varId, S>>{};
  }
}

// Constant: no new bindings
template <auto V, Symbolic S, typename Context>
constexpr auto extractBindingsImpl(Constant<V>, S, Context ctx) {
  return ctx;
}

// Symbol: no new bindings
template <typename U, Symbolic S, typename Context>
constexpr auto extractBindingsImpl(Symbol<U>, S, Context ctx) {
  return ctx;
}

// Wildcards: no new bindings (they match but don't capture)
template <Symbolic S, typename Context>
constexpr auto extractBindingsImpl(AnyArg, S, Context ctx) {
  return ctx;
}

template <Symbolic S, typename Context>
constexpr auto extractBindingsImpl(AnyExpr, S, Context ctx) {
  return ctx;
}

template <Symbolic S, typename Context>
constexpr auto extractBindingsImpl(AnyConstant, S, Context ctx) {
  return ctx;
}

template <Symbolic S, typename Context>
constexpr auto extractBindingsImpl(AnySymbol, S, Context ctx) {
  return ctx;
}

// Helper to get Nth type from parameter pack (compile-time only, no std::)
template <std::size_t N, typename First, typename... Rest>
struct GetNthType {
  using type = typename GetNthType<N - 1, Rest...>::type;
};

template <typename First, typename... Rest>
struct GetNthType<0, First, Rest...> {
  using type = First;
};

}  // namespace detail

// Helper to get the Nth argument from an expression (compile-time only)
template <std::size_t N, typename Op, Symbolic... Args>
constexpr auto getNthArg(Expression<Op, Args...>) {
  return typename detail::GetNthType<N, Args...>::type{};
}

namespace detail {

// Forward declarations for mutual recursion
template <std::size_t Idx, typename Op, Symbolic... PatternArgs,
          typename ExprOp, Symbolic... ExprArgs, typename Context>
constexpr auto extractBindingsThreaded(Expression<Op, PatternArgs...>,
                                       Expression<ExprOp, ExprArgs...> expr,
                                       Context ctx);

// Helper to thread context through argument pairs
template <std::size_t Idx, typename Op, Symbolic... PatternArgs,
          typename ExprOp, Symbolic... ExprArgs, typename Context>
constexpr auto extractBindingsThreaded(Expression<Op, PatternArgs...>,
                                       Expression<ExprOp, ExprArgs...> expr,
                                       Context ctx) {
  if constexpr (Idx >= sizeof...(PatternArgs)) {
    return ctx;  // Done with all arguments
  } else {
    // Extract bindings for argument Idx and thread to next
    auto new_ctx =
        extractBindingsImpl(getNthArg<Idx>(Expression<Op, PatternArgs...>{}),
                            getNthArg<Idx>(expr), ctx);

    // Check if binding failed
    if constexpr (isBindingFailure<decltype(new_ctx)>()) {
      return BindingFailure{};
    } else {
      return extractBindingsThreaded<Idx + 1>(Expression<Op, PatternArgs...>{},
                                              expr, new_ctx);
    }
  }
}

// Expression: recursively collect bindings from all arguments
template <typename Op, Symbolic... PatternArgs, typename ExprOp,
          Symbolic... ExprArgs, typename Context>
constexpr auto extractBindingsImpl(Expression<Op, PatternArgs...>,
                                   Expression<ExprOp, ExprArgs...> expr,
                                   Context ctx) {
  if constexpr (std::is_same_v<Op, ExprOp> &&
                sizeof...(PatternArgs) == sizeof...(ExprArgs)) {
    // Thread the context through each argument pair
    return extractBindingsThreaded<0>(Expression<Op, PatternArgs...>{}, expr,
                                      ctx);
  } else {
    return ctx;  // No match, return unchanged context
  }
}

// Public API: extract bindings from pattern/expression match
template <typename Pattern, Symbolic Expr>
constexpr auto extractBindings(Pattern pattern, Expr expr) {
  return extractBindingsImpl(pattern, expr, EmptyContext{});
}

}  // namespace detail
// REWRITE RULES - Pattern-based transformations with substitution
// =============================================================================

// A rewrite rule: pattern → replacement (with optional predicate)
// When applied, pattern variables in the replacement are substituted
// with the matched values from the expression
//
// Example: Rewrite{pow(x_, 0_c), 1_c}
//   Matches pow(anything, 0) and replaces with 1
//
// Example with predicate: Rewrite{x_ + y_, y_ + x_, [](auto ctx) {
//   return symbolicLessThan(ctx.template lookup<y_.id>(), ctx.template
//   lookup<x_.id>());
// }}
//   Matches x + y and replaces with y + x only if y < x (for canonical
//   ordering)

// Helper type to represent "no predicate" (always true)
struct NoPredicate {
  template <typename Context>
  static constexpr bool operator()(Context) {
    return true;
  }
};

template <typename Pattern, typename Replacement,
          typename Predicate = NoPredicate>
struct Rewrite {
  Pattern pattern;
  Replacement replacement;
  [[no_unique_address]] Predicate predicate = {};

  // Check if pattern matches expression and predicate holds
  template <Symbolic S>
  static constexpr bool matches(S expr) {
    if constexpr (!match(Pattern{}, expr)) {
      return false;
    } else {
      // Extract bindings and check predicate
      auto bindings_ctx = detail::extractBindings(Pattern{}, expr);

      // Check if binding extraction failed (repeated PatternVar with different
      // values)
      if constexpr (detail::isBindingFailure<decltype(bindings_ctx)>()) {
        return false;
      } else {
        return Predicate{}(bindings_ctx);
      }
    }
  }

  // Apply the rewrite rule with substitution
  // This is compile-time pattern matching and substitution
  template <Symbolic S>
  static constexpr auto apply([[maybe_unused]] S expr) {
    if constexpr (!match(Pattern{}, expr)) {
      return expr;  // Pattern doesn't match, return unchanged
    } else {
      // Extract bindings from the pattern match into a context
      auto bindings_ctx = detail::extractBindings(Pattern{}, expr);

      // Check if binding extraction failed
      if constexpr (detail::isBindingFailure<decltype(bindings_ctx)>()) {
        return expr;  // Binding failed, return unchanged
      } else {
        // Check predicate
        if constexpr (!Predicate{}(bindings_ctx)) {
          return expr;  // Predicate failed, return unchanged
        } else {
          // Apply the bindings to the replacement expression
          return substitute(Replacement{}, bindings_ctx);
        }
      }
    }
  }
};

// Deduction guides for clean syntax
template <typename P, typename R>
Rewrite(P, R) -> Rewrite<P, R, NoPredicate>;

template <typename P, typename R, typename Pred>
Rewrite(P, R, Pred) -> Rewrite<P, R, Pred>;

// =============================================================================
// REWRITE SYSTEM - Apply multiple rewrite rules
// =============================================================================

// Apply a sequence of rewrite rules
// Tries each rule in order, returns first match or original expression
template <typename... Rules>
struct RewriteSystem {
  // Constructor that accepts rule values but doesn't store them
  // This enables CTAD: RewriteSystem{rule1, rule2, ...}
  constexpr RewriteSystem(Rules...) {}

  template <Symbolic S, std::size_t Index = 0>
  static constexpr auto apply(S expr) {
    if constexpr (Index >= sizeof...(Rules)) {
      return expr;  // No rules matched
    } else {
      using CurrentRule = typename detail::GetNthType<Index, Rules...>::type;

      if constexpr (CurrentRule::matches(expr)) {
        return CurrentRule::apply(expr);
      } else {
        return apply<S, Index + 1>(expr);
      }
    }
  }
};

// Deduction guide for clean syntax
template <typename... Rules>
RewriteSystem(Rules...) -> RewriteSystem<Rules...>;

// =============================================================================
// USAGE EXAMPLES
// =============================================================================

/*

// Pattern matching with pattern variables:
constexpr auto expr = pow(a, 2_c);
static_assert(match(pow(x_, n_), expr));  // Matches!

// Substitution:
constexpr auto replacement = x_ * x_;
constexpr auto result = substitute(replacement, a);
// result is: a * a

// Rewrite rules (with CTAD - no decltype needed!):
constexpr Rewrite PowerZero{pow(x_, 0_c), 1_c};
constexpr auto expr2 = pow(a, 0_c);
constexpr auto simplified = PowerZero.apply(expr2);
// simplified is: 1_c

// Rewrite rules with predicates for conditional transformations:
constexpr auto CanonicalAdd = Rewrite{
  x_ + y_,
  y_ + x_,
  [](auto ctx) {
    return symbolicLessThan(get(ctx, y_), get(ctx, x_));
  }
};
// Matches x + y and replaces with y + x only if y < x (canonical ordering)

// Rewrite systems (constructor + CTAD - completely decltype-free!):
constexpr auto PowerRules = RewriteSystem{
  Rewrite{pow(x_, 0_c), 1_c},
  Rewrite{pow(x_, 1_c), x_}
};
constexpr auto result = PowerRules.apply(pow(b, 0_c));
// result is: 1_c

*/

}  // namespace tempura
