#pragma once

#include "symbolic4/compressed.h"
#include "symbolic4/core.h"

// ============================================================================
// cata.h - Unified catamorphism/paramorphism scheme
// ============================================================================
//
// A single traversal implementation that handles both:
//   - Catamorphism (fold): combine receives child RESULTS only
//   - Paramorphism (para): combine receives Pair{original, result} for each child
//
// The distinction is controlled by ChildMode:
//   - ResultOnly:   combine<Op>(result1, result2, ...)
//   - WithOriginal: combine<Op>(Pair{child1, result1}, Pair{child2, result2}, ...)
//
// Context (ctx) is optional - pass it if your interpreter needs external state
// (like bindings for evaluation), omit it for stateless interpreters (like simplify).
//
// Interpreter interface:
//   With ctx:    terminal(T, ctx&), combine<Op>(ctx&, children...)
//   Without ctx: terminal(T),       combine<Op>(children...)
//
// Return type is always deduced from interpreter methods.
//
// ============================================================================

namespace tempura::symbolic4 {

enum class ChildMode { ResultOnly, WithOriginal };

// ============================================================================
// cata - The unified traversal
// ============================================================================

// Forward declaration
template <typename I, ChildMode Mode, Symbolic E, typename... Ctx>
constexpr auto cata(E expr, Ctx&... ctx);

namespace detail {

// ResultOnly mode: pass just the recursive results
template <typename I, typename Op, typename Ctx, SizeT... Is, typename... Args>
constexpr auto cataExprResult([[maybe_unused]] Expression<Op, Args...> expr,
                               Ctx& ctx, IndexSequence<Is...>) {
  return I::template combine<Op>(ctx,
      cata<I, ChildMode::ResultOnly>(expr.template arg<Is>(), ctx)...);
}

template <typename I, typename Op, SizeT... Is, typename... Args>
constexpr auto cataExprResult([[maybe_unused]] Expression<Op, Args...> expr,
                               IndexSequence<Is...>) {
  return I::template combine<Op>(
      cata<I, ChildMode::ResultOnly>(expr.template arg<Is>())...);
}

// WithOriginal mode: pass Pair{original, result}
template <typename I, typename Op, typename Ctx, SizeT... Is, typename... Args>
constexpr auto cataExprPair([[maybe_unused]] Expression<Op, Args...> expr,
                             Ctx& ctx, IndexSequence<Is...>) {
  return I::template combine<Op>(ctx,
      Pair{expr.template arg<Is>(),
           cata<I, ChildMode::WithOriginal>(expr.template arg<Is>(), ctx)}...);
}

template <typename I, typename Op, SizeT... Is, typename... Args>
constexpr auto cataExprPair([[maybe_unused]] Expression<Op, Args...> expr,
                             IndexSequence<Is...>) {
  return I::template combine<Op>(
      Pair{expr.template arg<Is>(),
           cata<I, ChildMode::WithOriginal>(expr.template arg<Is>())}...);
}

}  // namespace detail

// With context
template <typename I, ChildMode Mode, Symbolic E, typename Ctx>
constexpr auto cata(E expr, Ctx& ctx) {
  if constexpr (is_terminal_v<E>) {
    return I::terminal(expr, ctx);
  } else {
    if constexpr (Mode == ChildMode::ResultOnly) {
      return detail::cataExprResult<I>(expr, ctx, MakeIndexSequence<E::arity>{});
    } else {
      return detail::cataExprPair<I>(expr, ctx, MakeIndexSequence<E::arity>{});
    }
  }
}

// Without context
template <typename I, ChildMode Mode, Symbolic E>
constexpr auto cata(E expr) {
  if constexpr (is_terminal_v<E>) {
    return I::terminal(expr);
  } else {
    if constexpr (Mode == ChildMode::ResultOnly) {
      return detail::cataExprResult<I>(expr, MakeIndexSequence<E::arity>{});
    } else {
      return detail::cataExprPair<I>(expr, MakeIndexSequence<E::arity>{});
    }
  }
}

// ============================================================================
// Convenience aliases: fold and para
// ============================================================================

// fold - catamorphism (results only)
template <typename I, Symbolic E, typename Ctx>
constexpr auto fold(E expr, Ctx& ctx) {
  return cata<I, ChildMode::ResultOnly>(expr, ctx);
}

template <typename I, Symbolic E>
constexpr auto fold(E expr) {
  return cata<I, ChildMode::ResultOnly>(expr);
}

// para - paramorphism (with originals)
template <typename I, Symbolic E, typename Ctx>
constexpr auto para(E expr, Ctx& ctx) {
  return cata<I, ChildMode::WithOriginal>(expr, ctx);
}

template <typename I, Symbolic E>
constexpr auto para(E expr) {
  return cata<I, ChildMode::WithOriginal>(expr);
}

}  // namespace tempura::symbolic4
