#pragma once

#include "symbolic4/indexed/sum_over.h"
#include "symbolic4/interpreter/diff.h"

// ============================================================================
// sum_over_diff.h - Differentiation through SumOver
// ============================================================================
//
// diff(SumOver<D, E>, x) = SumOver<D, diff(E, x)>
//
// Differentiation distributes over the sum. This is a separate header to
// avoid circular dependencies between diff.h and sum_over.h.
//
// ============================================================================

namespace tempura::symbolic4 {

// diff(SumOver<D, E>, x) = SumOver<D, diff(E, x)>
template <typename DimTag, Symbolic E, typename Var>
constexpr auto diff(SumOver<DimTag, E> sum, [[maybe_unused]] Var) {
  auto inner_diff = diff(sum.expr_, Var{});
  return SumOver<DimTag, decltype(inner_diff)>{inner_diff};
}

}  // namespace tempura::symbolic4
