#pragma once

#include "symbolic4/constraints.h"
#include "symbolic4/core.h"
#include "symbolic4/indexed/reduce_over.h"         // For is_reduce_over_v

// ============================================================================
// eval.h - Numerical evaluation of symbolic expressions
// ============================================================================
//
// Converts symbolic expressions to concrete numeric values using direct
// recursion with if constexpr dispatch. No cata/fold apparatus needed.
//
// Usage:
//   Symbol<struct X> x;
//   auto expr = x * x + one;
//   double result = evaluate(expr, x = 5.0);  // result = 26.0
//
// How it works:
//
//   1. At terminals:
//      - Constants/Fractions: return their compile-time value
//      - Literals: return their embedded runtime value
//      - Symbols: look up in bindings and return the bound value
//
//   2. At expressions:
//      - Recursively evaluate all children to get double values
//      - Apply the operator functor: Op{}(child_values...)
//
//   3. At ReduceOver:
//      - Static assert: non-indexed eval doesn't handle reductions.
//        Use evaluateIndexed() for expressions containing ReduceOver.
//
// ============================================================================

namespace tempura::symbolic4 {

namespace eval_detail {

// Forward declaration for mutual recursion
template <Symbolic E, typename Bindings>
constexpr auto evalImpl(E expr, Bindings& ctx) -> double;

// Helper: evaluate an Expression node by expanding children via index sequence
template <typename Op, typename... Args, typename Bindings, SizeT... Is>
constexpr auto evalExpression(Expression<Op, Args...> expr, Bindings& ctx,
                              IndexSequence<Is...>) -> double {
  return Op{}(evalImpl(expr.template arg<Is>(), ctx)...);
}

// Direct recursive evaluation
template <Symbolic E, typename Bindings>
constexpr auto evalImpl(E expr, Bindings& ctx) -> double {
  if constexpr (is_constant_v<E>) {
    return static_cast<double>(E::value);
  } else if constexpr (is_fraction_v<E>) {
    return E::value;
  } else if constexpr (is_literal_v<E>) {
    return static_cast<double>(expr.effect_.value);
  } else if constexpr (is_random_var_atom_v<E>) {
    // RandomVarSymbol<Id,D> - look up using the corresponding free symbol
    using FreeSymbol = Atom<get_id_t<E>, Free>;
    double z = ctx[FreeSymbol{}];
    // Apply constraint transform based on distribution support
    using Dist = get_distribution_t<typename E::effect_type>;
    using Support = typename Dist::support_type;
    return constraints::applyNumeric<Support>(z);
  } else if constexpr (is_atom_v<E>) {
    return ctx[expr];
  } else if constexpr (is_expression_v<E>) {
    return evalExpression(expr, ctx, MakeIndexSequence<E::arity>{});
  } else if constexpr (is_reduce_over_v<E>) {
    // ReduceOver shouldn't appear in non-indexed eval
    static_assert(!is_reduce_over_v<E>,
        "ReduceOver in non-indexed eval -- use evaluateIndexed()");
    return 0.0;
  } else {
    static_assert(is_atom_v<E>, "Unknown symbolic type");
    return 0.0;
  }
}

}  // namespace eval_detail

// ============================================================================
// Convenience function: evaluate(expr, x = value, ...)
// ============================================================================

// Overload accepting a pre-constructed BinderPack
template <Symbolic E, typename... Binders>
constexpr auto evaluate(E expr, BinderPack<Binders...> bindings) -> double {
  return eval_detail::evalImpl(expr, bindings);
}

// Main overload: accepts individual binders (x = value, y = value, ...)
template <Symbolic E, typename... Args>
constexpr auto evaluate(E expr, Args... binders) -> double {
  return evaluate(expr, BinderPack{binders...});
}

}  // namespace tempura::symbolic4
