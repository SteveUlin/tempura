#include "src/symbolic/operators.h"
#include "src/symbolic/symbolic.h"
#include "src/symbolic/to_string.h"
#include "src/unit.h"

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Constants"_test = [] {
    expectEq("2", toString(Constant<2>{}, Substitution{}));
  };

  "Symbols"_test = [] {
    constexpr auto a = SYMBOL();

    expectEq("a", toString(a, Substitution{a = "a"}));
    expectEq("b", toString(a, Substitution{a = "b"}));
  };

  "Addition"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    constexpr auto c = SYMBOL();

    expectEq("(a + b) + c", toString(a + b + c, Substitution{a = "a", b = "b", c = "c"}));
  };

  "Sin"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    constexpr auto c = SYMBOL();

    expectEq("sin(a + b) + c", toString(sin(a + b) + c, Substitution{a = "a", b = "b", c = "c"}));
  };

  return TestRegistry::result();
}
