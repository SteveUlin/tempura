#pragma once

#include <utility>

#include "symbolic4/core.h"

// ============================================================================
// fold.h - Catamorphism: the fundamental way to consume expression trees
// ============================================================================
//
// What is a catamorphism?
//
// A catamorphism (Greek: "downward shape") is a generalized fold operation.
// It recursively transforms a data structure from leaves to root, combining
// results bottom-up. For expression trees:
//
//   1. Process each leaf (terminal) and produce a result
//   2. For internal nodes, first recursively process children
//   3. Combine children's results using the operator to produce this node's result
//
// Why "fold"?
//
// The name comes from functional programming where folding a list combines
// all elements into a single value. Here we're folding a TREE - each subtree
// gets "folded" into a value, then those values are combined at the parent.
//
// The Interpreter Pattern:
//
// Fold is parameterized by an "Interpreter" - a type that defines:
//   - result_type: What type does the fold produce?
//   - context_type: What state is threaded through the traversal?
//   - terminal(T, ctx): How to handle leaf nodes
//   - combine(ctx, Op, children...): How to combine child results
//
// Example Interpreters:
//   - Eval: Produces double, combines by applying operators numerically
//   - ToString: Produces strings, combines by formatting with operator symbols
//   - CountNodes: Produces int, combines by summing child counts + 1
//
// When to use fold vs para:
//
// Use FOLD when you only need the RESULTS of processing children.
// Use PARA when you also need the ORIGINAL child expressions.
//
// For example, evaluation only needs child values (fold is sufficient).
// But differentiation needs original children for rules like d/dx(f*g) = f*dg + df*g,
// so it requires para (see scheme/para.h).
//
// ============================================================================

namespace tempura::symbolic4 {

// Interpreter concept: must define a result_type
// Full interpreter interface (checked by usage, not this concept):
//   - static auto terminal(T, context_type&) -> result_type
//   - static auto combine(context_type&, Op, Rs... results) -> result_type
template <typename I, typename Ctx>
concept Interpreter = requires {
  typename I::result_type;
};

// ============================================================================
// fold<Interpreter>(expr, ctx) - The main entry point
// ============================================================================
//
// Recursively traverses the expression tree bottom-up:
//   - For terminals: calls I::terminal(expr, ctx)
//   - For expressions: first folds all children, then calls
//     I::combine(ctx, Op{}, child_result_1, child_result_2, ...)
//
// The context is passed by reference and can accumulate state during traversal.
// Context comes FIRST in combine() to avoid variadic template deduction issues.

template <typename I, Symbolic E, typename Ctx>
constexpr auto fold(E expr, Ctx& ctx) -> typename I::result_type;

namespace detail {

// Implementation detail: unfolds expression children via index sequence
// and recursively folds each child before combining.
template <typename I, typename Op, typename Ctx, SizeT... Is, typename... Args>
constexpr auto foldExpression([[maybe_unused]] Expression<Op, Args...> expr,
                               Ctx& ctx, IndexSequence<Is...>) {
  // Each arg<Is>() is recursively folded, then all results passed to combine
  return I::combine(ctx, Op{}, fold<I>(expr.template arg<Is>(), ctx)...);
}

}  // namespace detail

template <typename I, Symbolic E, typename Ctx>
constexpr auto fold(E expr, Ctx& ctx) -> typename I::result_type {
  if constexpr (is_terminal_v<E>) {
    return I::terminal(expr, ctx);
  } else {
    return detail::foldExpression<I>(expr, ctx, MakeIndexSequence<E::arity>{});
  }
}

// Convenience overload: default-constructs context
// Useful when the interpreter needs no external state.
template <typename I, Symbolic E>
constexpr auto fold(E expr) -> typename I::result_type {
  typename I::context_type ctx{};
  return fold<I>(expr, ctx);
}

}  // namespace tempura::symbolic4
