#pragma once

#include "src/symbolic/matchers.h"
#include "src/symbolic/operators.h"
#include "src/symbolic/symbolic.h"
#include "src/symbolic/meta.h"

#include <string_view>

namespace tempura::symbolic {

namespace internal {

template <Symbolic T>
struct SortRightFn {
  template <Symbolic U>
  static constexpr auto operator()() -> bool {
    return typeName(T{}.terms().tail().head()) <= typeName(U{}.terms().tail().head());
  }
};

template <Symbolic T>
struct SortLeftFn {
  template <Symbolic U>
  static constexpr auto operator()() -> bool {
    return typeName(T{}.terms().head()) <= typeName(U{}.terms().head());
  }
};

}  // namespace internal

// merge constant terms
//   - addition
//   - subtraction
//   - multiplication
//   - division

consteval auto mergeConstants(MatchingExpr<Plus, AnyNTerms> auto expr) {
  constexpr auto flattened = flatten(expr).terms() >>= [] (auto... args) {
    return TypeList(mergeConstants(args)...);
  };

  constexpr auto raw_constants = flattened.template filter<IsRawConstantFn>();
  constexpr auto merged = raw_constants >>= [](auto... args) {
    return Constant<(0 + ... + args.value())>{};
  };

  constexpr auto rest = flattened.template filter<tempura::internal::Not<IsRawConstantFn>>();
  if constexpr (rest.empty()) {
    return merged;
  } else if constexpr (merged == Constant<0>{}) {
    return rest >>= [](auto... args) {
      return SymbolicExpression{Multiplies{}, args...};
    };
  } else {
    return rest >>= [](auto... args) {
      return SymbolicExpression{Multiplies{}, merged, args...};
    };
  }
}

consteval auto mergeConstants(MatchingExpr<Multiplies, AnyNTerms> auto expr) {
  constexpr auto flattened = flatten(expr).terms() >>= [] (auto... args) {
    return TypeList(mergeConstants(args)...);
  };

  constexpr auto raw_constants = flattened.template filter<IsRawConstantFn>();
  constexpr auto merged = raw_constants >>= [](auto... args) {
    return Constant<(1 * ... * args.value())>{};
  };

  constexpr auto rest = flattened.template filter<tempura::internal::Not<IsRawConstantFn>>();
  if constexpr (rest.empty()) {
    return merged;
  } else if constexpr (merged == Constant<0>{}) {
    return Constant<0>{};
  } else if constexpr (merged == Constant<1>{}) {
    if constexpr (rest.size() == 1) {
      return rest.head();
    } else {
      return rest >>= [](auto... args) {
        return SymbolicExpression{Multiplies{}, args...};
      };
    }
  } else {
    return rest >>= [](auto... args) {
      return SymbolicExpression{Multiplies{}, merged, args...};
    };
  }
}

consteval auto mergeConstants(Symbolic auto expr) {
  if constexpr (match(expr, MatcherExpression{AnyOp{}, AnyNTerms{}})) {
    return expr.terms() >>= [expr](auto... args) {
      return SymbolicExpression{expr.op(), mergeConstants(args)...};
    };
  } else {
    return expr;
  }
}

// Assume variable-power form and sorted
template <Symbolic... Terms>
consteval auto mergeMultiplication(TypeList<Terms...> terms) {
  if constexpr (terms.size() <= 1) {
    return terms;
  } else {
    constexpr auto a = terms.head();
    constexpr auto b = terms.tail().head();

    if constexpr (match(a.terms().head(), b.terms().head())) {
      return mergeMultiplication(
        concat(
          TypeList{a.terms().head() ^ (a.terms().tail().head() + b.terms().tail().head())},
          terms.tail().tail())
      );
    } else {
      return concat(TypeList{a}, mergeMultiplication(terms.tail()));
    }
  }
}

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
          TypeList{mergeConstants(a.terms().head() + b.terms().head()) * a.terms().tail().head()},
          terms.tail().tail())
      );
    } else {
      return concat(TypeList{a}, mergeAddition(terms.tail()));
    }
  }
}

template <Symbolic T>
requires (match(T{}, MatcherExpression{Multiplies{}, AnyNTerms{}}))
consteval auto reduceMultiplication(T expr) {
  constexpr auto terms = flatten(expr).terms() >>= [](auto... args) {
    return TypeList(makeVariablePower(args)...);
  };

  constexpr auto sorted = sort<internal::SortLeftFn>(terms);

  return mergeMultiplication(sorted) >>= [] (auto... args) {
    return mergeConstants(SymbolicExpression{Multiplies{}, args...});
  };
}

consteval auto reduceAddition(MatchingExpr<Plus, AnyNTerms> auto expr) {
  constexpr auto terms = flatten(expr).terms() >>= [](auto... args) {
    return TypeList(makeCoefficientVariable(args)...);
  };

  constexpr auto sorted = sort<internal::SortRightFn>(terms);

  return mergeAddition(sorted) >>= [] (auto... args) {
    return mergeConstants(SymbolicExpression{Plus{}, flatten(args)...});
  };
}

consteval auto simplify(MatchingExpr<AnyOp, AnyNTerms> auto expr) {
  return expr.terms() >>= [expr] (auto... terms) {
    auto simplified_terms = SymbolicExpression{decltype(expr)::op(), simplify(terms)...};
    if constexpr (decltype(expr)::op() == Plus{}) {
      return reduceAddition(simplified_terms);
    } else if constexpr (decltype(expr)::op() == Multiplies{}) {
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

