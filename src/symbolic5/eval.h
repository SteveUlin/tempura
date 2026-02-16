#pragma once
#include "symbolic5/expr.h"

namespace tempura {

// ─── Type-dispatched evaluation ──────────────────────────────────────────────
//
// Partial specializations of EvalImpl handle each node kind. The App
// specialization uses pack expansion (Children...) to evaluate any arity
// — no hardcoded arity limits, no mutual recursion.
//
// Return types are fully deduced: Constant<V> returns decltype(V),
// Symbol<Tag> returns whatever the binding provides, and App<Op, ...>
// returns whatever Op::apply produces. This lets the same eval machinery
// handle doubles, ints, matrices, spans — anything the operators support.

namespace eval_detail {

template <typename Tree, typename Ctx>
struct EvalImpl;

// Constant<V> → return the compile-time value in its native type
template <auto V, typename Ctx>
struct EvalImpl<Constant<V>, Ctx> {
  static constexpr auto call(const Ctx&) { return V; }
};

// Symbol<Tag> → look up in the BinderPack via overload resolution
template <typename Tag, typename Ctx>
struct EvalImpl<Symbol<Tag>, Ctx> {
  static constexpr auto call(const Ctx& ctx) { return ctx[Symbol<Tag>{}]; }
};

// App<Op, Children...> → evaluate children, apply Op
//
// Pack expansion does the recursion: EvalImpl<Children, Ctx>::call(ctx)...
// expands to one call per child. Op::apply receives the evaluated values
// and its return type propagates back up.
template <typename Op, typename... Children, typename Ctx>
struct EvalImpl<App<Op, Children...>, Ctx> {
  static constexpr auto call(const Ctx& ctx) {
    return Op::apply(EvalImpl<Children, Ctx>::call(ctx)...);
  }
};

}  // namespace eval_detail

// ─── Public API ──────────────────────────────────────────────────────────────
//
// The splice boundary: [:tree:] converts the info-domain tree into a
// type-level AST. This instantiation happens once per unique expression.
// The evaluator compiles to straight-line arithmetic — no virtual dispatch,
// no runtime tree walking.
//
// Return type is deduced from the expression and bindings:
//   eval<f>(x = 3.0)              → double (if all ops return double)
//   eval<f>(x = some_matrix)      → whatever the ops produce with matrices
//
// Two entry points:
//   eval<f>(bindings...)           — takes Expr as NTTP
//   evalTree<f.tree>(bindings...)  — takes raw std::meta::info (fallback)

template <Expr E>
constexpr auto eval(auto... bindings) {
  using Tree = [:E.tree:];
  auto ctx = BinderPack{bindings...};
  return eval_detail::EvalImpl<Tree, decltype(ctx)>::call(ctx);
}

template <std::meta::info tree>
constexpr auto evalTree(auto... bindings) {
  using Tree = [:tree:];
  auto ctx = BinderPack{bindings...};
  return eval_detail::EvalImpl<Tree, decltype(ctx)>::call(ctx);
}

}  // namespace tempura
