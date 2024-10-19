#pragma once

#include "symbolic/matchers.h"
#include "symbolic/meta.h"
#include "symbolic/operators.h"
#include "symbolic/symbolic.h"

namespace tempura::symbolic {

template <Symbolic Expr, Symbolic Var>
  requires(match(Var{}, AnySymbol{}))
consteval auto diff(Expr expr, Var var) {
  if constexpr (match(expr, var)) {
    return 1_c;
  }

  else if constexpr (match(expr, AnySymbol{})) {
    return 0_c;
  }

  else if constexpr (match(expr, AnyConstant{})) {
    return 0_c;
  }

  else if constexpr (match(expr, Any{} + Any{})) {
    return diff(expr.left(), var) + diff(expr.right(), var);
  }

  else if constexpr (match(expr, Any{} - Any{})) {
    return diff(expr.left(), var) - diff(expr.right(), var);
  }

  else if constexpr (match(expr, -Any{})) {
    return -diff(expr.operand(), var);
  }

  else if constexpr (match(expr, Any{} * Any{})) {
    return diff(expr.left(), var) * expr.right() +
           expr.left() * diff(expr.right(), var);
  }

  else if constexpr (match(expr, Any{} / Any{})) {
    return (diff(expr.left(), var) * expr.right() -
            expr.left() * diff(expr.right(), var)) /
           pow(expr.right(), 2_c);

  }

  else if constexpr (match(expr, pow(Any{}, Any{}))) {
    return expr.right() * pow(expr.left(), expr.right() - 1_c) *
           diff(expr.left(), var);
  }

  else if constexpr (match(expr, sin(Any{}))) {
    return cos(expr.operand()) * diff(expr.operand(), var);
  }

  else if constexpr (match(expr, cos(Any{}))) {
    return -sin(expr.operand()) * diff(expr.operand(), var);
  }

  else {
    static_assert(false, "Unimplemented derivative");
  }

}

}  // namespace tempura::symbolic
