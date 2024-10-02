#include "symbolic/matchers.h"

#include "symbolic/operators.h"
#include "symbolic/symbolic.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Identity"_test = [] {
    TEMPURA_SYMBOL(a);
    static_assert(match(a, a));
    static_assert(match(a + a, a + a));
  };

  "Any"_test = [] {
    TEMPURA_SYMBOL(a);
    static_assert(match(Any{}, Constant<3>{}));
    static_assert(match(Any{}, a));
    static_assert(match(Any{}, a + a));
  };

  "AnyNTerms"_test = [] {
    TEMPURA_SYMBOL(a);
    static_assert(match(MatcherExpression<Plus, AnyNTerms>{}, a + a));
    static_assert(!match(MatcherExpression<Plus, AnyNTerms>{}, a - a));
    static_assert(!match(MatcherExpression<Plus, AnyNTerms>{}, a));
  };

  return TestRegistry::result();
}
