#pragma once
#include "symbolic5/math.h"
#include <string>

namespace tempura {

// ─── Expression → string ────────────────────────────────────────────────────
//
// Splice-and-dispatch, same pattern as eval. The tree is spliced to a type-
// level AST at the call site, then partial specializations walk it to build
// a string. Handles operator precedence for minimal parenthesization:
//
//   x * x + sin(x)     not   ((x * x) + sin(x))
//   x - (y - z)        not   x - y - z  (right-assoc needs parens)
//
// Named symbols display their name. Anonymous symbols display "?".
//
// Ops carry their own notation (InfixNotation, PrefixNotation, FunctionNotation)
// so three generic Repr specializations replace the old per-op boilerplate.

namespace to_string_detail {

// Precedence: higher binds tighter. Plain int — matches notation descriptors.
constexpr int kAtomPrec = 100;

struct Fragment {
  std::string text;
  int prec;
};

// Parenthesize when child precedence is too low for its context.
inline std::string wrap(const Fragment& f, int min_prec,
                        bool right_wraps = false) {
  bool needs = (f.prec < min_prec) ||
               (right_wraps && f.prec == min_prec);
  return needs ? "(" + f.text + ")" : f.text;
}

template <typename Tree>
struct Repr;

// ─── Leaves ─────────────────────────────────────────────────────────────────

// Anonymous symbol — no name available
template <typename Tag>
struct Repr<Symbol<Tag>> {
  static Fragment call() { return {"?", kAtomPrec}; }
};

// Named symbol — extract display name
template <typename Sym, FixedString Name>
struct Repr<Named<Sym, Name>> {
  static Fragment call() {
    return {std::string(std::string_view(Name)), kAtomPrec};
  }
};

template <auto V>
struct Repr<Constant<V>> {
  static Fragment call() {
    std::string s = std::to_string(V);
    // Clean trailing zeros from floating-point representation
    if constexpr (std::is_floating_point_v<decltype(V)>) {
      auto dot = s.find('.');
      if (dot != std::string::npos) {
        auto last = s.find_last_not_of('0');
        if (last == dot) ++last;  // keep at least "5.0" not "5."
        s.erase(last + 1);
      }
    }
    if constexpr (std::is_signed_v<decltype(V)> && V < 0) {
      return {"(" + s + ")", kAtomPrec};
    }
    return {s, kAtomPrec};
  }
};

// ─── Generic infix: dispatches on Op::notation ──────────────────────────────

template <InfixOp Op, typename L, typename R>
struct Repr<App<Op, L, R>> {
  static Fragment call() {
    constexpr auto n = Op::notation;
    return {wrap(Repr<L>::call(), n.precedence) +
            std::string(n.symbol) +
            wrap(Repr<R>::call(), n.precedence, n.right_wraps),
            n.precedence};
  }
};

// ─── Generic prefix: dispatches on Op::notation ─────────────────────────────

template <PrefixOp Op, typename Child>
struct Repr<App<Op, Child>> {
  static Fragment call() {
    constexpr auto n = Op::notation;
    return {std::string(n.symbol) + wrap(Repr<Child>::call(), n.precedence),
            n.precedence};
  }
};

// ─── Generic constant: zero-arg op with ConstantNotation ─────────────────────

template <ConstantOp Op>
struct Repr<App<Op>> {
  static Fragment call() {
    return {std::string(Op::notation.symbol), kAtomPrec};
  }
};

// ─── Generic function call: dispatches on Op::notation ──────────────────────

template <FunctionOp Op, typename Child>
struct Repr<App<Op, Child>> {
  static Fragment call() {
    return {std::string(Op::notation.name) + "(" +
            Repr<Child>::call().text + ")",
            kAtomPrec};
  }
};

// Binary function call (e.g. pow)
template <FunctionOp Op, typename L, typename R>
struct Repr<App<Op, L, R>> {
  static Fragment call() {
    return {std::string(Op::notation.name) + "(" +
            Repr<L>::call().text + ", " +
            Repr<R>::call().text + ")",
            kAtomPrec};
  }
};

// ─── Annotation → display tree rewrite ──────────────────────────────────────
//
// Walks the expression tree, replacing bare Symbol<Tag> nodes with
// Named<Symbol<Tag>, Name> when a matching annotation exists in meta.
// This is the only place Named enters the tree — purely for display.

consteval std::meta::info injectNames(std::meta::info tree, std::meta::info meta) {
  if (!std::meta::has_template_arguments(tree)) return tree;

  auto tmpl = std::meta::template_of(tree);

  // Symbol<Tag> → search meta for a Named<Symbol<Tag>, Name> entry
  if (tmpl == ^^Symbol) {
    if (meta != ^^EmptyAnnotations) {
      auto entries = std::meta::template_arguments_of(meta);
      for (auto entry : entries) {
        if (std::meta::has_template_arguments(entry) &&
            std::meta::template_of(entry) == ^^Named) {
          auto named_args = std::meta::template_arguments_of(entry);
          if (named_args[0] == tree) return entry;
        }
      }
    }
    return tree;
  }

  // Recurse into App children
  if (tmpl == ^^App) {
    auto args = std::meta::template_arguments_of(tree);
    std::vector<std::meta::info> new_args;
    new_args.push_back(args[0]);
    bool changed = false;
    for (std::size_t i = 1; i < args.size(); ++i) {
      auto rewritten = injectNames(args[i], meta);
      new_args.push_back(rewritten);
      if (rewritten != args[i]) changed = true;
    }
    return changed ? std::meta::substitute(^^App, new_args) : tree;
  }

  return tree;
}

}  // namespace to_string_detail

// ─── Public API ─────────────────────────────────────────────────────────────

template <Expr E>
std::string toString() {
  // Merge name annotations into the tree for display, then dispatch
  constexpr auto display_tree = to_string_detail::injectNames(E.tree, E.meta);
  using Tree = [:display_tree:];
  return to_string_detail::Repr<Tree>::call().text;
}

}  // namespace tempura
