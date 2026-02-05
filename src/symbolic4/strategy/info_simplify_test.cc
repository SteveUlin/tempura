#include <experimental/meta>

#include "symbolic4/constants.h"
#include "symbolic4/core.h"
#include "symbolic4/operators.h"
#include "symbolic4/strategy/info_simplify.h"
#include "symbolic4/strategy/pattern.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// ============================================================================
// Phase 3a: NTTP feasibility gate
// Phase 3b: Info-domain matching and substitution
// ============================================================================

// --- Phase 3a helpers ---

consteval auto roundtrips(std::meta::info type) -> bool {
  auto tmpl = std::meta::template_of(type);
  auto args = std::meta::template_arguments_of(type);
  return std::meta::substitute(tmpl, args) == type;
}

consteval auto roundtripsNormalized(std::meta::info type) -> bool {
  auto normalized = std::meta::remove_cvref(type);
  auto tmpl = std::meta::template_of(normalized);
  auto args = std::meta::template_arguments_of(normalized);
  return std::meta::substitute(tmpl, args) == normalized;
}

consteval auto argCount(std::meta::info type) -> std::size_t {
  return std::meta::template_arguments_of(type).size();
}

consteval auto nthArg(std::meta::info type, std::size_t n) -> std::meta::info {
  return std::meta::template_arguments_of(type)[n];
}

consteval auto hasTemplate(std::meta::info type, std::meta::info tmpl) -> bool {
  return std::meta::template_of(type) == tmpl;
}

consteval auto buildFrom(std::meta::info tmpl, std::vector<std::meta::info> args)
    -> std::meta::info {
  return std::meta::substitute(tmpl, args);
}

consteval auto replaceArg(std::meta::info type, std::size_t n, std::meta::info replacement)
    -> std::meta::info {
  auto tmpl = std::meta::template_of(type);
  auto args = std::meta::template_arguments_of(type);
  args[n] = replacement;
  return std::meta::substitute(tmpl, args);
}

// --- Phase 3b helpers ---

// Wrappers for matchInfo/substituteInfo/tryRulesInfo that return consteval-friendly
// results consumable by static_assert.

consteval auto matchSucceeds(std::meta::info pattern, std::meta::info expr) -> bool {
  return matchInfo(pattern, expr).success;
}

consteval auto matchFails(std::meta::info pattern, std::meta::info expr) -> bool {
  return !matchInfo(pattern, expr).success;
}

consteval auto matchBindingCount(std::meta::info pattern, std::meta::info expr) -> std::size_t {
  auto result = matchInfo(pattern, expr);
  return result.success ? result.bindings.size() : 0;
}

// Match, then check that a specific PatternVar was bound to the expected info.
consteval auto matchBindsTo(
    std::meta::info pattern, std::meta::info expr,
    std::meta::info pv_key, std::meta::info expected) -> bool {
  auto result = matchInfo(pattern, expr);
  if (!result.success) return false;
  for (const auto& [k, v] : result.bindings) {
    if (k == pv_key) return v == expected;
  }
  return false;
}

// Match and substitute: apply pattern→replacement on expr, return result.
consteval auto applyRule(
    std::meta::info pattern, std::meta::info replacement,
    std::meta::info expr) -> std::meta::info {
  auto result = matchInfo(pattern, expr);
  if (!result.success) return expr;
  return substituteInfo(replacement, result.bindings);
}

// Namespace-scope types for stable reflection.
struct TagX {};
struct TagY {};
struct TagA {};
struct TagB {};
struct PVTag {};
struct PVx {};
struct PVy {};
struct PVz {};

// --- tryRulesInfo test helpers (consteval functions can't live inside lambdas) ---

consteval auto makeBasicRules() -> std::vector<InfoRule> {
  return {
    {^^Expression<AddOp, PatternVar<PVx>, Constant<0.0>>, ^^PatternVar<PVx>},
    {^^Expression<MulOp, PatternVar<PVx>, Constant<0.0>>, ^^Constant<0.0>}
  };
}

consteval auto testTryRulesAddZero() -> bool {
  auto rules = makeBasicRules();
  return tryRulesInfo(rules,
      ^^Expression<AddOp, Atom<TagA, Free>, Constant<0.0>>)
      == ^^Atom<TagA, Free>;
}

consteval auto testTryRulesMulZero() -> bool {
  auto rules = makeBasicRules();
  return tryRulesInfo(rules,
      ^^Expression<MulOp, Atom<TagA, Free>, Constant<0.0>>)
      == ^^Constant<0.0>;
}

consteval auto testTryRulesNoMatch() -> bool {
  auto rules = makeBasicRules();
  return tryRulesInfo(rules,
      ^^Expression<SubOp, Atom<TagA, Free>, Atom<TagB, Free>>)
      == ^^Expression<SubOp, Atom<TagA, Free>, Atom<TagB, Free>>;
}

// --- innermostInfo test helpers ---

consteval auto makeSimplifyRules() -> std::vector<InfoRule> {
  return {
    // x + 0 → x
    {^^Expression<AddOp, PatternVar<PVx>, Constant<0.0>>, ^^PatternVar<PVx>},
    // 0 + x → x
    {^^Expression<AddOp, Constant<0.0>, PatternVar<PVx>>, ^^PatternVar<PVx>},
    // x * 1 → x
    {^^Expression<MulOp, PatternVar<PVx>, Constant<1.0>>, ^^PatternVar<PVx>},
    // 1 * x → x
    {^^Expression<MulOp, Constant<1.0>, PatternVar<PVx>>, ^^PatternVar<PVx>},
    // x * 0 → 0
    {^^Expression<MulOp, PatternVar<PVx>, Constant<0.0>>, ^^Constant<0.0>},
    // 0 * x → 0
    {^^Expression<MulOp, Constant<0.0>, PatternVar<PVx>>, ^^Constant<0.0>},
    // x - x → 0
    {^^Expression<SubOp, PatternVar<PVx>, PatternVar<PVx>>, ^^Constant<0.0>},
    // x + x → 2 * x
    {^^Expression<AddOp, PatternVar<PVx>, PatternVar<PVx>>,
     ^^Expression<MulOp, Constant<2.0>, PatternVar<PVx>>},
  };
}

// Single-step simplification: (a + 0) → a
consteval auto testInnermostSimple() -> bool {
  auto rules = makeSimplifyRules();
  return innermostInfo(
      ^^Expression<AddOp, Atom<TagA, Free>, Constant<0.0>>, rules)
      == ^^Atom<TagA, Free>;
}

// Nested: (a + 0) + (b * 1) → a + b  (children simplified first)
consteval auto testInnermostNested() -> bool {
  auto rules = makeSimplifyRules();
  // Expression: (a + 0) + (b * 1)
  auto expr = ^^Expression<AddOp,
      Expression<AddOp, Atom<TagA, Free>, Constant<0.0>>,
      Expression<MulOp, Atom<TagB, Free>, Constant<1.0>>>;
  auto result = innermostInfo(expr, rules);
  // After simplification: a + b
  return result == ^^Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>;
}

// Multi-step fixpoint: (a * 1) + 0 → a + 0 → a (two steps)
consteval auto testInnermostFixpoint() -> bool {
  auto rules = makeSimplifyRules();
  // (a * 1) + 0: first child simplifies to a, then a + 0 simplifies to a
  auto expr = ^^Expression<AddOp,
      Expression<MulOp, Atom<TagA, Free>, Constant<1.0>>,
      Constant<0.0>>;
  return innermostInfo(expr, rules) == ^^Atom<TagA, Free>;
}

// Deep nesting: ((a + 0) * 1) + 0 → a
consteval auto testInnermostDeep() -> bool {
  auto rules = makeSimplifyRules();
  auto expr = ^^Expression<AddOp,
      Expression<MulOp,
          Expression<AddOp, Atom<TagA, Free>, Constant<0.0>>,
          Constant<1.0>>,
      Constant<0.0>>;
  return innermostInfo(expr, rules) == ^^Atom<TagA, Free>;
}

// Non-linear: a - a → 0
consteval auto testInnermostNonLinear() -> bool {
  auto rules = makeSimplifyRules();
  return innermostInfo(
      ^^Expression<SubOp, Atom<TagA, Free>, Atom<TagA, Free>>, rules)
      == ^^Constant<0.0>;
}

// No rules apply: a + b stays unchanged
consteval auto testInnermostNoChange() -> bool {
  auto rules = makeSimplifyRules();
  return innermostInfo(
      ^^Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>, rules)
      == ^^Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>;
}

// Cascading: (a - a) + b → 0 + b → b
consteval auto testInnermostCascade() -> bool {
  auto rules = makeSimplifyRules();
  auto expr = ^^Expression<AddOp,
      Expression<SubOp, Atom<TagA, Free>, Atom<TagA, Free>>,
      Atom<TagB, Free>>;
  return innermostInfo(expr, rules) == ^^Atom<TagB, Free>;
}

// --- Phase 3d: full simplification rule set tests ---

// Helper: simplify using the full rule set and compare to expected
consteval auto infoSimplifies(std::meta::info expr, std::meta::info expected) -> bool {
  return infoSimplify(expr) == expected;
}

// Helper: build Expression infos for concise tests
consteval auto binExpr(std::meta::info op, std::meta::info l, std::meta::info r) -> std::meta::info {
  return std::meta::substitute(^^Expression, {op, l, r});
}
consteval auto unExpr(std::meta::info op, std::meta::info arg) -> std::meta::info {
  return std::meta::substitute(^^Expression, {op, arg});
}

// Complex multi-rule test: (a * 1 + 0) / exp(log(b)) → a / b
consteval auto testComplexMultiRule() -> bool {
  auto a = ^^Atom<TagA, Free>;
  auto b = ^^Atom<TagB, Free>;
  auto div = ^^::tempura::DivOp;
  auto add = ^^::tempura::AddOp;
  auto mul = ^^::tempura::MulOp;
  auto exp_ = ^^::tempura::ExpOp;
  auto log_ = ^^::tempura::LogOp;
  auto expr = binExpr(div,
      binExpr(add,
          binExpr(mul, a, ^^Constant<1.0>),
          ^^Constant<0.0>),
      unExpr(exp_, unExpr(log_, b)));
  return infoSimplify(expr) == binExpr(div, a, b);
}

auto main() -> int {
  // ==========================================================================
  // PHASE 3a: NTTP ROUNDTRIPPING
  // ==========================================================================

  "Constant roundtrip via substitute"_test = [] {
    static_assert(roundtrips(^^Constant<5.0>));
  };

  "Constant template_of identifies template"_test = [] {
    static_assert(std::meta::template_of(^^Constant<42.0>) == ^^Constant);
  };

  "Constant args has exactly one element"_test = [] {
    static_assert(argCount(^^Constant<7.0>) == 1);
  };

  "Different Constants produce different infos"_test = [] {
    static_assert(^^Constant<1.0> != ^^Constant<2.0>);
    static_assert(std::meta::template_of(^^Constant<1.0>) ==
                  std::meta::template_of(^^Constant<2.0>));
  };

  "Fraction roundtrip via substitute"_test = [] {
    static_assert(roundtrips(^^Fraction<1, 2>));
  };

  "Fraction args has two elements"_test = [] {
    static_assert(argCount(^^Fraction<3, 7>) == 2);
  };

  "Atom roundtrip via substitute"_test = [] {
    static_assert(roundtrips(^^Atom<TagX, Free>));
  };

  "Expression roundtrip - direct syntax"_test = [] {
    static_assert(roundtrips(^^Expression<AddOp, Atom<TagX, Free>, Constant<1.0>>));
  };

  "Expression roundtrip - via alias (normalized)"_test = [] {
    using E = Expression<AddOp, Atom<TagX, Free>, Constant<1.0>>;
    static_assert(roundtripsNormalized(^^E));
  };

  "Nested Expression roundtrip"_test = [] {
    static_assert(roundtrips(
        ^^Expression<MulOp,
            Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>,
            Constant<2.0>>));
  };

  "Substitute can build Expression from reflected parts"_test = [] {
    static_assert(
        buildFrom(^^Expression, {^^MulOp, ^^Constant<3.0>, ^^Constant<4.0>}) ==
        ^^Expression<MulOp, Constant<3.0>, Constant<4.0>>);
  };

  "Substitute can swap operator in Expression"_test = [] {
    static_assert(
        replaceArg(^^Expression<AddOp, Atom<TagX, Free>, Atom<TagY, Free>>, 0, ^^MulOp) ==
        ^^Expression<MulOp, Atom<TagX, Free>, Atom<TagY, Free>>);
  };

  "PatternVar identifiable via template_of"_test = [] {
    static_assert(std::meta::has_template_arguments(^^PatternVar<PVTag>));
    static_assert(hasTemplate(^^PatternVar<PVTag>, ^^PatternVar));
  };

  "PatternVar roundtrip"_test = [] {
    static_assert(roundtrips(^^PatternVar<PVTag>));
  };

  // ==========================================================================
  // PHASE 3b: INFO-DOMAIN MATCHING
  // ==========================================================================

  // --- Exact matches (no PatternVars) ---

  "matchInfo: Constant matches itself"_test = [] {
    static_assert(matchSucceeds(^^Constant<0.0>, ^^Constant<0.0>));
  };

  "matchInfo: Constant rejects different value"_test = [] {
    static_assert(matchFails(^^Constant<0.0>, ^^Constant<1.0>));
  };

  "matchInfo: Fraction matches itself"_test = [] {
    static_assert(matchSucceeds(^^Fraction<1, 2>, ^^Fraction<1, 2>));
  };

  "matchInfo: Fraction rejects different fraction"_test = [] {
    static_assert(matchFails(^^Fraction<1, 2>, ^^Fraction<1, 3>));
  };

  "matchInfo: Atom matches itself"_test = [] {
    static_assert(matchSucceeds(^^Atom<TagX, Free>, ^^Atom<TagX, Free>));
  };

  "matchInfo: Atom rejects different Id"_test = [] {
    static_assert(matchFails(^^Atom<TagX, Free>, ^^Atom<TagY, Free>));
  };

  "matchInfo: Expression matches same structure"_test = [] {
    static_assert(matchSucceeds(
        ^^Expression<AddOp, Atom<TagX, Free>, Constant<1.0>>,
        ^^Expression<AddOp, Atom<TagX, Free>, Constant<1.0>>));
  };

  "matchInfo: Expression rejects different operator"_test = [] {
    static_assert(matchFails(
        ^^Expression<AddOp, Atom<TagX, Free>, Constant<1.0>>,
        ^^Expression<MulOp, Atom<TagX, Free>, Constant<1.0>>));
  };

  "matchInfo: Expression rejects different child"_test = [] {
    static_assert(matchFails(
        ^^Expression<AddOp, Atom<TagX, Free>, Constant<1.0>>,
        ^^Expression<AddOp, Atom<TagX, Free>, Constant<2.0>>));
  };

  "matchInfo: cross-template mismatch"_test = [] {
    static_assert(matchFails(^^Constant<1.0>, ^^Atom<TagX, Free>));
    static_assert(matchFails(^^Atom<TagX, Free>, ^^Constant<1.0>));
  };

  // --- PatternVar binding ---

  "matchInfo: PatternVar matches Constant"_test = [] {
    static_assert(matchSucceeds(^^PatternVar<PVx>, ^^Constant<5.0>));
    static_assert(matchBindingCount(^^PatternVar<PVx>, ^^Constant<5.0>) == 1);
    static_assert(matchBindsTo(
        ^^PatternVar<PVx>, ^^Constant<5.0>,
        ^^PVx, ^^Constant<5.0>));
  };

  "matchInfo: PatternVar matches Atom"_test = [] {
    static_assert(matchBindsTo(
        ^^PatternVar<PVx>, ^^Atom<TagA, Free>,
        ^^PVx, ^^Atom<TagA, Free>));
  };

  "matchInfo: PatternVar matches Expression"_test = [] {
    static_assert(matchBindsTo(
        ^^PatternVar<PVx>, ^^Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>,
        ^^PVx, ^^Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>));
  };

  // --- Pattern expressions (PatternVar inside Expression) ---

  "matchInfo: x_ + 0 matches a + 0"_test = [] {
    // Pattern: Expression<AddOp, PatternVar<PVx>, Constant<0.0>>
    // Expr:    Expression<AddOp, Atom<TagA, Free>, Constant<0.0>>
    static_assert(matchSucceeds(
        ^^Expression<AddOp, PatternVar<PVx>, Constant<0.0>>,
        ^^Expression<AddOp, Atom<TagA, Free>, Constant<0.0>>));
    static_assert(matchBindsTo(
        ^^Expression<AddOp, PatternVar<PVx>, Constant<0.0>>,
        ^^Expression<AddOp, Atom<TagA, Free>, Constant<0.0>>,
        ^^PVx, ^^Atom<TagA, Free>));
  };

  "matchInfo: x_ + y_ matches a + b (two variables)"_test = [] {
    static_assert(matchBindingCount(
        ^^Expression<AddOp, PatternVar<PVx>, PatternVar<PVy>>,
        ^^Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>) == 2);
    static_assert(matchBindsTo(
        ^^Expression<AddOp, PatternVar<PVx>, PatternVar<PVy>>,
        ^^Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>,
        ^^PVx, ^^Atom<TagA, Free>));
    static_assert(matchBindsTo(
        ^^Expression<AddOp, PatternVar<PVx>, PatternVar<PVy>>,
        ^^Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>,
        ^^PVy, ^^Atom<TagB, Free>));
  };

  "matchInfo: x_ + y_ rejects mismatched operator"_test = [] {
    static_assert(matchFails(
        ^^Expression<AddOp, PatternVar<PVx>, PatternVar<PVy>>,
        ^^Expression<MulOp, Atom<TagA, Free>, Atom<TagB, Free>>));
  };

  // --- Non-linear patterns ---

  "matchInfo: x_ - x_ matches a - a (non-linear)"_test = [] {
    static_assert(matchSucceeds(
        ^^Expression<SubOp, PatternVar<PVx>, PatternVar<PVx>>,
        ^^Expression<SubOp, Atom<TagA, Free>, Atom<TagA, Free>>));
    static_assert(matchBindingCount(
        ^^Expression<SubOp, PatternVar<PVx>, PatternVar<PVx>>,
        ^^Expression<SubOp, Atom<TagA, Free>, Atom<TagA, Free>>) == 1);
  };

  "matchInfo: x_ - x_ rejects a - b (non-linear)"_test = [] {
    static_assert(matchFails(
        ^^Expression<SubOp, PatternVar<PVx>, PatternVar<PVx>>,
        ^^Expression<SubOp, Atom<TagA, Free>, Atom<TagB, Free>>));
  };

  // --- Nested pattern matching ---

  "matchInfo: (x_ + y_) * z_ matches nested expression"_test = [] {
    // Pattern: (x_ + y_) * z_
    // Expr:    (a + b) * 2
    static_assert(matchSucceeds(
        ^^Expression<MulOp,
            Expression<AddOp, PatternVar<PVx>, PatternVar<PVy>>,
            PatternVar<PVz>>,
        ^^Expression<MulOp,
            Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>,
            Constant<2.0>>));
    // All three variables bound
    static_assert(matchBindingCount(
        ^^Expression<MulOp,
            Expression<AddOp, PatternVar<PVx>, PatternVar<PVy>>,
            PatternVar<PVz>>,
        ^^Expression<MulOp,
            Expression<AddOp, Atom<TagA, Free>, Atom<TagB, Free>>,
            Constant<2.0>>) == 3);
  };

  // ==========================================================================
  // PHASE 3b: INFO-DOMAIN SUBSTITUTION
  // ==========================================================================

  "substituteInfo: x_ + 0 → x_ eliminates zero"_test = [] {
    // Rule: x_ + 0 → x_
    static_assert(applyRule(
        ^^Expression<AddOp, PatternVar<PVx>, Constant<0.0>>,
        ^^PatternVar<PVx>,
        ^^Expression<AddOp, Atom<TagA, Free>, Constant<0.0>>)
        == ^^Atom<TagA, Free>);
  };

  "substituteInfo: x_ * 0 → 0 collapses to constant"_test = [] {
    // Rule: x_ * 0 → 0
    static_assert(applyRule(
        ^^Expression<MulOp, PatternVar<PVx>, Constant<0.0>>,
        ^^Constant<0.0>,
        ^^Expression<MulOp, Atom<TagA, Free>, Constant<0.0>>)
        == ^^Constant<0.0>);
  };

  "substituteInfo: x_ * 1 → x_ eliminates one"_test = [] {
    static_assert(applyRule(
        ^^Expression<MulOp, PatternVar<PVx>, Constant<1.0>>,
        ^^PatternVar<PVx>,
        ^^Expression<MulOp, Atom<TagA, Free>, Constant<1.0>>)
        == ^^Atom<TagA, Free>);
  };

  "substituteInfo: x_ - x_ → 0 (non-linear)"_test = [] {
    static_assert(applyRule(
        ^^Expression<SubOp, PatternVar<PVx>, PatternVar<PVx>>,
        ^^Constant<0.0>,
        ^^Expression<SubOp, Atom<TagA, Free>, Atom<TagA, Free>>)
        == ^^Constant<0.0>);
  };

  "substituteInfo: replacement builds new Expression"_test = [] {
    // Rule: x_ + x_ → 2 * x_
    static_assert(applyRule(
        ^^Expression<AddOp, PatternVar<PVx>, PatternVar<PVx>>,
        ^^Expression<MulOp, Constant<2.0>, PatternVar<PVx>>,
        ^^Expression<AddOp, Atom<TagA, Free>, Atom<TagA, Free>>)
        == ^^Expression<MulOp, Constant<2.0>, Atom<TagA, Free>>);
  };

  "substituteInfo: no-match returns expr unchanged"_test = [] {
    // x_ + 0 pattern doesn't match a * b
    static_assert(applyRule(
        ^^Expression<AddOp, PatternVar<PVx>, Constant<0.0>>,
        ^^PatternVar<PVx>,
        ^^Expression<MulOp, Atom<TagA, Free>, Atom<TagB, Free>>)
        == ^^Expression<MulOp, Atom<TagA, Free>, Atom<TagB, Free>>);
  };

  // ==========================================================================
  // PHASE 3b: tryRulesInfo
  // ==========================================================================

  "tryRulesInfo: add-zero rule"_test = [] {
    static_assert(testTryRulesAddZero());
  };

  "tryRulesInfo: mul-zero rule"_test = [] {
    static_assert(testTryRulesMulZero());
  };

  "tryRulesInfo: no match returns unchanged"_test = [] {
    static_assert(testTryRulesNoMatch());
  };

  // ==========================================================================
  // PHASE 3c: INNERMOST FIXPOINT
  // ==========================================================================

  "innermostInfo: single step (a + 0 → a)"_test = [] {
    static_assert(testInnermostSimple());
  };

  "innermostInfo: nested ((a+0) + (b*1) → a + b)"_test = [] {
    static_assert(testInnermostNested());
  };

  "innermostInfo: fixpoint ((a*1) + 0 → a)"_test = [] {
    static_assert(testInnermostFixpoint());
  };

  "innermostInfo: deep (((a+0)*1) + 0 → a)"_test = [] {
    static_assert(testInnermostDeep());
  };

  "innermostInfo: non-linear (a - a → 0)"_test = [] {
    static_assert(testInnermostNonLinear());
  };

  "innermostInfo: no change (a + b → a + b)"_test = [] {
    static_assert(testInnermostNoChange());
  };

  "innermostInfo: cascade ((a-a) + b → 0 + b → b)"_test = [] {
    static_assert(testInnermostCascade());
  };

  // ==========================================================================
  // PHASE 3d: FULL RULE SET (makeInfoSimplifyRules)
  // ==========================================================================

  // --- Addition identity ---
  "infoSimplify: x + 0 → x"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<AddOp, Atom<TagA, Free>, Constant<0.0>>,
        ^^Atom<TagA, Free>));
  };

  "infoSimplify: 0 + x → x"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<AddOp, Constant<0.0>, Atom<TagA, Free>>,
        ^^Atom<TagA, Free>));
  };

  // --- Subtraction ---
  "infoSimplify: x - 0 → x"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<SubOp, Atom<TagA, Free>, Constant<0.0>>,
        ^^Atom<TagA, Free>));
  };

  "infoSimplify: x - x → 0"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<SubOp, Atom<TagA, Free>, Atom<TagA, Free>>,
        ^^Constant<0.0>));
  };

  // --- Multiplication ---
  "infoSimplify: x * 1 → x"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<MulOp, Atom<TagA, Free>, Constant<1.0>>,
        ^^Atom<TagA, Free>));
  };

  "infoSimplify: x * 0 → 0"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<MulOp, Atom<TagA, Free>, Constant<0.0>>,
        ^^Constant<0.0>));
  };

  // --- Division ---
  "infoSimplify: x / 1 → x"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<DivOp, Atom<TagA, Free>, Constant<1.0>>,
        ^^Atom<TagA, Free>));
  };

  "infoSimplify: x / x → 1"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<DivOp, Atom<TagA, Free>, Atom<TagA, Free>>,
        ^^Constant<1.0>));
  };

  // --- Power ---
  "infoSimplify: x^0 → 1"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<PowOp, Atom<TagA, Free>, Constant<0.0>>,
        ^^Constant<1.0>));
  };

  "infoSimplify: x^1 → x"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<PowOp, Atom<TagA, Free>, Constant<1.0>>,
        ^^Atom<TagA, Free>));
  };

  // --- Double negation ---
  "infoSimplify: -(-x) → x"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<NegOp, Expression<NegOp, Atom<TagA, Free>>>,
        ^^Atom<TagA, Free>));
  };

  // --- Inverse pairs ---
  "infoSimplify: exp(log(x)) → x"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<ExpOp, Expression<LogOp, Atom<TagA, Free>>>,
        ^^Atom<TagA, Free>));
  };

  "infoSimplify: log(exp(x)) → x"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<LogOp, Expression<ExpOp, Atom<TagA, Free>>>,
        ^^Atom<TagA, Free>));
  };

  // --- Trig at zero ---
  "infoSimplify: sin(0) → 0"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<SinOp, Constant<0.0>>,
        ^^Constant<0.0>));
  };

  "infoSimplify: cos(0) → 1"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<CosOp, Constant<0.0>>,
        ^^Constant<1.0>));
  };

  // --- Complex expression: (a * 1 + 0) / (exp(log(b))) → a / b ---
  "infoSimplify: complex multi-rule"_test = [] {
    static_assert(testComplexMultiRule());
  };

  // --- Fraction rules ---
  "infoSimplify: x * Fraction<0,1> → Fraction<0,1>"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<MulOp, Atom<TagA, Free>, Fraction<0, 1>>,
        ^^Fraction<0, 1>));
  };

  "infoSimplify: x * Fraction<1,1> → x"_test = [] {
    static_assert(infoSimplifies(
        ^^Expression<MulOp, Atom<TagA, Free>, Fraction<1, 1>>,
        ^^Atom<TagA, Free>));
  };

  return TestRegistry::result();
}
