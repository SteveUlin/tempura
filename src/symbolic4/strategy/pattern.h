#pragma once

#include <type_traits>

#include "meta/type_id.h"
#include "meta/type_list.h"
#include "meta/utility.h"
#include "symbolic4/compressed.h"
#include "symbolic4/core.h"
#include "symbolic4/indexed/dim.h"
#include "symbolic4/operators.h"
#include "symbolic4/simplify/ordering.h"

// ============================================================================
// PATTERN MATCHING FOR SYMBOLIC4
// ============================================================================
//
// Pattern matching for the Atom<Id, Effect> model. Key insight:
//
//   Symbol<X>  = Atom<X, Free>           - Variable, needs binding
//   Literal<T> = Atom<void, Embedded<T>> - Runtime value baked in
//
// CRITICAL: Literals carry runtime values in their Effect.
// When a pattern variable captures a Literal, the binding stores the actual
// Atom object (with its embedded value) to preserve runtime state.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// PATTERN VARIABLES
// ============================================================================

// PatternVar is a SymbolicTag so it can participate in expressions
template <typename unique = decltype([] {})>
struct PatternVar : SymbolicTag {
  static constexpr auto id = kMeta<PatternVar<unique>>;
  constexpr PatternVar() { (void)id; }
};

template <typename T>
struct IsPatternVar : std::false_type {};

template <typename U>
struct IsPatternVar<PatternVar<U>> : std::true_type {};

template <typename T>
constexpr bool is_pattern_var_v = IsPatternVar<T>::value;

// Predefined pattern variables
inline constexpr PatternVar x_;
inline constexpr PatternVar y_;
inline constexpr PatternVar z_;
inline constexpr PatternVar a_;
inline constexpr PatternVar b_;
inline constexpr PatternVar n_;
inline constexpr PatternVar m_;

// ============================================================================
// WILDCARD TYPES
// ============================================================================
//
// Wildcards are Symbolic so they can participate in pattern expressions.
// E.g., diff(AnySymbol{}) creates Expression<DiffOp, AnySymbol>
//
// ============================================================================

struct AnyArg : SymbolicTag {};
struct AnyExpr : SymbolicTag {};
struct AnyConstant : SymbolicTag {};  // Matches Constant<V> and Fraction<N,D>
struct AnyFraction : SymbolicTag {};  // Matches only Fraction<N,D>
struct AnySymbol : SymbolicTag {};
struct AnyLiteral : SymbolicTag {};
struct Never : SymbolicTag {};

// ============================================================================
// MATCH FUNCTIONS
// ============================================================================

// PatternVar matches anything symbolic
template <typename Unique, Symbolic S>
constexpr bool match(PatternVar<Unique>, S) {
  return true;
}

template <typename Unique>
constexpr bool match(PatternVar<Unique>, Never) {
  return false;
}

// Wildcard matching
template <Symbolic S>
constexpr bool match(AnyArg, S) {
  return true;
}

template <Symbolic S>
constexpr bool match(S, AnyArg) {
  return true;
}

template <typename Op, Symbolic... Args>
  requires(sizeof...(Args) > 0)
constexpr bool match(AnyExpr, Expression<Op, Args...>) {
  return true;
}

template <auto V>
constexpr bool match(AnyConstant, Constant<V>) {
  return true;
}

template <long long N, long long D>
constexpr bool match(AnyConstant, Fraction<N, D>) {
  return true;
}

template <long long N, long long D>
constexpr bool match(AnyFraction, Fraction<N, D>) {
  return true;
}

template <typename Id>
constexpr bool match(AnySymbol, Atom<Id, Free>) {
  return true;
}

// IndexedSymbol is also a "symbol" for pattern matching purposes
template <typename Id, typename... Dims>
constexpr bool match(AnySymbol, IndexedSymbol<Id, Dims...>) {
  return true;
}

template <typename T>
constexpr bool match(AnyLiteral, Atom<void, Embedded<T>>) {
  return true;
}

// Exact matching for atoms
template <typename Id1, typename Id2>
constexpr bool match(Atom<Id1, Free>, Atom<Id2, Free>) {
  return isSame<Id1, Id2>;
}

template <typename T1, typename T2>
constexpr bool match(Atom<void, Embedded<T1>>, Atom<void, Embedded<T2>>) {
  return isSame<T1, T2>;
}

template <typename Id1, typename E1, typename Id2, typename E2>
constexpr bool match(Atom<Id1, E1>, Atom<Id2, E2>) {
  return isSame<Atom<Id1, E1>, Atom<Id2, E2>>;
}

// Constants and fractions
template <auto V1, auto V2>
constexpr bool match(Constant<V1>, Constant<V2>) {
  return V1 == V2;
}

template <long long N1, long long D1, long long N2, long long D2>
constexpr bool match(Fraction<N1, D1>, Fraction<N2, D2>) {
  return Fraction<N1, D1>::numerator == Fraction<N2, D2>::numerator &&
         Fraction<N1, D1>::denominator == Fraction<N2, D2>::denominator;
}

// Never matching
constexpr bool match(Never, Never) {
  return false;
}

template <Symbolic S>
  requires(!isSame<S, Never>)
constexpr bool match(Never, S) {
  return false;
}

template <Symbolic S>
  requires(!isSame<S, Never>)
constexpr bool match(S, Never) {
  return false;
}

// Expression matching
template <typename Op1, Symbolic... Args1, typename Op2, Symbolic... Args2>
constexpr bool match(Expression<Op1, Args1...>, Expression<Op2, Args2...>) {
  if constexpr (!isSame<Op1, Op2>) {
    return false;
  } else if constexpr (sizeof...(Args1) != sizeof...(Args2)) {
    return false;
  } else if constexpr (sizeof...(Args1) == 0) {
    return true;
  } else {
    return (match(Args1{}, Args2{}) && ...);
  }
}

// Fallback
template <Symbolic S1, Symbolic S2>
constexpr bool match(S1, S2) {
  return false;
}

// ============================================================================
// BINDING CONTEXT - Simple approach using type->value mapping
// ============================================================================

namespace detail {

// Single binding entry
template <TypeId VarId, typename BoundExpr>
struct BindingEntry {
  static constexpr TypeId var_id = VarId;
  using bound_type = BoundExpr;
  [[no_unique_address]] BoundExpr bound_expr;

  constexpr BindingEntry() = default;
  constexpr BindingEntry(BoundExpr e) : bound_expr{e} {}
};

// Binding context with entries
template <typename... Entries>
struct BindingContext {
  static constexpr int size = sizeof...(Entries);
  [[no_unique_address]] CompressedTuple<Entries...> entries_;

  constexpr BindingContext() = default;
  constexpr BindingContext(Entries... es)
    requires(sizeof...(Entries) > 0)
      : entries_{es...} {}

  template <TypeId VarId, SizeT Idx = 0>
  static constexpr bool isBound() {
    if constexpr (sizeof...(Entries) == 0) {
      return false;
    } else if constexpr (Idx >= sizeof...(Entries)) {
      return false;
    } else {
      using CurrentEntry = Get_t<Idx, Entries...>;
      if constexpr (CurrentEntry::var_id == VarId) {
        return true;
      } else {
        return isBound<VarId, Idx + 1>();
      }
    }
  }

  template <TypeId VarId, SizeT Idx = 0>
  constexpr auto lookup() const {
    static_assert(sizeof...(Entries) > 0, "Cannot lookup in empty context");
    if constexpr (Idx >= sizeof...(Entries)) {
      // Should not reach here for well-formed patterns
      return get<0>(entries_).bound_expr;  // Fallback
    } else {
      using CurrentEntry = Get_t<Idx, Entries...>;
      if constexpr (CurrentEntry::var_id == VarId) {
        return get<Idx>(entries_).bound_expr;
      } else {
        return lookup<VarId, Idx + 1>();
      }
    }
  }
};

// Empty context
using EmptyContext = BindingContext<>;

// Marker for binding failure
struct BindingFailure {};

template <typename T>
constexpr bool isBindingFailure() {
  return isSame<T, BindingFailure>;
}

// Helper to add an entry to a context - specializations for each size

// Specialization for adding to contexts of different sizes
template <typename NewEntry>
constexpr auto addEntry(NewEntry entry, BindingContext<>) {
  return BindingContext<NewEntry>{entry};
}

template <typename NewEntry, typename E0>
constexpr auto addEntry(NewEntry entry, BindingContext<E0> ctx) {
  return BindingContext<E0, NewEntry>{get<0>(ctx.entries_), entry};
}

template <typename NewEntry, typename E0, typename E1>
constexpr auto addEntry(NewEntry entry, BindingContext<E0, E1> ctx) {
  return BindingContext<E0, E1, NewEntry>{
      get<0>(ctx.entries_), get<1>(ctx.entries_), entry};
}

template <typename NewEntry, typename E0, typename E1, typename E2>
constexpr auto addEntry(NewEntry entry, BindingContext<E0, E1, E2> ctx) {
  return BindingContext<E0, E1, E2, NewEntry>{
      get<0>(ctx.entries_), get<1>(ctx.entries_), get<2>(ctx.entries_), entry};
}

template <typename NewEntry, typename E0, typename E1, typename E2, typename E3>
constexpr auto addEntry(NewEntry entry, BindingContext<E0, E1, E2, E3> ctx) {
  return BindingContext<E0, E1, E2, E3, NewEntry>{
      get<0>(ctx.entries_), get<1>(ctx.entries_), get<2>(ctx.entries_),
      get<3>(ctx.entries_), entry};
}

// More entries if needed...
template <typename NewEntry, typename E0, typename E1, typename E2, typename E3,
          typename E4>
constexpr auto addEntry(NewEntry entry, BindingContext<E0, E1, E2, E3, E4> ctx) {
  return BindingContext<E0, E1, E2, E3, E4, NewEntry>{
      get<0>(ctx.entries_), get<1>(ctx.entries_), get<2>(ctx.entries_),
      get<3>(ctx.entries_), get<4>(ctx.entries_), entry};
}

}  // namespace detail

// Get binding value for a pattern variable
template <typename Unique, typename... Entries>
constexpr auto get(detail::BindingContext<Entries...> const& ctx,
                   PatternVar<Unique>) {
  return ctx.template lookup<PatternVar<Unique>::id>();
}

// ============================================================================
// BINDING EXTRACTION
// ============================================================================

namespace detail {

// PatternVar: bind it or check consistency
template <typename Unique, Symbolic S, typename Context>
constexpr auto extractBindingsImpl(PatternVar<Unique>, S expr, Context ctx) {
  constexpr TypeId varId = PatternVar<Unique>::id;

  if constexpr (Context::template isBound<varId>()) {
    // Already bound - check type consistency
    using ExistingType = decltype(ctx.template lookup<varId>());
    if constexpr (isSame<ExistingType, S>) {
      return ctx;
    } else {
      return BindingFailure{};
    }
  } else {
    // Add new binding
    using Entry = BindingEntry<varId, S>;
    return addEntry(Entry{expr}, ctx);
  }
}

// Constant: must match exactly
template <auto V, Symbolic S, typename Context>
constexpr auto extractBindingsImpl(Constant<V>, S, Context ctx) {
  if constexpr (match(Constant<V>{}, S{})) {
    return ctx;
  } else {
    return BindingFailure{};
  }
}

// Fraction: must match exactly
template <long long N, long long D, Symbolic S, typename Context>
constexpr auto extractBindingsImpl(Fraction<N, D>, S, Context ctx) {
  if constexpr (match(Fraction<N, D>{}, S{})) {
    return ctx;
  } else {
    return BindingFailure{};
  }
}

// Atom: must match exactly
template <typename Id, typename Effect, Symbolic S, typename Context>
constexpr auto extractBindingsImpl(Atom<Id, Effect>, S, Context ctx) {
  if constexpr (match(Atom<Id, Effect>{}, S{})) {
    return ctx;
  } else {
    return BindingFailure{};
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

template <Symbolic S, typename Context>
constexpr auto extractBindingsImpl(AnySymbol, S, Context ctx) {
  return ctx;
}

template <Symbolic S, typename Context>
constexpr auto extractBindingsImpl(AnyLiteral, S, Context ctx) {
  return ctx;
}

template <Symbolic S, typename Context>
constexpr auto extractBindingsImpl(AnyFraction, S, Context ctx) {
  return ctx;
}

// Forward declaration for expression matching
template <SizeT Idx, typename Op, Symbolic... PatternArgs,
          typename ExprOp, Symbolic... ExprArgs, typename Context>
constexpr auto extractBindingsThreaded(Expression<Op, PatternArgs...> pattern,
                                       Expression<ExprOp, ExprArgs...> expr,
                                       Context ctx);

// Expression: recursively extract bindings
template <typename Op, Symbolic... PatternArgs,
          typename ExprOp, Symbolic... ExprArgs, typename Context>
constexpr auto extractBindingsImpl(Expression<Op, PatternArgs...> pattern,
                                   Expression<ExprOp, ExprArgs...> expr,
                                   Context ctx) {
  if constexpr (!isSame<Op, ExprOp>) {
    return BindingFailure{};
  } else if constexpr (sizeof...(PatternArgs) != sizeof...(ExprArgs)) {
    return BindingFailure{};
  } else if constexpr (sizeof...(PatternArgs) == 0) {
    return ctx;
  } else {
    return extractBindingsThreaded<0>(pattern, expr, ctx);
  }
}

// Thread context through argument pairs
template <SizeT Idx, typename Op, Symbolic... PatternArgs,
          typename ExprOp, Symbolic... ExprArgs, typename Context>
constexpr auto extractBindingsThreaded(Expression<Op, PatternArgs...> pattern,
                                       Expression<ExprOp, ExprArgs...> expr,
                                       Context ctx) {
  if constexpr (Idx >= sizeof...(PatternArgs)) {
    return ctx;
  } else {
    auto pat_arg = pattern.template arg<Idx>();
    auto expr_arg = expr.template arg<Idx>();
    auto new_ctx = extractBindingsImpl(pat_arg, expr_arg, ctx);

    if constexpr (isBindingFailure<decltype(new_ctx)>()) {
      return BindingFailure{};
    } else {
      return extractBindingsThreaded<Idx + 1>(pattern, expr, new_ctx);
    }
  }
}

}  // namespace detail

// Public API
template <typename Pattern, Symbolic Expr>
constexpr auto extractBindings(Pattern pattern, Expr expr) {
  return detail::extractBindingsImpl(pattern, expr, detail::EmptyContext{});
}

// ============================================================================
// SUBSTITUTION
// ============================================================================

namespace detail {

// PatternVar: lookup and return bound value (preserves Literal values!)
template <typename Unique, typename Context>
constexpr auto substituteImpl(PatternVar<Unique>, Context const& ctx) {
  return ctx.template lookup<PatternVar<Unique>::id>();
}

// Terminal types: return as-is
template <auto V, typename Context>
constexpr auto substituteImpl(Constant<V>, Context) {
  return Constant<V>{};
}

template <long long N, long long D, typename Context>
constexpr auto substituteImpl(Fraction<N, D>, Context) {
  return Fraction<N, D>{};
}

// Atom: return as-is (preserves embedded Literal value!)
template <typename Id, typename Effect, typename Context>
constexpr auto substituteImpl(Atom<Id, Effect> atom, Context) {
  return atom;
}

// Wildcards: return as-is
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

template <typename Context>
constexpr auto substituteImpl(AnySymbol, Context) {
  return AnySymbol{};
}

template <typename Context>
constexpr auto substituteImpl(AnyLiteral, Context) {
  return AnyLiteral{};
}

template <typename Context>
constexpr auto substituteImpl(AnyFraction, Context) {
  return AnyFraction{};
}

// Expression: recursively substitute
template <typename Op, typename Context>
constexpr auto substituteImpl(Expression<Op>, Context) {
  return Expression<Op>{};
}

template <typename Op, Symbolic Arg0, typename Context>
constexpr auto substituteImpl(Expression<Op, Arg0> expr, Context const& ctx) {
  return Expression<Op, decltype(substituteImpl(expr.template arg<0>(), ctx))>{
      substituteImpl(expr.template arg<0>(), ctx)};
}

template <typename Op, Symbolic Arg0, Symbolic Arg1, typename Context>
constexpr auto substituteImpl(Expression<Op, Arg0, Arg1> expr, Context const& ctx) {
  return Expression<Op,
      decltype(substituteImpl(expr.template arg<0>(), ctx)),
      decltype(substituteImpl(expr.template arg<1>(), ctx))>{
      substituteImpl(expr.template arg<0>(), ctx),
      substituteImpl(expr.template arg<1>(), ctx)};
}

template <typename Op, Symbolic Arg0, Symbolic Arg1, Symbolic Arg2, typename Context>
constexpr auto substituteImpl(Expression<Op, Arg0, Arg1, Arg2> expr, Context const& ctx) {
  return Expression<Op,
      decltype(substituteImpl(expr.template arg<0>(), ctx)),
      decltype(substituteImpl(expr.template arg<1>(), ctx)),
      decltype(substituteImpl(expr.template arg<2>(), ctx))>{
      substituteImpl(expr.template arg<0>(), ctx),
      substituteImpl(expr.template arg<1>(), ctx),
      substituteImpl(expr.template arg<2>(), ctx)};
}

}  // namespace detail

// Public API
template <typename Pattern, typename Context>
constexpr auto substitute(Pattern pattern, Context const& ctx) {
  return detail::substituteImpl(pattern, ctx);
}

// ============================================================================
// NO PREDICATE
// ============================================================================

struct NoPredicate {
  template <typename Context>
  constexpr bool operator()(Context) const {
    return true;
  }
};

// ============================================================================
// COMMON PREDICATES
// ============================================================================

namespace predicates {

template <typename PatVar>
struct IsConstant {
  [[no_unique_address]] PatVar var{};

  template <typename Context>
  constexpr bool operator()(Context const& ctx) const {
    using BoundType = decltype(get(ctx, var));
    return is_constant_v<BoundType>;
  }
};

template <typename PatVar>
struct IsSymbol {
  [[no_unique_address]] PatVar var{};

  template <typename Context>
  constexpr bool operator()(Context const& ctx) const {
    using BoundType = decltype(get(ctx, var));
    return is_atom_v<BoundType> && std::is_same_v<get_effect_t<BoundType>, Free>;
  }
};

template <typename PatVar>
struct IsLiteral {
  [[no_unique_address]] PatVar var{};

  template <typename Context>
  constexpr bool operator()(Context const& ctx) const {
    using BoundType = decltype(get(ctx, var));
    return is_literal_v<BoundType>;
  }
};

template <typename PatVar>
struct IsExpression {
  [[no_unique_address]] PatVar var{};

  template <typename Context>
  constexpr bool operator()(Context const& ctx) const {
    using BoundType = decltype(get(ctx, var));
    return is_expression_v<BoundType>;
  }
};

template <typename PatVar>
struct IsFraction {
  [[no_unique_address]] PatVar var{};

  template <typename Context>
  constexpr bool operator()(Context const& ctx) const {
    using BoundType = decltype(get(ctx, var));
    return is_fraction_v<BoundType>;
  }
};

template <typename PatVar1, typename PatVar2>
struct LessThan {
  [[no_unique_address]] PatVar1 var1{};
  [[no_unique_address]] PatVar2 var2{};

  template <typename Context>
  constexpr bool operator()(Context const& ctx) const {
    return lessThan(get(ctx, var1), get(ctx, var2));
  }
};

template <typename PatVar>
constexpr auto var_is_constant(PatVar var) {
  return IsConstant<PatVar>{var};
}

template <typename PatVar>
constexpr auto var_is_symbol(PatVar var) {
  return IsSymbol<PatVar>{var};
}

template <typename PatVar>
constexpr auto var_is_literal(PatVar var) {
  return IsLiteral<PatVar>{var};
}

template <typename PatVar>
constexpr auto var_is_expression(PatVar var) {
  return IsExpression<PatVar>{var};
}

template <typename PatVar>
constexpr auto var_is_fraction(PatVar var) {
  return IsFraction<PatVar>{var};
}

template <typename PatVar1, typename PatVar2>
constexpr auto var_less_than(PatVar1 v1, PatVar2 v2) {
  return LessThan<PatVar1, PatVar2>{v1, v2};
}

}  // namespace predicates

}  // namespace tempura::symbolic4
