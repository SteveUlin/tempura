#include <cassert>

#include "symbolic2/symbolic.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Addition identities"_test = [] {
    Symbol x;

    // 0 + x = x
    auto expr1 = 0_c + x;
    auto s1 = simplify(expr1);
    static_assert(match(s1, x));

    // x + 0 = x
    auto expr2 = x + 0_c;
    auto s2 = simplify(expr2);
    static_assert(match(s2, x));

    // x + x = 2 * x
    auto expr3 = x + x;
    auto s3 = simplify(expr3);
    static_assert(match(s3, x * 2_c) || match(s3, 2_c * x));
  };

  "Multiplication identities"_test = [] {
    Symbol x;

    // 0 * x = 0
    auto expr1 = 0_c * x;
    auto s1 = simplify(expr1);
    static_assert(match(s1, 0_c));

    // x * 0 = 0
    auto expr2 = x * 0_c;
    auto s2 = simplify(expr2);
    static_assert(match(s2, 0_c));

    // 1 * x = x
    auto expr3 = 1_c * x;
    auto s3 = simplify(expr3);
    static_assert(match(s3, x));

    // x * 1 = x
    auto expr4 = x * 1_c;
    auto s4 = simplify(expr4);
    static_assert(match(s4, x));
  };

  "Power identities"_test = [] {
    Symbol x;

    // x^0 = 1
    auto expr1 = pow(x, 0_c);
    auto s1 = simplify(expr1);
    static_assert(match(s1, 1_c));

    // x^1 = x
    auto expr2 = pow(x, 1_c);
    auto s2 = simplify(expr2);
    static_assert(match(s2, x));

    // 1^x = 1
    auto expr3 = pow(1_c, x);
    auto s3 = simplify(expr3);
    static_assert(match(s3, 1_c));

    // 0^x = 0 (for non-zero x)
    auto expr4 = pow(0_c, x);
    auto s4 = simplify(expr4);
    static_assert(match(s4, 0_c));
  };

  "Constant folding"_test = [] {
    // Constant expressions should evaluate
    auto expr1 = 2_c + 3_c;
    auto s1 = simplify(expr1);
    static_assert(match(s1, 5_c));

    auto expr2 = 10_c * 5_c;
    auto s2 = simplify(expr2);
    static_assert(match(s2, 50_c));
  };

  "Subtraction to addition"_test = [] {
    Symbol x;
    Symbol y;

    // x - y = x + (-1) * y
    auto expr = x - y;
    auto s = simplify(expr);

    // After simplification, subtraction should be converted
    // The exact form may vary, but it should be equivalent
    constexpr auto result = evaluate(s, BinderPack{x = 10, y = 3});
    static_assert(result == 7);
  };

  "Division to multiplication"_test = [] {
    Symbol x;
    Symbol y;

    // x / y = x * y^(-1)
    auto expr = x / y;
    auto s = simplify(expr);

    // Verify by runtime evaluation
    auto result = evaluate(s, BinderPack{x = 10, y = 2});
    assert(result == 5);
  };

  "Logarithm identities"_test = [] {
    // log(1) = 0
    auto expr1 = log(1_c);
    auto s1 = simplify(expr1);
    static_assert(match(s1, 0_c));

    // log(e) = 1
    auto expr2 = log(e);
    auto s2 = simplify(expr2);
    static_assert(match(s2, 1_c));
  };

  "Exponential identities"_test = [] {
    Symbol x;

    // exp(log(x)) = x
    auto expr1 = exp(log(x));
    auto s1 = simplify(expr1);
    static_assert(match(s1, x));
  };

  "Trigonometric identities"_test = [] {
    // sin(π/2) = 1
    auto expr1 = sin(π * 0.5_c);
    auto s1 = simplify(expr1);
    static_assert(match(s1, 1_c));

    // sin(π) = 0
    auto expr2 = sin(π);
    auto s2 = simplify(expr2);
    static_assert(match(s2, 0_c));

    // cos(π) = -1
    auto expr3 = cos(π);
    auto s3 = simplify(expr3);
    static_assert(match(s3, Constant<-1>{}));

    // sin(0) = 0
    constexpr auto expr4 = sin(0_c);
    constexpr auto s4 = simplify(expr4);
    static_assert(match(s4, 0_c));

    // cos(0) = 1
    constexpr auto expr5 = cos(0_c);
    constexpr auto s5 = simplify(expr5);
    static_assert(match(s5, 1_c));

    // tan(0) = 0
    constexpr auto expr6 = tan(0_c);
    constexpr auto s6 = simplify(expr6);
    static_assert(match(s6, 0_c));
  };

  "Trigonometric symmetry"_test = [] {
    Symbol x;

    // sin(-x) = -sin(x)
    auto expr1 = sin(-x);
    auto s1 = simplify(expr1);
    static_assert(match(s1, -sin(x)));

    // cos(-x) = cos(x)
    auto expr2 = cos(-x);
    auto s2 = simplify(expr2);
    static_assert(match(s2, cos(x)));

    // tan(-x) = -tan(x)
    auto expr3 = tan(-x);
    auto s3 = simplify(expr3);
    static_assert(match(s3, -tan(x)));
  };

  "Hyperbolic identities"_test = [] {
    Symbol x;

    // sinh(0) = 0
    auto expr1 = sinh(0_c);
    auto s1 = simplify(expr1);
    static_assert(match(s1, 0_c));

    // cosh(0) = 1
    auto expr2 = cosh(0_c);
    auto s2 = simplify(expr2);
    static_assert(match(s2, 1_c));

    // tanh(0) = 0
    auto expr3 = tanh(0_c);
    auto s3 = simplify(expr3);
    static_assert(match(s3, 0_c));
  };

  "Hyperbolic symmetry"_test = [] {
    Symbol x;

    // sinh(-x) = -sinh(x) (odd function)
    auto expr1 = sinh(-x);
    auto s1 = simplify(expr1);
    static_assert(match(s1, -sinh(x)));

    // cosh(-x) = cosh(x) (even function)
    auto expr2 = cosh(-x);
    auto s2 = simplify(expr2);
    static_assert(match(s2, cosh(x)));

    // tanh(-x) = -tanh(x) (odd function)
    auto expr3 = tanh(-x);
    auto s3 = simplify(expr3);
    static_assert(match(s3, -tanh(x)));
  };

  "Complex expression"_test = [] {
    Symbol x;

    // (x + 1) * (x + 1) should distribute
    auto expr = (x + 1_c) * (x + 1_c);
    auto s = simplify(expr);

    // Verify by evaluation
    constexpr auto result = evaluate(s, BinderPack{x = 5});
    static_assert(result == 36);  // (5+1)^2 = 36
  };

  return TestRegistry::result();
}
