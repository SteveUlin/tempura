#pragma once

#include "src/symbolic/symbolic.h"
#include "src/symbolic/operators.h"
#include "src/symbolic/matchers.h"
#include "src/type_list.h"

namespace tempura::symbolic {

// flatten scans down the tree of expressions for terms matching the top
// level operator, and flattens them into a single top level expression.
//
// The resulting express will be reflect an in order traversal of the
// original expression tree.

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

} // namespace internal

template <Symbolic Expr>
consteval auto flatten(Expr expr) {
  return expr;
}

template <Symbolic Expr>
  requires MatchingExpr<Expr, AnyOp, AnyNTerms>
consteval auto flatten(Expr expr) {
  return SymbolicExpression(expr.op(), internal::flattenImpl(expr.op(), expr.terms()));
}

// IsConstantFn is true_type if the provided Symbolic is one of
// - a Constant
// - an Expression with zero args
// - an Expression comprised of args with IsConstnatsFn == true.

template <Symbolic Expr>
struct IsConstantFn : std::false_type {};

template <Matching<AnyConstant> Expr>
struct IsConstantFn<Expr> : std::true_type {};

template <MatchingExpr<AnyOp> Expr>
struct IsConstantFn<Expr> : std::true_type {};

template <Operator Op, Symbolic... Args>
  requires (IsConstantFn<Args>::value && ...)
struct IsConstantFn<SymbolicExpression<Op, Args...>> : std::true_type {};

// IsRawConstantFn is true_type if the provided Symbolic is a Constant

template <Symbolic Expr>
struct IsRawConstantFn : std::false_type {};

template <Matching<AnyConstant> Expr>
struct IsRawConstantFn<Expr> : std::true_type {};

// constantPart returns the Constant<3> in 3 * x.
//
// If no such constant exsit, constantPart returns Constant<1>.

template <Symbolic Expr>
consteval auto constantPart(Expr expr) {
  return Constant<1>{};
}

template <Symbolic Expr>
  requires IsConstantFn<Expr>::value
consteval auto constantPart(Expr expr) {
  return expr;
}

template <MatchingExpr<Multiplies, AnyNTerms> Expr>
consteval auto constantPart(Expr expr) {
  constexpr auto constants = expr.terms().template filter<IsConstantFn>();
  if constexpr (constants.empty()) {
      return Constant<1>{};
  } else if constexpr (constants.size() == 1) {
    return constants.head();
  } else {
    return SymbolicExpression{Multiplies{}, constants};
  }
}

// variablePart

template <Symbolic Expr>
consteval auto variablePart(Expr expr) {
    return expr;
}

template <Symbolic Expr>
  requires Matching<Expr, AnyConstant>
consteval auto variablePart(Expr expr) {
    return Constant<1>{};
}

template <Symbolic Expr>
  requires MatchingExpr<Expr, Multiplies, AnyNTerms>
consteval auto variablePart(Expr expr) {
  constexpr auto variables = expr.terms().template invFilter<IsConstantFn>();
  if constexpr (variables.empty()) {
      return Constant<1>{};
  } else if constexpr (variables.size() == 1) {
    return variables.head();
  } else {
    return SymbolicExpression{Multiplies{}, variables};
  }
}

template <Symbolic Expr>
consteval auto makeCoefficientVariable(Expr expr) {
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

