#pragma once

#include "src/symbolic/symbolic.h"
#include "src/symbolic/matchers.h"
#include "src/type_list.h"

namespace tempura::symbolic {

namespace internal {

template <Operator OpT, Symbolic... Terms>
consteval auto flattenImpl(OpT op, TypeList<Terms...> terms) {
  if constexpr (sizeof...(Terms) == 0) {
    return TypeList<>{};
  } else if constexpr (match(terms.head(),
                             MatcherExpression<OpT, AnyNTerms>{})) {
    return flattenImpl(op, concat(terms.head().terms(), terms.tail()));
  } else {
    return flattenImpl(op, terms.tail()) >>= [&](Symbolic auto... rest) {
        return TypeList{terms.head(), rest...};
    };
  }
}

struct IsConstantFn {
  template <Symbolic T>
  static constexpr auto operator()() {
    if constexpr (match(T{}, AnySymbol{})) {
      return false;
    } else if constexpr (match(T{}, MatcherExpression{AnyOp{}, AnyNTerms{}})) {
      return T::terms() >>= [](auto... args) {
        return (true && ... && IsConstantFn::operator()<decltype(args)>());
      };
    } else {
      return true;
    }
  }
};

} // namespace internal

struct IsRawConstantFn {
  template <Symbolic T>
  static constexpr auto operator()() {
    return match(T{}, AnyConstant{});
  }
};

consteval auto flatten(Symbolic auto expr) {
  if constexpr (match(expr, MatcherExpression{AnyOp{}, AnyNTerms{}})) {
    return internal::flattenImpl(expr.op(), expr.terms()) >>= [&](auto... terms) {
      return SymbolicExpression{expr.op(), terms...};
    };
  } else {
    return expr;
  }
}

consteval auto constantPart(Symbolic auto expr) {
  if constexpr (internal::IsConstantFn::operator()<decltype(expr)>()) {
    return expr;
  } else if constexpr (!match(expr, MatcherExpression{Multiplies{}, AnyNTerms{}})) {
    return Constant<1>{};
  } else {
    return expr.terms().template filter<internal::IsConstantFn>() >>= [&](auto... args) {
      if constexpr (sizeof...(args) == 0) {
        return Constant<1>{};
      } else if constexpr (sizeof...(args) == 1) {
        return TypeList(args...).head();
      } else {
        return SymbolicExpression{Multiplies{}, args...};
      }
    };
  }
}

consteval auto variablePart(Symbolic auto expr) {
  if constexpr (match(expr, AnyConstant{})) {
    return Constant<1>{};
  } else if constexpr (!match(expr, MatcherExpression{Multiplies{}, AnyNTerms{}})) {
    return expr;
  } else {
    return expr.terms().template filter<tempura::internal::Not<internal::IsConstantFn>>() >>= [&](auto... args) {
      if constexpr (sizeof...(args) == 0) {
        return Constant<1>{};
      } else if constexpr (sizeof...(args) == 1) {
        return TypeList(args...).head();
      } else {
        return SymbolicExpression{Multiplies{}, args...};
      }
    };
  }
}

consteval auto makeCoefficientVariable(Symbolic auto expr) {
  return SymbolicExpression{Multiplies{}, constantPart(expr), variablePart(expr)};
}

consteval auto makeVariablePower(Symbolic auto expr) {
  if constexpr (match(expr, MatcherExpression{Power{}, Any{}, Any{}})) {
    return expr;
  } else {
    return SymbolicExpression{Power{}, expr, Constant<1>{}};
  }
}

} // namespace tempura::symbolic

