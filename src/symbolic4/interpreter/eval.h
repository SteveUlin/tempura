#pragma once

#include <cmath>

#include "symbolic4/core.h"
#include "symbolic4/operators.h"
#include "symbolic4/scheme/fold.h"

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
// Eval is a fold interpreter with result_type = double. It traverses the
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
// Bindings are passed through the context. The syntax x = 5.0 creates a
// TypeValueBinder<X, double>, and multiple binders are packed into a
// BinderPack. During evaluation, ctx.bindings[x] retrieves the bound value
// using type-based dispatch (see core.h for details).
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Eval Interpreter - evaluates symbolic expressions with bindings
// ============================================================================
//
// Template parameter Bindings is typically BinderPack<TypeValueBinder<...>...>
// which holds the symbol-to-value mappings passed to evaluate().

template <typename Bindings>
struct Eval {
  using result_type = double;

  struct context_type {
    Bindings bindings;  // The symbol bindings passed by the user
  };

  // Terminal handling: dispatch based on terminal type
  // Compile-time constants are embedded in the type, so we extract T::value.
  // Literals carry runtime values in their strategy.
  // Symbols require lookup in the bindings.
  template <typename T>
  static constexpr auto terminal(T term, context_type& ctx) -> double {
    if constexpr (is_constant_v<T>) {
      return static_cast<double>(T::value);  // Constant<5> → 5.0
    } else if constexpr (is_fraction_v<T>) {
      return T::value;  // Fraction<1,2> → 0.5
    } else if constexpr (is_literal_v<T>) {
      return static_cast<double>(term.strategy_.value);  // Embedded value
    } else if constexpr (is_atom_v<T>) {
      return ctx.bindings[term];  // Symbol: look up in bindings
    } else {
      static_assert(is_atom_v<T>, "Unknown terminal type");
      return 0.0;
    }
  }

  // Combine child results using the operator functor.
  // Op types like AddOp, SinOp are callable: op(a, b) performs the operation.
  template <typename Op, typename... Rs>
  static constexpr auto combine(context_type&, Op op, Rs... children) -> double {
    return op(children...);
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
  using EvalType = Eval<BinderPack<Binders...>>;
  typename EvalType::context_type ctx{bindings};
  return fold<EvalType>(expr, ctx);
}

// Main overload: accepts individual binders (x = value, y = value, ...)
// and packs them into a BinderPack automatically.
template <Symbolic E, typename... Args>
constexpr auto evaluate(E expr, Args... binders) -> double {
  return evaluate(expr, BinderPack{binders...});
}

}  // namespace tempura::symbolic4
