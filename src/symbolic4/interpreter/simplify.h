#pragma once

#include "symbolic4/strategy/info_simplify.h"

// ============================================================================
// simplify.h — Algebraic simplification (info-domain wrapper)
// ============================================================================
//
// Thin wrapper around the info-domain simplifier. The expression is reflected
// to std::meta::info, simplified with zero intermediate type instantiations,
// then spliced back to a single result type.
//
// Unlike diff(), this is constexpr (not consteval) so it can absorb consteval
// calls like diff() inside runtime lambdas — the test framework stores lambdas
// in std::function<void()> which can't hold immediate functions.
// ============================================================================

namespace tempura::symbolic4 {

template <Symbolic E>
constexpr auto simplify(E) {
  constexpr auto result = infoSimplify(^^E);
  using Result = [:result:];
  return Result{};
}

}  // namespace tempura::symbolic4
