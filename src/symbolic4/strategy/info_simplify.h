#pragma once

#include <experimental/meta>
#include <utility>
#include <vector>

#include "symbolic4/core.h"
#include "symbolic4/indexed/gather.h"
#include "symbolic4/indexed/reduce_over.h"
#include "symbolic4/strategy/pattern.h"

// ============================================================================
// info_simplify.h — Info-domain pattern matching and rewriting
// ============================================================================
//
// The type-domain simplifier (simplify.h + pattern.h + rule.h) rewrites
// expression *types* by matching/substituting through template specialization.
// Every intermediate expression instantiates a new C++ type — that's the
// compile-time cost we want to eliminate.
//
// This module does the same matching and rewriting on std::meta::info values.
// The expression tree is represented as reflected infos, manipulated via
// template_arguments_of / template_of / substitute. No intermediate types
// are instantiated until the final result is spliced back to the type domain.
//
// Key functions:
//   matchInfo(pattern, expr)       → MatchResult (bindings on success)
//   substituteInfo(repl, bindings) → info (bound replacement)
//   tryRulesInfo(rules, expr)      → info (first matching rule applied)
//
// All functions are consteval — they run entirely at compile time within the
// consteval evaluator, producing std::meta::info values that can be spliced
// into types when needed.
//
// ============================================================================

namespace tempura::symbolic4 {

// Result of an info-domain match: success flag + variable bindings.
// Each binding maps a PatternVar's tag (its template argument) to the
// info of the matched expression subtree.
struct MatchResult {
  bool success;
  std::vector<std::pair<std::meta::info, std::meta::info>> bindings;
};

// A rewrite rule in the info domain: pattern → replacement.
struct InfoRule {
  std::meta::info pattern;
  std::meta::info replacement;
};

namespace info_detail {

// Detect PatternVar<Tag> specializations.
consteval auto isPatternVar(std::meta::info i) -> bool {
  if (!std::meta::has_template_arguments(i)) return false;
  return std::meta::template_of(i) == ^^PatternVar;
}

// Extract the tag type from PatternVar<Tag>.
consteval auto patternVarKey(std::meta::info pv) -> std::meta::info {
  return std::meta::template_arguments_of(pv)[0];
}

// Look up a key in the bindings list.
consteval auto findBinding(
    const std::vector<std::pair<std::meta::info, std::meta::info>>& bindings,
    std::meta::info key) -> std::pair<bool, std::meta::info> {
  for (const auto& [k, v] : bindings) {
    if (k == key) return {true, v};
  }
  return {false, std::meta::info{}};
}

// ============================================================================
// Expression-building helpers — shared by info_simplify and info_diff
// ============================================================================

// Build unary Expression<Op, Arg> info.
consteval auto unary(std::meta::info op, std::meta::info arg) -> std::meta::info {
  return std::meta::substitute(^^Expression, {op, arg});
}

// Build binary Expression<Op, Lhs, Rhs> info.
consteval auto binary(std::meta::info op, std::meta::info lhs, std::meta::info rhs)
    -> std::meta::info {
  return std::meta::substitute(^^Expression, {op, lhs, rhs});
}

// Zero/one detection — used by simplifyNode for single-layer normalization.
consteval auto isInfoZero(std::meta::info i) -> bool {
  return i == ^^Constant<0.0> || i == ^^Fraction<0, 1>;
}

consteval auto isInfoOne(std::meta::info i) -> bool {
  return i == ^^Constant<1.0> || i == ^^Fraction<1, 1>;
}

}  // namespace info_detail

// ============================================================================
// matchInfo — Structural pattern matching in the info domain
// ============================================================================
//
// Walks pattern and expr trees in parallel:
//   - PatternVar: binds expr to the variable's key (or checks consistency)
//   - Template specialization: checks template identity, then recurses on args
//   - Non-template info: checks exact equality (operators, NTTP values)
//
// Non-linear patterns (same PatternVar appearing twice) work automatically:
// the merge step detects conflicting bindings for the same key.
//
consteval auto matchInfo(std::meta::info pattern, std::meta::info expr) -> MatchResult {
  using namespace info_detail;

  // PatternVar matches anything — bind the variable
  if (isPatternVar(pattern)) {
    auto key = patternVarKey(pattern);
    return {true, {{key, expr}}};
  }

  // Non-template pattern: operators (AddOp), NTTP values, tags
  if (!std::meta::has_template_arguments(pattern)) {
    return {pattern == expr, {}};
  }

  // Pattern is a template specialization but expr isn't: structural mismatch
  if (!std::meta::has_template_arguments(expr)) {
    return {false, {}};
  }

  // Different templates: Atom vs Expression, Constant vs Fraction, etc.
  if (std::meta::template_of(pattern) != std::meta::template_of(expr)) {
    return {false, {}};
  }

  auto pattern_args = std::meta::template_arguments_of(pattern);
  auto expr_args = std::meta::template_arguments_of(expr);

  if (pattern_args.size() != expr_args.size()) {
    return {false, {}};
  }

  // Recursively match all arguments, accumulating and merging bindings
  MatchResult result{true, {}};
  for (std::size_t i = 0; i < pattern_args.size(); ++i) {
    auto child = matchInfo(pattern_args[i], expr_args[i]);
    if (!child.success) return {false, {}};

    // Merge child bindings with non-linear consistency check
    for (const auto& [key, val] : child.bindings) {
      auto [found, existing] = findBinding(result.bindings, key);
      if (found) {
        if (existing != val) return {false, {}};  // same var, different subtrees
      } else {
        result.bindings.push_back({key, val});
      }
    }
  }

  return result;
}

// ============================================================================
// substituteInfo — Replace PatternVars with bound values in replacement
// ============================================================================
//
// Walks the replacement tree, replacing each PatternVar<Tag> with the
// corresponding bound value from the bindings, and reconstructing
// template specializations via substitute().
//
consteval auto substituteInfo(
    std::meta::info replacement,
    const std::vector<std::pair<std::meta::info, std::meta::info>>& bindings) -> std::meta::info {
  using namespace info_detail;

  if (isPatternVar(replacement)) {
    auto key = patternVarKey(replacement);
    auto [found, val] = findBinding(bindings, key);
    return val;  // Always found — pattern was already matched
  }

  // Non-template: operators, NTTP values, tags — pass through unchanged
  if (!std::meta::has_template_arguments(replacement)) {
    return replacement;
  }

  // Template specialization: recursively substitute each argument, rebuild
  auto tmpl = std::meta::template_of(replacement);
  auto args = std::meta::template_arguments_of(replacement);
  std::vector<std::meta::info> new_args;
  new_args.reserve(args.size());
  for (auto arg : args) {
    new_args.push_back(substituteInfo(arg, bindings));
  }
  return std::meta::substitute(tmpl, new_args);
}

// ============================================================================
// tryRulesInfo — Apply first matching rule from a rule set
// ============================================================================
//
// Tries each rule in order. Returns the substituted replacement for the
// first rule whose pattern matches. If no rule matches, returns expr unchanged.
//
consteval auto tryRulesInfo(
    const std::vector<InfoRule>& rules,
    std::meta::info expr) -> std::meta::info {
  for (const auto& rule : rules) {
    auto result = matchInfo(rule.pattern, expr);
    if (result.success) {
      return substituteInfo(rule.replacement, result.bindings);
    }
  }
  return expr;
}

// ============================================================================
// innermostInfo — Bottom-up fixpoint rewriting
// ============================================================================
//
// The "innermost" strategy from Stratego: normalize children first (bottom-up),
// then try rules at the current node. If a rule fires, re-enter on the result
// (the new subtree might enable further rules). Loop until fixpoint or depth limit.
//
// This is the info-domain equivalent of Innermost<S, MaxDepth> in combinator.h.
// The key difference: no intermediate C++ types are instantiated. Every step
// operates on std::meta::info values, and only the final result gets spliced
// back to a type.
//
// Depth limit prevents infinite loops from cyclic rule sets (e.g., x+y ↔ y+x).
//
constexpr int kMaxInfoDepth = 256;

consteval auto innermostInfo(
    std::meta::info expr,
    const std::vector<InfoRule>& rules,
    int depth = 0) -> std::meta::info {
  if (depth >= kMaxInfoDepth) return expr;

  // Step 1: Normalize children (bottom-up)
  // Non-template nodes and nodes without children pass through unchanged.
  auto normalized = expr;
  if (std::meta::has_template_arguments(expr)) {
    auto tmpl = std::meta::template_of(expr);
    auto args = std::meta::template_arguments_of(expr);

    // Expression: arg[0] is the operator — skip it; recurse into arg[1..N].
    // ReduceOver: arg[0] is ReduceOp, arg[1] is DimTag, arg[2] is the body.
    // Gather: arg[0] is param (expression), arg[1] is index (data, don't recurse).
    // Other templates (Atom, Constant, etc.): children are type params, skip.
    bool is_expression = (tmpl == ^^Expression);
    bool is_reduce_over = (tmpl == ^^ReduceOver);
    bool is_gather = (tmpl == ^^Gather);
    bool any_changed = false;

    std::vector<std::meta::info> new_args;
    new_args.reserve(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
      bool should_recurse =
          (is_expression && i > 0) ||
          (is_reduce_over && i == 2) ||  // body of ReduceOver
          (is_gather && i == 0);         // param of Gather
      if (should_recurse) {
        auto simplified = innermostInfo(args[i], rules, depth + 1);
        new_args.push_back(simplified);
        if (simplified != args[i]) any_changed = true;
      } else {
        new_args.push_back(args[i]);
      }
    }

    if (any_changed) {
      normalized = std::meta::substitute(tmpl, new_args);
    }
  }

  // Step 2: Try rules at this node
  auto result = tryRulesInfo(rules, normalized);

  // Step 3: Fixpoint — if a rule fired, re-enter (the result might match again)
  if (result != normalized) {
    return innermostInfo(result, rules, depth + 1);
  }

  return normalized;
}

// ============================================================================
// makeInfoSimplifyRules — Port of simplify.h rules to info domain
// ============================================================================
//
// These mirror the exact structural simplification rules from simplify.h:
//   - Identity elimination (x+0, x*1, x/1, x^1, etc.)
//   - Zero annihilation (x*0, 0/x, x^0, etc.)
//   - Self cancellation (x-x, x/x)
//   - Double negation
//   - Inverse function pairs
//   - Trig/hyperbolic at zero
//   - Fraction variants
//
// Not included: constant folding (partial_eval.h), which requires evaluation.
//

namespace info_detail {

// Pattern variable tags for info-domain rules.
struct IPVx {};
struct IPVy {};

}  // namespace info_detail

consteval auto makeInfoSimplifyRules() -> std::vector<InfoRule> {
  using info_detail::unary;
  using info_detail::binary;

  auto x_ = ^^PatternVar<info_detail::IPVx>;
  auto y_ = ^^PatternVar<info_detail::IPVy>;

  auto zero = ^^Constant<0.0>;
  auto one = ^^Constant<1.0>;
  auto two = ^^Constant<2.0>;
  auto frac_zero = ^^Fraction<0, 1>;
  auto frac_one = ^^Fraction<1, 1>;

  // Reflect operators via their original definitions in ::tempura.
  // Using-declarations in symbolic4 namespace are not reflectable.
  auto add = ^^::tempura::AddOp;
  auto sub = ^^::tempura::SubOp;
  auto mul = ^^::tempura::MulOp;
  auto div = ^^::tempura::DivOp;
  auto pow = ^^::tempura::PowOp;
  auto neg = ^^::tempura::NegOp;
  auto exp_ = ^^::tempura::ExpOp;
  auto log_ = ^^::tempura::LogOp;
  auto sqrt_ = ^^::tempura::SqrtOp;
  auto abs_ = ^^::tempura::AbsOp;
  auto sin_ = ^^::tempura::SinOp;
  auto cos_ = ^^::tempura::CosOp;
  auto tan_ = ^^::tempura::TanOp;
  auto sinh_ = ^^::tempura::SinhOp;
  auto cosh_ = ^^::tempura::CoshOp;
  auto tanh_ = ^^::tempura::TanhOp;

  return {
    // --- Addition ---
    {binary(add, x_, zero), x_},               // x + 0 → x
    {binary(add, zero, x_), x_},               // 0 + x → x

    // --- Subtraction ---
    {binary(sub, x_, zero), x_},               // x - 0 → x
    {binary(sub, x_, x_), zero},               // x - x → 0

    // --- Multiplication ---
    {binary(mul, x_, one), x_},                // x * 1 → x
    {binary(mul, one, x_), x_},                // 1 * x → x
    {binary(mul, x_, zero), zero},             // x * 0 → 0
    {binary(mul, zero, x_), zero},             // 0 * x → 0

    // --- Division ---
    {binary(div, x_, one), x_},                // x / 1 → x
    {binary(div, zero, x_), zero},             // 0 / x → 0
    {binary(div, x_, x_), one},                // x / x → 1

    // --- Power ---
    {binary(pow, x_, zero), one},              // x^0 → 1
    {binary(pow, x_, one), x_},                // x^1 → x
    {binary(pow, zero, x_), zero},             // 0^x → 0
    {binary(pow, one, x_), one},               // 1^x → 1

    // --- Double negation ---
    {unary(neg, unary(neg, x_)), x_},          // -(-x) → x

    // --- Inverse function pairs ---
    {unary(exp_, unary(log_, x_)), x_},                        // exp(log(x)) → x
    {unary(log_, unary(exp_, x_)), x_},                        // log(exp(x)) → x
    {unary(sqrt_, binary(pow, x_, two)), unary(abs_, x_)},     // sqrt(x^2) → abs(x)
    {binary(pow, unary(sqrt_, x_), two), x_},                  // sqrt(x)^2 → x

    // --- Trig at zero ---
    {unary(sin_, zero), zero},                 // sin(0) → 0
    {unary(cos_, zero), one},                  // cos(0) → 1
    {unary(tan_, zero), zero},                 // tan(0) → 0

    // --- Hyperbolic at zero ---
    {unary(sinh_, zero), zero},                // sinh(0) → 0
    {unary(cosh_, zero), one},                 // cosh(0) → 1
    {unary(tanh_, zero), zero},                // tanh(0) → 0

    // --- Fraction variants ---
    {binary(add, x_, frac_zero), x_},          // x + 0/1 → x
    {binary(add, frac_zero, x_), x_},          // 0/1 + x → x
    {binary(sub, x_, frac_zero), x_},          // x - 0/1 → x
    {binary(mul, x_, frac_one), x_},           // x * 1/1 → x
    {binary(mul, frac_one, x_), x_},           // 1/1 * x → x
    {binary(div, x_, frac_one), x_},           // x / 1/1 → x
    {binary(mul, x_, frac_zero), frac_zero},   // x * 0/1 → 0/1
    {binary(mul, frac_zero, x_), frac_zero},   // 0/1 * x → 0/1
    {binary(div, frac_zero, x_), frac_zero},   // 0/1 / x → 0/1
    {binary(pow, x_, frac_zero), frac_one},    // x^(0/1) → 1/1
    {binary(pow, x_, frac_one), x_},           // x^(1/1) → x
    {binary(pow, frac_zero, x_), frac_zero},   // (0/1)^x → 0/1
    {binary(pow, frac_one, x_), frac_one},     // (1/1)^x → 1/1
  };
}

// ============================================================================
// simplifyNode — Single-layer algebraic normalization
// ============================================================================
//
// Applies zero/one identity rules to just the root node of an Expression.
// No recursion, no fixpoint — O(1) per call. Callers that build expressions
// incrementally (e.g., differentiation) can wrap each construction to keep
// the tree small, so the full innermostInfo fixpoint has less work.
//
// This is opt-in: raw binary()/unary() still produce exactly what you ask for.
//
consteval auto simplifyNode(std::meta::info expr) -> std::meta::info {
  using info_detail::isInfoZero;
  using info_detail::isInfoOne;

  if (!std::meta::has_template_arguments(expr)) return expr;
  auto tmpl = std::meta::template_of(expr);
  if (tmpl != ^^Expression) return expr;

  auto args = std::meta::template_arguments_of(expr);
  auto op = args[0];
  auto zero = ^^Constant<0.0>;
  auto one = ^^Constant<1.0>;

  // --- Unary: Expression<Op, Arg> ---
  if (args.size() == 2) {
    auto arg = args[1];
    if (op == ^^::tempura::NegOp) {
      if (isInfoZero(arg)) return zero;                         // -0 → 0
      // -(-x) → x
      if (std::meta::has_template_arguments(arg) &&
          std::meta::template_of(arg) == ^^Expression) {
        auto inner = std::meta::template_arguments_of(arg);
        if (inner.size() == 2 && inner[0] == ^^::tempura::NegOp)
          return inner[1];
      }
    }
    return expr;
  }

  // --- Binary: Expression<Op, Lhs, Rhs> ---
  if (args.size() == 3) {
    auto lhs = args[1];
    auto rhs = args[2];

    if (op == ^^::tempura::AddOp) {
      if (isInfoZero(lhs)) return rhs;                         // 0 + x → x
      if (isInfoZero(rhs)) return lhs;                         // x + 0 → x
    } else if (op == ^^::tempura::SubOp) {
      if (isInfoZero(rhs)) return lhs;                         // x - 0 → x
      if (lhs == rhs) return zero;                             // x - x → 0
    } else if (op == ^^::tempura::MulOp) {
      if (isInfoZero(lhs) || isInfoZero(rhs)) return zero;    // 0·x, x·0 → 0
      if (isInfoOne(lhs)) return rhs;                          // 1·x → x
      if (isInfoOne(rhs)) return lhs;                          // x·1 → x
    } else if (op == ^^::tempura::DivOp) {
      if (isInfoZero(lhs)) return zero;                        // 0/x → 0
      if (isInfoOne(rhs)) return lhs;                          // x/1 → x
      if (lhs == rhs) return one;                              // x/x → 1
    } else if (op == ^^::tempura::PowOp) {
      if (isInfoZero(rhs)) return one;                         // x⁰ → 1
      if (isInfoOne(rhs)) return lhs;                          // x¹ → x
      if (isInfoZero(lhs)) return zero;                        // 0ˣ → 0
      if (isInfoOne(lhs)) return one;                          // 1ˣ → 1
    }
  }

  return expr;
}

// ============================================================================
// infoSimplify — Simplify an expression type in the info domain
// ============================================================================
//
// Takes a reflected expression type, applies all structural simplification
// rules via innermost fixpoint, returns the simplified info.
//
// Usage:
//   constexpr auto simplified = infoSimplify(^^decltype(expr));
//   using Result = [:simplified:];
//
consteval auto infoSimplify(std::meta::info expr) -> std::meta::info {
  auto rules = makeInfoSimplifyRules();
  return innermostInfo(expr, rules);
}

}  // namespace tempura::symbolic4
