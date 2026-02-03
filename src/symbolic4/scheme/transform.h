#pragma once

// transform.h is now provided by cata.h
// transform<I>(expr) is now fold<I>(expr) (no ctx version)
// paraTransform<I>(expr) is now para<I>(expr) (no ctx version)
// This header exists for backwards compatibility.
#include "symbolic4/scheme/cata.h"

namespace tempura::symbolic4 {

// Backwards compatibility aliases
template <typename I, Symbolic E>
constexpr auto transform(E expr) {
  return fold<I>(expr);
}

template <typename I, Symbolic E>
constexpr auto paraTransform(E expr) {
  return para<I>(expr);
}

}  // namespace tempura::symbolic4
