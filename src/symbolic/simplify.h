#pragma once

#include <string_view>

#include "symbolic/matchers.h"
#include "symbolic/meta.h"
#include "symbolic/operators.h"
#include "symbolic/symbolic.h"

namespace tempura::symbolic {

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
  requires(requires() { [] constexpr { return Op{}(VALUES...); }; })
consteval auto evalIfConstantExpr(
    SymbolicExpression<Op, Constant<VALUES>...> /*unused*/) {
  return Constant<Op{}(VALUES...)>{};
}

consteval auto multiplicationIdentities(Symbolic auto expr) {
  if constexpr (match(expr, 0_c * Any{})) {
    return 0_c;
  } else if constexpr (match(expr, Any{} * 0_c)) {
    return 0_c;
  } else {
    return expr;
  }
}

consteval auto powIdentities(Symbolic auto expr) {
  if constexpr (match(expr, pow(Any{}, 0_c))) {
    return 1_c;
  } else if constexpr (match(expr, pow(Any{}, 1_c))) {
    return expr.left();
  } else {
    return expr;
  }
}

// Sort terms by the variables in the expression.
// a + b + 2 * a -> a + 2 * a + b
template <Symbolic Lhs, Symbolic Rhs>
struct AdditionCmp {
  constexpr static bool value = [] {
    if constexpr (match(Lhs{}, (Any{} * Any{})) and
                  match(Rhs{}, (Any{} * Any{}))) {
      if (match(Lhs{}.right(), Rhs{}.right())) {
        return Lhs{}.left() <= Rhs{}.left();
      }
      return Lhs{}.right() <= Rhs{}.right();
    }

    else if constexpr (match(Lhs{}, (Any{} * Any{}))) {
      if (match(Lhs{}.right(), Rhs{})) {
        return false;
      }
      return Lhs{}.right() <= Rhs{};
    }

    else if constexpr (match(Rhs{}, (Any{} * Any{}))) {
      if (match(Lhs{}, Rhs{}.right())) {
        return true;
      }
      return Lhs{} <= Rhs{}.right();
    }

    else {
      return Lhs{} <= Rhs{};
    }
  }();
};

// Combine like terms in addition. This assumes that the TypeList has
// already been sorted by variables.
// a + b + 2 * a -> 3 * a + b
template <Symbolic... Terms>
consteval auto mergeAddition(TypeList<Terms...> list) {
  if constexpr (list.size <= 1) {
    return list;
  } else {
    constexpr auto a = list.head();
    constexpr auto b = list.tail().head();
    constexpr auto rest = list.tail().tail();

    if constexpr (match(a, 0_c)) {
      return mergeAddition(list.tail());
    }

    else if constexpr (match(b, 0_c)) {
      return mergeAddition(concat(TypeList{a}, rest));
    }

    else if constexpr (match(a, Any{} * Any{}) and match(b, Any{} * Any{})) {
      if constexpr (match(a.right(), b.right())) {
        auto coeff = evalIfConstantExpr(a.left() + b.left());
        return mergeAddition(concat(
            TypeList{multiplicationIdentities(coeff * a.right())}, rest));
      } else {
        return concat(TypeList{a}, mergeAddition(list.tail()));
      }
    }

    else if constexpr (match(a, Any{} * Any{})) {
      if constexpr (match(a.right(), b)) {
        auto coeff = evalIfConstantExpr(1_c + a.left());
        return mergeAddition(
            concat(TypeList{multiplicationIdentities(coeff * b)}, rest));
      } else {
        return concat(TypeList{a}, mergeAddition(list.tail()));
      }
    }

    else if constexpr (match(b, Any{} * Any{})) {
      if constexpr (match(a, b.right())) {
        auto coeff = evalIfConstantExpr(1_c + b.left());
        return mergeAddition(
            concat(TypeList{multiplicationIdentities(coeff * a)}, rest));
      } else {
        return concat(TypeList{a}, mergeAddition(list.tail()));
      }
    }

    else if constexpr (match(a, b)) {
      return mergeAddition(concat(TypeList{2_c * a}, rest));
    }

    else {
      return concat(TypeList{a}, mergeAddition(list.tail()));
    }
  }
}

consteval auto reduceAddition(Symbolic auto expr) {
  auto terms = mergeAddition(sort<AdditionCmp>(flatten(Plus{}, expr.terms())));
  return
      []<typename... Args>(TypeList<Args...>) { return (Args{} + ...); }(terms);
}

// Sort terms by the variables in the expression.
// a^b * b * a -> a * a^b * b
template <Symbolic Lhs, Symbolic Rhs>
struct MultiplicationCmp {
  constexpr static bool value = [] {
    if constexpr (match(Lhs{}, pow(Any{}, Any{})) and
                  match(Rhs{}, pow(Any{}, Any{}))) {
      if (match(Lhs{}.right(), Rhs{}.right())) {
        return Lhs{}.left() <= Rhs{}.left();
      }
      return Lhs{}.right() <= Rhs{}.right();
    }

    else if constexpr (match(Lhs{}, pow(Any{}, Any{}))) {
      if (match(Lhs{}.right(), Rhs{})) {
        return false;
      }
      return Lhs{}.right() <= Rhs{};
    }

    else if constexpr (match(Rhs{}, pow(Any{}, Any{}))) {
      if (match(Lhs{}, Rhs{}.right())) {
        return true;
      }
      return Lhs{} <= Rhs{}.right();
    }

    else {
      return Lhs{} <= Rhs{};
    }
  }();
};

// mergeMultiplication combines power expressions in multiplication.
//
// mergeMultiplication assumes add of the terms are in the form of a^b and
// sorted
//   a^2 * a^3 -> a^5
template <Symbolic... Terms>
consteval auto mergeMultiplication(TypeList<Terms...> list) {
  if constexpr (list.size <= 1) {
    return list;
  } else {
    constexpr auto a = list.head();
    constexpr auto b = list.tail().head();
    constexpr auto rest = list.tail().tail();

    if constexpr (match(a, 0_c) || match(b, 0_c)) {
      return TypeList{0_c};
    }

    else if constexpr (match(a, 1_c)) {
      return mergeMultiplication(list.tail());
    }

    else if constexpr (match(b, 1_c)) {
      return mergeMultiplication(concat(TypeList{a}, rest));
    }

    else if constexpr (match(a, pow(Any{}, Any{})) and
                       match(b, pow(Any{}, Any{}))) {
      if constexpr (match(a.left(), b.left())) {
        auto power = evalIfConstantExpr(a.right() + b.right());
        return mergeMultiplication(
            concat(TypeList{powIdentities(pow(a.left(), power))}, rest));
      } else {
        return concat(TypeList{a}, mergeMultiplication(list.tail()));
      }
    }

    else if constexpr (match(a, pow(Any{}, Any{}))) {
      if constexpr (match(a.left(), b)) {
        auto power = evalIfConstantExpr(1_c + a.right());
        return mergeMultiplication(
            concat(TypeList{powIdentities(pow(a.left(), power))}, rest));
      } else {
        return concat(TypeList{a}, mergeMultiplication(list.tail()));
      }
    }

    else if constexpr (match(b, pow(Any{}, Any{}))) {
      if constexpr (match(a, b.left())) {
        auto power = evalIfConstantExpr(1_c + b.right());
        return mergeMultiplication(
            concat(TypeList{powIdentities(pow(b.left(), power))}, rest));
      } else {
        return concat(TypeList{a}, mergeMultiplication(list.tail()));
      }
    }

    else if constexpr (match(a, b)) {
      return mergeMultiplication(concat(TypeList{pow(a, 2_c)}, rest));
    }

    else {
      return concat(TypeList{a}, mergeMultiplication(list.tail()));
    }
  }
}

consteval auto distribute(Symbolic auto expr) {
  if constexpr (match(expr, Any{} * (Any{} + Any{}))) {
    auto a = expr.left();
    auto b = expr.right().left();
    auto c = expr.right().right();
    return a * b + a * c;
  }

  else if constexpr (match(expr, (Any{} + Any{}) * Any{})) {
    auto a = expr.left().left();
    auto b = expr.left().right();
    auto c = expr.right();
    return a * c + b * c;
  }

  else {
    return expr;
  }
}

consteval auto reduceMultiplication(Symbolic auto expr) {
  auto terms = mergeMultiplication(
      sort<MultiplicationCmp>(flatten(Multiplies{}, expr.terms())));
  return
      []<typename... Args>(TypeList<Args...>) { return (Args{} * ...); }(terms);
}

consteval auto simplify(Symbolic auto expr);

// Like flatten, but calls simplify on each term.
template <Operator Op, Symbolic... Terms>
consteval auto simplifyFlatten(Op op, TypeList<Terms...> terms) {
  if constexpr (sizeof...(Terms) == 0) {
    return TypeList<>{};
  }

  else if constexpr (match(terms.head(), makeExpr(op, AnyNTerms{}))) {
    return simplifyFlatten(op, concat(terms.head().terms(), terms.tail()));
  }

  else {
    auto simplified = simplify(terms.head());
    if constexpr (match(simplified, makeExpr(op, AnyNTerms{}))) {
      return simplifyFlatten(op, concat(simplified, terms.tail()));
    } else {
      return concat(TypeList{simplified}, simplifyFlatten(op, terms.tail()));
    }
  }
}

consteval auto simplifyAddition(Symbolic auto expr) {
  auto flattented = simplifyFlatten(Plus{}, expr.terms());
  auto sorted = sort<AdditionCmp>(flattented);
  auto merged = mergeAddition(sorted);
  return []<typename... Args>(TypeList<Args...>) {
    return (Args{} + ...);
  }(merged);
}

consteval auto simplifyMultiplication(Symbolic auto expr) {
  auto flattented = simplifyFlatten(Multiplies{}, expr.terms());
  auto sorted = sort<MultiplicationCmp>(flattented);
  auto merged = mergeMultiplication(sorted);
  return []<typename... Args>(TypeList<Args...>) {
    return (Args{} * ...);
  }(merged);
}

consteval auto simplifyChildren(Symbolic auto expr) {
  auto result = [&]<typename... Args>(TypeList<Args...> /*unused*/) {
    return makeExpr(expr.op(), simplify(Args{})...);
  }(expr.terms());
  return evalIfConstantExpr(result);
}

consteval auto simplify(Symbolic auto expr) {
  if constexpr (match(expr, Any{} + Any{})) {
    return simplifyAddition(expr);
  }

  else if constexpr (match(expr, (Any{} + Any{}) * Any{}) or
                     match(expr, Any{} * (Any{} + Any{}))) {
    return simplify(distribute(expr));
  }

  else if constexpr (match(expr, Any{} * Any{})) {
    return simplifyMultiplication(expr);
  }

  else if constexpr (match(expr, pow(Any{}, Any{}))) {
    auto tmp = simplifyChildren(expr);
    return powIdentities(tmp);
  }

  else if constexpr (match(expr, makeExpr(AnyOp{}, AnyNTerms{}))) {
    return simplifyChildren(expr);
  }

  else {
    return expr;
  }
}

}  // namespace tempura::symbolic
