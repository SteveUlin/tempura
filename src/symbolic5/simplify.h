#pragma once
#include "symbolic5/math.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace tempura {

// ─── Info-domain pattern matching and rewriting ──────────────────────────────
//
// Data-driven rewrite engine: rules are {pattern, replacement} pairs stored in
// a flat vector. The engine walks patterns and expressions in parallel, binds
// PatternVar nodes, and rebuilds the replacement with bound values.
//
// Ported from symbolic4's info_simplify.h, simplified for symbolic5's
// App<Op, Children...> tree (no ReduceOver/Gather special cases).

struct MatchResult {
  bool success;
  std::vector<std::pair<std::meta::info, std::meta::info>> bindings;
};

struct InfoRule {
  std::meta::info pattern;
  std::meta::info replacement;
};

// ─── Detection and lookup helpers ────────────────────────────────────────────

namespace simplify_detail {

consteval bool isPatternVar(std::meta::info i) {
  if (!std::meta::has_template_arguments(i)) return false;
  return std::meta::template_of(i) == ^^PatternVar;
}

consteval std::meta::info patternVarKey(std::meta::info pv) {
  return std::meta::template_arguments_of(pv)[0];
}

consteval std::pair<bool, std::meta::info> findBinding(
    const std::vector<std::pair<std::meta::info, std::meta::info>>& bindings,
    std::meta::info key) {
  for (const auto& [k, v] : bindings) {
    if (k == key) return {true, v};
  }
  return {false, std::meta::info{}};
}

// ─── Expression builders ─────────────────────────────────────────────────────

consteval std::meta::info binary(std::meta::info op,
                                 std::meta::info lhs, std::meta::info rhs) {
  return std::meta::substitute(^^App, {op, lhs, rhs});
}

consteval std::meta::info unary(std::meta::info op, std::meta::info arg) {
  return std::meta::substitute(^^App, {op, arg});
}

// ─── Constant folding primitives ─────────────────────────────────────────────
//
// Moved before canonicalization because sum/product collection needs foldBinary
// and foldUnary to combine coefficients and exponents at compile time.

template <typename Op, auto A, auto B>
struct BinaryFold {
  static constexpr auto value = Op::apply(A, B);
};

template <typename Op, auto A>
struct UnaryFold {
  static constexpr auto value = Op::apply(A);
};

consteval std::meta::info foldBinary(std::meta::info op,
                                     std::meta::info a, std::meta::info b) {
  auto fold = std::meta::substitute(^^BinaryFold, {op, a, b});
  auto member = std::meta::static_data_members_of(
      fold, std::meta::access_context::unchecked())[0];
  return std::meta::substitute(^^Constant, {std::meta::constant_of(member)});
}

consteval std::meta::info foldUnary(std::meta::info op, std::meta::info a) {
  auto fold = std::meta::substitute(^^UnaryFold, {op, a});
  auto member = std::meta::static_data_members_of(
      fold, std::meta::access_context::unchecked())[0];
  return std::meta::substitute(^^Constant, {std::meta::constant_of(member)});
}

consteval bool isFoldableArithBinary(std::meta::info op) {
  return op == ^^AddOp || op == ^^SubOp || op == ^^MulOp || op == ^^DivOp;
}

// ─── Total ordering for canonical forms ──────────────────────────────────────
//
// Canonical ordering enables like-term grouping: sort terms by a deterministic
// key, then adjacent equal keys get combined. Two orderings exist:
//   - infoCompare: for sum terms (complex App first, constants last)
//   - productCompare: for product factors (constants first, App last)

consteval int infoCategory(std::meta::info i) {
  if (!std::meta::has_template_arguments(i)) return 2;  // bare non-template → treat as constant
  auto tmpl = std::meta::template_of(i);
  if (tmpl == std::meta::template_of(^^Constant<0.0>)) return 2;  // Constant
  if (tmpl == ^^Symbol) return 1;
  return 0;  // App (functions, operators)
}

consteval int infoOpIndex(std::meta::info op) {
  if (op == ^^AddOp) return 0;
  if (op == ^^SubOp) return 1;
  if (op == ^^MulOp) return 2;
  if (op == ^^DivOp) return 3;
  if (op == ^^NegOp) return 4;
  if (op == ^^PowOp) return 5;
  if (op == ^^SqrtOp) return 10;
  if (op == ^^ExpOp) return 11;
  if (op == ^^LogOp) return 12;
  if (op == ^^SinOp) return 20;
  if (op == ^^CosOp) return 21;
  if (op == ^^TanOp) return 22;
  if (op == ^^AbsOp) return 25;
  if (op == ^^PiOp) return 40;
  if (op == ^^EulerOp) return 41;
  return 100;
}

consteval int infoCompare(std::meta::info lhs, std::meta::info rhs) {
  if (lhs == rhs) return 0;

  int l_cat = infoCategory(lhs);
  int r_cat = infoCategory(rhs);
  if (l_cat != r_cat) return l_cat - r_cat;

  // Both App
  if (l_cat == 0) {
    auto l_args = std::meta::template_arguments_of(lhs);
    auto r_args = std::meta::template_arguments_of(rhs);
    // Compare op index
    int l_op = infoOpIndex(l_args[0]);
    int r_op = infoOpIndex(r_args[0]);
    if (l_op != r_op) return l_op - r_op;
    // Lexicographic on children
    auto l_sz = l_args.size();
    auto r_sz = r_args.size();
    auto min_sz = l_sz < r_sz ? l_sz : r_sz;
    for (std::size_t i = 1; i < min_sz; ++i) {
      int c = infoCompare(l_args[i], r_args[i]);
      if (c != 0) return c;
    }
    return static_cast<int>(l_sz) - static_cast<int>(r_sz);
  }

  // Both Symbol or both Constant — compare display strings
  auto l_str = std::meta::display_string_of(lhs);
  auto r_str = std::meta::display_string_of(rhs);
  if (l_str < r_str) return -1;
  if (l_str > r_str) return 1;
  return 0;
}

// Product ordering: constants < symbols < apps (numeric coefficients float left)
consteval int productCompare(std::meta::info lhs, std::meta::info rhs) {
  if (lhs == rhs) return 0;

  // Reversed category: Constant=0, Symbol=1, App=2
  auto prodCat = [](std::meta::info i) -> int {
    int c = infoCategory(i);
    if (c == 0) return 2;  // App → last
    if (c == 1) return 1;  // Symbol → middle
    return 0;              // Constant → first
  };

  int l_cat = prodCat(lhs);
  int r_cat = prodCat(rhs);
  if (l_cat != r_cat) return l_cat - r_cat;

  // Within same category, fall back to infoCompare
  return infoCompare(lhs, rhs);
}

// ─── Sum canonicalization (like-term collection for +/-) ─────────────────────
//
// Flattens Add/Sub chains → extracts coefficient × base pairs → sorts by base
// → groups adjacent same-base terms → sums coefficients → rebuilds.

struct ConstantBase {};  // sentinel key for bare numeric addends

struct SumTerm {
  std::meta::info coeff;  // always a Constant<V> info
  std::meta::info base;   // symbolic base, or ^^ConstantBase for bare numbers
};

consteval void flattenAdd(std::meta::info expr, bool negate,
                          std::vector<std::pair<std::meta::info, bool>>& terms) {
  if (std::meta::has_template_arguments(expr) &&
      std::meta::template_of(expr) == ^^App) {
    auto args = std::meta::template_arguments_of(expr);
    auto op = args[0];
    if (op == ^^AddOp) {
      flattenAdd(args[1], negate, terms);
      flattenAdd(args[2], negate, terms);
      return;
    }
    if (op == ^^SubOp) {
      flattenAdd(args[1], negate, terms);
      flattenAdd(args[2], !negate, terms);
      return;
    }
  }
  terms.push_back({expr, negate});
}

consteval std::meta::info getNttpOf(std::meta::info constant_info) {
  return std::meta::template_arguments_of(constant_info)[0];
}

consteval SumTerm extractSumCoeff(std::meta::info expr, bool negated) {
  auto constant_tmpl = std::meta::template_of(^^Constant<0.0>);

  auto isConstant = [&](std::meta::info node) {
    return std::meta::has_template_arguments(node) &&
           std::meta::template_of(node) == constant_tmpl;
  };

  // App<NegOp, child> → flip and recurse
  if (std::meta::has_template_arguments(expr) &&
      std::meta::template_of(expr) == ^^App) {
    auto args = std::meta::template_arguments_of(expr);
    if (args[0] == (^^NegOp) && args.size() == 2) {
      return extractSumCoeff(args[1], !negated);
    }
    // App<MulOp, Constant<c>, base> or App<MulOp, base, Constant<c>>
    if (args[0] == (^^MulOp) && args.size() == 3) {
      if (isConstant(args[1])) {
        auto coeff = args[1];
        auto base = args[2];
        if (negated) {
          auto neg_nttp = foldUnary(^^NegOp, getNttpOf(coeff));
          coeff = std::meta::substitute(constant_tmpl, {std::meta::template_arguments_of(neg_nttp)[0]});
        }
        return {coeff, base};
      }
      if (isConstant(args[2])) {
        auto coeff = args[2];
        auto base = args[1];
        if (negated) {
          auto neg_nttp = foldUnary(^^NegOp, getNttpOf(coeff));
          coeff = std::meta::substitute(constant_tmpl, {std::meta::template_arguments_of(neg_nttp)[0]});
        }
        return {coeff, base};
      }
    }
  }

  // Bare Constant<c>
  if (isConstant(expr)) {
    auto coeff = expr;
    if (negated) {
      auto neg_nttp = foldUnary(^^NegOp, getNttpOf(coeff));
      coeff = std::meta::substitute(constant_tmpl, {std::meta::template_arguments_of(neg_nttp)[0]});
    }
    return {coeff, ^^ConstantBase};
  }

  // Default: coeff = 1 (or -1 if negated)
  auto coeff = negated ? ^^Constant<-1.0> : ^^Constant<1.0>;
  return {coeff, expr};
}

consteval std::meta::info collectSumTerms(std::meta::info expr) {
  // Only process Add/Sub roots
  if (!std::meta::has_template_arguments(expr) ||
      std::meta::template_of(expr) != ^^App) return expr;
  auto root_args = std::meta::template_arguments_of(expr);
  auto root_op = root_args[0];
  if (root_op != (^^AddOp) && root_op != ^^SubOp) return expr;

  auto constant_tmpl = std::meta::template_of(^^Constant<0.0>);

  // Flatten
  std::vector<std::pair<std::meta::info, bool>> flat;
  flattenAdd(expr, false, flat);

  // Extract
  std::vector<SumTerm> terms;
  terms.reserve(flat.size());
  for (auto [e, neg] : flat) {
    terms.push_back(extractSumCoeff(e, neg));
  }

  // Sort by base so adjacent like terms can be grouped
  std::sort(terms.begin(), terms.end(), [](const SumTerm& a, const SumTerm& b) {
    return infoCompare(a.base, b.base) < 0;
  });

  // Group adjacent same-base, sum coefficients
  std::vector<SumTerm> grouped;
  for (std::size_t i = 0; i < terms.size(); ) {
    auto base = terms[i].base;
    auto coeff_nttp = getNttpOf(terms[i].coeff);
    std::size_t j = i + 1;
    while (j < terms.size() && terms[j].base == base) {
      // Sum coefficients: foldBinary(AddOp, c1, c2) → Constant<c1+c2>
      auto sum = foldBinary(^^AddOp, coeff_nttp, getNttpOf(terms[j].coeff));
      coeff_nttp = getNttpOf(sum);
      ++j;
    }
    auto coeff = std::meta::substitute(constant_tmpl, {coeff_nttp});
    grouped.push_back({coeff, base});
    i = j;
  }

  // Rebuild
  auto zero_nttp = getNttpOf(^^Constant<0.0>);
  auto one_nttp = getNttpOf(^^Constant<1.0>);
  auto neg_one_nttp = getNttpOf(^^Constant<-1.0>);

  std::vector<std::meta::info> result_terms;
  for (auto& t : grouped) {
    auto c_nttp = getNttpOf(t.coeff);
    // Drop zero-coefficient terms
    if (c_nttp == zero_nttp) continue;

    if (t.base == ^^ConstantBase) {
      // Pure constant term
      result_terms.push_back(t.coeff);
    } else if (c_nttp == one_nttp) {
      result_terms.push_back(t.base);
    } else if (c_nttp == neg_one_nttp) {
      result_terms.push_back(std::meta::substitute(^^App, {^^NegOp, t.base}));
    } else {
      result_terms.push_back(std::meta::substitute(^^App, {^^MulOp, t.coeff, t.base}));
    }
  }

  if (result_terms.empty()) return ^^Constant<0.0>;
  if (result_terms.size() == 1) return result_terms[0];

  // Left-fold into Add chain
  auto acc = result_terms[0];
  for (std::size_t i = 1; i < result_terms.size(); ++i) {
    acc = std::meta::substitute(^^App, {^^AddOp, acc, result_terms[i]});
  }
  return acc;
}

// ─── Product canonicalization (like-base collection for *) ───────────────────
//
// Flattens Mul chains → separates numeric coefficient from symbolic factors →
// extracts base^exponent pairs → sorts → groups → sums exponents → rebuilds.

struct ProductFactor {
  std::meta::info base;
  std::meta::info exponent;  // always a Constant<V> info
};

consteval void flattenMul(std::meta::info expr,
                          std::vector<std::meta::info>& factors) {
  if (std::meta::has_template_arguments(expr) &&
      std::meta::template_of(expr) == ^^App) {
    auto args = std::meta::template_arguments_of(expr);
    if (args[0] == (^^MulOp) && args.size() == 3) {
      flattenMul(args[1], factors);
      flattenMul(args[2], factors);
      return;
    }
  }
  factors.push_back(expr);
}

consteval std::meta::info collectProductFactors(std::meta::info expr) {
  // Only process Mul roots
  if (!std::meta::has_template_arguments(expr) ||
      std::meta::template_of(expr) != ^^App) return expr;
  auto root_args = std::meta::template_arguments_of(expr);
  if (root_args[0] != ^^MulOp) return expr;

  auto constant_tmpl = std::meta::template_of(^^Constant<0.0>);

  auto isConstant = [&](std::meta::info node) {
    return std::meta::has_template_arguments(node) &&
           std::meta::template_of(node) == constant_tmpl;
  };

  // Flatten
  std::vector<std::meta::info> flat;
  flattenMul(expr, flat);

  // Separate numeric coefficient from symbolic factors
  auto coeff_nttp = getNttpOf(^^Constant<1.0>);
  std::vector<ProductFactor> factors;

  for (auto f : flat) {
    if (isConstant(f)) {
      // Accumulate numeric coefficient
      auto prod = foldBinary(^^MulOp, coeff_nttp, getNttpOf(f));
      coeff_nttp = getNttpOf(prod);
      continue;
    }

    // App<PowOp, base, Constant<e>> → (base, e)
    if (std::meta::has_template_arguments(f) &&
        std::meta::template_of(f) == ^^App) {
      auto args = std::meta::template_arguments_of(f);
      if (args[0] == (^^PowOp) && args.size() == 3 && isConstant(args[2])) {
        factors.push_back({args[1], args[2]});
        continue;
      }
      // App<SqrtOp, base> → (base, 0.5)
      if (args[0] == (^^SqrtOp) && args.size() == 2) {
        factors.push_back({args[1], ^^Constant<0.5>});
        continue;
      }
    }

    // Default: (factor, 1)
    factors.push_back({f, ^^Constant<1.0>});
  }

  // Sort factors by base so adjacent like bases can be grouped
  std::sort(factors.begin(), factors.end(), [](const ProductFactor& a, const ProductFactor& b) {
    return productCompare(a.base, b.base) < 0;
  });

  // Group adjacent same-base, sum exponents
  std::vector<ProductFactor> grouped;
  for (std::size_t i = 0; i < factors.size(); ) {
    auto base = factors[i].base;
    auto exp_nttp = getNttpOf(factors[i].exponent);
    std::size_t j = i + 1;
    while (j < factors.size() && factors[j].base == base) {
      auto sum = foldBinary(^^AddOp, exp_nttp, getNttpOf(factors[j].exponent));
      exp_nttp = getNttpOf(sum);
      ++j;
    }
    auto exp_info = std::meta::substitute(constant_tmpl, {exp_nttp});
    grouped.push_back({base, exp_info});
    i = j;
  }

  // Rebuild factor list
  auto zero_nttp = getNttpOf(^^Constant<0.0>);
  auto one_nttp = getNttpOf(^^Constant<1.0>);
  auto half_nttp = getNttpOf(^^Constant<0.5>);

  std::vector<std::meta::info> result_factors;

  // Prefix with numeric coefficient if != 1
  if (coeff_nttp != one_nttp) {
    result_factors.push_back(std::meta::substitute(constant_tmpl, {coeff_nttp}));
  }

  for (auto& f : grouped) {
    auto e_nttp = getNttpOf(f.exponent);
    if (e_nttp == zero_nttp) continue;  // base^0 = 1, drop
    if (e_nttp == one_nttp) {
      result_factors.push_back(f.base);
    } else if (e_nttp == half_nttp) {
      result_factors.push_back(std::meta::substitute(^^App, {^^SqrtOp, f.base}));
    } else {
      result_factors.push_back(std::meta::substitute(^^App, {^^PowOp, f.base, f.exponent}));
    }
  }

  if (result_factors.empty()) return ^^Constant<1.0>;
  if (result_factors.size() == 1) return result_factors[0];

  // Left-fold into Mul chain
  auto acc = result_factors[0];
  for (std::size_t i = 1; i < result_factors.size(); ++i) {
    acc = std::meta::substitute(^^App, {^^MulOp, acc, result_factors[i]});
  }
  return acc;
}

// ─── Bottom-up canonicalization ──────────────────────────────────────────────
//
// Product canonicalization runs first so sum collection sees consistent bases
// (e.g. x*x → pow(x,2) before x*x + x*x → 2*pow(x,2)).

consteval std::meta::info canonicalizeInfo(std::meta::info expr) {
  if (!std::meta::has_template_arguments(expr)) return expr;
  auto tmpl = std::meta::template_of(expr);
  if (tmpl != ^^App) return expr;

  // Step 1: recurse into children (skip arg[0] the op)
  auto args = std::meta::template_arguments_of(expr);
  bool any_changed = false;
  std::vector<std::meta::info> new_args;
  new_args.reserve(args.size());
  new_args.push_back(args[0]);
  for (std::size_t i = 1; i < args.size(); ++i) {
    auto c = canonicalizeInfo(args[i]);
    new_args.push_back(c);
    if (c != args[i]) any_changed = true;
  }
  if (any_changed) {
    expr = std::meta::substitute(^^App, new_args);
  }

  // Step 2: product canonicalization (on Mul nodes)
  expr = collectProductFactors(expr);

  // Step 3: sum canonicalization (on Add/Sub nodes)
  expr = collectSumTerms(expr);

  return expr;
}

}  // namespace simplify_detail

// ─── matchInfo — structural pattern matching ─────────────────────────────────
//
// Parallel walk of pattern and expr trees:
//   - PatternVar → bind expr to the variable's key
//   - Template specialization → check template identity, recurse args, merge
//   - Non-template → exact equality
//
// Non-linear patterns (same var twice) work via merge consistency check.

consteval MatchResult matchInfo(std::meta::info pattern, std::meta::info expr) {
  using namespace simplify_detail;

  // PatternVar matches anything
  if (isPatternVar(pattern)) {
    auto key = patternVarKey(pattern);
    return {true, {{key, expr}}};
  }

  // Non-template: operators, NTTP values, tags — exact equality
  if (!std::meta::has_template_arguments(pattern)) {
    return {pattern == expr, {}};
  }

  // Pattern is template specialization but expr isn't — structural mismatch
  if (!std::meta::has_template_arguments(expr)) {
    return {false, {}};
  }

  // Different templates
  if (std::meta::template_of(pattern) != std::meta::template_of(expr)) {
    return {false, {}};
  }

  auto pattern_args = std::meta::template_arguments_of(pattern);
  auto expr_args = std::meta::template_arguments_of(expr);

  if (pattern_args.size() != expr_args.size()) {
    return {false, {}};
  }

  // Recurse on all arguments, accumulating and merging bindings
  MatchResult result{true, {}};
  for (std::size_t i = 0; i < pattern_args.size(); ++i) {
    auto child = matchInfo(pattern_args[i], expr_args[i]);
    if (!child.success) return {false, {}};

    // Merge with non-linear consistency check
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

// ─── substituteInfo — replace PatternVars with bound values ──────────────────
//
// Walks the replacement tree, replacing each PatternVar<Tag> with its bound
// value and reconstructing template specializations via substitute().

consteval std::meta::info substituteInfo(
    std::meta::info replacement,
    const std::vector<std::pair<std::meta::info, std::meta::info>>& bindings) {
  using namespace simplify_detail;

  if (isPatternVar(replacement)) {
    auto key = patternVarKey(replacement);
    auto [found, val] = findBinding(bindings, key);
    return val;  // always found — pattern was already matched
  }

  // Non-template: pass through unchanged
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

// ─── tryRulesInfo — first matching rule wins ─────────────────────────────────

consteval std::meta::info tryRulesInfo(
    const std::vector<InfoRule>& rules,
    std::meta::info expr) {
  for (const auto& rule : rules) {
    auto result = matchInfo(rule.pattern, expr);
    if (result.success) {
      return substituteInfo(rule.replacement, result.bindings);
    }
  }
  return expr;
}

// ─── innermostInfo — bottom-up fixpoint rewriting ────────────────────────────
//
// Normalize children first (bottom-up), then try rules at the current node.
// If a rule fires, re-enter on the result (fixpoint). Simplified from
// symbolic4: only App has expression children (skip arg[0] which is the op).

constexpr int kMaxSimplifyDepth = 256;

consteval std::meta::info innermostInfo(
    std::meta::info expr,
    const std::vector<InfoRule>& rules,
    int depth = 0) {
  if (depth >= kMaxSimplifyDepth) return expr;

  // Step 1: normalize children (bottom-up)
  auto normalized = expr;
  if (std::meta::has_template_arguments(expr)) {
    auto tmpl = std::meta::template_of(expr);
    auto args = std::meta::template_arguments_of(expr);

    // Only App nodes have expression children — arg[0] is the op, skip it
    if (tmpl == ^^App) {
      bool any_changed = false;
      std::vector<std::meta::info> new_args;
      new_args.reserve(args.size());
      new_args.push_back(args[0]);  // op — don't recurse
      for (std::size_t i = 1; i < args.size(); ++i) {
        auto simplified = innermostInfo(args[i], rules, depth + 1);
        new_args.push_back(simplified);
        if (simplified != args[i]) any_changed = true;
      }
      if (any_changed) {
        normalized = std::meta::substitute(^^App, new_args);
      }
    }
  }

  // Step 2: try rules at this node
  auto result = tryRulesInfo(rules, normalized);

  // Step 3: fixpoint — if a rule fired, re-enter
  if (result != normalized) {
    return innermostInfo(result, rules, depth + 1);
  }

  return normalized;
}

// ─── Constant normalization ──────────────────────────────────────────────────
//
// Canonicalizes integer zero/one constants to their double equivalents so the
// simplification rules only need one form. Runs bottom-up over the tree before
// rule application.
//
// Separated from simplification because it's a type-level canonicalization
// concern, not an algebraic identity.

consteval std::meta::info normalizeConstantsInfo(std::meta::info expr) {
  // Leaf rewrites: Constant<0> → Constant<0.0>, Constant<1> → Constant<1.0>
  if (expr == ^^Constant<0>) return ^^Constant<0.0>;
  if (expr == ^^Constant<1>) return ^^Constant<1.0>;

  if (!std::meta::has_template_arguments(expr)) return expr;
  auto tmpl = std::meta::template_of(expr);
  if (tmpl != ^^App) return expr;

  // Recurse into children (skip arg[0] — the op)
  auto args = std::meta::template_arguments_of(expr);
  bool any_changed = false;
  std::vector<std::meta::info> new_args;
  new_args.reserve(args.size());
  new_args.push_back(args[0]);
  for (std::size_t i = 1; i < args.size(); ++i) {
    auto n = normalizeConstantsInfo(args[i]);
    new_args.push_back(n);
    if (n != args[i]) any_changed = true;
  }

  return any_changed ? std::meta::substitute(^^App, new_args) : expr;
}

// ─── Rule set ────────────────────────────────────────────────────────────────
//
// Pattern variable tags — namespace-scope for stable reflection.
namespace simplify_detail {
struct PVx {};
struct PVy {};
struct PVz {};
}  // namespace simplify_detail

consteval std::vector<InfoRule> makeSimplifyRules() {
  using simplify_detail::binary;
  using simplify_detail::unary;

  auto x_ = ^^PatternVar<simplify_detail::PVx>;
  auto y_ = ^^PatternVar<simplify_detail::PVy>;
  auto z_ = ^^PatternVar<simplify_detail::PVz>;

  auto zero    = ^^Constant<0.0>;
  auto one     = ^^Constant<1.0>;
  auto neg_one = ^^Constant<-1.0>;

  auto add = ^^AddOp;
  auto sub = ^^SubOp;
  auto mul = ^^MulOp;
  auto div = ^^DivOp;
  auto neg = ^^NegOp;

  auto pi_node = std::meta::substitute(^^App, {^^PiOp});
  auto e_node  = std::meta::substitute(^^App, {^^EulerOp});

  // Rules assume normalizeConstantsInfo has already canonicalized int→double.
  return {
    // ── Addition identity ──
    {binary(add, x_, zero), x_},                // x + 0 → x
    {binary(add, zero, x_), x_},                // 0 + x → x

    // ── Subtraction ──
    {binary(sub, x_, zero), x_},                // x - 0 → x
    {binary(sub, x_, x_), zero},                // x - x → 0

    // ── Multiplication identity ──
    {binary(mul, one, x_), x_},                 // 1 * x → x
    {binary(mul, x_, one), x_},                 // x * 1 → x

    // ── Multiplication annihilation ──
    {binary(mul, zero, x_), zero},              // 0 * x → 0
    {binary(mul, x_, zero), zero},              // x * 0 → 0

    // ── Division ──
    {binary(div, x_, one), x_},                 // x / 1 → x
    {binary(div, zero, x_), zero},              // 0 / x → 0

    // ── Negation ──
    {unary(neg, zero), zero},                   // -(0) → 0
    {unary(neg, unary(neg, x_)), x_},           // -(-x) → x

    // ── Self-cancellation ──
    {binary(div, x_, x_), one},                 // x / x → 1

    // ── Inverse function pairs ──
    {unary(^^ExpOp, unary(^^LogOp, x_)), x_},  // exp(log(x)) → x
    {unary(^^LogOp, unary(^^ExpOp, x_)), x_},  // log(exp(x)) → x

    // ── Transcendental identities ──
    {unary(^^SinOp, zero), zero},               // sin(0) → 0
    {unary(^^CosOp, zero), one},                // cos(0) → 1
    {unary(^^SinOp, pi_node), zero},            // sin(π) → 0
    {unary(^^CosOp, pi_node), neg_one},         // cos(π) → -1
    {unary(^^ExpOp, zero), one},                // exp(0) → 1
    {unary(^^LogOp, one), zero},                // log(1) → 0
    {unary(^^LogOp, e_node), one},              // log(e) → 1

    // ── Power ──
    {binary(^^PowOp, x_, zero), one},           // pow(x, 0) → 1
    {binary(^^PowOp, x_, one), x_},             // pow(x, 1) → x

    // ── Coefficient collection ──
    // General form first — specific (bare term) cases fall through to x+x.
    // Non-linear x_ ensures both sides share the same subtree.
    // Constant folding collapses the coefficient sum afterward.
    {binary(add, binary(mul, y_, x_), binary(mul, z_, x_)),    // c*x + d*x → (c+d)*x
     binary(mul, binary(add, y_, z_), x_)},
    {binary(add, x_, binary(mul, y_, x_)),                     // x + c*x → (1+c)*x
     binary(mul, binary(add, one, y_), x_)},
    {binary(add, binary(mul, y_, x_), x_),                     // c*x + x → (c+1)*x
     binary(mul, binary(add, y_, one), x_)},
    {binary(add, x_, x_), binary(mul, ^^Constant<2.0>, x_)},  // x + x → 2*x

    // ── Tan at known values ──
    {unary(^^TanOp, zero), zero},               // tan(0) → 0
    {unary(^^TanOp, pi_node), zero},            // tan(π) → 0

    // ── Sqrt ──
    {unary(^^SqrtOp, zero), zero},              // sqrt(0) → 0
    {unary(^^SqrtOp, one), one},                // sqrt(1) → 1

    // ── Abs ──
    {unary(^^AbsOp, zero), zero},               // abs(0) → 0
    {unary(^^AbsOp, unary(^^AbsOp, x_)), unary(^^AbsOp, x_)},  // abs(abs(x)) → abs(x)

    // ── Cleanup (canonicalization artifacts) ──
    {binary(mul, neg_one, x_), unary(neg, x_)},  // (-1) * x → -x
  };
}

// ─── Constant folding (tree pass) ────────────────────────────────────────────
//
// Bottom-up tree pass that evaluates App<ArithOp, Constant<A>, Constant<B>> →
// Constant<A op B>. Uses foldBinary/foldUnary (defined in simplify_detail above).

consteval std::meta::info foldConstantsInfo(std::meta::info expr) {
  if (!std::meta::has_template_arguments(expr)) return expr;
  auto tmpl = std::meta::template_of(expr);
  if (tmpl != ^^App) return expr;

  // Extract Constant's template identity from a known specialization.
  // Bare ^^Constant doesn't work for NTTP templates in this compiler.
  auto constant_tmpl = std::meta::template_of(^^Constant<0.0>);

  // Bottom-up: fold children first
  auto args = std::meta::template_arguments_of(expr);
  bool any_changed = false;
  std::vector<std::meta::info> new_args;
  new_args.reserve(args.size());
  new_args.push_back(args[0]);  // op
  for (std::size_t i = 1; i < args.size(); ++i) {
    auto folded = foldConstantsInfo(args[i]);
    new_args.push_back(folded);
    if (folded != args[i]) any_changed = true;
  }
  if (any_changed) {
    expr = std::meta::substitute(^^App, new_args);
    args = new_args;
  }

  auto op = args[0];
  auto arity = args.size() - 1;

  auto isConstant = [&](std::meta::info node) {
    return std::meta::has_template_arguments(node) &&
           std::meta::template_of(node) == constant_tmpl;
  };

  // Binary: App<ArithOp, Constant<A>, Constant<B>> → Constant<A op B>
  if (arity == 2 && simplify_detail::isFoldableArithBinary(op)) {
    auto lhs = args[1], rhs = args[2];
    if (isConstant(lhs) && isConstant(rhs)) {
      auto a = std::meta::template_arguments_of(lhs)[0];
      auto b = std::meta::template_arguments_of(rhs)[0];
      return simplify_detail::foldBinary(op, a, b);
    }
  }

  // Unary: App<NegOp, Constant<A>> → Constant<-A>
  if (arity == 1 && op == ^^NegOp) {
    auto operand = args[1];
    if (isConstant(operand)) {
      auto a = std::meta::template_arguments_of(operand)[0];
      return simplify_detail::foldUnary(op, a);
    }
  }

  return expr;
}

// ─── Public API ──────────────────────────────────────────────────────────────

consteval std::meta::info simplifyInfo(std::meta::info expr) {
  auto normalized = normalizeConstantsInfo(expr);
  auto rules = makeSimplifyRules();
  auto simplified = innermostInfo(normalized, rules);
  auto folded = foldConstantsInfo(simplified);
  auto resimplified = (folded != simplified) ? innermostInfo(folded, rules) : folded;

  // Structural canonicalization (product first, then sum)
  auto canonical = simplify_detail::canonicalizeInfo(resimplified);

  // Re-fold and re-simplify if canonicalization changed the tree
  if (canonical != resimplified) {
    auto refolded = foldConstantsInfo(canonical);
    return innermostInfo(refolded, rules);
  }
  return canonical;
}

consteval Expr simplify(Symbolic auto e) {
  return Expr{
    .tree = simplifyInfo(toInfo(e)),
    .meta = metaOf(e)
  };
}

}  // namespace tempura
