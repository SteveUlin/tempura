#include "symbolic/meta.h"

#include <utility>

#include "symbolic/matchers.h"
#include "symbolic/operators.h"
#include "symbolic/symbolic.h"
#include "symbolic/to_string.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic;

template <typename Lhs, typename Rhs>
struct Cmp {
  constexpr static bool value = Lhs{} <= Rhs{};
};

auto main() -> int {
  "Collect"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);
    static_assert(flatten(Plus{}, TypeList{a + b + c}) == TypeList{a, b, c});
    static_assert(flatten(Plus{}, TypeList{a + b + c + (a - a)}) ==
                  TypeList{a, b, c, a - a});
  };

  "Sort"_test = [] {
    TEMPURA_SYMBOLS(a, b, c, d, e, f, g);
    std::cout << toString(SymbolicExpression{Plus{}, sort<Cmp>(TypeList{g, f, e, d, c, b, a})});
  };

  // "Constant Part"_test = [] {
  //   constexpr auto a = SYMBOL();
  //   constexpr auto b = SYMBOL();
  //   static_assert(constantPart(Constant<3>{}) == Constant<3>{});
  //   static_assert(constantPart(a) == Constant<1>{});
  //   static_assert(constantPart(a + b) == Constant<1>{});

  //   static_assert(constantPart(Constant<3>{} * a) == Constant<3>{});
  //   static_assert(constantPart(a * Constant<3>{}) == Constant<3>{});
  //   static_assert(constantPart(π * a) == π);
  //   static_assert(constantPart(a * π) == π);
  //   static_assert(constantPart((Constant<1>{} + Constant<2>{}) * (a + b)) ==
  //   (Constant<1>{} + Constant<2>{}));
  // };

  // "Variable Part"_test = [] {
  //   constexpr auto a = SYMBOL();
  //   constexpr auto b = SYMBOL();
  //   static_assert(variablePart(Constant<3>{}) == Constant<1>{});
  //   static_assert(variablePart(a) == a);
  //   static_assert(variablePart(a + b) == a + b);
  //   static_assert(variablePart(Constant<3>{} * a) == a);
  //   static_assert(variablePart(a * Constant<3>{}) == a);
  //   static_assert(variablePart(π * a) == a);
  //   static_assert(variablePart(a * π) == a);
  //   static_assert(variablePart((Constant<1>{} + Constant<2>{}) * (a + b)) ==
  //   (a + b));
  // };

  return TestRegistry::result();
}
