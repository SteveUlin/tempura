#pragma once

#include "symbolic3/core.h"
#include "symbolic3/matching.h"

// Pattern-based expression rewriting with variable capture and substitution
//
// Simplified migration from symbolic2/pattern_matching.h
// Enables declarative transformation rules like:
//   Rewrite{pow(x_, Constant<0>{}), Constant<1>{}}  // x^0 → 1

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
  template <TypeId VarId>
  static constexpr auto lookup() {
    return lookupImpl<VarId, Entries...>();
  }

  // Check if a pattern variable is bound
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
      return PatternVar<TypeOf<VarId>>{};
    }
  }

  template <TypeId VarId>
  static constexpr auto lookupImpl() {
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

using EmptyContext = BindingContext<>;

// Marker for binding failure
struct BindingFailure {};

template <typename T>
constexpr bool isBindingFailure() {
  return __is_same(T, BindingFailure);
}

// Helper to get Nth type from parameter pack
template <int N, typename First, typename... Rest>
struct GetNthType {
  using type = typename GetNthType<N - 1, Rest...>::type;
};

template <typename First, typename... Rest>
struct GetNthType<0, First, Rest...> {
  using type = First;
};

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
template <std::size_t N, typename Op, Symbolic... Args>
constexpr auto getNthArg(Expression<Op, Args...>) {
  return typename GetNthType<N, Args...>::type{};
}

// Forward declaration for mutual recursion
template <std::size_t Idx, typename Op, Symbolic... PatternArgs,
          typename ExprOp, Symbolic... ExprArgs, typename Context>
constexpr auto extractBindingsThreaded(Expression<Op, PatternArgs...> pattern,
                                       Expression<ExprOp, ExprArgs...> expr,
                                       Context ctx);

// Helper to thread context through argument pairs (index-based)
template <std::size_t Idx, typename Op, Symbolic... PatternArgs,
          typename ExprOp, Symbolic... ExprArgs, typename Context>
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
  if constexpr (!std::is_same_v<Op, ExprOp>) {
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
  Pattern pattern;
  Replacement replacement;
  [[no_unique_address]] Predicate predicate = {};

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

  // Legacy static interface for compatibility
  template <Symbolic S>
  static constexpr auto apply(S expr) {
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
      using CurrentRule = typename detail::GetNthType<Index, Rules...>::type;
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
      using CurrentRule = typename detail::GetNthType<Index, Rules...>::type;
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
