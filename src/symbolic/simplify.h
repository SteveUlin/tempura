#pragma once

#include <functional>

#include "src/symbolic/matchers.h"
#include "src/symbolic/operators.h"
#include "src/symbolic/symbolic.h"
#include "src/symbolic/meta.h"

#include <string_view>

namespace tempura::symbolic {

namespace internal {

template <typename T, typename U>
struct SortLeftFn : std::conditional_t<std::less{}(typeName(T::terms().template get<0>()),
                                                   typeName(U::terms().template get<0>())),
                                       std::true_type, std::false_type> {};

template <typename T, typename U>
struct SortRightFn : std::conditional_t<std::less{}(typeName(T::terms().template get<1>()),
                                                    typeName(U::terms().template get<1>())),
                                        std::true_type, std::false_type> {};

template <typename T, typename U>
struct GroupByLeftFn : std::conditional_t<std::equal_to{}(typeName(T::terms().template get<0>()),
                                                          typeName(U::terms().template get<0>())),
                                          std::true_type, std::false_type> {};

template <typename T, typename U>
struct GroupByRightFn : std::conditional_t<std::equal_to{}(typeName(T::terms().template get<1>()),
                                                           typeName(U::terms().template get<1>())),
                                           std::true_type, std::false_type> {};

}  // namespace internal

// Evaluates Symbolic Expressions if every term is a Constant<...>. This is
// not recursive, only operating on the topmost expression.
//
// Example:
// a + 2 -> a + 2
// 2 * 7 -> 14
// 2 * (3 + 4) -> 2 * (3 + 4)
template <Symbolic Expr>
consteval auto evalIfConstantExpr(Expr expr) {
  return expr;
}

template <Operator Op, auto... VALUES>
consteval auto evalIfConstantExpr(SymbolicExpression<Op, Constant<VALUES>...>) {
  return Constant<Op{}(VALUES...)>{};
}

// mergeMultiplication combines power expressions in multiplication.
//
// mergeMultiplication assumes add of the terms are in the form of a^b and sorted
// Example:
//   a^2 * a^3 -> a^5

// template <Symbolic... Terms>
//   requires (MatchingExpr<Terms, Power, Any, Any> && ...)
// consteval auto mergeMultiplication(TypeList<Terms...> terms) {
//   if constexpr (terms.size() <= 1) {
//     return terms;
//   } else {
//     constexpr auto a = terms.template get<0>();
//     constexpr auto b = terms.template get<1>();
//
//     if constexpr (match(a.terms().head(), b.terms().head())) {
//       return mergeMultiplication(
//         concat(
//           TypeList{a.terms().head() ^ (a.terms().tail().head() + b.terms().tail().head())},
//           terms.tail().tail())
//       );
//     } else {
//       return concat(TypeList{a}, mergeMultiplication(terms.tail()));
//     }
//   }
// }

// Assume constant-variable form and sorted
template <Symbolic... Terms>
consteval auto mergeAddition(TypeList<Terms...> terms) {
  if constexpr (terms.size() <= 1) {
    return terms;
  } else {
    constexpr auto a = terms.head();
    constexpr auto b = terms.tail().head();

    if constexpr (match(a.terms().tail().head(), b.terms().tail().head())) {
      return mergeAddition(
        concat(
          TypeList{evalIfConstantExpr(a.terms().head() + b.terms().head()) * a.terms().tail().head()},
          terms.tail().tail())
      );
    } else {
      return concat(TypeList{a}, mergeAddition(terms.tail()));
    }
  }
}

consteval auto reducePower(Symbolic auto expr) {
  return expr;
}

consteval auto reducePower(MatchingExpr<Power, Any, Constant<0>> auto expr) {
  return Constant<1>{};
}

consteval auto reducePower(MatchingExpr<Power, Any, Constant<1>> auto expr) {
  return expr.terms().head();
}

template <Symbolic Base, Symbolic... Powers>
consteval auto mergeMultiplicationGroup(TypeList<decltype(Base{} ^ Powers{})...> terms) {
  constexpr auto right = terms >>= [] (auto... args) {
    return SymbolicExpression{Plus{}, args.terms().template get<1>()...};
  };
  return Base{} ^ right;
}

template <Symbolic... Terms>
consteval auto reduceMultiplication(SymbolicExpression<Multiplies, Terms...> expr) {
  constexpr auto terms = flatten(expr).terms() >>= [](auto... args) {
    return TypeList(makeVariablePower(args)...);
  };

  constexpr auto sorted = sort<internal::SortLeftFn>(terms);
  constexpr auto grouped = groupBy<internal::GroupByLeftFn>(sorted);
  // return SymbolicExpression{Multiplies{}, sorted};

  return grouped >>= [] (auto... groups) {
    return SymbolicExpression{Multiplies{}, mergeMultiplicationGroup(groups)...};
  };
}

consteval auto reduceAddition(MatchingExpr<Plus, AnyNTerms> auto expr) {
  constexpr auto terms = flatten(expr).terms() >>= [](auto... args) {
    return TypeList(makeCoefficientVariable(args)...);
  };

  constexpr auto sorted = sort<internal::SortRightFn>(terms);

  return mergeAddition(sorted) >>= [] (auto... args) {
    return SymbolicExpression{Plus{}, flatten(args)...};
  };
}

consteval auto simplify(MatchingExpr<AnyOp, AnyNTerms> auto expr) {
  return expr.terms() >>= [expr] (auto... terms) {
    auto simplified_terms = SymbolicExpression{expr.op(), simplify(terms)...};
    if constexpr (expr.op() == Plus{}) {
      return reduceAddition(simplified_terms);
    } else if constexpr (expr.op() == Multiplies{}) {
      return reduceMultiplication(simplified_terms);
    } else {
      return simplified_terms;
    }
    // if constexpr (expr.op() == Addition{}) {
    //   return reduceAddition(simplified_terms);
    // } else if constexpr (expr.op() == Multiplication{}) {
    //   return reduceMultiplication(simplified_terms);
    // } else {
    //   return simplified_terms;
    // }
  };
}

consteval auto simplify(Matching<AnySymbol> auto expr) {
  return expr;
}

consteval auto simplify(Matching<AnyConstant> auto expr) {
  return expr;
}

}  // namespace tempura::symbolic

