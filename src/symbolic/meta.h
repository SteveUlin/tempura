#pragma once

#include <iostream>

#include "symbolic/matchers.h"
#include "symbolic/operators.h"
#include "symbolic/symbolic.h"
#include "symbolic/to_string.h"
#include "type_list.h"

namespace tempura::symbolic {

// Flatten nested expressions of the same operator into a single TypeList.
// Returns an in-order traversal of the expression tree.
template <Operator Op, Symbolic... Terms>
consteval auto flatten(Op op, TypeList<Terms...> terms) {
  if constexpr (sizeof...(Terms) == 0) {
    return TypeList<>{};
  } else if constexpr (match(terms.head(), makeExpr(op, AnyNTerms{}))) {
    return flatten(op, concat(terms.head().terms(), terms.tail()));
  } else {
    return concat(TypeList{terms.head()}, flatten(op, terms.tail()));
  }
}

// Flatten and apply a functor to each base term.
template <template <typename> typename Fn, Operator Op, Symbolic... Terms>
consteval auto mapFlatten(Op op, TypeList<Terms...> terms) {
  if constexpr (sizeof...(Terms) == 0) {
    return TypeList<>{};
  } else if constexpr (match(terms.head(), makeExpr(op, AnyNTerms{}))) {
    return mapFlatten<Fn>(op, concat(terms.head().terms(), terms.tail()));
  } else {
    auto value = Fn<decltype(terms.head())>::value;
    if constexpr (match(value, makeExpr(op, AnyNTerms{}))) {
      return mapFlatten<Fn>(op, concat(value.terms(), terms.tail()));
    } else {
      return concat(TypeList{value}, mapFlatten<Fn>(op, terms.tail()));
    }
  }
}

// Apply a functor to each element of a TypeList.
template <template <typename> typename Fn, Symbolic... Terms>
consteval auto map(TypeList<Terms...> /*unused*/) {
  return TypeList{Fn<Terms>::value...};
}

// Sort a TypeList using quicksort with the provided comparator.
template <template <typename, typename> typename Cmp, Symbolic... Terms>
consteval auto sort(TypeList<Terms...> terms) {
  if constexpr (sizeof...(Terms) <= 1) {
    return terms;
  } else {
    auto pivot = terms.head();
    auto [lhs, rhs] = [&]<typename... Args>(TypeList<Args...>) {
      return std::pair{
          concat(std::conditional_t<Cmp<Args, decltype(pivot)>::value,
                                    TypeList<Args>, TypeList<>>{}...),
          concat(std::conditional_t<not Cmp<Args, decltype(pivot)>::value,
                                    TypeList<Args>, TypeList<>>{}...)};
    }(terms.tail());
    return concat(sort<Cmp>(lhs), TypeList{pivot}, sort<Cmp>(rhs));
  }
}

}  // namespace tempura::symbolic
