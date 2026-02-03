# Phase 3: Kill Cata — Replace with Direct Recursion

## Goal

Replace all cata/fold/para-based interpreters with simple direct-recursion
functions, then delete the scheme/ directory entirely.

## Prerequisites

Phase 2 complete. After Phase 2, the only remaining cata users are:
- `eval.h` — uses `fold<Eval<Bindings>>`
- `to_string.h` — uses `fold<ToString<Pack>>`
- `simplify.h` — uses strategies already, but `partialEval` in `partial_eval.h` may use fold

Check: `grep -r "fold<\|para<\|cata<" src/symbolic4/` to find ALL remaining users.

## Context

The cata apparatus (`scheme/cata.h`) provides:
1. Terminal dispatch via `I::terminal(T, ctx)`
2. Recursive descent via `detail::cataExprResult`
3. Child combination via `I::combine<Op>(ctx, children...)`

But for evaluation, `combine<Op>` is just `Op{}(children...)` — the abstraction
buys nothing. Direct recursion with `if constexpr` is shorter and handles
ReduceOver natively.

## Task 3.1: Rewrite eval.h as direct recursion

**File to modify:** `src/symbolic4/interpreter/eval.h`

Replace the `Eval` interpreter struct + `fold<Eval<...>>` with:

```cpp
namespace tempura::symbolic4 {

namespace eval_detail {

template <typename Support>
constexpr auto applyConstraint(double z) -> double {
  if constexpr (is_positive_support_v<Support>) {
    return std::exp(z);
  } else if constexpr (is_unit_support_v<Support>) {
    return 1.0 / (1.0 + std::exp(-z));
  } else {
    return z;
  }
}

// Direct recursive evaluation
template <Symbolic E, typename Bindings>
constexpr auto evalImpl(E expr, Bindings& ctx) -> double {
  if constexpr (is_reduce_over_v<E>) {
    // ReduceOver: loop with ReduceOp::identity/combine
    // NOTE: This needs dimension size from context. For the non-indexed
    // evaluator, ReduceOver shouldn't appear. Assert or delegate to
    // indexed eval path.
    static_assert(!is_reduce_over_v<E>,
        "ReduceOver in non-indexed eval — use evaluateIndexed()");
  } else if constexpr (is_constant_v<E>) {
    return static_cast<double>(E::value);
  } else if constexpr (is_fraction_v<E>) {
    return E::value;
  } else if constexpr (is_literal_v<E>) {
    return static_cast<double>(expr.effect_.value);
  } else if constexpr (is_random_var_atom_v<E>) {
    using FreeSymbol = Atom<get_id_t<E>, Free>;
    double z = ctx[FreeSymbol{}];
    using Dist = get_distribution_t<typename E::effect_type>;
    using Support = typename Dist::support_type;
    return applyConstraint<Support>(z);
  } else if constexpr (is_atom_v<E>) {
    return ctx[expr];
  } else if constexpr (is_expression_v<E>) {
    return evalExpression(expr, ctx, MakeIndexSequence<E::arity>{});
  } else {
    static_assert(is_atom_v<E>, "Unknown symbolic type");
    return 0.0;
  }
}

template <typename Op, Symbolic... Args, typename Bindings, SizeT... Is>
constexpr auto evalExpression(Expression<Op, Args...> expr, Bindings& ctx,
                               IndexSequence<Is...>) -> double {
  return Op{}(evalImpl(expr.template arg<Is>(), ctx)...);
}

}  // namespace eval_detail

template <Symbolic E, typename... Binders>
constexpr auto evaluate(E expr, BinderPack<Binders...> bindings) -> double {
  return eval_detail::evalImpl(expr, bindings);
}

template <Symbolic E, typename... Args>
constexpr auto evaluate(E expr, Args... binders) -> double {
  return evaluate(expr, BinderPack{binders...});
}

}  // namespace tempura::symbolic4
```

No more `#include "symbolic4/scheme/cata.h"`.

## Task 3.2: Rewrite indexed_eval.h as direct recursion

**File to modify:** `src/symbolic4/indexed/indexed_eval.h`

The indexed evaluator already IS direct recursion (`indexedFold` is a hand-written
recursive function). The changes:

1. Replace `is_sum_over_v` checks with `is_reduce_over_v`
2. Use `ReduceOp::identity()` and `ReduceOp::combine()` instead of hardcoded `0.0` and `+=`
3. Remove the `#include "symbolic4/scheme/cata.h"` if present
4. Replace `DimIndexMap` (runtime `std::unordered_map<std::type_index, SizeT>`) with a
   compile-time typed approach (optional — can defer to Phase 5)

The ReduceOver evaluation loop:

```cpp
if constexpr (is_reduce_over_v<E>) {
  using ROp = typename E::reduce_op;
  using DimTag = typename E::dim_tag;
  auto size = getDimensionSize<DimTag>(ctx.indexed);

  double accum = ROp::identity();
  for (SizeT i = 0; i < size; ++i) {
    ctx.dim_indices.template set<DimTag>(i, size);
    accum = ROp::combine(accum, indexedEval(expr.expr(), ctx));
  }
  return accum;
}
```

## Task 3.3: Rewrite to_string.h as direct recursion

**File to modify:** `src/symbolic4/interpreter/to_string.h`

Replace the `ToString` interpreter struct + `fold<ToString<Pack>>` with direct recursion.
Also add ReduceOver rendering:

```cpp
template <Symbolic E, typename Bindings>
auto toStringImpl(E expr, Bindings& ctx, int parent_prec) -> StringResult {
  if constexpr (is_reduce_over_v<E>) {
    using ROp = typename E::reduce_op;
    auto body = toStringImpl(expr.expr(), ctx, 0);
    return {std::format("{}({})", ROp::symbol(), body.str), Precedence::kUnary};
  } else if constexpr (is_terminal_v<E>) {
    return toStringTerminal(expr, ctx);
  } else {
    return toStringExpression(expr, ctx);
  }
}
```

This also fixes the bug where SumOver renders as "?" — ReduceOver now gets
proper rendering via `ROp::symbol()`.

## Task 3.4: Simplify collectLogProbs and discoverParams

**Files to modify:**
- `src/symbolic4/distributions/collect_log_prob.h`
- `src/symbolic4/distributions/discover_params.h`

These already use hand-written recursion. The changes:
1. Replace `is_sum_over_v` with `is_reduce_over_v`
2. Replace the hardcoded arity-0/1/2/3 `collectFromChildren` overloads with variadic
3. Remove any `#include "symbolic4/scheme/..."` includes

The `collectFromChildren` variadic version:

```cpp
template <typename Visited, typename Op, Symbolic... Children>
constexpr auto collectFromChildren(Visited visited, Op, Children... children) {
  // Fold-expression threading visited set through each child
  auto result = Constant<0>{};
  auto current_visited = visited;
  auto process = [&](auto child) {
    auto [r, v] = collectFromExprImpl(current_visited, child);
    result = addNonZero(result, r);
    current_visited = v;
  };
  (process(children), ...);
  return std::pair{result, current_visited};
}
```

NOTE: This won't work directly because `visited` changes type with each insertion
(it's a compile-time `IdSet<...>`). The threading pattern needs recursive template
instantiation, similar to the current approach but variadic. Consider using
a helper that processes one child and recurses for the rest:

```cpp
template <typename Visited, typename Accum>
constexpr auto collectFromChildrenImpl(Visited v, Accum a) {
  return std::pair{a, v};
}

template <typename Visited, typename Accum, typename C0, typename... Rest>
constexpr auto collectFromChildrenImpl(Visited v, Accum a, C0 c0, Rest... rest) {
  auto [r, v2] = collectFromExprImpl(v, c0);
  return collectFromChildrenImpl(v2, addNonZero(a, r), rest...);
}
```

## Task 3.5: Update partial_eval.h

**File to check:** `src/symbolic4/interpreter/partial_eval.h`

Check if this uses cata/fold. If so, rewrite as direct recursion or a strategy.
Partial evaluation is "evaluate ground subexpressions to constants" which
could be expressed as an innermost strategy rule:

```cpp
// If all children are constants/literals, evaluate and replace with literal
rule(groundExpr, lit(evaluate(groundExpr)))
```

Or keep as direct recursion — it's a simple bottom-up pass.

## Task 3.6: Delete scheme/ directory

**Files to delete:**
- `src/symbolic4/scheme/cata.h`
- `src/symbolic4/scheme/fold.h` (if separate from cata.h)
- `src/symbolic4/scheme/para.h` (if separate from cata.h)
- `src/symbolic4/scheme/fold_unique.h`
- `src/symbolic4/scheme/transform.h`
- `src/symbolic4/scheme/CLAUDE.md`
- The entire `src/symbolic4/scheme/` directory

**But first check:** `fold_unique.h` provides `LetNode`-aware DAG folding
with `IdSet` deduplication. Check if anything outside scheme/ depends on it.
The `IdSet` itself and `LetNode` are defined in `let.h` (outside scheme/),
so they survive. But `foldUnique()` in `fold_unique.h` may be used by
`collect_log_prob.h`. If so, either inline the logic or keep the function
as a standalone utility outside scheme/.

Run `grep -r "foldUnique\|fold_unique" src/symbolic4/` to check usage.

Also update `src/symbolic4/symbolic4.h` to remove any `#include "scheme/..."`.

## Acceptance Criteria

- [ ] All `symbolic4` tests pass
- [ ] No file includes anything from `scheme/`
- [ ] `scheme/` directory is deleted
- [ ] `eval.h` uses direct recursion, no Interpreter pattern
- [ ] `to_string.h` uses direct recursion, renders ReduceOver with proper symbols
- [ ] `indexed_eval.h` uses `is_reduce_over_v` and `ROp::combine()`
- [ ] `collectLogProbs` handles ReduceOver generically
- [ ] No more hardcoded arity-0/1/2/3 overloads in collect_log_prob.h

## Build & Test

```bash
cmake --build build && ctest --test-dir build -R symbolic4
```
