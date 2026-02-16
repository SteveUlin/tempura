#include "symbolic5/diff.h"
#include "symbolic5/simplify.h"
#include "symbolic5/eval.h"
#include "unit.h"

#include <cmath>

using namespace tempura;

// ─── Consteval test helpers ──────────────────────────────────────────────────
//
// matchInfo/substituteInfo/tryRulesInfo are consteval — results must be
// consumed in consteval context. These wrappers produce bool/info results
// usable by static_assert.

// Pattern variable tags for test patterns
struct PVa {};
struct PVb {};

consteval bool matchSucceeds(std::meta::info pattern, std::meta::info expr) {
  return matchInfo(pattern, expr).success;
}

consteval bool matchFails(std::meta::info pattern, std::meta::info expr) {
  return !matchInfo(pattern, expr).success;
}

consteval std::size_t matchBindingCount(std::meta::info pattern, std::meta::info expr) {
  auto result = matchInfo(pattern, expr);
  return result.success ? result.bindings.size() : 0;
}

consteval bool matchBindsTo(
    std::meta::info pattern, std::meta::info expr,
    std::meta::info pv_key, std::meta::info expected) {
  auto result = matchInfo(pattern, expr);
  if (!result.success) return false;
  for (const auto& [k, v] : result.bindings) {
    if (k == pv_key) return v == expected;
  }
  return false;
}

consteval std::meta::info applyRule(
    std::meta::info pattern, std::meta::info replacement,
    std::meta::info expr) {
  auto result = matchInfo(pattern, expr);
  if (!result.success) return expr;
  return substituteInfo(replacement, result.bindings);
}

// ─── tryRulesInfo test helpers (consteval can't live inside lambdas) ──────────

consteval bool testTryRulesAddZero() {
  auto rules = makeSimplifyRules();
  auto x = sym<"x">();
  auto expr = (lit<0.0> + x).tree;
  return tryRulesInfo(rules, expr) == toInfo(x);
}

consteval bool testTryRulesMulZero() {
  auto rules = makeSimplifyRules();
  auto x = sym<"x">();
  auto expr = (lit<0.0> * x).tree;
  return tryRulesInfo(rules, expr) == ^^Constant<0.0>;
}

consteval bool testTryRulesNoMatch() {
  auto rules = makeSimplifyRules();
  auto x = sym<"x">();
  auto y = sym<"y">();
  auto expr = (x + y).tree;
  return tryRulesInfo(rules, expr) == expr;
}

// ─── innermostInfo test helpers ──────────────────────────────────────────────

consteval bool testInnermostNested() {
  // (0 + (x + 0)) → x
  auto rules = makeSimplifyRules();
  auto x = sym<"x">();
  auto inner = (x + lit<0.0>).tree;
  auto outer_args = std::vector<std::meta::info>{^^AddOp, ^^Constant<0.0>, inner};
  auto expr = std::meta::substitute(^^App, outer_args);
  return innermostInfo(expr, rules) == toInfo(x);
}

consteval bool testInnermostMultiStep() {
  // 0 * x + 1.0 * y → 0.0 + y → y
  auto rules = makeSimplifyRules();
  auto x = sym<"x">();
  auto y = sym<"y">();
  auto lhs = (lit<0.0> * x).tree;
  auto rhs = (lit<1.0> * y).tree;
  auto expr_args = std::vector<std::meta::info>{^^AddOp, lhs, rhs};
  auto expr = std::meta::substitute(^^App, expr_args);
  return innermostInfo(expr, rules) == toInfo(y);
}

int main() {
  // ═══════════════════════════════════════════════════════════════════════════
  // Pattern matching
  // ═══════════════════════════════════════════════════════════════════════════

  "matchInfo: PatternVar binds anything"_test = [] {
    static_assert(matchSucceeds(^^PatternVar<PVa>, ^^Constant<5.0>));
    static_assert(matchSucceeds(^^PatternVar<PVa>, ^^AddOp));
    static_assert(matchBindingCount(^^PatternVar<PVa>, ^^Constant<5.0>) == 1);
    static_assert(matchBindsTo(
        ^^PatternVar<PVa>, ^^Constant<5.0>,
        ^^PVa, ^^Constant<5.0>));
    expectTrue(true);
  };

  "matchInfo: exact match"_test = [] {
    static_assert(matchSucceeds(^^Constant<0.0>, ^^Constant<0.0>));
    static_assert(matchSucceeds(^^AddOp, ^^AddOp));
    expectTrue(true);
  };

  "matchInfo: mismatch"_test = [] {
    static_assert(matchFails(^^Constant<0.0>, ^^Constant<1.0>));
    static_assert(matchFails(^^AddOp, ^^MulOp));
    // Cross-template mismatch
    static_assert(matchFails(^^Constant<0.0>, ^^Constant<0>));
    expectTrue(true);
  };

  "matchInfo: non-linear x - x succeeds when equal"_test = [] {
    constexpr auto x = sym<"x">();
    static_assert(matchSucceeds(
        ^^App<SubOp, PatternVar<PVa>, PatternVar<PVa>>,
        (x - x).tree));
    // Only one binding despite two occurrences
    static_assert(matchBindingCount(
        ^^App<SubOp, PatternVar<PVa>, PatternVar<PVa>>,
        (x - x).tree) == 1);
    expectTrue(true);
  };

  "matchInfo: non-linear x - x fails when different"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    static_assert(matchFails(
        ^^App<SubOp, PatternVar<PVa>, PatternVar<PVa>>,
        (x - y).tree));
    expectTrue(true);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // Substitution
  // ═══════════════════════════════════════════════════════════════════════════

  "substituteInfo: PatternVar replacement"_test = [] {
    // x + 0 → x
    static_assert(applyRule(
        ^^App<AddOp, PatternVar<PVa>, Constant<0.0>>,
        ^^PatternVar<PVa>,
        ^^App<AddOp, Constant<5.0>, Constant<0.0>>)
        == ^^Constant<5.0>);
    expectTrue(true);
  };

  "substituteInfo: nested reconstruction"_test = [] {
    // Replacement builds a new App: x * 0 → 0 (just replaces entire node)
    static_assert(applyRule(
        ^^App<MulOp, PatternVar<PVa>, Constant<0.0>>,
        ^^Constant<0.0>,
        ^^App<MulOp, Constant<5.0>, Constant<0.0>>)
        == ^^Constant<0.0>);
    expectTrue(true);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // tryRulesInfo — individual rules
  // ═══════════════════════════════════════════════════════════════════════════

  "tryRulesInfo: 0 + x → x"_test = [] {
    static_assert(testTryRulesAddZero());
    expectTrue(true);
  };

  "tryRulesInfo: 0 * x → 0"_test = [] {
    static_assert(testTryRulesMulZero());
    expectTrue(true);
  };

  "tryRulesInfo: no match returns unchanged"_test = [] {
    static_assert(testTryRulesNoMatch());
    expectTrue(true);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // End-to-end simplification
  // ═══════════════════════════════════════════════════════════════════════════

  "simplify: nested 0 + (x + 0) → x"_test = [] {
    static_assert(testInnermostNested());
    expectTrue(true);
  };

  "simplify: multi-step 0*x + 1*y → y"_test = [] {
    static_assert(testInnermostMultiStep());
    expectTrue(true);
  };

  "simplify: 0 + x → x (eval check)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<0.0> + x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 42.0);
    expectEq(result, 42.0);
  };

  "simplify: x * 1 → x (eval check)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * lit<1.0>;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 42.0);
    expectEq(result, 42.0);
  };

  "simplify: 0 * x → 0 (eval check)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<0.0> * x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 42.0);
    expectEq(result, 0.0);
  };

  "simplify: -(-x) → x (eval check)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = -(-x);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 7.0);
    expectEq(result, 7.0);
  };

  "simplify: x - x → 0 (eval check)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x - x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 99.0);
    expectEq(result, 0.0);
  };

  "simplify: derivative cleanup d(x*x)/dx"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * x;
    constexpr auto df = simplify(diff(f, x));
    // d(x²)/dx = x + x after simplification
    auto result = eval<df>(x = 3.0);
    expectEq(result, 6.0);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // New simplification rules
  // ═══════════════════════════════════════════════════════════════════════════

  "simplify: exp(log(x)) → x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = exp(log(x));
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 7.0);
    expectEq(result, 7.0);
  };

  "simplify: log(exp(x)) → x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = log(exp(x));
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 3.0);
    expectEq(result, 3.0);
  };

  "simplify: x / x → 1"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x / x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 42.0);
    expectEq(result, 1.0);
  };

  "simplify: sin(0) → 0"_test = [] {
    constexpr auto f = sin(lit<0.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 0.0);
  };

  "simplify: cos(0) → 1"_test = [] {
    constexpr auto f = cos(lit<0.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 1.0);
  };

  "simplify: sin(π) → 0"_test = [] {
    constexpr auto f = sin(pi());
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 0.0);
  };

  "simplify: cos(π) → -1"_test = [] {
    constexpr auto f = cos(pi());
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, -1.0);
  };

  "simplify: exp(0) → 1"_test = [] {
    constexpr auto f = exp(lit<0.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 1.0);
  };

  "simplify: log(1) → 0"_test = [] {
    constexpr auto f = log(lit<1.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 0.0);
  };

  "simplify: log(e) → 1"_test = [] {
    constexpr auto f = log(euler());
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 1.0);
  };

  "simplify: pow(x, 0) → 1"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = pow(x, lit<0.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 42.0);
    expectEq(result, 1.0);
  };

  "simplify: pow(x, 1) → x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = pow(x, lit<1.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 7.0);
    expectEq(result, 7.0);
  };

  "simplify: cascading exp(log(x)) * 1 → x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = exp(log(x)) * lit<1.0>;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 5.0);
    expectEq(result, 5.0);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // Like-term collection + new op rules
  // ═══════════════════════════════════════════════════════════════════════════

  "simplify: x + x → 2*x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x + x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 7.0);
    expectEq(result, 14.0);
  };

  "simplify: x + 2*x → 3*x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x + lit<2.0> * x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 5.0);
    expectEq(result, 15.0);
  };

  "simplify: 2*x + x → 3*x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<2.0> * x + x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 5.0);
    expectEq(result, 15.0);
  };

  "simplify: 2*x + 3*x → 5*x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<2.0> * x + lit<3.0> * x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 4.0);
    expectEq(result, 20.0);
  };

  "simplify: x + x + x → 3*x (cascading)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x + x + x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 10.0);
    expectEq(result, 30.0);
  };

  "simplify: (x*y) + (x*y) → 2*(x*y)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = (x * y) + (x * y);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 3.0, y = 5.0);
    expectEq(result, 30.0);
  };

  "simplify: tan(0) → 0"_test = [] {
    constexpr auto f = tan(lit<0.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 0.0);
  };

  "simplify: sqrt(0) → 0"_test = [] {
    constexpr auto f = sqrt(lit<0.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 0.0);
  };

  "simplify: sqrt(1) → 1"_test = [] {
    constexpr auto f = sqrt(lit<1.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 1.0);
  };

  "simplify: abs(0) → 0"_test = [] {
    constexpr auto f = abs(lit<0.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 0.0);
  };

  "simplify: abs(abs(x)) → abs(x)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = abs(abs(x));
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = -3.0);
    expectEq(result, 3.0);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // Constant folding
  // ═══════════════════════════════════════════════════════════════════════════

  "fold: 2 + 3 → 5"_test = [] {
    constexpr auto f = lit<2.0> + lit<3.0>;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 5.0);
  };

  "fold: 10 - 4 → 6"_test = [] {
    constexpr auto f = lit<10.0> - lit<4.0>;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 6.0);
  };

  "fold: 3 * 7 → 21"_test = [] {
    constexpr auto f = lit<3.0> * lit<7.0>;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 21.0);
  };

  "fold: 10 / 4 → 2.5"_test = [] {
    constexpr auto f = lit<10.0> / lit<4.0>;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 2.5);
  };

  "fold: -5 → -5"_test = [] {
    constexpr auto f = -lit<5.0>;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, -5.0);
  };

  "fold: nested (2 + 3) * (4 - 1) → 15"_test = [] {
    constexpr auto f = (lit<2.0> + lit<3.0>) * (lit<4.0> - lit<1.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>();
    expectEq(result, 15.0);
  };

  "fold: mixed x + (2 + 3) → x + 5"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x + (lit<2.0> + lit<3.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 10.0);
    expectEq(result, 15.0);
  };

  "fold: cascading x + (3 - 3) → x (fold then re-simplify)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x + (lit<3.0> - lit<3.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 42.0);
    expectEq(result, 42.0);
  };

  "fold: derivative cleanup d(3x)/dx → 3"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<3.0> * x;
    constexpr auto df = simplify(diff(f, x));
    // d(3x)/dx = 3·1 + 0·x → 3 + 0 → 3
    auto result = eval<df>(x = 99.0);
    expectEq(result, 3.0);
  };

  "fold: derivative d(2x + 3x)/dx → 5"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<2.0> * x + lit<3.0> * x;
    constexpr auto df = simplify(diff(f, x));
    // d(2x+3x)/dx = 2+3 → 5
    auto result = eval<df>(x = 99.0);
    expectEq(result, 5.0);
  };

  "simplify: idempotent (applying twice gives same result)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<0.0> + (x * lit<1.0>);
    constexpr auto sf1 = simplify(f);
    constexpr auto sf2 = simplify(sf1);
    auto r1 = eval<sf1>(x = 5.0);
    auto r2 = eval<sf2>(x = 5.0);
    expectEq(r1, r2);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // Sum canonicalization
  // ═══════════════════════════════════════════════════════════════════════════

  "canon sum: x + 2*y + 3*x + 4*y → 4*x + 6*y"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = x + lit<2.0> * y + lit<3.0> * x + lit<4.0> * y;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 1.0, y = 1.0);
    expectEq(result, 10.0);
  };

  "canon sum: 1 + x + 2 → x + 3"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<1.0> + x + lit<2.0>;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 10.0);
    expectEq(result, 13.0);
  };

  "canon sum: x + y - x → y"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = x + y - x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 99.0, y = 7.0);
    expectEq(result, 7.0);
  };

  "canon sum: 2*x - 3*x → -x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = lit<2.0> * x - lit<3.0> * x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 5.0);
    expectEq(result, -5.0);
  };

  "canon sum: (a + b) - (a - b) → 2*b"_test = [] {
    constexpr auto a = sym<"a">();
    constexpr auto b = sym<"b">();
    constexpr auto f = (a + b) - (a - b);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(a = 10.0, b = 3.0);
    expectEq(result, 6.0);
  };

  "canon sum: x - x + y - y → 0"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = x - x + y - y;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 42.0, y = 17.0);
    expectEq(result, 0.0);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // Product canonicalization
  // ═══════════════════════════════════════════════════════════════════════════

  "canon product: x * x → pow(x, 2)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 3.0);
    expectEq(result, 9.0);
  };

  "canon product: pow(x, 2) * pow(x, 3) → pow(x, 5)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = pow(x, lit<2.0>) * pow(x, lit<3.0>);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 2.0);
    expectEq(result, 32.0);
  };

  "canon product: 2 * x * 3 * y → 6 * x * y"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = lit<2.0> * x * lit<3.0> * y;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 1.0, y = 5.0);
    expectEq(result, 30.0);
  };

  "canon product: y * x commutes to same as x * y"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f1 = y * x;
    constexpr auto f2 = x * y;
    constexpr auto sf1 = simplify(f1);
    constexpr auto sf2 = simplify(f2);
    auto r1 = eval<sf1>(x = 3.0, y = 7.0);
    auto r2 = eval<sf2>(x = 3.0, y = 7.0);
    expectEq(r1, r2);
  };

  "canon product: sqrt(x) * sqrt(x) → x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = sqrt(x) * sqrt(x);
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 5.0);
    expectEq(result, 5.0);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // Integration (product + sum interaction)
  // ═══════════════════════════════════════════════════════════════════════════

  "canon integration: x*x + x*x → 2*pow(x, 2)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto f = x * x + x * x;
    constexpr auto sf = simplify(f);
    auto result = eval<sf>(x = 3.0);
    expectEq(result, 18.0);
  };

  "canon integration: d(x*x + y*y)/dx → 2*x"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = x * x + y * y;
    constexpr auto df = simplify(diff(f, x));
    auto result = eval<df>(x = 3.0, y = 5.0);
    expectEq(result, 6.0);
  };

  "canon: idempotent (simplify twice gives same result)"_test = [] {
    constexpr auto x = sym<"x">();
    constexpr auto y = sym<"y">();
    constexpr auto f = x + lit<2.0> * y + lit<3.0> * x + lit<4.0> * y;
    constexpr auto sf1 = simplify(f);
    constexpr auto sf2 = simplify(sf1);
    auto r1 = eval<sf1>(x = 2.0, y = 3.0);
    auto r2 = eval<sf2>(x = 2.0, y = 3.0);
    expectEq(r1, r2);
  };

  return TestRegistry::result();
}
