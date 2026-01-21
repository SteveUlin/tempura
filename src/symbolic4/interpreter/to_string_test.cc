// String conversion interpreter tests
#include "symbolic4/core.h"
#include "symbolic4/interpreter/to_string.h"
#include "symbolic4/operators.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // Shared symbols for tests
  Symbol<struct X> x;
  Symbol<struct Y> y;

  "toString constants"_test = [] {
    expectEq(toString(Constant<42>{}), std::string("42"));
    expectEq(toString(Constant<-7>{}), std::string("-7"));
    expectEq(toString(Constant<0>{}), std::string("0"));
  };

  "toString fractions"_test = [] {
    expectEq(toString(Fraction<1, 2>{}), std::string("1/2"));
    expectEq(toString(Fraction<3, 4>{}), std::string("3/4"));
    expectEq(toString(Fraction<3, 1>{}), std::string("3"));  // Simplifies to integer
  };

  "toString special constants"_test = [] {
    expectEq(toString(pi), std::string("π"));
    expectEq(toString(e), std::string("e"));
  };

  "toString symbols"_test = [&] {
    expectEq(toString(x, name(x, "x")), std::string("x"));
    expectEq(toString(y, name(y, "y")), std::string("y"));
    expectEq(toString(x, name(x, "α")), std::string("α"));  // Unicode names
  };

  "toString arithmetic"_test = [&] {
    expectEq(toString(x + y, name(x, "x"), name(y, "y")), std::string("x + y"));
    expectEq(toString(x - y, name(x, "x"), name(y, "y")), std::string("x - y"));
    expectEq(toString(x * y, name(x, "x"), name(y, "y")), std::string("x * y"));
    expectEq(toString(x / y, name(x, "x"), name(y, "y")), std::string("x / y"));
    expectEq(toString(pow(x, Constant<3>{}), name(x, "x")), std::string("x ^ 3"));
  };

  "toString unary"_test = [&] {
    expectEq(toString(-x, name(x, "x")), std::string("-(x)"));
    expectEq(toString(sin(x), name(x, "x")), std::string("sin(x)"));
    expectEq(toString(cos(x), name(x, "x")), std::string("cos(x)"));
    expectEq(toString(tan(x), name(x, "x")), std::string("tan(x)"));
    expectEq(toString(exp(x), name(x, "x")), std::string("exp(x)"));
    expectEq(toString(log(x), name(x, "x")), std::string("log(x)"));
    expectEq(toString(sqrt(x), name(x, "x")), std::string("sqrt(x)"));
  };

  "toString precedence"_test = [&] {
    // Addition/subtraction vs multiplication: no parens needed
    expectEq(toString(x + y * x, name(x, "x"), name(y, "y")),
             std::string("x + y * x"));
    expectEq(toString(x * y + x, name(x, "x"), name(y, "y")),
             std::string("x * y + x"));

    // Parens needed when lower precedence is child of higher
    expectEq(toString((x + y) * x, name(x, "x"), name(y, "y")),
             std::string("(x + y) * x"));
    expectEq(toString(x * (y + x), name(x, "x"), name(y, "y")),
             std::string("x * (y + x)"));
  };

  "toString associativity"_test = [&] {
    // Non-associative operators need parens on right operand
    expectEq(toString(x - (y - x), name(x, "x"), name(y, "y")),
             std::string("x - (y - x)"));
    expectEq(toString(x / (y / x), name(x, "x"), name(y, "y")),
             std::string("x / (y / x)"));

    // Left-associative grouping doesn't need parens
    expectEq(toString((x - y) - x, name(x, "x"), name(y, "y")),
             std::string("x - y - x"));
    expectEq(toString((x / y) / x, name(x, "x"), name(y, "y")),
             std::string("x / y / x"));
  };

  "toString nested"_test = [&] {
    expectEq(toString(sin(x * x), name(x, "x")), std::string("sin(x * x)"));
    expectEq(toString(sin(x * x + Constant<1>{}), name(x, "x")),
             std::string("sin(x * x + 1)"));
    expectEq(toString(exp(sin(x)), name(x, "x")), std::string("exp(sin(x))"));
    expectEq(toString(log(Constant<1>{} + x * x), name(x, "x")),
             std::string("log(1 + x * x)"));
  };

  "toString polynomials"_test = [&] {
    auto poly = x * x + Constant<2>{} * x + Constant<1>{};
    expectEq(toString(poly, name(x, "x")), std::string("x * x + 2 * x + 1"));

    auto cubic =
        x * x * x - Constant<3>{} * x * x + Constant<3>{} * x - Constant<1>{};
    expectEq(toString(cubic, name(x, "x")),
             std::string("x * x * x - 3 * x * x + 3 * x - 1"));
  };

  "toString complex expressions"_test = [&] {
    // sin(x)cos(y) + cos(x)sin(y)
    auto sum_formula = sin(x) * cos(y) + cos(x) * sin(y);
    expectEq(toString(sum_formula, name(x, "x"), name(y, "y")),
             std::string("sin(x) * cos(y) + cos(x) * sin(y)"));

    // sigmoid: e^x / (1 + e^x)
    auto sigmoid = exp(x) / (Constant<1>{} + exp(x));
    expectEq(toString(sigmoid, name(x, "x")),
             std::string("exp(x) / (1 + exp(x))"));

    // distance: sqrt(x^2 + y^2)
    auto distance = sqrt(x * x + y * y);
    expectEq(toString(distance, name(x, "x"), name(y, "y")),
             std::string("sqrt(x * x + y * y)"));
  };

  return TestRegistry::result();
}
