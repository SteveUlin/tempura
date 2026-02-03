# Phase 4: New Capabilities

## Goal

Build the mathematical abstractions enabled by the clean foundation:
Grad<Expr> with typed rank via nesting, new reduction types, new
strategy combinators, and reduction-aware rewrite rules.

## Prerequisites

Phase 3 complete. Strategy system is the sole transform mechanism.
ReduceOver is understood everywhere. No cata remains.

## Task 4.1: Grad<Expr> — Typed Rank via Nesting

**File to create:** `src/symbolic4/grad.h`

### Design Principle

The rank of a gradient object is encoded in its **type**, not in how many
arguments you pass to `operator[]`. Each `grad()` call adds exactly one
covariant index:

- `Grad<Scalar>` — rank 1 covector. `g[x]` → scalar.
- `Grad<Grad<Scalar>>` — rank 2 tensor. `H[x, y]` → scalar.
- `Grad<Grad<Grad<Scalar>>>` — rank 3 tensor. `T[x, y, z]` → scalar.

Single indexing on a higher-rank object **peels one level**:
- `H[x]` on `Grad<Grad<F>>` → `Grad<∂f/∂x>` (the x-row of the Hessian — still a covector)

C++26 multidim `operator[]` is for extracting scalar components from
higher-rank tensors — not for computing higher-order derivatives on a rank-1 object.

```cpp
#pragma once

#include "symbolic4/core.h"
#include "symbolic4/strategy/diff.h"

namespace tempura::symbolic4 {

template <typename T>
constexpr bool is_grad_v = false;

template <typename E>
constexpr bool is_grad_v<Grad<E>> = true;

// Forward declare
template <typename Expr>
struct Grad;

// Grad wraps an expression (or another Grad) and computes derivatives on demand.
// NOT a SymbolicTag — Grad is a tensor lookup object, not an expression node.
template <typename Expr>
struct Grad {
  [[no_unique_address]] Expr expr_;

  constexpr Grad() = default;
  constexpr Grad(Expr e) : expr_{e} {}

  // Single index: peel one level of gradient
  template <typename V>
  constexpr auto operator[](V) const {
    if constexpr (is_grad_v<Expr>) {
      // Wrapping a Grad<F>: differentiate the inner F, re-wrap in Grad
      // grad(grad(f))[x] = grad(df/dx) — still needs more indices
      auto df = differentiate(expr_.expr_, V{});
      return Grad<decltype(df)>{df};
    } else {
      // Wrapping a plain expression: differentiate to scalar
      return differentiate(expr_, V{});
    }
  }

  // C++26 multidim: peel indices one at a time from outside in
  template <typename V1, typename V2, typename... Vs>
  constexpr auto operator[](V1 v1, V2 v2, Vs... vs) const {
    auto inner = (*this)[v1];    // peel first index
    return inner[v2, vs...];     // recurse into inner Grad (or extract scalar)
  }
};

template <Symbolic E>
constexpr auto grad(E expr) {
  return Grad<E>{expr};
}

// grad of a Grad — builds Grad<Grad<E>>
template <typename E>
constexpr auto grad(Grad<E> g) {
  return Grad<Grad<E>>{g};
}

// Convenience aliases
template <Symbolic E>
constexpr auto hessian(E expr) {
  return grad(grad(expr));
}

}  // namespace tempura::symbolic4
```

**Tests to write:** `src/symbolic4/grad_test.cc`

```cpp
Symbol<struct X> x;
Symbol<struct Y> y;

auto f = x*x + x*y + y*y;

// Rank 1: covector
auto g = grad(f);
// g[x] → scalar expression: 2*x + y
// g[y] → scalar expression: x + 2*y
expectEq(6.0, evaluate(g[x], x = 2.0, y = 2.0));   // 2*2 + 2 = 6
expectEq(6.0, evaluate(g[y], x = 2.0, y = 2.0));   // 2 + 2*2 = 6

// Rank 2: Hessian via grad(grad(f))
auto H = grad(grad(f));
// H[x, y] → scalar: ∂²f/∂x∂y = 1
// H[x, x] → scalar: ∂²f/∂x² = 2
expectEq(2.0, evaluate(H[x, x], x = 0.0, y = 0.0));
expectEq(1.0, evaluate(H[x, y], x = 0.0, y = 0.0));
expectEq(2.0, evaluate(H[y, y], x = 0.0, y = 0.0));

// Partial indexing: H[x] returns a covector (Grad)
auto Hx = H[x];                // Grad<∂f/∂x> — still rank 1
expectEq(2.0, evaluate(Hx[x], x = 0.0, y = 0.0));  // d²f/dx² = 2
expectEq(1.0, evaluate(Hx[y], x = 0.0, y = 0.0));  // d²f/dxdy = 1

// hessian() convenience
auto H2 = hessian(f);
expectEq(2.0, evaluate(H2[x, x], x = 0.0, y = 0.0));
```

### Integration with Posterior

Update `PlateTransformedPosterior` to offer a `grad()` method:

```cpp
auto g = posterior.grad();     // Grad<LogProbExpr> — rank 1 covector
g[alpha]      // scalar gradient w.r.t. alpha
g[theta]      // indexed gradient w.r.t. theta (a family over plates)

auto H = posterior.hessian();  // Grad<Grad<LogProbExpr>> — rank 2
H[alpha, theta]               // Hessian component
```

These are convenience wrappers returning `Grad<LogProbExpr>` and
`Grad<Grad<LogProbExpr>>` respectively.

## Task 4.2: New ReduceOp Types (already defined in Phase 1)

Phase 1 defined `SumReduce`, `ProdReduce`, `MaxReduce`, `LogSumExpReduce`.
Now add differentiation rules for each in `strategy/diff.h`.

Modify `Recursive::apply()` in `recursive.h` to dispatch on the reduce_op:

```cpp
if constexpr (is_reduce_over_v<E>) {
  using ROp = typename E::reduce_op;
  using DimTag = typename E::dim_tag;

  if constexpr (std::is_same_v<ROp, SumReduce>) {
    // Linear: d/dx Sigma f = Sigma d/dx f
    return E::rebuild(apply(expr.expr()));

  } else if constexpr (std::is_same_v<ROp, ProdReduce>) {
    // d/dx Prod_i f_i = (Prod_i f_i) * Sum_i (d/dx f_i / f_i)
    auto body = expr.expr();
    auto dbody = apply(body);
    return expr * ReduceOver<SumReduce, DimTag, decltype(dbody / body)>{dbody / body};

  } else if constexpr (std::is_same_v<ROp, LogSumExpReduce>) {
    // d/dx LSE_i(f_i) = Sum_i softmax(f_i) * d/dx f_i
    // where softmax(f_i) = exp(f_i) / Sum_i exp(f_i)
    // = exp(f_i - LSE_i(f_i))
    auto body = expr.expr();
    auto dbody = apply(body);
    auto weights = exp(body - expr);  // softmax weights
    return ReduceOver<SumReduce, DimTag, decltype(weights * dbody)>{weights * dbody};

  } else {
    // MaxReduce, MinReduce: not differentiable symbolically
    // Return the expression unchanged and let the user handle it
    return expr;
  }
}
```

## Task 4.3: Strategy Combinators — one(s) and some(s)

**File to modify:** `src/symbolic4/strategy/combinator.h`

Add `one(s)` — applies s to the first child that succeeds:

```cpp
template <typename S>
struct One {
  [[no_unique_address]] S s;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    if constexpr (is_reduce_over_v<E>) {
      auto r = s.apply(e.expr());
      if constexpr (isSame<decltype(r), Never>) {
        return Never{};
      } else {
        return e.rebuild(r);
      }
    } else if constexpr (!is_expression_v<E>) {
      return Never{};  // No children to try
    } else {
      return tryChildren(e, MakeIndexSequence<E::arity>{});
    }
  }

private:
  // Try each child in order, return on first success
  template <typename Expr, SizeT I0, SizeT... IRest>
  constexpr auto tryChildren(Expr e, IndexSequence<I0, IRest...>) const {
    auto r = s.apply(e.template arg<I0>());
    if constexpr (!isSame<decltype(r), Never> && !isSame<decltype(r), decltype(e.template arg<I0>())>) {
      // Success: rebuild with this child replaced
      return rebuildWith<I0>(e, r);
    } else if constexpr (sizeof...(IRest) > 0) {
      return tryChildren(e, IndexSequence<IRest...>{});
    } else {
      return Never{};
    }
  }
};

template <typename S>
constexpr auto one(S s) { return One<S>{s}; }
```

Add `some(s)` — applies s to all children that succeed, fails if none do:

```cpp
template <typename S>
struct Some {
  [[no_unique_address]] S s;
  // Apply s to each child; keep originals where s fails.
  // Fail only if NO child was transformed.
  // ...
};

template <typename S>
constexpr auto some(S s) { return Some<S>{s}; }
```

## Task 4.4: Reduction-Aware Simplification Rules

**File to modify:** `src/symbolic4/interpreter/simplify.h`

Add rules that the simplifier can now apply (because ReduceOver is understood):

```cpp
// Distribution of sum into reduce
// sum_D(f + g) → sum_D(f) + sum_D(g)   [for any additive reduction]

// Factor constants out of reductions
// sum_D(k * f) → k * sum_D(f)          [when k doesn't depend on D]

// Zero annihilation
// sum_D(0) → 0

// Nested same-op reductions
// sum_D1(sum_D2(f)) = sum_D2(sum_D1(f)) [commutativity when dims are independent]
```

These require pattern matching on ReduceOver, which Phase 1 should have enabled.
If pattern matching for ReduceOver isn't ready, these rules can be added as
special cases in the simplifier's strategy.

## Task 4.5: MeanOver and VarOver as Derived Reductions

**File to create or extend:** `src/symbolic4/indexed/reduce_over.h`

Mean and Variance aren't monoids — they're derived from Sum:

```cpp
// meanOver<D>(f) = sumOver<D>(f) / dimSize<D>
// This is a convenience function, not a new ReduceOp
template <typename DimTag, Symbolic Expr>
constexpr auto meanOver(Expr expr) {
  return sumOver<DimTag>(expr) / DimSizeSymbol<DimTag>{};
}
```

Where `DimSizeSymbol<DimTag>` is a special atom that evaluates to the
runtime dimension size. This may need a new effect type:

```cpp
// A symbol that evaluates to the size of a dimension
template <typename DimTag>
struct DimSize : SymbolicTag {
  // Evaluated by looking up DimTag's size in the indexed context
};
```

This is optional for Phase 4 — can be deferred if the design isn't clean yet.

## Acceptance Criteria

- [ ] `grad(f)[x]` returns simplified derivative expression (rank 1 → scalar)
- [ ] `grad(grad(f))[x, y]` returns Hessian component (rank 2, C++26 multidim operator[])
- [ ] `grad(grad(f))[x]` returns a `Grad` (partial indexing peels one rank)
- [ ] `evaluate(grad(f)[x], x = 3.0, y = 1.0)` produces correct numeric result
- [ ] Differentiation through `ProdReduce` produces correct symbolic result
- [ ] Differentiation through `LogSumExpReduce` produces correct result
- [ ] `one(s)` and `some(s)` combinators work
- [ ] All existing tests still pass

## Build & Test

```bash
cmake --build build && ctest --test-dir build -R symbolic4
```
