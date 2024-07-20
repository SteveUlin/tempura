#include "src/symbolic/meta.h"

#include "src/symbolic/matchers.h"
#include "src/symbolic/operators.h"
#include "src/symbolic/symbolic.h"
#include "src/unit.h"

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Flatten"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    constexpr auto c = SYMBOL();
    static_assert(!match(a + b + c, SymbolicExpression(Plus{}, a, b, c)));
    static_assert(match(flatten(a + b + c), SymbolicExpression(Plus{}, a, b, c)));
    static_assert(match(flatten(a + b + c + (a - a)), SymbolicExpression(Plus{}, a, b, c, a - a)));
  };

  "Constant Part"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    static_assert(constantPart(Constant<3>{}) == Constant<3>{});
    static_assert(constantPart(a) == Constant<1>{});
    static_assert(constantPart(a + b) == Constant<1>{});

    static_assert(constantPart(Constant<3>{} * a) == Constant<3>{});
    static_assert(constantPart(a * Constant<3>{}) == Constant<3>{});
    static_assert(constantPart(π * a) == π);
    static_assert(constantPart(a * π) == π);
    static_assert(constantPart((Constant<1>{} + Constant<2>{}) * (a + b)) == (Constant<1>{} + Constant<2>{}));
  };

  "Variable Part"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    static_assert(variablePart(Constant<3>{}) == Constant<1>{});
    static_assert(variablePart(a) == a);
    static_assert(variablePart(a + b) == a + b);
    static_assert(variablePart(Constant<3>{} * a) == a);
    static_assert(variablePart(a * Constant<3>{}) == a);
    static_assert(variablePart(π * a) == a);
    static_assert(variablePart(a * π) == a);
    static_assert(variablePart((Constant<1>{} + Constant<2>{}) * (a + b)) == (a + b));
  };

  return TestRegistry::result();
}
