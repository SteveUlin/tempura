# symbolic4 Strategy Migration

## Vision

Transition symbolic4 from a split cata/strategy architecture to a unified system:
- **Strategies** for all expression-to-expression transforms (diff, simplify, future rewrites)
- **Direct recursion** for all interpretations (eval, toString, collectLogProbs)
- **Kill cata** entirely — it's the wrong abstraction for both jobs
- **ReduceOver** as the unified reduction node, replacing the special-cased SumOver

## Why

The current architecture has two parallel traversal systems (cata + strategies) that
don't compose. SumOver is special-cased in 9+ locations because each system handles
it independently. The strategy system is strictly better for transforms (composable,
programmable control flow), and direct recursion is strictly simpler for evaluation
(no Interpreter concept needed). Cata sits between them, serving neither well.

## Current State

| Component | Current mechanism | Target |
|-----------|------------------|--------|
| `simplify()` | Strategy (`innermost`) | Keep as-is |
| `diff()` | Cata (`para` in `interpreter/diff.h`) | Strategy (`differentiate` in `strategy/diff.h`) |
| `evaluate()` | Cata (`fold` in `interpreter/eval.h`) | Direct recursion |
| `toString()` | Cata (`fold` in `interpreter/to_string.h`) | Direct recursion |
| `evaluateIndexed()` | Custom loop (`indexed_eval.h`) | Direct recursion (merge with eval) |
| `collectLogProbs()` | Hand-written recursion | Simplified direct recursion |
| `discoverParams()` | Hand-written recursion | Simplified direct recursion |

## Phase Ordering

```
Phase 1: Strategy Infrastructure     ← no production changes, pure preparation
    │
    v
Phase 2: Switch diff                 ← biggest win, enables Phase 3
    │
    v
Phase 3: Kill cata                   ← removes scheme/, interpreter/eval, interpreter/to_string
    │
    v
Phase 4: New Capabilities            ← Grad<E>, new reductions, new combinators
    │
    v
Phase 5: Cleanup                     ← deduplicate, remove RTTI, extract shared code


✓ Phase 6: Symbol-Forward MCMC      ← ✓ DONE
                                        samples[expr], grad spans, constrained init

Phase 7: Decouple Infrastructure     ← INDEPENDENT of phase 6, parallel ok
    │                                   open effects, extensible eval, symbolic state
    ├── ✓ 7.0 SymbolicSlot/State    ← ✓ DONE (symbolic_state.h)
    ├── ✓ 7.1 Open effect system    ← ✓ DONE (distributions/effects.h)
    ├── ✓ 7.3 TransformPack         ← ✓ DONE (mcmc/transform_pack.h)
    ├── ✓ 7.2 Extensible eval       ← ✓ DONE (terminals.h, prob_terminals.h, indexed_eval.h)
    ├── 7.4 Unify posteriors         ← NEXT: all prerequisites done (7.0, 7.2, 7.3)
    ├── 7.5 Remove lookupByAtomId    ← depends on 7.4 (expressions must use Free atoms)
    └── 7.6 Diff cleanup            ← depends on 7.4 (diff must not see Sample atoms)
```

**Standalone fix (any time, no dependencies):** Relax operator `requires` clause
in `operators.h` so RandomVar/IndexedRandomVar work directly in expressions.

Each phase has its own detailed task file: `phase1.md` through `phase7.md`.

## Key Design Decisions

### ReduceOver<ReduceOp, DimTag, Expr>

Replaces `SumOver<DimTag, Expr>` with a parameterized reduction:

```cpp
template <typename ReduceOp, typename DimTag, Symbolic Expr>
struct ReduceOver : SymbolicTag {
  using reduce_op = ReduceOp;
  using dim_tag = DimTag;
  using expr_type = Expr;
  [[no_unique_address]] Expr expr_;
  constexpr auto expr() const { return expr_; }
  template <Symbolic NewExpr>
  static constexpr auto rebuild(NewExpr e) {
    return ReduceOver<ReduceOp, DimTag, NewExpr>{e};
  }
};

template <typename DimTag, Symbolic Expr>
using SumOver = ReduceOver<SumReduce, DimTag, Expr>;
```

### Grad<Expr> — Typed Rank via Nesting

Each `grad()` adds one covariant index. The rank is the type, not
the number of `operator[]` arguments:

```cpp
auto g = grad(f);           // Grad<F> — rank 1 covector
g[x]                        // scalar: ∂f/∂x

auto H = grad(grad(f));     // Grad<Grad<F>> — rank 2 tensor
H[x, y]                     // scalar: ∂²f/∂x∂y (C++26 multidim operator[])
H[x]                        // Grad<∂f/∂x> — still rank 1 (partial indexing)
```

`Grad` is NOT a `SymbolicTag` — it's a tensor lookup object, not an expression node.
Multidim `operator[]` peels indices from outside in, each removing one `Grad` layer.

### Direct recursion replaces cata for evaluation

```cpp
template <Symbolic E, typename Ctx>
constexpr auto eval(E expr, Ctx& ctx) -> double {
  if constexpr (is_reduce_over_v<E>) {
    return evalReduce(expr, ctx);
  } else if constexpr (is_terminal_v<E>) {
    return evalTerminal(expr, ctx);
  } else {
    return evalExpression(expr, ctx);
  }
}
```

### Symbol-Forward MCMC (Phase 6)

Independent of the strategy migration. Extends the symbol-lookup pattern
through the full MCMC pipeline: model → posterior → samples → post-processing.

**Principle:** a container binds symbols to values; an expression is a query.
`container[expr]` = "evaluate this expression at the values I hold."

Key deliverables:

1. **`samples[expr]`** — evaluate any symbolic expression across all draws.
   `samples[logistic(a + sigma * z_b)]` returns a matrix of country probabilities,
   eliminating manual post-processing loops that re-implement model formulas.

2. **`grad[indexed_param]` → span** — `GradientResult::operator[]` dispatches
   on scalar vs indexed spec. Scalar → `double`, indexed → `span<const double>`.

3. **Constrained init values** — `sample(config, {sigma = 0.5}, rng)` instead of
   `{sigma = log(0.5)}`. Posterior applies `inverse()` to convert internally.

### Standalone Fix: Operator Constraint

`operators.h` binary operators have `requires(Symbolic<L> || Symbolic<R>)`.
This prevents two `SymbolicLike` operands (e.g., `RandomVar + IndexedRandomVar`)
from combining, forcing users to write `.constrainedExpr()` / `.sym()`.

Fix: relax or remove the extra `requires` clause. The `SymbolicLike L, SymbolicLike R`
template parameters already exclude `int`/`double`.

Before: `a.constrainedExpr() + sigma.constrainedExpr() * z_b.sym()`
After: `a + sigma * z_b`

## Testing Strategy

Each phase must pass all existing tests before proceeding. Run:
```bash
cmake --build build && ctest --test-dir build -R symbolic4
```

Phase 2 (switch diff) is the highest risk — diff is used in 10+ production files.
Run the full test suite after each file migration.
