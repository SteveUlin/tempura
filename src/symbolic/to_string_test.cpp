#include "symbolic/to_string.h"

#include "symbolic/operators.h"
#include "symbolic/symbolic.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Constants"_test = [] { expectEq("2", toString(Constant<2>{})); };

  "Symbols"_test = [] {
    TEMPURA_SYMBOL(a);

    expectEq("a", toString(a));
  };

  "Addition"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);

    expectEq("(a + b) + c", toString(a + b + c));
  };

  "Subtraction"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);
    expectEq("(a - b) - c", toString(a - b - c));
  };

  "Multiplication"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);
    expectEq("(a * b) * c", toString(a * b * c));
  };

  "Division"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);
    expectEq("(a / b) / c", toString(a / b / c));
  };

  "Sin"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);
    expectEq("sin(a + b) + c", toString(sin(a + b) + c));
  };

  "Cos"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);
    expectEq("cos(a + b) + c", toString(cos(a + b) + c));
  };

  "Tan"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);
    expectEq("tan(a + b) + c", toString(tan(a + b) + c));
  };

  "E"_test = [] { expectEq("e", toString(e)); };

  "Pi"_test = [] { expectEq("π", toString(π)); };

  return TestRegistry::result();
}
