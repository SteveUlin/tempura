#include "symbolic5/eval.h"
#include "symbolic5/math.h"
#include "unit.h"

#include <cmath>

using namespace tempura;

int main() {
  // ─── Construction ──────────────────────────────────────────────────────────

  "construct symbol"_test = [] {
    constexpr auto x = sym<"x">();
    // Symbol<λ> with .meta carrying the display name
    expectTrue(true);
  };

  "construct constant"_test = [] {
    constexpr auto c = lit<42.0>;
    static_assert(Constant<42.0>::value == 42.0);
    expectTrue(true);
  };

  "construct binary expression"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = x + y;
    // f is an Expr — just verify it compiled and has a tree
    static_assert(std::is_same_v<decltype(f), const Expr>);
    expectTrue(true);
  };

  "construct nested expression"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * x + sin(x);
    static_assert(std::is_same_v<decltype(f), const Expr>);
    expectTrue(true);
  };

  "construct nullary (pi)"_test = [] {
    constexpr auto f = pi();
    static_assert(std::is_same_v<decltype(f), const Expr>);
    expectTrue(true);
  };

  "eval pi"_test = [] {
    constexpr auto f = pi();
    auto result = eval<f>();
    expectEq(result, M_PI);
  };

  // ─── BinderPack ────────────────────────────────────────────────────────────

  "binder pack lookup"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    auto ctx = BinderPack{x = 3.0, y = 5.0};
    expectEq(ctx[x], 3.0);
    expectEq(ctx[y], 5.0);
  };

  // ─── Evaluation ────────────────────────────────────────────────────────────

  "eval constant"_test = [] {
    constexpr Expr f{.tree = ^^Constant<42.0>};
    auto result = eval<f>();
    expectEq(result, 42.0);
  };

  "eval symbol"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr Expr f{.tree = toInfo(x)};
    auto result = eval<f>(x = 7.0);
    expectEq(result, 7.0);
  };

  "eval x + y"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = x + y;
    auto result = eval<f>(x = 3.0, y = 4.0);
    expectEq(result, 7.0);
  };

  "eval x * x + x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * x + x;
    auto result = eval<f>(x = 3.0);
    expectEq(result, 12.0);  // 9 + 3
  };

  "eval (x + y) * (x - y)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = (x + y) * (x - y);
    auto result = eval<f>(x = 5.0, y = 3.0);
    expectEq(result, 16.0);  // (8) * (2)
  };

  "eval sin"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = sin(x);
    auto result = eval<f>(x = 0.0);
    expectEq(result, 0.0);
  };

  "eval exp"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = exp(x);
    auto result = eval<f>(x = 0.0);
    expectEq(result, 1.0);
  };

  "eval log"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = log(x);
    auto result = eval<f>(x = 1.0);
    expectEq(result, 0.0);
  };

  "eval unary negation"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = -x;
    auto result = eval<f>(x = 5.0);
    expectEq(result, -5.0);
  };

  "eval x / y"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = x / y;
    auto result = eval<f>(x = 10.0, y = 4.0);
    expectEq(result, 2.5);
  };

  "eval tan"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = tan(x);
    auto result = eval<f>(x = 0.0);
    expectEq(result, 0.0);
  };

  "eval sqrt"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = sqrt(x);
    auto result = eval<f>(x = 9.0);
    expectEq(result, 3.0);
  };

  "eval abs"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = abs(x);
    expectEq(eval<f>(x = -5.0), 5.0);
    expectEq(eval<f>(x = 3.0), 3.0);
  };

  "eval pow"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = pow(x, y);
    auto result = eval<f>(x = 2.0, y = 3.0);
    expectEq(result, 8.0);
  };

  "eval pow fractional exponent"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = pow(x, lit<0.5>);
    auto result = eval<f>(x = 9.0);
    expectEq(result, 3.0);
  };

  "eval compound: sin(x) * exp(y) + log(x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = sin(x) * exp(y) + log(x);
    auto result = eval<f>(x = 1.0, y = 0.0);
    // sin(1)*exp(0) + log(1) = sin(1)*1 + 0 = sin(1) ≈ 0.8415
    expectNear(result, std::sin(1.0), 1e-10);
  };

  // ─── Type deduction ─────────────────────────────────────────────────────

  "eval preserves integer type"_test = [] {
    constexpr Expr f{.tree = ^^Constant<42>};
    auto result = eval<f>();
    static_assert(std::is_same_v<decltype(result), int>);
    expectEq(result, 42);
  };

  "eval deduces type from bindings"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr Expr f{.tree = toInfo(x)};
    auto result = eval<f>(x = 3.14f);
    static_assert(std::is_same_v<decltype(result), float>);
    expectNear(result, 3.14f, 1e-5f);
  };

  "eval int + int stays int"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x + lit<1>;
    auto result = eval<f>(x = 5);
    static_assert(std::is_same_v<decltype(result), int>);
    expectEq(result, 6);
  };

  return TestRegistry::result();
}
