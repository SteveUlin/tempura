#pragma once

#include <cmath>

#include "symbolic4/core.h"
#include "symbolic4/distributions/wrappers.h"  // For support type traits
#include "symbolic4/scheme/cata.h"

// ============================================================================
// eval.h - Numerical evaluation of symbolic expressions
// ============================================================================
//
// This interpreter converts symbolic expressions to concrete numeric values.
// It's the bridge between the symbolic world (types) and the numeric world
// (runtime values).
//
// Usage:
//   Symbol<struct X> x;
//   auto expr = x * x + one;
//   double result = evaluate(expr, x = 5.0);  // result = 26.0
//
// How it works:
//
// Eval is a fold interpreter that produces double. It traverses the
// expression tree bottom-up:
//
//   1. At terminals:
//      - Constants/Fractions: return their compile-time value
//      - Literals: return their embedded runtime value
//      - Symbols: look up in bindings and return the bound value
//
//   2. At expressions:
//      - Recursively evaluate all children to get double values
//      - Apply the operator functor: Op{}(child_values...)
//      - Return the computed result
//
// Op functors (AddOp, SinOp, etc.) are callable: AddOp{}(a, b) returns a + b.
// This is why the same Op types work for both AST construction and evaluation.
//
// The binding mechanism:
//
// Bindings are passed as context. The syntax x = 5.0 creates a
// TypeValueBinder<X, double>, and multiple binders are packed into a
// BinderPack. During evaluation, ctx[x] retrieves the bound value
// using type-based dispatch (see core.h for details).
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Constraint Transform Helpers
// ============================================================================
//
// Apply constraint transforms when evaluating Sample atoms.
// The transform is determined by the distribution's support type.

namespace eval_detail {

// Apply constraint transform based on support type
template <typename Support>
constexpr auto applyConstraint(double z) -> double {
  if constexpr (is_positive_support_v<Support>) {
    return std::exp(z);  // Positive: x = exp(z)
  } else if constexpr (is_unit_support_v<Support>) {
    return 1.0 / (1.0 + std::exp(-z));  // Unit: x = sigmoid(z)
  } else {
    return z;  // Real: x = z (no transform)
  }
}

}  // namespace eval_detail

// ============================================================================
// Eval Interpreter - evaluates symbolic expressions with bindings
// ============================================================================
//
// Template parameter Bindings is typically BinderPack<TypeValueBinder<...>...>
// which holds the symbol-to-value mappings passed to evaluate().

template <typename Bindings>
struct Eval {
  // Terminal handling: dispatch based on terminal type
  template <typename T>
  static constexpr auto terminal(T term, Bindings& ctx) -> double {
    if constexpr (is_constant_v<T>) {
      return static_cast<double>(T::value);  // Constant<5> → 5.0
    } else if constexpr (is_fraction_v<T>) {
      return T::value;  // Fraction<1,2> → 0.5
    } else if constexpr (is_literal_v<T>) {
      return static_cast<double>(term.effect_.value);  // Embedded value
    } else if constexpr (is_random_var_atom_v<T>) {
      // RandomVarSymbol<Id,D> - look up using the corresponding free symbol
      // Bindings are keyed by Symbol<Id> (Atom<Id, Free>), not the Sample effect
      using FreeSymbol = Atom<get_id_t<T>, Free>;
      double z = ctx[FreeSymbol{}];

      // Apply constraint transform based on distribution support
      using Dist = get_distribution_t<typename T::effect_type>;
      using Support = typename Dist::support_type;
      return eval_detail::applyConstraint<Support>(z);
    } else if constexpr (is_atom_v<T>) {
      return ctx[term];  // Symbol: look up in bindings
    } else {
      static_assert(is_atom_v<T>, "Unknown terminal type");
      return 0.0;
    }
  }

  // Combine child results using the operator functor.
  template <typename Op, typename... Rs>
  static constexpr auto combine(Bindings&, Rs... children) -> double {
    return Op{}(children...);
  }
};

// ============================================================================
// Convenience function: evaluate(expr, x = value, ...)
// ============================================================================
//
// The main user-facing API. Accepts individual binders created by x = value.
//
// Examples:
//   evaluate(x + y, x = 3.0, y = 4.0)           // returns 7.0
//   evaluate(sin(x), x = M_PI / 2)              // returns 1.0
//   evaluate(pi)                                 // returns M_PI (no bindings needed)

// Overload accepting a pre-constructed BinderPack
template <Symbolic E, typename... Binders>
constexpr auto evaluate(E expr, BinderPack<Binders...> bindings) -> double {
  return fold<Eval<BinderPack<Binders...>>>(expr, bindings);
}

// Main overload: accepts individual binders (x = value, y = value, ...)
// and packs them into a BinderPack automatically.
template <Symbolic E, typename... Args>
constexpr auto evaluate(E expr, Args... binders) -> double {
  return evaluate(expr, BinderPack{binders...});
}

}  // namespace tempura::symbolic4
