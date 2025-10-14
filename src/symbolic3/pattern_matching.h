#pragma once

#include "meta/type_list.h"  // For Get_t
#include "meta/utility.h"
#include "symbolic3/core.h"
#include "symbolic3/matching.h"
#include "symbolic3/ordering.h"

// ============================================================================
// PATTERN MATCHING & REWRITING SYSTEM
// ============================================================================
//
// Provides compile-time pattern matching with variable capture and
// substitution for symbolic expressions. Enables declarative transformation
// rules in the style of term rewriting systems.
//
// KEY COMPONENTS:
// ---------------
// 1. PatternVar - Pattern variables (x_, y_, z_, etc.) that capture and bind
//    subexpressions during matching
//
// 2. BindingContext - Compile-time heterogeneous map storing pattern variable
//    bindings (TypeId → bound expression)
//
// 3. Rewrite - Single rewrite rule with pattern, replacement, and optional
//    predicate: Rewrite{pattern, replacement, predicate}
//
// 4. RewriteSystem - Collection of rewrite rules applied in order until first
//    match succeeds
//
// EXAMPLE USAGE:
// --------------
//   // Define a rewrite rule: x^0 → 1
//   constexpr auto rule = Rewrite{pow(x_, 0_c), 1_c};
//
//   // Apply the rule
//   constexpr auto result = rule.apply(pow(y, 0_c), default_context());
//   // result == 1_c, with x_ bound to y
//
//   // Conditional rewrite with predicate
//   constexpr auto comm_rule = Rewrite{
//       x_ + y_,
//       y_ + x_,
//       [](auto ctx) { return lessThan(get(ctx, y_), get(ctx, x_)); }
//   };
//
// STRATEGY INTERFACE:
// -------------------
// Both Rewrite and RewriteSystem implement the Strategy concept:
//   - apply(expr, context) - Try to rewrite expr, return original if no match
//   - Can be composed with other strategies using operators (>>, |, etc.)
//
// PERFORMANCE NOTES:
// ------------------
// - All operations are compile-time when used with constexpr expressions
// - BindingContext uses index-based lookup (O(N) but optimized by compiler)
// - Pattern matching uses structural recursion through expression trees
//
// See symbolic3/README.md for comprehensive documentation and examples.

namespace tempura::symbolic3 {

// =============================================================================
// PATTERN VARIABLES - Capture and bind subexpressions during matching
// =============================================================================

// Pattern variable: uses lambda trick for unique type identity
template <typename unique = decltype([] {})>
struct PatternVar : SymbolicTag {
  static constexpr auto id = kMeta<PatternVar<unique>>;
  constexpr PatternVar() { (void)id; }
};

// Predefined pattern variables for rewrite rules
inline constexpr PatternVar x_;
inline constexpr PatternVar y_;
inline constexpr PatternVar z_;
inline constexpr PatternVar a_;
inline constexpr PatternVar b_;
inline constexpr PatternVar c_;
inline constexpr PatternVar n_;
inline constexpr PatternVar m_;

// Unicode wildcards (already defined in core.h as AnyArg, etc.)
inline constexpr AnyArg 𝐚𝐧𝐲{};
inline constexpr AnyExpr 𝐚𝐧𝐲𝐞𝐱𝐩𝐫{};
inline constexpr AnyConstant 𝐜{};

// =============================================================================
// PATTERN MATCHING - PatternVar matches any expression
// =============================================================================

// PatternVar matches any symbolic expression (captures it for binding)
template <typename Unique, Symbolic S>
constexpr bool match(PatternVar<Unique>, S) {
  return true;
}

// Special case: PatternVar should NOT match Never
// (Never represents "no value", so it shouldn't bind to pattern variables)
template <typename Unique>
constexpr bool match(PatternVar<Unique>, Never) {
  return false;
}

// =============================================================================
// BINDING CONTEXT - Compile-time heterogeneous map
// =============================================================================

namespace detail {

// Entry in the binding map: PatternVar ID → bound expression
template <TypeId VarId, typename BoundExpr>
struct BindingEntry {
  static constexpr TypeId var_id = VarId;
  using bound_expr = BoundExpr;
};

// Context holding all bindings
template <typename... Entries>
struct BindingContext {
  static constexpr int size = sizeof...(Entries);

  // Lookup a pattern variable's bound expression
  // Returns the bound expression if found, otherwise returns the PatternVar
  // itself (unbound variables act as identity in substitution)
  template <TypeId VarId>
  static constexpr auto lookup() {
    if constexpr (sizeof...(Entries) == 0) {
      return PatternVar<TypeOf<VarId>>{};  // Empty context: nothing bound
    } else {
      return lookupImpl<VarId, 0>();  // Search from start
    }
  }

  // Check if a pattern variable is bound in this context
  // Used by predicates to verify pattern matches before substitution
  template <TypeId VarId>
  static constexpr bool isBound() {
    if constexpr (sizeof...(Entries) == 0) {
      return false;  // Empty context: nothing bound
    } else {
      return isBoundImpl<VarId, 0>();  // Check from start
    }
  }

 private:
  // Index-based lookup - linear search through entries at compile-time
  //
  // IMPLEMENTATION NOTES:
  // - While O(N) in theory, the compiler optimizes this aggressively
  // - Common case: N < 10 pattern variables, so linear search is fast
  // - Compiler may optimize to jump table or unrolled comparisons
  // - Uses Get_t<Idx, Entries...> from meta/type_list.h for type-level indexing
  //
  // ALGORITHM:
  // 1. Get entry at current index using Get_t
  // 2. Compare entry's var_id with search key VarId
  // 3. If match: return bound expression
  // 4. If no match: recurse to next index (tail recursion)
  // 5. If exhausted: return unbound PatternVar
  template <TypeId VarId, SizeT Idx>
  static constexpr auto lookupImpl() {
    if constexpr (Idx >= sizeof...(Entries)) {
      // Base case: exhausted all entries without finding VarId
      return PatternVar<TypeOf<VarId>>{};  // Return unbound variable
    } else {
      using CurrentEntry = Get_t<Idx, Entries...>;  // Type-level array access
      if constexpr (CurrentEntry::var_id == VarId) {
        // Found the binding: return the bound expression type
        return typename CurrentEntry::bound_expr{};
      } else {
        // Not found yet: tail-recurse to next index
        return lookupImpl<VarId, Idx + 1>();
      }
    }
  }

  // Check if variable is bound - same algorithm as lookupImpl but returns bool
  template <TypeId VarId, SizeT Idx>
  static constexpr bool isBoundImpl() {
    if constexpr (Idx >= sizeof...(Entries)) {
      return false;  // Base case: not found
    } else {
      using CurrentEntry = Get_t<Idx, Entries...>;
      if constexpr (CurrentEntry::var_id == VarId) {
        return true;  // Found: variable is bound
      } else {
        return isBoundImpl<VarId, Idx + 1>();  // Keep searching
      }
    }
  }
};

using EmptyContext = BindingContext<>;

// Marker for binding failure
struct BindingFailure {};

template <typename T>
constexpr bool isBindingFailure() {
  return __is_same(T, BindingFailure);
}

}  // namespace detail

// Get binding value for a pattern variable from context
template <typename Unique, typename... Entries>
constexpr auto get(detail::BindingContext<Entries...> ctx, PatternVar<Unique>) {
  return ctx.template lookup<PatternVar<Unique>::id>();
}

// =============================================================================
// SUBSTITUTION - Replace pattern variables with bound values
// =============================================================================

namespace detail {

// Substitute pattern variable - lookup in context
template <typename Unique, typename... Entries>
constexpr auto substituteImpl(PatternVar<Unique>,
                              BindingContext<Entries...> ctx) {
  return ctx.template lookup<PatternVar<Unique>::id>();
}

// Substitute constant - return unchanged
template <auto V, typename Context>
constexpr auto substituteImpl(Constant<V>, Context) {
  return Constant<V>{};
}

// Substitute symbol - return unchanged
template <typename U, typename Context>
constexpr auto substituteImpl(Symbol<U>, Context) {
  return Symbol<U>{};
}

// Substitute expression - recursively substitute arguments
template <typename Op, Symbolic... Args, typename Context>
constexpr auto substituteImpl(Expression<Op, Args...>, Context ctx) {
  return Expression<Op, decltype(substituteImpl(Args{}, ctx))...>{};
}

// Wildcards - return unchanged
template <typename Context>
constexpr auto substituteImpl(AnyArg, Context) {
  return AnyArg{};
}

template <typename Context>
constexpr auto substituteImpl(AnyExpr, Context) {
  return AnyExpr{};
}

template <typename Context>
constexpr auto substituteImpl(AnyConstant, Context) {
  return AnyConstant{};
}

}  // namespace detail

// Public substitution API
template <Symbolic Expr, typename Context>
constexpr auto substitute(Expr expr, Context ctx) {
  return detail::substituteImpl(expr, ctx);
}

// =============================================================================
// BINDING EXTRACTION - Extract bindings from pattern match
// =============================================================================

namespace detail {

// Extract bindings by walking pattern and expression together
// Returns BindingFailure if pattern variable binds to different expressions

// PatternVar: bind it or check consistency
template <typename Unique, Symbolic S, typename... Entries>
constexpr auto extractBindingsImpl(PatternVar<Unique>, S expr,
                                   BindingContext<Entries...> ctx) {
  constexpr TypeId varId = PatternVar<Unique>::id;

  if constexpr (ctx.template isBound<varId>()) {
    // Already bound - check consistency
    auto existing = ctx.template lookup<varId>();
    if constexpr (match(existing, expr)) {
      return ctx;
    } else {
      return BindingFailure{};
    }
  } else {
    // Bind the variable
    return BindingContext<Entries..., BindingEntry<varId, S>>{};
  }
}

// Constant/Symbol: must match exactly
template <auto V, Symbolic S, typename Context>
constexpr auto extractBindingsImpl(Constant<V>, S expr, Context ctx) {
  if constexpr (match(Constant<V>{}, expr)) {
    return ctx;
  } else {
    return BindingFailure{};
  }
}

template <typename U, Symbolic S, typename Context>
constexpr auto extractBindingsImpl(Symbol<U>, S expr, Context ctx) {
  if constexpr (match(Symbol<U>{}, expr)) {
    return ctx;
  } else {
    return BindingFailure{};
  }
}

// Helper to get Nth argument from an expression at compile-time
template <SizeT N, typename Op, Symbolic... Args>
constexpr auto getNthArg(Expression<Op, Args...>) {
  return Get_t<N, Args...>{};
}

// Forward declaration for mutual recursion
template <SizeT Idx, typename Op, Symbolic... PatternArgs, typename ExprOp,
          Symbolic... ExprArgs, typename Context>
constexpr auto extractBindingsThreaded(Expression<Op, PatternArgs...> pattern,
                                       Expression<ExprOp, ExprArgs...> expr,
                                       Context ctx);

// Helper to thread context through argument pairs (index-based)
template <SizeT Idx, typename Op, Symbolic... PatternArgs, typename ExprOp,
          Symbolic... ExprArgs, typename Context>
constexpr auto extractBindingsThreaded(Expression<Op, PatternArgs...> pattern,
                                       Expression<ExprOp, ExprArgs...> expr,
                                       Context ctx) {
  if constexpr (Idx >= sizeof...(PatternArgs)) {
    return ctx;  // Done with all arguments
  } else {
    // Extract bindings for argument Idx
    auto new_ctx =
        extractBindingsImpl(getNthArg<Idx>(pattern), getNthArg<Idx>(expr), ctx);

    // Check if binding failed
    if constexpr (isBindingFailure<decltype(new_ctx)>()) {
      return BindingFailure{};
    } else {
      // Thread to next argument
      return extractBindingsThreaded<Idx + 1>(pattern, expr, new_ctx);
    }
  }
}

// Expression: recursively extract bindings from all arguments
template <typename Op, Symbolic... PatternArgs, typename ExprOp,
          Symbolic... ExprArgs, typename Context>
constexpr auto extractBindingsImpl(Expression<Op, PatternArgs...> pattern,
                                   Expression<ExprOp, ExprArgs...> expr,
                                   Context ctx) {
  if constexpr (!isSame<Op, ExprOp>) {
    return BindingFailure{};  // Operations don't match
  } else if constexpr (sizeof...(PatternArgs) != sizeof...(ExprArgs)) {
    return BindingFailure{};  // Argument counts don't match
  } else {
    // Thread the context through each argument pair
    return extractBindingsThreaded<0>(pattern, expr, ctx);
  }
}

// Wildcards: always match, no binding
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

}  // namespace detail

// Public API to extract bindings
template <Symbolic Pattern, Symbolic Expr>
constexpr auto extractBindings(Pattern pattern, Expr expr) {
  return detail::extractBindingsImpl(pattern, expr, detail::EmptyContext{});
}

// =============================================================================
// COMPOSABLE PREDICATE SYSTEM
// =============================================================================
//
// Predicates can be composed using logical operators to build complex
// conditions from simple building blocks.
//
// USAGE EXAMPLES:
//   // Simple predicate
//   auto pred1 = [](auto ctx) { return is_constant(get(ctx, x_)); };
//
//   // Composed predicates
//   auto pred2 = pred1 && [](auto ctx) { return get(ctx, x_) != 0; };
//   auto pred3 = pred1 || pred2;
//   auto pred4 = !pred1;
//
//   // Use in rewrite rules
//   Rewrite{pattern, replacement, pred2}

namespace predicates {

// Base predicate concept - any callable that takes Context and returns bool
template <typename P>
concept PredicateLike = requires(P pred) {
  { pred(kDeclVal<P>()) };
  requires requires(P pred) {
    { pred(kDeclVal<P>()) } -> Invocable;
  };
};

// Logical AND combinator
template <typename Pred1, typename Pred2>
struct AndPredicate {
  [[no_unique_address]] Pred1 pred1{};
  [[no_unique_address]] Pred2 pred2{};

  template <typename Context>
  constexpr bool operator()(Context ctx) const {
    return pred1(ctx) && pred2(ctx);
  }
};

// Logical OR combinator
template <typename Pred1, typename Pred2>
struct OrPredicate {
  [[no_unique_address]] Pred1 pred1{};
  [[no_unique_address]] Pred2 pred2{};

  template <typename Context>
  constexpr bool operator()(Context ctx) const {
    return pred1(ctx) || pred2(ctx);
  }
};

// Logical NOT combinator
template <typename Pred>
struct NotPredicate {
  [[no_unique_address]] Pred pred{};

  template <typename Context>
  constexpr bool operator()(Context ctx) const {
    return !pred(ctx);
  }
};

// Deduction guides
template <typename P1, typename P2>
AndPredicate(P1, P2) -> AndPredicate<P1, P2>;

template <typename P1, typename P2>
OrPredicate(P1, P2) -> OrPredicate<P1, P2>;

template <typename P>
NotPredicate(P) -> NotPredicate<P>;

// =============================================================================
// PREDICATE OPERATORS
// =============================================================================

// Logical AND: pred1 && pred2
template <typename Pred1, typename Pred2>
constexpr auto operator&&(Pred1 pred1, Pred2 pred2) {
  return AndPredicate{pred1, pred2};
}

// Logical OR: pred1 || pred2
template <typename Pred1, typename Pred2>
constexpr auto operator||(Pred1 pred1, Pred2 pred2) {
  return OrPredicate{pred1, pred2};
}

// Logical NOT: !pred
template <typename Pred>
constexpr auto operator!(Pred pred) {
  return NotPredicate{pred};
}

// =============================================================================
// COMMON PREDICATE BUILDERS
// =============================================================================

// Check if bound pattern variable is a constant
template <typename PatternVar>
struct IsConstant {
  [[no_unique_address]] PatternVar var{};

  template <typename Context>
  constexpr bool operator()(Context ctx) const {
    return is_constant<decltype(get(ctx, var))>;
  }
};

// Check if bound pattern variable is a symbol
template <typename PatternVar>
struct IsSymbol {
  [[no_unique_address]] PatternVar var{};

  template <typename Context>
  constexpr bool operator()(Context ctx) const {
    return is_symbol<decltype(get(ctx, var))>;
  }
};

// Check if bound pattern variable is an expression
template <typename PatternVar>
struct IsExpression {
  [[no_unique_address]] PatternVar var{};

  template <typename Context>
  constexpr bool operator()(Context ctx) const {
    return is_expression<decltype(get(ctx, var))>;
  }
};

// Check if two bound pattern variables satisfy an ordering
template <typename PatternVar1, typename PatternVar2, typename Comparator>
struct ComparePredicate {
  [[no_unique_address]] PatternVar1 var1{};
  [[no_unique_address]] PatternVar2 var2{};
  [[no_unique_address]] Comparator comp{};

  template <typename Context>
  constexpr bool operator()(Context ctx) const {
    return comp(get(ctx, var1), get(ctx, var2));
  }
};

// =============================================================================
// PREDICATE FACTORY FUNCTIONS
// =============================================================================
// Note: These predicates work with pattern variable bindings, not raw
// symbolic expressions. For expression-level predicates, see matching.h

// var_is_constant(x_) - Check if bound pattern variable is a constant
template <typename PatternVar>
constexpr auto var_is_constant(PatternVar var) {
  return IsConstant<PatternVar>{var};
}

// var_is_symbol(x_) - Check if bound pattern variable is a symbol
template <typename PatternVar>
constexpr auto var_is_symbol(PatternVar var) {
  return IsSymbol<PatternVar>{var};
}

// var_is_expression(x_) - Check if bound pattern variable is an expression
template <typename PatternVar>
constexpr auto var_is_expression(PatternVar var) {
  return IsExpression<PatternVar>{var};
}

// var_compare(x_, y_, comp) - Compare two bound pattern variables using
// comparator
template <typename PatternVar1, typename PatternVar2, typename Comparator>
constexpr auto var_compare(PatternVar1 var1, PatternVar2 var2,
                           Comparator comp) {
  return ComparePredicate<PatternVar1, PatternVar2, Comparator>{var1, var2,
                                                                comp};
}

// var_less_than(x_, y_) - Check if bound x < bound y
template <typename PatternVar1, typename PatternVar2>
constexpr auto var_less_than(PatternVar1 var1, PatternVar2 var2) {
  return var_compare(var1, var2, [](auto a, auto b) { return lessThan(a, b); });
}

// var_greater_than(x_, y_) - Check if bound x > bound y
template <typename PatternVar1, typename PatternVar2>
constexpr auto var_greater_than(PatternVar1 var1, PatternVar2 var2) {
  return var_compare(var1, var2,
                     [](auto a, auto b) { return greaterThan(a, b); });
}

// var_equal_to(x_, y_) - Check if bound x == bound y (structural equality)
template <typename PatternVar1, typename PatternVar2>
constexpr auto var_equal_to(PatternVar1 var1, PatternVar2 var2) {
  return var_compare(var1, var2, [](auto a, auto b) { return match(a, b); });
}

// var_not_equal_to(x_, y_) - Check if bound x != bound y
template <typename PatternVar1, typename PatternVar2>
constexpr auto var_not_equal_to(PatternVar1 var1, PatternVar2 var2) {
  return !var_equal_to(var1, var2);
}

// =============================================================================
// PREDICATE BUILDERS FOR LITERAL COMPARISONS
// =============================================================================

// Compare bound pattern variable with literal value using lessThan
template <typename PatternVar, Symbolic Literal>
struct VarLessThanLiteral {
  [[no_unique_address]] PatternVar var{};
  [[no_unique_address]] Literal lit{};

  template <typename Context>
  constexpr bool operator()(Context ctx) const {
    return lessThan(get(ctx, var), lit);
  }
};

// Compare bound pattern variable with literal value using greaterThan
template <typename PatternVar, Symbolic Literal>
struct VarGreaterThanLiteral {
  [[no_unique_address]] PatternVar var{};
  [[no_unique_address]] Literal lit{};

  template <typename Context>
  constexpr bool operator()(Context ctx) const {
    return greaterThan(get(ctx, var), lit);
  }
};

// Compare bound pattern variable with literal value using match
template <typename PatternVar, Symbolic Literal>
struct VarEqualToLiteral {
  [[no_unique_address]] PatternVar var{};
  [[no_unique_address]] Literal lit{};

  template <typename Context>
  constexpr bool operator()(Context ctx) const {
    return match(get(ctx, var), lit);
  }
};

// var_less_than_literal(x_, 5_c) - Check if bound x < 5
template <typename PatternVar, Symbolic Literal>
constexpr auto var_less_than_literal(PatternVar var, Literal lit) {
  return VarLessThanLiteral<PatternVar, Literal>{var, lit};
}

// var_greater_than_literal(x_, 0_c) - Check if bound x > 0
template <typename PatternVar, Symbolic Literal>
constexpr auto var_greater_than_literal(PatternVar var, Literal lit) {
  return VarGreaterThanLiteral<PatternVar, Literal>{var, lit};
}

// var_equal_to_literal(x_, 5_c) - Check if bound x == 5
template <typename PatternVar, Symbolic Literal>
constexpr auto var_equal_to_literal(PatternVar var, Literal lit) {
  return VarEqualToLiteral<PatternVar, Literal>{var, lit};
}

}  // namespace predicates

// =============================================================================
// NO PREDICATE - Default for rewrite rules without conditions
// =============================================================================

struct NoPredicate {
  template <typename Context>
  constexpr bool operator()(Context) const {
    return true;
  }
};

// =============================================================================
// REWRITE - Single rewrite rule with optional predicate (also a Strategy!)
// =============================================================================

template <typename Pattern, typename Replacement,
          typename Predicate = NoPredicate>
struct Rewrite {
  [[no_unique_address]] Pattern pattern{};
  [[no_unique_address]] Replacement replacement{};
  [[no_unique_address]] Predicate predicate{};

  // Check if pattern matches and predicate holds
  template <Symbolic S>
  static constexpr bool matches(S expr) {
    if constexpr (!match(Pattern{}, expr)) {
      return false;
    } else {
      auto bindings_ctx = extractBindings(Pattern{}, expr);
      if constexpr (detail::isBindingFailure<decltype(bindings_ctx)>()) {
        return false;
      } else {
        return Predicate{}(bindings_ctx);
      }
    }
  }

  // Strategy interface: apply(expr, context)
  // Note: transform_ctx is the SimplificationMode context
  // bindings_ctx is the pattern variable bindings
  template <Symbolic S, typename TransformCtx>
  constexpr auto apply(S expr, TransformCtx) const {
    return applyImpl(expr);
  }

  // Legacy static interface for compatibility
  template <Symbolic S>
  static constexpr auto apply(S expr) {
    return Rewrite{}.applyImpl(expr);
  }

 private:
  // Core implementation shared by both apply methods
  template <Symbolic S>
  constexpr auto applyImpl(S expr) const {
    if constexpr (!match(Pattern{}, expr)) {
      return expr;
    } else {
      auto bindings_ctx = extractBindings(Pattern{}, expr);
      if constexpr (detail::isBindingFailure<decltype(bindings_ctx)>()) {
        return expr;
      } else {
        if constexpr (!Predicate{}(bindings_ctx)) {
          return expr;
        } else {
          return substitute(Replacement{}, bindings_ctx);
        }
      }
    }
  }
};

// Deduction guides
template <typename P, typename R>
Rewrite(P, R) -> Rewrite<P, R, NoPredicate>;

template <typename P, typename R, typename Pred>
Rewrite(P, R, Pred) -> Rewrite<P, R, Pred>;

// =============================================================================
// REWRITE SYSTEM - Apply multiple rewrite rules (also a Strategy!)
// =============================================================================

template <typename... Rules>
struct RewriteSystem {
  constexpr RewriteSystem(Rules...) {}

  // Strategy interface: apply(expr, context)
  template <Symbolic S, typename Context, int Index = 0>
  constexpr auto apply(S expr, Context ctx) const {
    if constexpr (Index >= sizeof...(Rules)) {
      return expr;
    } else {
      using CurrentRule = Get_t<Index, Rules...>;
      if constexpr (CurrentRule::matches(expr)) {
        return CurrentRule{}.apply(expr, ctx);
      } else {
        return apply<S, Context, Index + 1>(expr, ctx);
      }
    }
  }

  // Legacy static interface for compatibility
  template <Symbolic S, int Index = 0>
  static constexpr auto apply(S expr) {
    if constexpr (Index >= sizeof...(Rules)) {
      return expr;
    } else {
      using CurrentRule = Get_t<Index, Rules...>;
      if constexpr (CurrentRule::matches(expr)) {
        return CurrentRule::apply(expr);
      } else {
        return apply<S, Index + 1>(expr);
      }
    }
  }
};

template <typename... Rules>
RewriteSystem(Rules...) -> RewriteSystem<Rules...>;

// =============================================================================
// COMPOSE - Combine multiple RewriteSystems
// =============================================================================

namespace detail {
template <typename... Rules>
constexpr auto composeImpl(RewriteSystem<Rules...>) {
  return RewriteSystem<Rules...>{Rules{}...};
}

template <typename... Rules1, typename... Rules2, typename... Rest>
constexpr auto composeImpl(RewriteSystem<Rules1...>, RewriteSystem<Rules2...>,
                           Rest... rest) {
  auto combined = RewriteSystem<Rules1..., Rules2...>{Rules1{}..., Rules2{}...};
  if constexpr (sizeof...(Rest) > 0) {
    return composeImpl(combined, rest...);
  } else {
    return combined;
  }
}
}  // namespace detail

template <typename... Systems>
constexpr auto compose(Systems... systems) {
  return detail::composeImpl(systems...);
}

}  // namespace tempura::symbolic3
