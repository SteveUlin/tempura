#pragma once

#include <utility>

#include "symbolic4/compressed.h"
#include "symbolic4/core.h"

// ============================================================================
// para.h - Paramorphism: fold with access to original children
// ============================================================================
//
// What is a paramorphism?
//
// A paramorphism (Greek: "beside shape") extends catamorphism (fold) by
// providing access to the ORIGINAL child expressions alongside their results.
// For each child, you get a Pair{original_expr, processed_result}.
//
// Why do we need this?
//
// Some operations require the original expression structure, not just results.
// The classic example is symbolic differentiation:
//
//   Product rule: d/dx(f * g) = f * dg + df * g
//
// To compute this, we need:
//   - f (the original left child)
//   - g (the original right child)
//   - df (the derivative of left child - the "result" of recursing)
//   - dg (the derivative of right child)
//
// A regular fold only gives us df and dg. Para gives us Pair{f, df} and Pair{g, dg}.
//
// Interface:
//
// Para's combine() receives Pair objects instead of raw results:
//   combine(ctx, Op, Pair{child1, result1}, Pair{child2, result2}, ...)
//
// Each Pair provides:
//   - pair.first:  The original child expression (for building new expressions)
//   - pair.second: The result of recursively processing that child
//
// When to use para vs fold:
//
// Use FOLD (simpler) when:
//   - You only need computed results (evaluation, counting, depth)
//   - You're reducing the tree to a single value
//
// Use PARA (more powerful) when:
//   - You need to build new expressions using original subexpressions
//   - You're implementing mathematical rules that reference children symbolically
//   - Examples: differentiation, algebraic simplification, pattern matching
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// para<Interpreter>(expr, ctx) - The main entry point
// ============================================================================
//
// For terminals: calls I::terminal(expr, ctx)
// For expressions: calls I::combine(ctx, Op{}, Pair{child, para(child)}...)
//
// Each child is passed as Pair{original_expression, recursive_result}.

template <typename I, Symbolic E, typename Ctx>
constexpr auto para(E expr, Ctx& ctx) -> typename I::result_type;

namespace detail {

// Implementation detail: builds Pair{child, para(child)} for each argument.
// Uses compressed::Pair to avoid overhead for empty result types.
template <typename I, typename Op, typename Ctx, SizeT... Is, typename... Args>
constexpr auto paraExpression(Expression<Op, Args...> expr, Ctx& ctx,
                               IndexSequence<Is...>) {
  // Each child becomes Pair{original, recursive_result}
  return I::combine(
      ctx, Op{},
      Pair{expr.template arg<Is>(), para<I>(expr.template arg<Is>(), ctx)}...);
}

}  // namespace detail

template <typename I, Symbolic E, typename Ctx>
constexpr auto para(E expr, Ctx& ctx) -> typename I::result_type {
  if constexpr (is_terminal_v<E>) {
    return I::terminal(expr, ctx);
  } else {
    return detail::paraExpression<I>(expr, ctx, MakeIndexSequence<E::arity>{});
  }
}

// Convenience overload: default-constructs context
// Useful when the interpreter needs no external state.
template <typename I, Symbolic E>
constexpr auto para(E expr) -> typename I::result_type {
  typename I::context_type ctx{};
  return para<I>(expr, ctx);
}

}  // namespace tempura::symbolic4
