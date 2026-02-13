#pragma once

#include "symbolic4/strategy/info_canonicalize.h"

// ============================================================================
// canonicalize.h - Canonical ordering for commutative operations
// ============================================================================
//
// Reorders children of commutative operations (Add, Mul) so that the smaller
// child (by type ordering) comes first.
//
// Thin wrapper over the info-domain implementation — no intermediate C++ types
// are instantiated. canonicalizeInfo() does the work on std::meta::info values,
// and only the final result gets spliced back.
//
// ============================================================================

namespace tempura::symbolic4 {

template <Symbolic E>
constexpr auto canonicalize(E) {
  constexpr auto result = canonicalizeInfo(^^E);
  using Result = [:result:];
  return Result{};
}

}  // namespace tempura::symbolic4
