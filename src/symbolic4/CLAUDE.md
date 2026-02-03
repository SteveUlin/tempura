# symbolic4 - Compile-Time Symbolic Algebra with Symbol Lookups

## Vision

symbolic4 is a compile-time symbolic algebra library where **mathematical operations
return indexed symbolic objects, and `operator[]` with symbols extracts components
on demand**. This is lazy evaluation at the type level — the library computes only
the components you ask for, and the return type encodes the mathematical structure.

The driving application is probabilistic programming (making MCMC "boring"), but the
core idea generalizes: the gradient of different objects should be different types.

```cpp
Symbol<struct Mu> mu;
Symbol<struct Sigma> sigma;

auto f = mu*mu + log(sigma);

// grad(f) returns a rank-1 covector — takes exactly one index
auto g = grad(f);
g[mu]             // ∂f/∂μ = 2μ         (scalar expression)
g[sigma]          // ∂f/∂σ = 1/σ        (scalar expression)

// Hessian = grad of grad — a rank-2 tensor, takes two indices
auto H = grad(grad(f));
H[mu, sigma]      // ∂²f/∂μ∂σ = 0       (C++26 multidim operator[])
H[sigma, sigma]   // ∂²f/∂σ² = -1/σ²
H[mu]             // ∂f/∂μ row — still a covector (Grad), needs one more index

// Each grad() adds one rank. Each index peels one off.
// grad(scalar)       → rank 1 covector    → [x] extracts scalar
// grad(grad(scalar)) → rank 2 tensor      → [x, y] extracts scalar
// grad³(scalar)      → rank 3 tensor      → [x, y, z] extracts scalar

// For MCMC: the posterior knows its parameters
auto posterior = infer(y).bind(y = data);
auto g = posterior.grad();
g[mu]           // scalar gradient expression
g[theta]        // indexed gradient (a family over plates)

// Evaluate at a point
evaluate(g[mu], mu = 3.0, sigma = 1.0);  // 6.0
```

The same pattern extends to indexed reductions, vector-valued functions, and
(eventually) tensor fields and differential forms.

## Core Design Principle: Types ARE the Representation

Expressions are encoded entirely in C++ types at compile time:
- `x + y` creates `Expression<AddOp, Atom<X,Free>, Atom<Y,Free>>`
- No runtime AST, no heap allocation, no type tags
- Full compiler optimization and constexpr support

**Symbol lookup** (`operator[]` dispatched by type) is the central API pattern:
- `bindings[x]` — look up a value for symbol x
- `samples[alpha]` — extract draws for parameter alpha
- `samples[expr]` — evaluate any symbolic expression across all draws
- `grad(f)[x]` — partial derivative (covector component)
- `grad(grad(f))[x, y]` — Hessian entry (rank-2 tensor, C++26 multidim `operator[]`)

## Active Migration: Strategy Unification

**See `migration/README.md` for the full execution plan.**

We are transitioning from a split cata/strategy architecture to:
- **Strategies** (`strategy/`) for all expression→expression transforms (diff, simplify)
- **Direct recursion** for all interpretations (eval, toString)
- **ReduceOver<ROp, DimTag, Expr>** as the unified reduction node (replaces SumOver)
- **Grad<Expr>** with typed rank via nesting (`grad(grad(f))` = rank 2)
- **Deleting** `scheme/cata.h` and the `interpreter/diff.h` cata-based differentiator

When modifying files, prefer the strategy system over cata. Do not add new
cata-based interpreters. New expression transforms should be rewrite rules
composed via `recursive()`, `innermost()`, etc.

| Phase | What | Status |
|-------|------|--------|
| 1 | ReduceOver + make strategies ReduceOver-aware | Pending |
| 2 | Migrate diff() → strategy-based differentiate() | Pending (blocked by 1) |
| 3 | Kill cata — direct recursion for eval/toString | Pending (blocked by 2) |
| 4 | Grad<Expr>, new reductions, new combinators | Pending (blocked by 3) |
| 5 | Deduplicate constraints, kill RTTI, cleanup | Pending (blocked by 4) |
| 6 | Symbol-forward MCMC: `samples[expr]`, init API, grad spans | Pending (independent) |

**Standalone fix (no phase dependency):** Relax `operators.h` `requires` clause so
RandomVar/IndexedRandomVar work directly in expressions without `.constrainedExpr()`/`.sym()`.

## Expression Algebra

### Atoms (leaf nodes)

`Atom<Id, Effect>` with four effects:
- `Free` — Variable needing binding (`Symbol<X>`)
- `Embedded<T>` — Runtime literal value
- `Sample<D>` — Random variable from distribution D
- `Compute<E>` — Deterministic function

### Expressions (internal nodes)

`Expression<Op, Args...>` where operators are callable functors serving dual roles:
- AST construction: `x + y` builds the type
- Evaluation: `AddOp{}(3.0, 4.0)` returns `7.0`

### Reductions (indexed operations)

`ReduceOver<ReduceOp, DimTag, Expr>` — a monoidal fold over an indexed family.
`SumOver<DimTag, Expr>` is an alias for `ReduceOver<SumReduce, DimTag, Expr>`.

Each ReduceOp defines: `identity()`, `combine(a, b)`, `symbol()`.

| ReduceOp | Identity | Combine | Diff rule |
|----------|----------|---------|-----------|
| SumReduce | 0 | a + b | ∂Σf = Σ∂f (linearity) |
| ProdReduce | 1 | a × b | ∂Πf = Π·Σ(∂f/f) (log-derivative) |
| MaxReduce | −∞ | max(a,b) | ∂max f = ∂f(argmax) (subgradient) |
| LogSumExpReduce | −∞ | log(eᵃ+eᵇ) | Σ softmax(f)·∂f |

Derived reductions (not monoids): Mean = Sum/N, Variance = Mean(x²) − Mean(x)².

### Gradient objects (typed rank via nesting)

`Grad<Expr>` wraps an expression and computes derivatives on demand via `operator[]`.
**Each `grad()` adds exactly one covariant index** — the rank is encoded in the type:

- `Grad<Scalar>` — rank 1 (covector). `g[x]` → scalar.
- `Grad<Grad<Scalar>>` — rank 2 (matrix). `H[x, y]` → scalar via C++26 multidim `operator[]`.
- `Grad<Grad<Grad<Scalar>>>` — rank 3. `T[x, y, z]` → scalar.

Single indexing on a higher-rank object peels one level:
- `H[x]` on `Grad<Grad<F>>` → `Grad<∂f/∂x>` (a covector — the x-row of the Hessian)

The rank is **not** determined by how many arguments you pass to `operator[]`.
It is determined by the **type** — how many `Grad` layers are nested.

### RandomVar

`RandomVar<Dist, Id>` pairs a distribution with a unique identity, enabling:
- Symbolic log-probability computation
- Automatic parent discovery via expression traversal
- Transform inference from distribution support

## Transform System: Strategies + Direct Recursion

### Strategies (expression → expression)

Stratego-inspired rewriting framework. Strategies are types with `apply(E) → E'`.

**Primitives:** `Id`, `Fail`, `Seq` (`>>`), `Choice` (`|`), `Try`, `All`
**Traversals:** `BottomUp`, `TopDown`, `Innermost` (fixpoint), `Outermost`
**Recursive rules:** `recursive(rrule(pattern, replacement_with_rec))`

Used for: **differentiation** (`strategy/diff.h`), **simplification** (`simplify.h`)

Differentiation rules read like mathematics:
```cpp
auto diff = recursive(
    rrule(f_ + g_,    rec(f_) + rec(g_))
  | rrule(f_ * g_,    rec(f_) * g_ + f_ * rec(g_))
  | rrule(sin(f_),    cos(f_) * rec(f_))
  | rrule(exp(f_),    exp(f_) * rec(f_))
  | rrule(log(f_),    rec(f_) / f_)
  | rule(Var{}, 1_c)
  | rule(AnySymbol{}, 0_c)
  | rule(AnyConstant{}, 0_c)
);
```

### Direct recursion (expression → value)

Simple `if constexpr` dispatch for evaluation, toString, collectLogProbs.
No Interpreter pattern, no cata apparatus. Handles ReduceOver natively:

```cpp
if constexpr (is_reduce_over_v<E>) {
  using ROp = typename E::reduce_op;
  double accum = ROp::identity();
  for (SizeT i = 0; i < size; ++i)
    accum = ROp::combine(accum, eval(expr.expr(), ctx));
  return accum;
}
```

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ User API                                                        │
│   grad(f)[mu]   grad(grad(f))[mu,σ]   infer(y)   samples[α]    │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│ Probabilistic Layer                                             │
│   distributions/  RandomVar<Dist,Id>  plates  collectLogProbs   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│ MCMC Layer                                                      │
│   transforms  support inference  TransformedPosterior  HMC      │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│ Strategy Layer (expr → expr)                                    │
│   differentiate()  simplify()  Recursive  Innermost  All        │
├─────────────────────────────────────────────────────────────────┤
│ Evaluation Layer (expr → value, direct recursion)               │
│   eval()  evalIndexed()  toString()                             │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│ Expression Algebra                                              │
│   Atom<Id,Effect>   Expression<Op,Args...>                      │
│   ReduceOver<ROp,DimTag,Expr>   Grad<Expr>                      │
└─────────────────────────────────────────────────────────────────┘
```

## File Organization

| Directory | Purpose |
|-----------|---------|
| `core.h`, `operators.h`, `constants.h` | Expression system, ops, compile-time constants |
| `strategy/` | Stratego-style rewrite framework (diff, simplify, combinators) |
| `interpreter/` | eval, simplify, to_string (transitioning to direct recursion) |
| `distributions/` | Distribution wrappers, RandomVar, log-prob collection |
| `indexed/` | ReduceOver, plates, indexed evaluation, dimension tags |
| `mcmc/` | Transforms, posteriors, HMC adapter, samples |
| `matrix/` | Multivariate support (MVN, LKJ) |
| `migration/` | Phased plan for strategy unification |

## Case Studies: Statistical Rethinking Examples

The `examples/stat_rethinking/` directory contains implementations of homework
problems from Richard McElreath's Statistical Rethinking 2025 course.
**These examples are the litmus test for API completeness.**

| Example | Status | What It Tests |
|---------|--------|---------------|
| `linear_regression.cpp` | Manual gradients | Basic regression with plates |
| `logistic_regression.cpp` | Manual gradients | Binary outcomes, Bernoulli likelihood |
| `bangladesh_contraception.cpp` | Manual everything | Hierarchical model, varying effects, Dirichlet simplex, ordered monotonic |
| `bmj_weekend_submissions.cpp` | Manual | Poisson regression |
| `bmj_symbolic.cpp` | Symbolic | Simpler model using symbolic4 API |

### What "Complete" Looks Like

```cpp
// Hyperpriors
auto a_bar = normal(0, 1);
auto sigma_a = exponential(1);

// Varying intercepts (non-centered)
// RandomVars work directly in expressions — no .constrainedExpr()/.sym() needed
auto z_a = plate<Districts>(normal(0, 1));
auto a = a_bar + sigma_a * z_a;

// Likelihood
auto y = plate<Obs>(bernoulli(logistic(a[district] + beta * x)));

// One line to get posterior
auto posterior = infer(y).bind(x = x_data, y = y_data);

// Gradients via symbol lookup
auto g = posterior.grad();
g[a_bar]     // scalar gradient
g[sigma_a]   // scalar gradient (through log transform)
g[z_a]       // std::span<const double> — indexed gradient family over Districts

// Sample — init values in CONSTRAINED space (posterior handles inverse)
auto samples = posterior.sample(
    HmcConfig{.epsilon = 0.01, .steps = 20, .warmup = 500, .draws = 1000},
    BinderPack{a_bar = 0.0, sigma_a = 1.0, z_a = zeros(n_districts)},
    rng);

// Symbol lookup on raw parameters
mean(samples[a_bar]);    // posterior mean
samples[z_a];            // matrix: draws × districts

// Symbol lookup on DERIVED quantities — evaluate model expressions across draws
samples[a]               // a = a_bar + sigma_a * z_a, evaluated per-draw
samples[logistic(a)]     // transformed quantities, no manual loops
```

### API Gaps to Close

1. **RandomVars in expressions**: `a + sigma * z_b` without `.constrainedExpr()`/`.sym()`
2. **`samples[expr]`**: Evaluate derived symbolic quantities across draws
3. **Constrained init**: `sample(config, {sigma = 0.5}, rng)` — not `sigma = log(0.5)`
4. **Indexed gradient spans**: `grad[z_b]` → `span<const double>`, not manual offset/size
5. **Data-dependent likelihoods**: `plate<Obs>(normal(mu, sigma))` with external covariates `x[i]`
6. **Non-centered parameterization**: Built into plate notation
7. **Ordered monotonic effects**: Simplex → cumulative transform
8. **Varying effects syntax**: `a[district]` indexing into plates

## Symbol-Lookup-Forward MCMC

The symbol-lookup pattern should extend through the entire MCMC pipeline:
model definition → posterior → sampling → post-processing. Currently, several
places force users out of the symbolic world into manual computation.

### Principle: Container IS the Binding Context, Expression IS the Query

A symbolic expression is a function of its free symbols. Every MCMC container
(samples, state, gradients) binds symbols to values. So `container[expr]`
means "evaluate this function at the values I hold."

### Operator constraint fix (`operators.h`)

The binary operators have `requires(Symbolic<L> || Symbolic<R>)` which prevents
two `SymbolicLike`-but-not-`Symbolic` operands (e.g., two RandomVars) from
combining. Since `SymbolicLike` already excludes `int`/`double`, the extra
constraint is unnecessary. Relaxing it enables:

```cpp
// Before (current): manual extraction
auto p = 1_c / (1_c + exp(-(a.constrainedExpr() + sigma.constrainedExpr() * z_b.sym())));

// After (goal): RandomVars directly in expressions
auto p = logistic(a + sigma * z_b);
```

### `samples[expr]` — evaluate derived quantities across draws

The biggest win. Instead of manual loops re-implementing model formulas:

```cpp
// Before: 15 lines of manual computation to get country probabilities
for (std::size_t draw = 0; draw < samples.size(); ++draw) {
    for (std::size_t country = 0; country < N; ++country) {
        p_samples[country].push_back(invLogit(a_draws[draw] + sigma_val * z_b_draws[draw, country]));
    }
}

// After: expression already encodes the formula
auto p = logistic(a + sigma * z_b);
auto p_draws = samples[p];           // matrix: draws × countries
```

Implementation: for each draw, bind each parameter to that draw's values,
evaluate the expression. Return type depends on whether expr is scalar or indexed.

### `grad[indexed_param]` → span

Currently `GradientResult::operator[]` returns a single `double`, even for
indexed params. For `z_b` with 38 components, the user needs `offset()` +
`paramSize()` and manual slicing. Fix: dispatch on spec type.

```cpp
grad[alpha]     // double (scalar param)
grad[z_b]       // std::span<const double> (indexed param, all 38 components)
```

### Init values in constrained space

Users currently pass unconstrained values (`sigma = std::log(0.5)`) because
the posterior operates in unconstrained space internally. The posterior already
has `inverse()` — apply it to init values so users can write:

```cpp
posterior.sample(config, {sigma = 0.5, a = -2.0, z_b = zeros}, rng);
// posterior internally: z_sigma = log(0.5)
```

## Known Rough Edges (to fix during migration)

### Constraint transform duplication (4 copies)
`applyConstraint` logic appears independently in `eval.h`, `indexed_eval.h`,
`collect_log_prob.h`, and `random_var.h`. Consolidate into `constraints.h`.

### Atom ID matching duplication (3 copies)
"Two atoms share the same Id regardless of effect" is reimplemented in `diff.h`,
`indexed_eval.h`, and `collect_log_prob.h`. Extract to `same_atom_id_v` in `core.h`.

### DimIndexMap uses runtime RTTI
`indexed_eval.h` uses `std::unordered_map<std::type_index, SizeT>` — the only
RTTI in the codebase. Replace with compile-time typed dimension index pack.

### SumOver operator overloads incomplete
Only `operator+` is defined. Missing `*`, `-`, `/`. Trap: `2 * SumOver<...>` fails.
Fix when generalizing to ReduceOver.

### Hardcoded arity {0,1,2,3}
`All<S>`, `substituteRecImpl`, and `collectFromChildren` have explicit overloads
for 0-3 children. Replace with variadic implementations.

### toString can't render SumOver/IndexedSymbol
The standard fold sees SumOver as a terminal and prints "?". Fix when moving
toString to direct recursion with ReduceOver awareness.

## Mathematical Direction

The symbol-lookup pattern extends naturally into deeper mathematics:

**Level 1 (current target): Multivariable calculus for MCMC**
- `grad(f)[x]` — partial derivatives (rank 1 covector)
- `grad(grad(f))[x, y]` — Hessian components (rank 2 tensor)
- Indexed reductions: Sum, Product, LogSumExp

**Level 2: Tensor algebra for Riemannian methods**
- Fisher information metric: `fisher(model)[x, y]`
- Metric tensors for Riemannian HMC
- Einstein summation via typed indices

**Level 3: Differential forms for variational methods**
- `d(omega)` — exterior derivative (generalizes grad, curl, div)
- Wedge products
- Integration on manifolds

For now, focus on Level 1 — it covers all practical MCMC needs. The design
should not preclude Levels 2-3 but shouldn't over-engineer toward them.

## Roadmap

| Priority | Feature | See |
|----------|---------|-----|
| **Now** | Relax operator `requires` clause | `operators.h:156` |
| **Now** | Strategy migration (phases 1–5) | `migration/README.md` |
| **Next** | Symbol-forward MCMC (phase 6) | `migration/phase6.md` |
| **Next** | `Grad<Expr>` with typed rank via nesting | `migration/phase4.md` |
| **Later** | Simplex/Cholesky auto-transforms | `mcmc/support.h` TODO |
| **Later** | MCMC diagnostics (ESS, R-hat) via `diag[alpha].ess` | — |
| **Later** | Riemannian HMC with Fisher metric | — |
