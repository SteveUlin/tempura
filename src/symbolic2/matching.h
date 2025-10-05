#pragma once

#include "core.h"
#include "meta/tags.h"

// Pattern matching for symbolic expressions

namespace tempura {

// --- Matching Logic ---

// Rank5: Highest priority - Never is always a mismatch.
constexpr auto matchImpl(Rank5, Never, Never) -> bool { return false; }
template <Symbolic S>
constexpr auto matchImpl(Rank5, Never, S) -> bool {
  return false;
}
template <Symbolic S>
constexpr auto matchImpl(Rank5, S, Never) -> bool {
  return false;
}

// Rank4: Exact Type Match
template <Symbolic S>
constexpr auto matchImpl(Rank4, S, S) -> bool {
  return true;
}

// Rank3: Wildcard matching (symmetric - order doesn't matter)
template <Symbolic S>
constexpr auto matchImpl(Rank3, S, AnyArg) -> bool {
  return true;
}
template <Symbolic S>
constexpr auto matchImpl(Rank3, AnyArg, S) -> bool {
  return true;
}

template <typename Op, Symbolic... Args>
constexpr auto matchImpl(Rank3, Expression<Op, Args...>, AnyExpr) -> bool {
  return true;
}
template <typename Op, Symbolic... Args>
constexpr auto matchImpl(Rank3, AnyExpr, Expression<Op, Args...>) -> bool {
  return true;
}

template <auto value>
constexpr auto matchImpl(Rank3, Constant<value>, AnyConstant) -> bool {
  return true;
}
template <auto value>
constexpr auto matchImpl(Rank3, AnyConstant, Constant<value>) -> bool {
  return true;
}

template <typename Unique>
constexpr auto matchImpl(Rank3, Symbol<Unique>, AnySymbol) -> bool {
  return true;
}
template <typename Unique>
constexpr auto matchImpl(Rank3, AnySymbol, Symbol<Unique>) -> bool {
  return true;
}

// Rank2: Constant values are compared for equality (e.g., 1.0 == 1)
template <auto value_lhs, auto value_rhs>
constexpr auto matchImpl(Rank2, Constant<value_lhs>, Constant<value_rhs>)
    -> bool {
  return value_lhs == value_rhs;
}

// Rank1: Expression matching - recursively matches operator and arguments
template <typename Op, Symbolic... LHSArgs, Symbolic... RHSArgs>
constexpr auto matchImpl(Rank1, Expression<Op, LHSArgs...>,
                         Expression<Op, RHSArgs...>) -> bool {
  if constexpr (sizeof...(LHSArgs) != sizeof...(RHSArgs)) {
    return false;
  } else if constexpr (sizeof...(LHSArgs) == 0 && (sizeof...(RHSArgs) == 0)) {
    return true;
  } else {
    return (matchImpl(Rank4{}, LHSArgs{}, RHSArgs{}) && ...);
  }
}

// Rank0: Default catch-all for non-matching cases
constexpr auto matchImpl(Rank0, Symbolic auto, Symbolic auto) -> bool {
  return false;
}

// Compile-time pattern matching using ranked overload resolution
template <Symbolic LHS, Symbolic RHS>
constexpr auto match(LHS lhs, RHS rhs) -> bool {
  return matchImpl(Rank5{}, lhs, rhs);
}

}  // namespace tempura
