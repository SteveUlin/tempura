// Evaluation interpreter tests
#include <cmath>

#include "symbolic4/core.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/operators.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "evaluate constant"_test = [] {
    Constant<42> c;
    expectNear(evaluate(c), 42.0);

    // Verify fully compile-time evaluation works
    static_assert(evaluate(Constant<42>{}) == 42.0);
  };

  "evaluate fraction"_test = [] {
    Fraction<1, 2> half;
    expectNear(evaluate(half), 0.5);
  };

  "evaluate symbol with binding"_test = [] {
    Symbol<struct X> x;
    expectNear(evaluate(x, x = 3.14), 3.14);
  };

  "evaluate addition"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;
    auto expr = x + y;
    expectNear(evaluate(expr, x = 3.0, y = 4.0), 7.0);

    // Fully compile-time with constexpr bindings
    constexpr Symbol<struct A> a;
    constexpr Symbol<struct B> b;
    constexpr auto sum = a + b;
    static_assert(evaluate(sum, a = 3.0, b = 4.0) == 7.0);
  };

  "evaluate multiplication"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;
    auto expr = x * y;
    expectNear(evaluate(expr, x = 3.0, y = 4.0), 12.0);
  };

  "evaluate division"_test = [] {
    Symbol<struct X> x;
    auto expr = x / Constant<2>{};
    expectNear(evaluate(expr, x = 10.0), 5.0);
  };

  "evaluate polynomial"_test = [] {
    Symbol<struct X> x;
    // x^2 + 2x + 1 = (x+1)^2
    auto expr = x * x + Constant<2>{} * x + Constant<1>{};
    // At x=3: 9 + 6 + 1 = 16
    expectNear(evaluate(expr, x = 3.0), 16.0);
  };

  "evaluate transcendental functions"_test = [] {
    Symbol<struct X> x;

    expectNear(evaluate(sin(x), x = 0.0), 0.0);
    expectNear(evaluate(cos(x), x = 0.0), 1.0);
    expectNear(evaluate(exp(x), x = 0.0), 1.0);
    expectNear(evaluate(log(x), x = M_E), 1.0, 1e-10);
  };

  "evaluate sqrt"_test = [] {
    Symbol<struct X> x;
    expectNear(evaluate(sqrt(x), x = 4.0), 2.0);
  };

  "evaluate power"_test = [] {
    Symbol<struct X> x;
    auto expr = pow(x, Constant<3>{});
    expectNear(evaluate(expr, x = 2.0), 8.0);
  };

  "evaluate pi constant"_test = [] { expectNear(evaluate(pi), M_PI); };

  "evaluate e constant"_test = [] { expectNear(evaluate(e), M_E); };

  return TestRegistry::result();
}
