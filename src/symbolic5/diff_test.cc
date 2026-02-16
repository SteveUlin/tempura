#include "symbolic5/diff.h"
#include "symbolic5/simplify.h"
#include "symbolic5/eval.h"
#include "unit.h"

#include <cmath>

using namespace tempura;

int main() {
  // ─── Differentiation ──────────────────────────────────────────────────────

  "diff constant → 0"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = Expr{.tree = ^^Constant<5.0>};
    constexpr auto df = simplify(diff(f, x));
    auto result = eval<df>(x = 99.0);
    expectEq(result, 0.0);
  };

  "diff x wrt x → 1"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr Expr f{.tree = toInfo(x)};
    constexpr auto df = simplify(diff(f, x));
    auto result = eval<df>(x = 99.0);
    expectEq(result, 1.0);
  };

  "diff x wrt y → 0"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr Expr f{.tree = toInfo(x)};
    constexpr auto df = simplify(diff(f, y));
    auto result = eval<df>(x = 1.0, y = 2.0);
    expectEq(result, 0.0);
  };

  "diff x*x → 2x (sum rule + product rule)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * x;
    constexpr auto df = simplify(diff(f, x));
    // d(x*x)/dx = x*1 + 1*x → x + x (after simplification)
    auto result = eval<df>(x = 3.0);
    expectEq(result, 6.0);
  };

  "diff x + x → 2 (constant)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x + x;
    constexpr auto df = simplify(diff(f, x));
    // d(x+x)/dx = 1 + 1, simplify doesn't merge constants, so eval gives 2
    auto result = eval<df>(x = 99.0);
    expectEq(result, 2.0);
  };

  "diff sin(x) → cos(x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = sin(x);
    constexpr auto df = simplify(diff(f, x));
    // d(sin x)/dx = cos(x) · 1 → cos(x)
    auto result = eval<df>(x = 0.0);
    expectEq(result, 1.0);  // cos(0) = 1
  };

  "diff exp(x) → exp(x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = exp(x);
    constexpr auto df = simplify(diff(f, x));
    auto result = eval<df>(x = 0.0);
    expectEq(result, 1.0);  // exp(0) = 1
  };

  "diff log(x) → 1/x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = log(x);
    constexpr auto df = simplify(diff(f, x));
    auto result = eval<df>(x = 2.0);
    expectEq(result, 0.5);  // 1/2
  };

  "diff chain rule: sin(x*x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = sin(x * x);
    constexpr auto df = simplify(diff(f, x));
    // d(sin(x²))/dx = cos(x²) · 2x
    // At x=1: cos(1) · 2 ≈ 1.0806
    auto result = eval<df>(x = 1.0);
    expectNear(result, std::cos(1.0) * 2.0, 1e-10);
  };

  "diff product rule: x * sin(x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * sin(x);
    constexpr auto df = simplify(diff(f, x));
    // d(x·sin(x))/dx = x·cos(x) + sin(x)
    // At x=π/2: (π/2)·cos(π/2) + sin(π/2) = 0 + 1 = 1
    auto result = eval<df>(x = M_PI / 2.0);
    expectNear(result, 1.0, 1e-10);
  };

  "diff quotient rule: x / (x + lit<1.0>)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x / (x + lit<1.0>);
    constexpr auto df = simplify(diff(f, x));
    // d(x/(x+1))/dx = ((1)(x+1) - x(1)) / (x+1)² = 1/(x+1)²
    // At x=1: 1/4 = 0.25
    auto result = eval<df>(x = 1.0);
    expectNear(result, 0.25, 1e-10);
  };

  "diff second derivative: x*x*x → 6x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * x * x;
    constexpr auto df = simplify(diff(f, x));
    constexpr auto ddf = simplify(diff(df, x));
    // d³/dx = 3x², d²/dx² = 6x
    // At x=2: d²f/dx² = 12
    auto result = eval<ddf>(x = 2.0);
    expectNear(result, 12.0, 1e-10);
  };

  // ─── Tan / Sqrt / Abs ──────────────────────────────────────────────────────

  "diff tan(x) → 1/cos²(x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = tan(x);
    constexpr auto df = simplify(diff(f, x));
    // d/dx[tan(x)] = sec²(x) = 1/cos²(x) — at x=0: 1/1 = 1
    auto result = eval<df>(x = 0.0);
    expectNear(result, 1.0, 1e-10);
  };

  "diff sqrt(x) → 1/(2√x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = sqrt(x);
    constexpr auto df = simplify(diff(f, x));
    // d/dx[√x] = 1/(2√x) — at x=4: 1/4 = 0.25
    auto result = eval<df>(x = 4.0);
    expectNear(result, 0.25, 1e-10);
  };

  "diff abs(x) → x/|x| (sign function)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = abs(x);
    constexpr auto df = simplify(diff(f, x));
    // d/dx[|x|] = x/|x| = sign(x)
    expectNear(eval<df>(x = 5.0), 1.0, 1e-10);
    expectNear(eval<df>(x = -3.0), -1.0, 1e-10);
  };

  "diff sqrt(x*x + 1)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = sqrt(x * x + lit<1.0>);
    constexpr auto df = simplify(diff(f, x));
    // d/dx[√(x²+1)] = x/√(x²+1) — at x=0: 0
    auto result = eval<df>(x = 0.0);
    expectNear(result, 0.0, 1e-10);
  };

  // ─── Power rule ────────────────────────────────────────────────────────────

  "diff pow(x, 3) → 3x² (power rule)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = pow(x, lit<3.0>);
    constexpr auto df = simplify(diff(f, x));
    // d(x³)/dx = 3x² — at x=2: 12
    auto result = eval<df>(x = 2.0);
    expectNear(result, 12.0, 1e-10);
  };

  "diff pow(2, x) → 2^x · ln(2) (exponential rule)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = pow(lit<2.0>, x);
    constexpr auto df = simplify(diff(f, x));
    // d(2^x)/dx = 2^x · ln(2) — at x=0: ln(2)
    auto result = eval<df>(x = 0.0);
    expectNear(result, std::log(2.0), 1e-10);
  };

  "diff pow(x, x) (general case)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = pow(x, x);
    constexpr auto df = simplify(diff(f, x));
    // d(x^x)/dx = x^x · (log(x) + 1) — at x=2: 4·(log(2)+1) ≈ 6.7726
    auto result = eval<df>(x = 2.0);
    expectNear(result, 4.0 * (std::log(2.0) + 1.0), 1e-10);
  };

  // ─── Simplification ───────────────────────────────────────────────────────

  "simplify 0 + x → x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<0.0> + x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 42.0);
    expectEq(result, 42.0);
  };

  "simplify x * 1 → x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * lit<1.0>;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 42.0);
    expectEq(result, 42.0);
  };

  "simplify 0 * x → 0"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<0.0> * x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 42.0);
    expectEq(result, 0.0);
  };

  return TestRegistry::result();
}
