#pragma once

#include "core.h"
#include "meta/tags.h"

// Pattern matching using ranked overload resolution for type-level dispatch

namespace tempura {

// Rank5: Never sentinel always fails to match
constexpr auto matchImpl(Rank5, Never, Never) -> bool { return false; }
template <Symbolic S>
constexpr auto matchImpl(Rank5, Never, S) -> bool {
  return false;
}
template <Symbolic S>
constexpr auto matchImpl(Rank5, S, Never) -> bool {
  return false;
}

// Rank4: Exact type identity
template <Symbolic S>
constexpr auto matchImpl(Rank4, S, S) -> bool {
  return true;
}

// Rank3: Wildcard matching (order-independent)
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

// Rank2: Constants match by value equality
template <auto value_lhs, auto value_rhs>
constexpr auto matchImpl(Rank2, Constant<value_lhs>, Constant<value_rhs>)
    -> bool {
  return value_lhs == value_rhs;
}

// Rank1: Structural recursion for compound expressions
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

// Rank0: Fallback for non-matching types
constexpr auto matchImpl(Rank0, Symbolic auto, Symbolic auto) -> bool {
  return false;
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto match(LHS lhs, RHS rhs) -> bool {
  return matchImpl(Rank5{}, lhs, rhs);
}

}  // namespace tempura
