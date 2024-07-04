#include "src/symbolic/symbolic.h"
#include "src/symbolic/operators.h"
#include "src/symbolic/matchers.h"

#include "src/unit.h"

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Identity"_test = [] {
    constexpr auto a = SYMBOL();
    static_assert(match(a, a));
    static_assert(match(a + a, a + a));
  };

  "Any"_test = [] {
    constexpr auto a = SYMBOL();
    static_assert(match(Any{}, Constant<3>{}));
    static_assert(match(Any{}, a));
    static_assert(match(Any{}, a + a));
  };

  "AnyNTerms"_test = [] {
    constexpr auto a = SYMBOL();
    static_assert(match(MatcherExpression<Plus, AnyNTerms>{}, a + a));
    static_assert(!match(MatcherExpression<Plus, AnyNTerms>{}, a - a));
    static_assert(!match(MatcherExpression<Plus, AnyNTerms>{}, a));
  };

  return TestRegistry::result();
}
