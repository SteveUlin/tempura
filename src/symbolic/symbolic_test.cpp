#include "src/symbolic/symbolic.h"
#include "src/symbolic/operators.h"

#include "src/unit.h"

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Constants"_test = [] {
    constexpr Constant<3> a;
    constexpr Constant<4> b;
    static_assert(a == a);
    static_assert(a != b);
  };

  "Symbols"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    static_assert(a == a);
    static_assert(a != b);
  };

  "Addition"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    static_assert((a + a)(Substitution{a = 5}) == 10);
    static_assert((a + b)(Substitution{a = 5, b = 2}) == 7);
  };

  "Subtraction"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    static_assert((a - a)(Substitution{a = 5}) == 0);
    static_assert((a - b)(Substitution{a = 5, b = 1}) == 4);
  };

  "Multiplication"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    static_assert((a * a)(Substitution{a = 5}) == 25);
    static_assert((a * b)(Substitution{a = 5, b = 2}) == 10);
  };

  "Division"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    static_assert((a / a)(Substitution{a = 5}) == 1);
    static_assert((a / b)(Substitution{a = 10, b = 2}) == 5);
  };

  return 0;
}
