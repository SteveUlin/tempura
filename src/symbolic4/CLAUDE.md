# symbolic4 - Staged Computation via Compile-Time Expression Graphs

## Vision

symbolic4 is a **staged computation framework**: expression graphs are built and
optimized at compile time as C++ types, then evaluated over runtime data of arbitrary
size. The expression is the program. Rewrite rules are the optimizer. Evaluation is
the runtime.

**Mathematical operations return typed symbolic objects, and `operator[]` extracts
components on demand.** This is lazy evaluation at the type level — the library
computes only what you ask for, and the return type encodes the structure.

The driving application is probabilistic programming (making MCMC "boring"), but the
core infrastructure — expression algebra, strategy-based rewriting, and typed
monoidal reductions — is domain-independent. Any domain with a term algebra,
equational laws, and evaluation against runtime data fits the same architecture.

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
H.row(mu)         // ∂f/∂μ row — explicit rank-1 covector extraction
// H[mu] is a compile error — partial indexing must be explicit via .row()

// Each grad() adds one rank. Full indexing extracts scalars.
// grad(scalar)       → rank 1 covector    → [x] extracts scalar
// grad(grad(scalar)) → rank 2 tensor      → [x, y] extracts scalar
// grad³(scalar)      → rank 3 tensor      → [x, y, z] extracts scalar

// For MCMC: the posterior knows its parameters
auto obs = dim();
auto posterior = infer(y).bind(y = indexed(data));
auto g = posterior.grad();
g[mu]           // scalar gradient expression
g[theta]        // indexed gradient (a family over plates)

// Evaluate at a point
evaluate(g[mu], mu = 3.0, sigma = 1.0);  // 6.0
```

The same pattern extends to indexed reductions, vector-valued functions, and
(eventually) tensor fields and differential forms.

## Core Design: Two-Stage Computation

### Stage 1 — Compile time: build, differentiate, optimize

Expressions are encoded entirely in C++ types:
- `x + y` creates `Expression<AddOp, Atom<X,Free>, Atom<Y,Free>>`
- No runtime AST, no heap allocation, no type tags
- Symbolic differentiation, simplification, and rewriting happen here
- The compiler sees and optimizes the entire computation structure

### Stage 2 — Runtime: bind data, evaluate over dimensions

The compiled expression graph executes over runtime-sized data:
- Plates loop over runtime-determined observation counts
- Matrices carry runtime-sized dimensions via `DynamicDense`
- `ReduceOver` folds over runtime-length dimension indices
- MCMC runs thousands of evaluations with different parameter values

**This is not a "compile-time only" library.** The expression *structure* is static,
but it processes arbitrarily large runtime data — the same execution model as SQL
query plans (optimizer produces a fixed plan, executor streams rows through it) or
GPU shader programs (compiled once, executed on millions of data points).

### Symbol lookup ties both stages together

`operator[]` dispatched by type is the central API pattern:
- `bindings[x]` — look up a value for symbol x
- `samples[alpha]` — extract draws for parameter alpha
- `samples[expr]` — evaluate any symbolic expression across all draws
- `grad(f)[x]` — partial derivative (covector component)
- `grad(grad(f))[x, y]` — Hessian entry (rank-2 tensor, C++26 multidim `operator[]`)

## Compiler & Reflection

symbolic4 requires **Clang P2996** (Bloomberg's C++26 reflection fork). GCC is not
supported. Use `nix develop .#reflection` for the dev shell and `cmake --preset clang-p2996`
to configure.

C++26 reflection (`<experimental/meta>`) is used throughout:
- **`meta/type_list.h`, `meta/type_list_ops.h`** — `template_arguments_of` + `template for` + `substitute`
  replace recursive template metaprogramming for `Get`, `Filter`, `Contains`, `Unique`, etc.
- **`meta/type_id.h`** — `^^T` provides unique compile-time type identity (replaces stateful friend injection)
- **`core.h` type traits** — `template_of()` queries replace 15 template specialization pairs
  (`is_atom_v`, `is_expression_v`, `get_id_t`, `same_atom_id_v`, etc.)
- **`symbolic_state.h`** — consteval loop for symbol lookup (replaces recursive `SymbolIndex`)
- **`indexed/dim.h`** — consteval loop for `IndexOf`, splice for `At`

All type traits use reflection, including NTTP templates (`Constant`, `Fraction`,
`SimplexTransform`, `VectorSymbol`, etc.) via an info-based `isSpecOf(^^Template)` overload.

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
| 1 | ReduceOver + make strategies ReduceOver-aware | ✓ Done |
| 2 | Migrate diff() → strategy-based | ✓ Done |
| 3 | Kill cata — direct recursion for eval/toString | ✓ Done |
| 4 | Grad<Expr>, new reductions, new combinators | ✓ Done |
| 5 | Deduplicate constraints, cleanup | ✓ Done |
| 6 | Symbol-forward MCMC: `samples[expr]`, init API, grad spans | ✓ Done |
| 7.0 | SymbolicSlot / SymbolicState | ✓ Done |
| 7.1 | Open effect system (Sample/IndexedSample → effects.h) | ✓ Done |
| 7.2 | Extensible evaluator terminal dispatch | ✓ Done |
| 7.3 | TransformPack | ✓ Done |
| 7.4 | Unify posteriors | ✓ Done (RandomVar uses Free symbols, eval no longer applies transforms, ConstrainedParamSymbol deleted) |
| 7.5 | Remove lookupByAtomId | ✓ Done (ProbTerminals/BaseTerminals use direct Free symbol lookup, lookupByAtomId deleted) |
| 7.6 | Diff cleanup | ✓ Done (removed SampleDerivative/same_symbolic_id_v, simplified to same_atom_id_v; wrappers.h include removed) |
| 7.7 | Unify MCMC containers | ✓ Done (deleted ParameterState/state.h, old Samples<Params...>, posterior.h, mcmc.h, matrix_mcmc.h; unified on Samples + SymbolicState; eliminated scalar/dynamic sampling fork) |

**Standalone fix (no phase dependency):** ✓ Done. Relax `operators.h` `requires` clause so
RandomVar/IndexedRandomVar work directly in expressions without `.constrainedExpr()`/`.sym()`.

## Expression Algebra

### Atoms (leaf nodes)

`Atom<Id, Effect>` — the universal leaf node. `Id` is a unique type for identity,
`Effect` determines behavior. The effect system is **open** — domains add their own.

**Core effects** (domain-independent, defined in `core.h`):
- `Free` — Variable needing binding (`Symbol<X>`)
- *(deleted)* `Embedded<T>` was runtime literal — replaced by `Constant<V>` + binders
- `Compute<E>` — Deterministic function

**Probabilistic effects** (domain-specific, should live in `distributions/`):
- `Sample<D>` — Random variable from distribution D
- `IndexedSample<D, DimsList>` — Indexed random variable

`Sample` and `IndexedSample` are now also re-exported from `distributions/effects.h`
(Phase 7.1 done). They remain in `core.h` for backward compat until Phase 7.4
completes the posterior unification.

### Expressions (internal nodes)

`Expression<Op, Args...>` where operators are callable functors serving dual roles:
- AST construction: `x + y` builds the type
- Evaluation: `AddOp{}(3.0, 4.0)` returns `7.0`

### Reductions (dimension elimination)

`ReduceOver<ReduceOp, DimTag, Expr>` — eliminates a dimension by folding over its
domain. In function terms: takes `f : (A, B) → ℝ` and returns `g : A → ℝ` where
`g(a) = ⊕_b f(a, b)`. This single mechanism covers plate log-prob summation,
matrix contraction, and statistical marginalization.

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

Partial indexing on higher-rank objects requires explicit `.row()`:
- `H[x]` on `Grad<Grad<F>>` → **compile error** (ambiguous intent)
- `H.row(x)` on `Grad<Grad<F>>` → `Grad<∂f/∂x>` (explicit row extraction)

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
╔═══════════════════════════════════════════════════════════════════╗
║ Domain: Probabilistic Programming (first application)            ║
║                                                                   ║
║  User API                                                         ║
║    infer(y)   posterior.sample()   samples[expr]   grad(f)[mu]    ║
║                                                                   ║
║  Probabilistic Layer                                              ║
║    distributions/  RandomVar<Dist,Id>  collectLogProbs  model()   ║
║                                                                   ║
║  MCMC Layer                                                       ║
║    PlateTransformedPosterior  Samples  HMC  support inference     ║
║                                                                   ║
║  Domain Transforms (probabilistic-specific)                       ║
║    exp/logit for support types  Cholesky  Jacobian corrections    ║
╠═══════════════════════════════════════════════════════════════════╣
║ Domain-Independent Infrastructure                                 ║
║                                                                   ║
║  Strategy Layer (expr → expr)                                     ║
║    differentiate()  simplify()  Recursive  Innermost  All         ║
║                                                                   ║
║  Evaluation Layer (expr → value)                                  ║
║    evaluate()  toString()  extensible terminal dispatch            ║
║                                                                   ║
║  Binding Layer (symbol → value, heterogeneous)                    ║
║    BinderPack  SymbolicState  IndexedBinding                      ║
║    compressed tuples with type-dispatched operator[]               ║
║                                                                   ║
║  Indexed / Reduction Layer (function algebra over named dims)      ║
║    ReduceOver<ROp, DimTag, Expr>  dim()  Gather (= composition)   ║
║    monoidal folds: Sum, Product, Max, LogSumExp                   ║
║                                                                   ║
║  Expression Algebra                                               ║
║    Atom<Id,Effect>   Expression<Op,Args...>   Grad<Expr>          ║
║    Constant<V>  Fraction<N,D>  CompressedTuple                    ║
║                                                                   ║
║  Numeric Backend (matrix3)                                        ║
║    Dense  DynamicDense  InlineDense  LU  multiply  transpose      ║
╚═══════════════════════════════════════════════════════════════════╝
```

The bottom half is a general-purpose staged computation engine. The top half is its
first domain application. A query optimizer, circuit synthesizer, or policy engine
would replace the top half while reusing everything below the line.

### Design Principle: Compressed Tuples with Symbolic Lookup

The BinderPack pattern — multiple inheritance from typed slots, `using...` to pull
`operator[]` overloads into scope — is the **canonical lookup pattern** for this
codebase. Each symbol maps to its own typed storage. No type erasure, no flat arrays
with manual offset computation.

```cpp
// The pattern: heterogeneous container, symbol-typed access
container[mu]       // → double&       (scalar parameter)
container[z_a]      // → span<double>  (indexed parameter)
container[Sigma]    // → DynamicDense& (matrix parameter)
container[expr]     // → evaluate expr at container's values
```

**Three properties make this work:**
1. **`[[no_unique_address]]`** — empty symbol types cost zero bytes
2. **Overload resolution as dispatch** — `operator[](SymType)` resolves at compile
   time, no runtime map or index computation
3. **Heterogeneous return types** — each symbol can return a different type (double,
   span, matrix), because each slot defines its own `operator[]`

The MCMC layer now follows this pattern: `Samples` stores draws in a flat matrix
but exposes symbol-typed access (`samples[alpha]`, `samples[expr]`, `draw[z_b]`).
Per-draw states are `SymbolicState` objects — compressed-tuple containers with the
same `operator[]` dispatch. GradientResult still uses flat arrays with manual offsets
(migrating it to spans is a future cleanup).

### Runtime Dependency Rule

Infrastructure headers must not include domain headers. Arrows point down only:

```
distributions/, mcmc/ ──→ core.h, indexed/, strategy/, interpreter/
                          (never the reverse)
```

Currently violated by: `indexed_eval.h` includes `distributions/indexed_node.h`
and `constraints.h`. Fix planned in phase 7.

## File Organization

**Domain-independent infrastructure** (reusable across domains):

| Directory | Purpose |
|-----------|---------|
| `core.h`, `operators.h`, `constants.h` | Expression algebra: Atom, Expression, Op functors |
| `strategy/` | Stratego-style rewriting: rules, combinators, traversals, diff |
| `interpreter/` | Evaluation: eval, simplify, to_string (direct recursion) |
| `indexed/` | Typed dimensions, ReduceOver, plates, indexed evaluation |
| `compressed.h` | Zero-overhead storage for stateless expression nodes |

**Probabilistic programming domain** (first application):

| Directory | Purpose |
|-----------|---------|
| `distributions/` | Distribution wrappers, RandomVar, log-prob collection |
| `mcmc/` | Transforms, posteriors, HMC adapter, samples |
| `matrix/` | Multivariate support (MVN, LKJ, Cholesky) |
| `constraints.h` | Support-driven transforms (Real, Positive, Unit) |
| `model.h`, `infer.h` | Model building and auto-discovery |

**Planning / documentation**:

| Directory | Purpose |
|-----------|---------|
| `migration/` | Strategy unification plan (phases 1–5 done, phase 6 pending) |
| `matrix_algebra/` | Matrix algebra extension plan (Track A numeric + Track B symbolic) |

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

### Verification

Changes should be verified against the symbolic4 tests AND the examples, since the
examples exercise deeper template instantiation paths (full model → posterior →
gradient → HMC chains) that unit tests may not cover.

**Requires Clang P2996** — run inside `nix develop .#reflection`:

```bash
# Configure (once)
cmake --preset clang-p2996

# Fast: unit tests only (~seconds)
ctest --test-dir build-reflection -R symbolic4

# Thorough: also build examples (~minutes, heavy template instantiation)
cmake --build build-reflection --target bmj_symbolic
cmake --build build-reflection --target linear_regression
```

⚠️ **The stat_rethinking examples are slow to compile** — deep expression trees with
full MCMC pipelines produce large template instantiations. Don't build all examples
on every change. Use the unit tests for iteration, build examples for validation
before committing.

### What "Complete" Looks Like

```cpp
auto districts = dim();
auto obs       = dim();

// Hyperpriors
auto a_bar = normal(0, 1);
auto sigma_a = exponential(1);

// Varying intercepts (non-centered)
// RandomVars work directly in expressions — no .constrainedExpr()/.sym() needed
auto z_a = plate(normal(0, 1), districts);
auto a = a_bar + sigma_a * z_a;

// Data and likelihood
auto district_idx = data(obs);
auto x = data(obs);
auto y = plate(bernoulli(logistic(a[district_idx] + beta * x)), obs);

// One line to get posterior
auto posterior = infer(y).bind(
    x = indexed(x_data), district_idx = indexed(idx_data), y = indexed(y_data));

// Gradients via symbol lookup
auto g = posterior.grad();
g[a_bar]     // scalar gradient
g[sigma_a]   // scalar gradient (through log transform)
g[z_a]       // std::span<const double> — indexed gradient family over districts

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

1. ~~**RandomVars in expressions**~~: ✓ Done (operators.h `requires` relaxed)
2. ~~**`samples[expr]`**~~: ✓ Done (Phase 7.7 — evaluateIndexed across draws)
3. **Constrained init**: `sample(config, {sigma = 0.5}, rng)` — not `sigma = log(0.5)`
4. **Indexed gradient spans**: `grad[z_b]` → `span<const double>`, not manual offset/size
5. **Unified dim/plate API**: `dim()` objects replace empty struct tags; `plate(dist, dims...)` takes values not template params
6. **Unified evaluator**: Merge `evaluate` and `evaluateIndexed` — scalar is nullary function, indexed is n-ary
7. **Non-centered parameterization**: Built into plate notation
8. **Ordered monotonic effects**: Simplex → cumulative transform

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

## Known Rough Edges

### ✓ Fixed: Constraint transform duplication → `constraints.h`
### ✓ Fixed: Atom ID matching duplication → `same_atom_id_v` in `core.h`
### ✓ Fixed: Hardcoded ndims {1,2,3} → pack-expansion in `indexed_eval.h`
### ✓ Fixed: SumOver operator overloads → ReduceOver with full operator support
### ✓ Fixed: Hardcoded arity {0,1,2,3} → variadic implementations
### ✓ Fixed: toString can't render SumOver → direct recursion with ReduceOver awareness

### DimIndexMap uses runtime RTTI
`indexed_eval.h` uses `std::unordered_map<std::type_index, SizeT>` — the only
RTTI in the codebase. Low priority — works correctly, just aesthetically unclean.

### ✓ Fixed: Constraint transform asymmetry (scalar vs indexed)
Phase 7.4 unified both paths: RandomVar uses `Symbol<Id>` (Free atom) for bindings,
the posterior's TransformPack pre-transforms z→x for ALL params (scalar and indexed),
and the evaluator no longer applies constraint transforms. `ConstrainedParamSymbol` deleted.

### ✓ Fixed: Evaluator coupled to distributions
Phase 7.5 removed `lookupByAtomId` (runtime ID-scanning fallback). ProbTerminals now
constructs `Atom<get_id_t<T>, Free>` directly for compile-time lookup. BaseTerminals
uses `ctx.scalars[term]` for Free atoms. `constraints.h` no longer imported by any
evaluator path.

### ✓ Fixed: MCMC containers unified on SymbolicState/Samples
Phase 7.7 deleted ParameterState and the old Samples<Params...>. The unified `Samples`
class uses `SymbolicState` for per-draw access. Both scalar and indexed params flow
through the same `TransformPack` → `DynamicDense` matrix path. Symbol-indexed lookup
(`samples[alpha]`, `samples[expr]`, `draw[z_b]`) replaces manual offset arithmetic.

## Mathematical Direction

The symbol-lookup pattern extends naturally into deeper mathematics:

**Level 1 (current target): Multivariable calculus for MCMC**
- `grad(f)[x]` — partial derivatives (rank 1 covector)
- `grad(grad(f))[x, y]` — Hessian components (rank 2 tensor)
- Indexed reductions: Sum, Product, LogSumExp

**Level 1.5 (unified indexing): Named dimensions as function domains**

All indexing — plates, matrices, tensors, data — is function application. A plate
variable `z` isn't "an array indexed by g"; it's a function `z : Groups → ℝ`. This
insight unifies the indexed and matrix systems into one algebra.

Three operations cover everything:
- **Define**: `plate(dist, dim)` creates `f : Dim → sample(dist)`. `data(dim)` creates
  `f : Dim → ℝ` bound to observed values. Multi-dim: `plate(dist, rows, cols)`.
- **Compose**: `z[idx]` is `z ∘ idx`. If `z : Groups → ℝ` and `idx : Obs → Groups`,
  then `z ∘ idx : Obs → ℝ`. This replaces the special "gather" concept.
- **Integrate**: `reduce(expr, dim, op)` eliminates an argument by folding over its
  domain. Plate log-prob, matrix contraction, and marginalization are the same operation.

Broadcasting is implicit domain extension — `x : Features → ℝ` lifts to
`(Obs, Features) → ℝ` by ignoring the argument it doesn't depend on.

Matrix multiply is compose + integrate:
`C(i,j) = reduce(A(i,k) * B(k,j), k, sum)` — elementwise product broadcasts to
`{i, j, k}`, reducing over `k` yields `{i, j}`.

**Design changes from this unification:**
- Replace empty struct dimension tags with `dim()` objects (unique type via
  `decltype([] {})`). Size is runtime, determined by bound data, not the dim object.
- `plate(dist, dims...)` takes dim values, not template parameters. Variadic dims
  replace nested `plate<A>(plate<B>(dist))`.
- `data(dim)` takes a dim value. Returns a bindable data placeholder.
- One evaluator, not two — `evaluate` (scalar) and `evaluateIndexed` merge.
  A scalar expression is a nullary function. The `reduce` node drives the loop.
- `StaticDimIndexMap` is partial application — the current set of bound arguments
  as the evaluator traverses nested reductions.
- Opaque nodes (Cholesky, QR, solve) consume and produce dimensioned expressions
  but call dense routines internally. They compose with the rest of the system
  through shared dimension metadata.

See `matrix_algebra/README.md` for numeric backend (Track A) and symbolic types (Track B).

**Level 2: Tensor algebra for Riemannian methods**
- Fisher information metric: `fisher(model)[x, y]`
- Metric tensors for Riemannian HMC
- General Einstein summation (Level 1.5 provides the typed-index foundation)

**Level 3: Differential forms for variational methods**
- `d(omega)` — exterior derivative (generalizes grad, curl, div)
- Wedge products
- Integration on manifolds

For now, focus on Levels 1 and 1.5 — they cover all practical MCMC and linear
model needs. The design should not preclude Levels 2-3 but shouldn't over-engineer
toward them.

## Extensibility Beyond Mathematics

The domain-independent infrastructure (expression algebra + strategy rewriting +
typed monoidal reductions) works for any domain with three properties:

1. **A term algebra** — domain objects composed from typed operators
2. **Equational laws** — rewrite rules that transform/optimize terms
3. **Evaluation against runtime data** — bind symbols to values, fold over dimensions

### Strong fits (same paradigm, different Op/Rule/Binding types)

| Domain | Expressions | Rewrite rules | Reductions |
|--------|------------|---------------|------------|
| **Query optimization** | SELECT/JOIN/WHERE nodes | Predicate pushdown, join reorder | GROUP BY aggregations (Sum, Count, Max) |
| **Circuit synthesis** | AND/OR/NOT gates | De Morgan, constant prop | Critical path (MaxReduce over delays) |
| **Policy engines** | Allow/Deny predicates | Contradiction elimination, DNF normalization | Policy combination (all-must-pass, any-pass) |
| **Compiler IR** | Load/Store/Add/Mul | Peephole opts, constant folding | — |
| **Chemical reactions** | Species + stoichiometry | Reaction rules | Conservation laws (SumOver species) |

### The runtime dimension system enables scale

Plates already demonstrate: **static expression graph, runtime data of arbitrary
size**. A query plan is a static expression evaluated over millions of rows. A
circuit simulation is a static netlist evaluated over streaming input vectors. The
monoidal reduction splitting property (`SumOver<All> → SumOver<Shards>(SumOver<Within>)`)
is valid for any associative ReduceOp, enabling parallel evaluation without
changing the expression.

### What doesn't fit

Domains requiring **runtime graph construction** — where the expression topology
depends on runtime input. Neural architecture search, data-driven behavior trees,
dynamic protocol parsing. The boundary: if the expression type can't be determined
at compile time, this framework isn't the right tool.

## Roadmap

| Priority | Feature | See |
|----------|---------|-----|
| ~~Done~~ | ~~Strategy migration (phases 1–5)~~ | `migration/README.md` |
| ~~Done~~ | ~~Relax operator `requires` clause~~ | ~~`operators.h:156`~~ |
| ~~Done~~ | ~~Symbol-forward MCMC (phase 6)~~ | ~~`migration/phase6.md`~~ |
| ~~Done~~ | ~~Decouple infrastructure from domain (phase 7)~~ | `migration/phase7.md` |
| **Next** | Unified indexing: `dim()` objects, `plate(dist, dims...)`, merge evaluators | — |
| **Next** | Matrix algebra: Track A (Cholesky, inverse in matrix3) | `matrix_algebra/track_a_numeric.md` |
| **Later** | Matrix algebra: Track B (symbolic matrix types + ops via unified dim system) | `matrix_algebra/track_b_symbolic.md` |
| **Later** | `Grad<Expr>` with typed rank via nesting | `migration/phase4.md` |
| **Later** | MCMC diagnostics (ESS, R-hat) via `diag[alpha].ess` | — |
| **Later** | Riemannian HMC with Fisher metric | — |
