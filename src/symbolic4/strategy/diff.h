#pragma once

#include "symbolic4/strategy/info_diff.h"

// ============================================================================
// diff.h — Symbolic differentiation API
// ============================================================================
//
// Differentiation operates entirely in the info domain: the expression is
// reflected to std::meta::info, differentiated + simplified with zero
// intermediate type instantiations, then spliced back to a single type.
//
// Usage:
//   Symbol<struct X> x;
//   auto result = differentiate(x * sin(x), x);
//
// All constants must use Constant<V> (via _c suffix). Runtime data uses
// Symbol + binding at eval time.
// ============================================================================

namespace tempura::symbolic4 {

template <Symbolic E, typename Var>
constexpr auto diff(E, Var) {
  constexpr auto result = diffInfo(^^E, ^^Var);
  using Result = [:result:];
  return Result{};
}

template <Symbolic E, typename Var>
constexpr auto differentiate(E expr, Var v) {
  return diff(expr, v);
}

}  // namespace tempura::symbolic4
