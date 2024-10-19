#include "symbolic/operators.h"

#include "symbolic/symbolic.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Addition"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);
    static_assert((a + a)(Substitution{a = 5}) == 10);
    static_assert((a + b)(Substitution{a = 5, b = 2}) == 7);
    static_assert((a + b + c)(Substitution{a = 5, b = 2, c = 1}) == 8);
  };

  "Subtraction"_test = [] {
    TEMPURA_SYMBOLS(a, b);
    static_assert((a - a)(Substitution{a = 5}) == 0);
    static_assert((a - b)(Substitution{a = 5, b = 1}) == 4);
  };

  "Negate"_test = [] {
    TEMPURA_SYMBOLS(a);
    expectEq(-a(Substitution{a = 5}), -5);
  };

  "Multiplication"_test = [] {
    TEMPURA_SYMBOLS(a, b);
    static_assert((a * a)(Substitution{a = 5}) == 25);
    static_assert((a * b)(Substitution{a = 5, b = 2}) == 10);
  };

  "Division"_test = [] {
    TEMPURA_SYMBOLS(a, b);
    static_assert((a / a)(Substitution{a = 5}) == 1);
    static_assert((a / b)(Substitution{a = 10, b = 2}) == 5);
  };

  "Power"_test = [] {
    TEMPURA_SYMBOLS(a, b);
    expectEq(pow(a, a)(Substitution{a = 5}), 3125);
    expectEq(pow(a, b)(Substitution{a = 10, b = 2}), 100);
  };

  "Sqrt"_test = [] {
    TEMPURA_SYMBOLS(a);
    expectEq(sqrt(a)(Substitution{a = 25}), 5);
  };

  "Exp"_test = [] {
    TEMPURA_SYMBOLS(a);
    expectNear(exp(a)(Substitution{a = 1}), std::numbers::e);
  };

  "Log"_test = [] {
    TEMPURA_SYMBOLS(a);
    expectEq(log(a)(Substitution{a = std::numbers::e}), 1);
  };

  "Sin"_test = [] {
    TEMPURA_SYMBOLS(a);
    expectEq(sin(a)(Substitution{a = 0}), 0);
    expectNear(sin(a)(Substitution{a = std::numbers::pi / 2}), 1);
  };

  "Cos"_test = [] {
    TEMPURA_SYMBOLS(a);
    expectEq(cos(a)(Substitution{a = 0}), 1);
    expectNear(cos(a)(Substitution{a = std::numbers::pi}), -1);
  };

  "Tan"_test = [] {
    TEMPURA_SYMBOLS(a);
    expectEq(tan(a)(Substitution{a = 0}), 0);
    expectNear(tan(a)(Substitution{a = std::numbers::pi / 4}), 1);
  };

  "E"_test = [] { expectNear(e(Substitution{}), std::numbers::e); };

  "Pi"_test = [] { expectNear(π(Substitution{}), std::numbers::pi); };

  return TestRegistry::result();
}
