#pragma once

#include "symbolic.h"


namespace tempura::symbolic {

namespace internal {

template <typename Action, Symbolic... Terms>
consteval auto flattenImpl(Action action, TypeList<Terms...> terms) {
  if constexpr (sizeof...(Terms) == 0) {
    return TypeList<>{};

  } else if constexpr (match(terms.head(),
                             SymbolicExpression<Action, AnyNTerms>{})) {
    return flattenImpl(action, concat(terms.head().terms(), terms.tail()));
  } else {
    return flattenImpl(action, terms.tail()) >>= [&](Symbolic auto... rest) {
        return TypeList<decltype(terms.head()), decltype(rest)...>{};
    };
  }
}

} // namespace internal

template <typename Action, Symbolic... Terms>
consteval auto flatten(SymbolicExpression<Action, Terms...> expr) {
  return internal::flattenImpl(Action{}, expr.terms()) >>= [&](auto... args) {
    return SymbolicExpression<Action, decltype(args)...>{};
  };
}

constexpr auto getConstantPart(Symbolic auto expr) {
  if constexpr (match(expr, AnyConstant{})) {
    return expr;
  } else if constexpr (match(expr, SymbolicExpression<Multiply, AnyConstant, AnyNTerms>{})) {
    return expr.terms().head();
  } else {
    return Constant<1>{};
  }
}

constexpr auto getVariablePart(Symbolic auto expr) {
  if constexpr (match(expr, AnyConstant{})) {
    return Constant<1>{};
  } else if constexpr (match(expr, SymbolicExpression<Multiply, AnyConstant, Any>{})) {
    return expr.terms().tail().head();
  } else if constexpr (match(expr, SymbolicExpression<Multiply, AnyConstant, AnyNTerms>{})) {
    return SymbolicExpression{Multiply{}, expr.terms().tail()};
  } else {
    return expr;
  }
}

template <Symbolic T>
struct PlusSorter {
  template <Symbolic U>
  static constexpr auto operator()() -> bool {
    return typeName<decltype(getVariablePart(T{}))>() <= typeName<decltype(getVariablePart(U{}))>();
  }
};

template <Symbolic... Terms>
constexpr auto sort(SymbolicExpression<Plus, Terms...> expr) {
  return SymbolicExpression{Plus{}, sort<PlusSorter>(expr.terms())};
}

namespace internal {
constexpr auto plusMergeImpl(TypeListed auto terms) {
  if constexpr (terms.size() <= 1) {
    return terms;
  } else {
    auto lhs = terms.head();
    auto rhs = terms.tail().head();
    if constexpr (match(getVariablePart(lhs), getVariablePart(rhs))) {
      return plusMergeImpl(
          concat(TypeList{Constant<getConstantPart(lhs).value + getConstantPart(rhs).value>{} *
                          getVariablePart(lhs)},
                 terms.tail().tail()));
    } else {
      return concat(TypeList{lhs}, plusMergeImpl(terms.tail()));
    }
  }
}
}  // namespace internal

template<Symbolic... Terms>
constexpr auto merge(SymbolicExpression<Plus, Terms...> expr) {
  auto terms = internal::plusMergeImpl(expr.terms());
  if constexpr (terms.size() == 1) {
    return terms.head();
  } else {
    return SymbolicExpression{Plus{}, terms};
  }
}

template<Symbolic... Terms>
constexpr auto flattenSortMerge(SymbolicExpression<Plus, Terms...> expr) {
  return merge(sort(flatten(expr)));
}

struct isZero {
  template <typename T>
  static constexpr auto operator()() -> bool {
    return std::is_same_v<T, Constant<0>>;
  }
};

template <Symbolic... Terms>
constexpr auto collapseIdentities(SymbolicExpression<Plus, Terms...> expr) {
  auto terms = expr.terms().template filter<tempura::internal::Not<isZero>>();
  if constexpr (terms.empty()) {
    return Constant<0>{};
  } else {
    return SymbolicExpression{Plus{}, terms};
  }
}

} // namespace tempura::symbolic

