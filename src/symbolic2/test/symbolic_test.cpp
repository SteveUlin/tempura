#include "symbolic2/symbolic.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Symbol creation"_test = [] {
    Symbol x;
    Symbol y;

    // Each Symbol is unique
    static_assert(!isSame<decltype(x), decltype(y)>);
  };

  "Constants"_test = [] {
    constexpr auto a = 3_c;
    constexpr auto b = 4_c;

    static_assert(match(a, 3_c));
    static_assert(!match(a, 4_c));
    static_assert(a.value == 3);
    static_assert(b.value == 4);
  };

  "Expression building"_test = [] {
    Symbol x;
    Symbol y;

    auto expr1 = x + y;
    auto expr2 = x * y;
    auto expr3 = x + y * 2_c;

    static_assert(match(expr1, AnyArg{} + AnyArg{}));
    static_assert(match(expr2, AnyArg{} * AnyArg{}));
  };

  "Evaluation"_test = [] {
    Symbol x;
    Symbol y;

    auto expr = x + y;
    constexpr auto result = evaluate(expr, BinderPack{x = 5, y = 3});
    static_assert(result == 8);

    auto expr2 = x * x + 2_c * x + 1_c;
    constexpr auto result2 = evaluate(expr2, BinderPack{x = 5});
    static_assert(result2 == 36);  // 25 + 10 + 1
  };

  "Pattern matching"_test = [] {
    Symbol x;

    auto expr1 = x + 1_c;
    static_assert(match(expr1, AnyArg{} + AnyArg{}));
    static_assert(match(expr1, AnySymbol{} + AnyConstant{}));
    static_assert(!match(expr1, AnyArg{} * AnyArg{}));

    auto expr2 = sin(x);
    static_assert(match(expr2, sin(AnyArg{})));
    static_assert(!match(expr2, cos(AnyArg{})));
  };

  "Constant evaluation"_test = [] {
    auto expr = 2_c + 3_c;
    constexpr auto result = evaluate(expr, BinderPack{});
    static_assert(result == 5);

    auto expr2 = 10_c * 5_c;
    constexpr auto result2 = evaluate(expr2, BinderPack{});
    static_assert(result2 == 50);
  };

  "Mathematical functions"_test = [] {
    Symbol x;

    auto expr1 = sin(x);
    auto expr2 = cos(x);
    auto expr3 = log(x);
    auto expr4 = exp(x);
    auto expr5 = pow(x, 2_c);

    static_assert(match(expr1, sin(AnyArg{})));
    static_assert(match(expr2, cos(AnyArg{})));
    static_assert(match(expr3, log(AnyArg{})));
    static_assert(match(expr4, exp(AnyArg{})));
    static_assert(match(expr5, pow(AnyArg{}, AnyArg{})));
  };

  "Special constants"_test = [] {
    Symbol x;

    auto expr1 = π * x;
    auto expr2 = e + x;

    static_assert(match(expr1, AnyExpr{} * AnyArg{}));
    static_assert(match(expr2, AnyExpr{} + AnyArg{}));
  };

  return TestRegistry::result();
}
