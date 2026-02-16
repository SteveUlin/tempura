#pragma once
#include "symbolic5/math.h"

namespace tempura {

// ─── Info-domain helpers ─────────────────────────────────────────────────────

namespace diff_detail {

// ─── Trampolines ──────────────────────────────────────────────────────────
//
// substitute creates e.g. BinaryDiffCall<MulOp, F, DF, G, DG> as info.
// Static initialization of `value` calls Op::diff() — the derivative rule
// runs inside the compiler, and extract/constant_of retrieves the resulting info.

template <typename Op, Expr F, Expr DF, Expr G, Expr DG>
struct BinaryDiffCall {
  static constexpr std::meta::info value = Op::diff(F, DF, G, DG).tree;
};

template <typename Op, Expr U, Expr DU>
struct UnaryDiffCall {
  static constexpr std::meta::info value = Op::diff(U, DU).tree;
};

consteval std::meta::info dispatchBinaryDiff(
    std::meta::info op,
    std::meta::info f, std::meta::info df,
    std::meta::info g, std::meta::info dg) {
  auto result = std::meta::substitute(^^BinaryDiffCall, {
    op,
    std::meta::reflect_constant(Expr{.tree = f}),
    std::meta::reflect_constant(Expr{.tree = df}),
    std::meta::reflect_constant(Expr{.tree = g}),
    std::meta::reflect_constant(Expr{.tree = dg})
  });
  return std::meta::extract<std::meta::info>(std::meta::constant_of(
    std::meta::static_data_members_of(result, std::meta::access_context::unchecked())[0]));
}

consteval std::meta::info dispatchUnaryDiff(
    std::meta::info op,
    std::meta::info u, std::meta::info du) {
  auto result = std::meta::substitute(^^UnaryDiffCall, {
    op,
    std::meta::reflect_constant(Expr{.tree = u}),
    std::meta::reflect_constant(Expr{.tree = du})
  });
  return std::meta::extract<std::meta::info>(std::meta::constant_of(
    std::meta::static_data_members_of(result, std::meta::access_context::unchecked())[0]));
}

}  // namespace diff_detail

// ─── Info-domain differentiation ─────────────────────────────────────────────
//
// Recursive consteval function operating entirely on std::meta::info values.
// No intermediate type instantiations — the full derivative tree is built
// as info, only spliced to types at the eval boundary.
//
// Derivative rules live on Op structs (Op::diff). Dispatch uses substitute
// to create a trampoline specialization as info, avoiding any if-chain.

consteval std::meta::info diffInfo(std::meta::info expr, std::meta::info var) {
  using namespace diff_detail;

  auto zero = ^^Constant<0.0>;
  auto one = ^^Constant<1.0>;

  // Non-template → bare type, treat as constant
  if (!std::meta::has_template_arguments(expr)) return zero;

  auto tmpl = std::meta::template_of(expr);

  // Constant<V> → 0
  if (tmpl == ^^Constant) return zero;

  // Symbol<Tag> → 1 if same as var, 0 otherwise
  if (tmpl == ^^Symbol) {
    return (expr == var) ? one : zero;
  }

  // App<Op, Children...> — dispatch to Op::diff() via trampoline
  if (tmpl == ^^App) {
    auto args = std::meta::template_arguments_of(expr);
    auto op = args[0];
    auto arity = args.size() - 1;

    if (arity == 2) {
      auto f = args[1], g = args[2];
      auto df = diffInfo(f, var), dg = diffInfo(g, var);
      return dispatchBinaryDiff(op, f, df, g, dg);
    }

    if (arity == 1) {
      auto u = args[1];
      auto du = diffInfo(u, var);
      return dispatchUnaryDiff(op, u, du);
    }
  }

  return zero;
}

// ─── Public API ──────────────────────────────────────────────────────────────

consteval Expr diff(Symbolic auto e, Symbolic auto var) {
  return Expr{
    .tree = diffInfo(toInfo(e), toInfo(var)),
    .meta = metaOf(e)
  };
}

}  // namespace tempura
