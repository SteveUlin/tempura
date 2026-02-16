#include "symbolic5/diff.h"
#include "symbolic5/simplify.h"
#include "symbolic5/to_string.h"
#include "unit.h"

using namespace tempura;

int main() {
  // ─── Leaves ────────────────────────────────────────────────────────────────

  "toString symbol"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr Expr f{.tree = toInfo(x), .meta = metaOf(x)};
    expectEq(toString<f>(), std::string("x"));
  };

  "toString constant"_test = [] {
    constexpr Expr f{.tree = ^^Constant<42>};
    expectEq(toString<f>(), std::string("42"));
  };

  "toString constant double"_test = [] {
    constexpr Expr f{.tree = ^^Constant<3.14>};
    expectEq(toString<f>(), std::string("3.14"));
  };

  // ─── Precedence and parenthesization ───────────────────────────────────────

  "toString x + y"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = x + y;
    expectEq(toString<f>(), std::string("x + y"));
  };

  "toString x * x + x (precedence)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * x + x;
    // mul binds tighter than add — no parens needed around x * x
    expectEq(toString<f>(), std::string("x * x + x"));
  };

  "toString (x + y) * z (lower-prec child gets parens)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto z = sym<"z">();
    constexpr auto f = (x + y) * z;
    expectEq(toString<f>(), std::string("(x + y) * z"));
  };

  "toString x - (y - z) (right-assoc parens)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto z = sym<"z">();
    constexpr auto f = x - (y - z);
    // Right operand of subtraction at equal precedence needs parens
    expectEq(toString<f>(), std::string("x - (y - z)"));
  };

  "toString x / (x + 1)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x / (x + lit<1.0>);
    expectEq(toString<f>(), std::string("x / (x + 1.0)"));
  };

  // ─── Functions and unary ops ───────────────────────────────────────────────

  "toString sin(x) * exp(y)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = sin(x) * exp(y);
    expectEq(toString<f>(), std::string("sin(x) * exp(y)"));
  };

  "toString negation"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = -x;
    expectEq(toString<f>(), std::string("-x"));
  };

  // ─── Binary function ops ───────────────────────────────────────────────

  "toString pow(x, y)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = pow(x, y);
    expectEq(toString<f>(), std::string("pow(x, y)"));
  };

  "toString pow(x + y, z) (args not parenthesized)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto z = sym<"z">();
    constexpr auto f = pow(x + y, z);
    expectEq(toString<f>(), std::string("pow(x + y, z)"));
  };

  "toString tan(x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = tan(x);
    expectEq(toString<f>(), std::string("tan(x)"));
  };

  "toString sqrt(x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = sqrt(x);
    expectEq(toString<f>(), std::string("sqrt(x)"));
  };

  "toString abs(x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = abs(x);
    expectEq(toString<f>(), std::string("abs(x)"));
  };

  // ─── Named constants (zero-arg ops) ────────────────────────────────────

  "toString pi"_test = [] {
    constexpr auto f = pi();
    expectEq(toString<f>(), std::string("π"));
  };

  "toString pi * x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = pi() * x;
    expectEq(toString<f>(), std::string("π * x"));
  };

  // ─── Integration with diff ─────────────────────────────────────────────────

  "toString derivative d(x²)/dx = 2 * x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * x;
    constexpr auto df = simplify(diff(f, x));
    // d(x²)/dx = x + x → 2.0 * x via like-term collection + constant folding
    expectEq(toString<df>(), std::string("2.0 * x"));
  };

  return TestRegistry::result();
}
