#pragma once

#include <algorithm>
#include <experimental/meta>
#include <vector>

#include "symbolic4/core.h"
#include "symbolic4/indexed/reduce_over.h"
#include "symbolic4/operators.h"
#include "symbolic4/strategy/info_simplify.h"

// ============================================================================
// info_canonicalize.h — Info-domain canonical ordering for commutative ops
// ============================================================================
//
// Mirrors canonicalize.h but operates on std::meta::info values. Reorders
// children of commutative binary ops (Add, Mul) so the "smaller" child
// comes first, using a total order on expression infos.
//
// No intermediate C++ types are instantiated — only the final spliced result.
//
// Key functions:
//   infoCategory(info)         → int   (Expression < Atom < Fraction < Constant)
//   infoOpIndex(info)          → int   (operator precedence)
//   infoCompare(info, info)    → int   (total order: <0, 0, >0)
//   canonicalizeInfo(info)     → info  (bottom-up canonical reordering)
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Expression category — mirrors ordering.h's Expression < Atom < Fraction < Constant
// ============================================================================

consteval auto infoCategory(std::meta::info i) -> int {
  if (!std::meta::has_template_arguments(i)) return -1;
  auto tmpl = std::meta::template_of(i);
  if (tmpl == ^^Expression) return 0;
  if (tmpl == ^^Atom) return 1;
  if (tmpl == ^^Fraction) return 2;
  if (tmpl == ^^Constant) return 3;
  return -1;
}

// ============================================================================
// Operator precedence — mirrors detail::opIndex in ordering.h
// ============================================================================

consteval auto infoOpIndex(std::meta::info op) -> int {
  if (op == ^^::tempura::AddOp) return 0;
  if (op == ^^::tempura::SubOp) return 1;
  if (op == ^^::tempura::MulOp) return 2;
  if (op == ^^::tempura::DivOp) return 3;
  if (op == ^^::tempura::NegOp) return 4;
  if (op == ^^::tempura::PowOp) return 5;
  if (op == ^^::tempura::SqrtOp) return 10;
  if (op == ^^::tempura::ExpOp) return 11;
  if (op == ^^::tempura::LogOp) return 12;
  if (op == ^^::tempura::SinOp) return 20;
  if (op == ^^::tempura::CosOp) return 21;
  if (op == ^^::tempura::TanOp) return 22;
  if (op == ^^::tempura::AsinOp) return 23;
  if (op == ^^::tempura::AcosOp) return 24;
  if (op == ^^::tempura::AtanOp) return 25;
  if (op == ^^::tempura::SinhOp) return 30;
  if (op == ^^::tempura::CoshOp) return 31;
  if (op == ^^::tempura::TanhOp) return 32;
  if (op == ^^::tempura::PiOp) return 40;
  if (op == ^^::tempura::EOp) return 41;
  // Unknown ops: use a high base + display_string_of for deterministic ordering
  return 100;
}

// ============================================================================
// Total order on expression infos
// ============================================================================
//
// Returns <0 if lhs < rhs, 0 if equal, >0 if lhs > rhs.
// Uses the same ordering hierarchy as ordering.h:
//   Expression < Atom < Fraction < Constant
// Within each category, uses structural comparison.
//
// For atoms and unknown sub-expressions, display_string_of provides a
// stable lexicographic tiebreaker (same technique ordering.h uses).

consteval auto infoCompare(std::meta::info lhs, std::meta::info rhs) -> int {
  // Identical infos are equal
  if (lhs == rhs) return 0;

  int l_cat = infoCategory(lhs);
  int r_cat = infoCategory(rhs);

  // Different categories: Expression < Atom < Fraction < Constant
  if (l_cat != r_cat) return l_cat - r_cat;

  // Both non-template (shouldn't normally occur as top-level)
  if (l_cat == -1) {
    auto l_name = std::meta::display_string_of(lhs);
    auto r_name = std::meta::display_string_of(rhs);
    if (l_name < r_name) return -1;
    if (l_name > r_name) return 1;
    return 0;
  }

  auto l_args = std::meta::template_arguments_of(lhs);
  auto r_args = std::meta::template_arguments_of(rhs);

  // Both Expressions: compare op first, then children lexicographically
  if (l_cat == 0) {
    int l_op = infoOpIndex(l_args[0]);
    int r_op = infoOpIndex(r_args[0]);
    if (l_op != r_op) return l_op - r_op;

    // Same op — compare children (args[1..])
    auto l_arity = l_args.size() - 1;
    auto r_arity = r_args.size() - 1;
    auto min_arity = l_arity < r_arity ? l_arity : r_arity;
    for (std::size_t i = 0; i < min_arity; ++i) {
      int child_cmp = infoCompare(l_args[i + 1], r_args[i + 1]);
      if (child_cmp != 0) return child_cmp;
    }
    // Shorter argument list comes first
    if (l_arity != r_arity) return static_cast<int>(l_arity) - static_cast<int>(r_arity);
    return 0;
  }

  // Both Atoms: compare by display_string_of for stable ordering
  if (l_cat == 1) {
    auto l_name = std::meta::display_string_of(lhs);
    auto r_name = std::meta::display_string_of(rhs);
    if (l_name < r_name) return -1;
    if (l_name > r_name) return 1;
    return 0;
  }

  // Both Fractions: compare by cross-multiplication
  // Fraction<N, D> — args are NTTP values
  if (l_cat == 2) {
    // display_string_of gives us the full type name for comparison
    auto l_name = std::meta::display_string_of(lhs);
    auto r_name = std::meta::display_string_of(rhs);
    if (l_name < r_name) return -1;
    if (l_name > r_name) return 1;
    return 0;
  }

  // Both Constants: compare by display_string_of
  if (l_cat == 3) {
    auto l_name = std::meta::display_string_of(lhs);
    auto r_name = std::meta::display_string_of(rhs);
    if (l_name < r_name) return -1;
    if (l_name > r_name) return 1;
    return 0;
  }

  return 0;
}

// ============================================================================
// canonicalizeInfo — Bottom-up canonical reordering in the info domain
// ============================================================================
//
// Single-pass bottom-up traversal:
//   1. Recursively canonicalize children
//   2. For binary Add/Mul: if infoCompare(rhs, lhs) < 0, swap
//   3. Handle ReduceOver: recurse into body
//   4. Return result (idempotent — no fixpoint loop needed)

consteval auto canonicalizeInfo(std::meta::info expr) -> std::meta::info {
  if (!std::meta::has_template_arguments(expr)) return expr;

  auto tmpl = std::meta::template_of(expr);

  // ReduceOver<ROp, DimTag, Body> — recurse into body (arg[2])
  if (tmpl == ^^ReduceOver) {
    auto args = std::meta::template_arguments_of(expr);
    auto body = canonicalizeInfo(args[2]);
    if (body == args[2]) return expr;
    return std::meta::substitute(^^ReduceOver, {args[0], args[1], body});
  }

  // Expression<Op, Args...>
  if (tmpl == ^^Expression) {
    auto args = std::meta::template_arguments_of(expr);
    auto arity = args.size() - 1;

    if (arity == 0) return expr;

    // Recursively canonicalize children
    std::vector<std::meta::info> new_args;
    new_args.reserve(args.size());
    new_args.push_back(args[0]);  // Op stays unchanged
    bool any_changed = false;
    for (std::size_t i = 1; i < args.size(); ++i) {
      auto child = canonicalizeInfo(args[i]);
      new_args.push_back(child);
      if (child != args[i]) any_changed = true;
    }

    // For binary commutative ops: reorder if rhs < lhs
    if (arity == 2) {
      auto op = new_args[0];
      bool is_commutative =
          (op == ^^::tempura::AddOp || op == ^^::tempura::MulOp);
      if (is_commutative && infoCompare(new_args[2], new_args[1]) < 0) {
        std::swap(new_args[1], new_args[2]);
        any_changed = true;
      }
    }

    if (!any_changed) return expr;
    return std::meta::substitute(^^Expression, new_args);
  }

  // Terminals (Atom, Constant, Fraction) and other templates: pass through
  return expr;
}

}  // namespace tempura::symbolic4
