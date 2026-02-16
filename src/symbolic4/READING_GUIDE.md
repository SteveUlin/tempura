# symbolic4 Reading Guide

A guided reading order through the symbolic4 library — a compile-time computation
graph framework for C++26 probabilistic programming. Read the files in order.
Each section tells you what to look for and what design pressures shaped the code.

---

## Phase 1: The Foundation — Types ARE the Representation

The single most important idea in symbolic4: there is no runtime AST. The
expression `x + 1` doesn't build a tree of heap-allocated nodes — it produces
the *type* `Expression<AddOp, Atom<X, Free>, Constant<1>>`. The AST lives
entirely in the C++ type system.

This means:
- Zero runtime overhead — the compiler does all the structural work
- Memoization is free — the compiler caches identical type instantiations
- Constexpr evaluation — everything can happen at compile time
- The "interpreter" is just template dispatch

### 1. `DESIGN.md`

**Read for:** The vision and the "why" behind every major decision.

This is the design document that drove the library's evolution. Start here to
understand the intellectual framework before seeing any code.

**Pay attention to:**
- §2.1 "Graphs as Types" — the core encoding strategy and why `sizeof` of
  deeply nested expressions can be 1 byte
- §2.4 "Pattern Matching as Foundation" — differentiation is rewriting, not
  a separate mechanism. `d/dx(f*g) = (d/dx f)*g + f*(d/dx g)` is literally
  a rewrite rule
- §2.5–2.6 — Stratego-inspired strategy combinators. Rules say *what* is
  valid. Strategies say *how* and *where* to apply them
- §2.9 — How distributions and plates map onto the expression algebra

### 2. `core.h`

**Read for:** The two node types and the effect system.

Everything in an expression tree is either:
- `Atom<Id, Effect>` — a leaf. `Id` is a unique type for identity, `Effect`
  determines how the leaf gets its value
- `Expression<Op, Args...>` — an internal node combining an operator with children

The `Effect` system makes `Atom` extensible without changing the core type:
- `Free` — variable needing a binding at eval time (`Symbol<X>`)
- `Compute<E>` — deterministic function of other vars
- `Sample<D>` — random variable from distribution D (probabilistic domain)

**Pay attention to:**
- `BinderPack` (line 442) — the canonical lookup pattern. Multiple inheritance
  from `TypeValueBinder` slots, with `using Binders::operator[]...` to pull
  all overloads into scope. Overload resolution IS the dispatch — no runtime
  maps, no switch statements. This pattern recurs in every container in the
  codebase
- `Fraction<N, D>` — compile-time rational arithmetic. Avoids floating-point
  in the type domain, auto-reduces to lowest terms
- The `isSpecOf()` helper (line 306) — uses C++26 reflection (`template_of()`,
  `template_arguments_of()`) to replace 15+ template specialization pairs.
  Works in the info domain (`std::meta::remove_cvref(^^T)`) to dodge a
  clang-p2996 bug with local type aliases

### 3. `compressed.h`

**Read for:** Why symbolic expressions can be 1 byte.

`CompressedTuple<Ts...>` uses `[[no_unique_address]]` to eliminate storage for
empty types. Since `Atom<X, Free>`, `Constant<5>`, and all operator tags are
empty, deeply nested expression types cost nothing at runtime.

This is the mechanical foundation for "types are the representation" — without
zero-cost storage, type-level ASTs would pay a real memory cost.

### 4. `operators.h`

**Read for:** How natural math syntax constructs the type-level AST.

`x + y` returns `Expression<AddOp, X, Y>` — it *builds structure*, not values.
No simplification happens here; `x + 0` stays as `Expression<AddOp, X, Constant<0>>`.
Simplification is a separate concern handled by interpreters.

Op types come from `meta/function_objects.h` and serve dual roles:
- As AST tags: `Expression<AddOp, ...>` records "this is addition"
- As callable functors: `AddOp{}(3.0, 4.0)` returns `7.0`

**Pay attention to:**
- The `SymbolicLike` concept and the `toSymbolic()` mechanism — this is how
  `RandomVar` participates in expressions. When you write `mu + sigma * x`,
  each `RandomVar` is converted to `Atom<Id, Sample<Dist>>` via `toSymbolic()`,
  embedding distribution metadata into the expression tree for auto-discovery

### 5. `constants.h`

**Read for:** The `_c` suffix for compile-time constants.

`42_c` becomes `Constant<42.0>`. All constants are doubles to avoid integer
promotion surprises in expressions. Short file, but essential for writing
readable model code.

---

## Phase 2: Evaluation — Collapsing Types to Values

### 6. `interpreter/eval.h`

**Read for:** How the type-level AST becomes a number.

Direct `if constexpr` recursion — no framework, no catamorphism apparatus.
The evaluator walks the expression type at compile time and generates a
function that's pure arithmetic at runtime:

```
Constant   → return V
Atom/Free  → look up in bindings
Expression → evaluate children, apply operator
ReduceOver → loop over dimension, fold with monoid
```

**Pay attention to:**
- How `BinderPack` provides the lookup (`ctx.scalars[term]` resolves via
  overload resolution to the specific `TypeValueBinder::operator[]` for
  that symbol's type)
- How `ReduceOver` evaluation drives a runtime loop — the expression
  *structure* is static but the *data size* is dynamic. This is the
  two-stage computation model: compile-time optimize, runtime evaluate

### 7. `interpreter/to_string.h`

**Read for:** Pretty-printing with correct precedence.

`toString(expr, name(x, "x"))` returns `"x * x + 1"`. Uses `DisplayTraits`
per operator to determine symbols, precedence, and associativity. Useful for
debugging gradient expressions.

---

## Phase 3: Differentiation — The Info-Domain Innovation

Differentiation is where C++26 reflection pays the biggest dividend.

### 8. `strategy/diff.h`

**Read for:** The public differentiation API.

The entire file is 35 lines. `diff(expr, var)` reflects the expression to
`std::meta::info`, differentiates + simplifies in that domain, then splices
the result back to a single C++ type.

```cpp
template <Symbolic E, typename Var>
constexpr auto diff(E, Var) {
  constexpr auto result = diffInfo(^^E, ^^Var);
  using Result = [:result:];   // splice: info → type
  return Result{};
}
```

**Why this matters:** The type-domain differentiator creates ~200+ intermediate
`Expression` types per differentiation — each subexpression, each application
of the chain rule, each simplification step instantiates a new type. The
info-domain approach does all that work on `std::meta::info` values —
zero intermediate type instantiations until the final result.

### 9. `strategy/info_diff.h`

**Read for:** The differentiation rules implemented as info-domain dispatch.

This is the real engine. `diffInfoImpl(expr, var)` pattern-matches on the
reflected template structure and applies calculus rules:

- Sum: `∂(f+g) = ∂f + ∂g`
- Product: `∂(f·g) = ∂f·g + f·∂g`
- Chain rule: `∂sin(f) = cos(f)·∂f`
- Quotient: `∂(f/g) = (∂f·g − f·∂g) / g²`
- Power: `∂(f^g) = f^g · (∂g·log(f) + g·∂f/f)`
- ReduceOver: dispatch by reduction op (Sum is linear, Prod uses
  log-derivative trick)

Each rule calls `simplifyInfo()` on its output to keep expression sizes
manageable — without this, gradient expressions grow exponentially.

**Pay attention to:**
- The `Ops` struct (line 43) — cached reflections of operator types.
  `^^::tempura::AddOp` is stored once instead of being reflected repeatedly
- `diffInfoImpl` dispatches on `template_of(expr)` — the reflection
  equivalent of `if constexpr (is_expression_v<T>)` but working in the
  info domain

### 10. `strategy/info_simplify.h`

**Read for:** Algebraic simplification in the info domain.

Runs after each differentiation step to prevent expression blowup.
Identity elimination (`x + 0 → x`), annihilation (`x * 0 → 0`),
constant folding (`3 * 4 → 12`), inverse cancellation (`exp(log(x)) → x`).

### 11. `grad.h`

**Read for:** Higher-order derivatives with typed rank.

`Grad<Expr>` wraps an expression and computes derivatives on demand.
Each `grad()` adds one covariant index — the rank is encoded in the type:

- `Grad<F>` — rank 1, `g[x]` extracts a scalar
- `Grad<Grad<F>>` — rank 2, `H[x, y]` extracts a scalar (C++26 multidim `operator[]`)
- Partial indexing requires explicit `.row()` to avoid ambiguity

---

## Phase 4: Indexed Expressions — Plates and Reductions

This layer bridges the type-level AST with runtime-sized data. The expression
structure is static, but it processes arbitrarily many observations.

### 12. `indexed/dim.h`

**Read for:** Named dimensions as unique types.

`Dim<Tag, Size>` gives names to dimensions. `dim()` creates a unique type
via `decltype([] {})`. These tags identify which axis a reduction folds over,
which parameters are indexed, and which data arrays correspond to which
dimensions.

`IndexedSymbol<Id, Dims...>` represents subscripted variables — `θ[c]`
becomes `IndexedSymbol<ThetaId, Countries>`.

### 13. `indexed/reduce_over.h`

**Read for:** The unified reduction mechanism — monoidal folds over dimensions.

`ReduceOver<ReduceOp, DimTag, Expr>` is the general form. Each `ReduceOp`
defines a monoid: `identity()`, `combine(a, b)`, and `symbol()`:

| ReduceOp | Identity | Combine | Symbol |
|----------|----------|---------|--------|
| SumReduce | 0 | a + b | Σ |
| ProdReduce | 1 | a × b | Π |
| MaxReduce | −∞ | max(a,b) | Max |
| LogSumExpReduce | −∞ | log(eᵃ+eᵇ) | LSE |

`SumOver<D, E>` is just an alias for `ReduceOver<SumReduce, D, E>`.
This single mechanism covers plate log-prob summation, matrix contraction,
and statistical marginalization.

**Why monoids:** Associativity means `Σ_all = Σ_shards(Σ_within)` — you can
split a reduction across parallel workers without changing the expression.

### 14. `indexed/indexed_eval.h`

**Read for:** How evaluation loops over dimensions.

When the evaluator hits a `ReduceOver`, it loops over the dimension's runtime
size, accumulating with the monoid's `combine()`. The current index is threaded
through context so nested indexed symbols (`θ[i]`) resolve correctly.

### 15. `indexed/data.h`

**Read for:** How observed data plugs into expressions.

`IndexedData` wraps a `std::span<const double>` and an `IndexedSymbol`,
allowing `y = indexed(data_vector)` in model construction.

---

## Phase 5: Distributions — The Probabilistic Domain

Everything above is domain-independent infrastructure. This layer adds
probabilistic semantics: distributions, random variables, and automatic
log-probability collection.

### 16. `distributions/log_prob.h`

**Read for:** Symbolic log-probability functions.

`logNormal(x, mu, sigma)` returns a *symbolic expression*, not a number.
This is the key insight — log-probabilities are expressions in the same
algebra as everything else, so they compose with differentiation and
simplification automatically.

Both normalized and unnormalized versions exist. Unnormalized versions
drop normalizing constants (they cancel in MCMC ratios).

### 17. `distributions/dist_base.h`

**Read for:** The CRTP base class for all distributions.

`DistBase<Derived, Support, Params...>` provides:
- `CompressedTuple<Params...>` storage (zero overhead for empty params)
- `logProbFor(x)` — unpacks params, delegates to `Derived::logProbImpl`
- `param<I>()` — typed parameter access

The `Support` type tag (`Real`, `PositiveReal`, `Unit`) drives automatic
transform selection later in the MCMC pipeline.

### 18. `distributions/wrappers.h`

**Read for:** Concrete distributions.

`NormalDist<Mu, Sigma>`, `BetaDist<A, B>`, `GammaDist<A, B>`, etc.
Each inherits from `DistBase`, provides `logProbImpl` and
`unnormalizedLogProbImpl`, and names its parameters
(`dist.mu()`, `dist.sigma()`).

### 19. `distributions/random_var.h`

**Read for:** The `RandomVar` abstraction and factory functions.

`RandomVar<Dist, Id>` pairs a distribution with a unique identity.
Factory functions (`normal()`, `beta()`, `gamma()`) use `decltype([] {})`
to generate unique IDs per call site:

```cpp
auto mu = normal(0_c, 10_c);     // unique Id
auto sigma = halfNormal(5_c);    // different unique Id
auto y = normal(mu, sigma);      // yet another unique Id
```

**Key design:** `toSymbolic(rv)` converts a `RandomVar` to
`Atom<Id, Sample<Dist>>` — embedding the distribution into the expression
tree. This enables auto-discovery: traversing `y`'s expression finds
`mu` and `sigma` as `Sample` atoms.

### 20. `distributions/effects.h`

**Read for:** Domain-specific effects for probabilistic atoms.

`Sample<D>` marks a random variable from distribution D.
`IndexedSample<D, Dims>` extends this to indexed (plate) variables.
These effects live in the probabilistic domain, not in `core.h` — though
`core.h` transitionally re-exports them.

### 21. `distributions/collect_log_prob.h`

**Read for:** Automatic parent discovery.

`collectLogProbs(y)` traverses `y.logProb()`, finds all `Sample<D>` atoms,
calls each distribution's `logProbFor()`, and sums everything. An `IdSet`
prevents double-counting.

This is what makes hierarchical models work without manual dependency
listing — you define `y ~ Normal(mu, sigma)` and the library discovers
that `mu` and `sigma` need priors too.

---

## Phase 6: Model Building and Inference

### 22. `model.h`

**Read for:** Joint model specification.

`Model<RVs...>` holds a collection of random variables and computes
`jointLogProb()` — the sum of all individual log-probabilities.
`.posterior()` starts building a posterior for MCMC.

### 23. `infer.h`

**Read for:** The simplified inference API.

`infer(y)` auto-discovers all latent parameters from the observed
variable's expression tree. `inferExplicit(mu, sigma)` lists them
manually. Both return a posterior ready for sampling.

### 24. `distributions/discover_params.h`

**Read for:** How `infer()` reconstructs typed `RandomVar` objects.

`discoverParams(observed_rv)` traverses the expression tree, finds
`Sample<Dist>` atoms, and reconstructs actual `RandomVar` objects
with their exact types. This is critical for making `samples[mu]`
work — the types must match exactly.

---

## Phase 7: MCMC — Making Inference Boring

The goal: users define a model, call `sample()`, and get draws from the
posterior. Everything else — constraint transforms, Jacobian corrections,
gradient chain rules, leapfrog integration — happens automatically.

### 25. `constraints.h`

**Read for:** The conceptual foundation of constraint transforms.

Why transforms: HMC explores unconstrained ℝⁿ, but `sigma > 0`.
Solution: `z ∈ ℝ` → `x = exp(z) > 0`. The Jacobian `|dx/dz| = exp(z) = x`
corrects the probability density.

This file provides numeric + symbolic constraint transforms.

### 26. `mcmc/transforms.h`

**Read for:** The typed transform catalog.

Each transform provides `transform()`, `inverse()`, `logJacobian()`, and
`chainRuleGrad()`:

| Transform | Forward (z→x) | When |
|-----------|---------------|------|
| `Unconstrained` | z | Real-valued params |
| `Positive` | exp(z) | sigma, rate params |
| `UnitInterval` | sigmoid(z) | probabilities |
| `CholeskyTransform` | diag=exp, off=id | covariance matrices |
| `SimplexTransform<K>` | stick-breaking | probability vectors |

### 27. `mcmc/support.h`

**Read for:** Automatic transform selection.

`autoTransform(rv)` reads the distribution's `support_type` and maps it
to the correct transform. Users never manually specify transforms —
the distribution's support *is* the specification.

### 28. `mcmc/transform_pack.h`

**Read for:** Coordinating all parameter transforms.

`TransformPack` holds one transform per parameter and provides:
- `transform(z)` → constrained values for all params at once
- `logJacobian(z)` → sum of all Jacobian corrections
- `chainRuleGrad()` → `grad_z[i] = grad_x[i] * dx/dz + d(logJ)/dz`

### 29. `symbolic_state.h`

**Read for:** The typed mutable state container.

`SymbolicState` is backed by a flat `vector<double>` but exposes
symbol-typed access: `state[mu]` → `double&`, `state[z_b]` → `span<double>`.
This is the `BinderPack` pattern applied to mutable state.

**Why flat backing:** HMC needs contiguous memory for momentum sampling
and leapfrog. The flat vector provides this while the typed interface
prevents offset arithmetic errors.

### 30. `mcmc/mutable_state.h`

**Read for:** Per-slot typed HMC container.

`MutableState<Slots...>` is used during leapfrog integration. Each slot
stores one parameter and dispatches `state[sym]` via overload resolution.
Leapfrog, gradient chain-rule, and momentum sampling all fold over slot
types — no index arithmetic anywhere.

### 31. `mcmc/samples.h`

**Read for:** Unified MCMC draw container.

`Samples` stores all draws in a flat `DynamicDense` matrix but exposes
symbol-indexed access:
- `samples[alpha]` → `vector<double>` (all draws for scalar param)
- `samples[z_b]` → `DynamicDense` matrix (draws × dimension)
- `samples[expr]` → evaluate any expression across all draws
- `samples[0]` → `SymbolicState` for a single draw

### 32. `mcmc/plate_transforms.h`

**Read for:** The production posterior — where everything comes together.

`PlateTransformedPosterior` is the sole posterior type. It orchestrates:
1. Transform unconstrained → constrained (`TransformPack`)
2. Evaluate log-prob (symbolic expression + `evaluate()`)
3. Add Jacobian correction
4. Compute gradients (symbolic `diff()` + chain rule)
5. Run leapfrog HMC integration on `MutableState`
6. Collect draws into `Samples`

HMC is *inline* — no external sampler library. The posterior owns the
entire pipeline from log-prob to samples. `debugGradientCheck()` verifies
analytic gradients against finite differences.

### 33. `mcmc/sampler.h`

**Read for:** HMC configuration.

`HmcConfig` struct: `epsilon` (step size), `steps` (leapfrog iterations),
`warmup` (adaptation draws), `draws` (posterior draws).

---

## Phase 8: Putting It All Together

### 34. `CLAUDE.md` (symbolic4's project doc)

**Read for:** Current state, architecture diagram, API vision.

Read this *after* seeing the code — it summarizes where each piece fits in
the layered architecture:

```
╔════════════════════════════════════════╗
║ Domain: Probabilistic Programming      ║
║   infer()  posterior.sample()          ║
║   distributions/  RandomVar  model()   ║
╠════════════════════════════════════════╣
║ Domain-Independent Infrastructure      ║
║   Strategy Layer (diff, simplify)      ║
║   Evaluation (eval, toString)          ║
║   Binding Layer (BinderPack, State)    ║
║   Indexed / Reduction (ReduceOver)     ║
║   Expression Algebra (Atom, Expr)      ║
╚════════════════════════════════════════╝
```

The bottom half is a general-purpose staged computation engine. The top
half is its first domain application. A query optimizer or circuit
synthesizer would replace the top, reuse the bottom.

---

## Supplementary Files

Read these as needed when you encounter them in the main sequence.

| File | When to read |
|------|-------------|
| `let.h` | Working with DAG representation / shared subexpressions |
| `fraction_ops.h` | Understanding exact rational arithmetic in the type domain |
| `strategy/pattern.h` | Ported from symbolic3 — pattern matching primitives |
| `strategy/combinator.h` | Strategy composition: `Seq`, `Choice`, `Try`, `All` |
| `strategy/info_canonicalize.h` | Structural canonicalization for normal forms |
| `simplify/ordering.h` | Term ordering for canonical forms |
| `indexed/gather.h` | Index composition: `z[idx]` = `z ∘ idx` |
| `mcmc/likelihoods.h` | Numerically stable logistic/log1pexp |
| `mcmc/non_centered.h` | Non-centered reparameterization for funnel geometries |
| `distributions/discrete.h` | Discrete distribution support |
| `distributions/multivariate.h` | Multivariate distributions |
| `distributions/dirichlet_simplex.h` | Simplex-support distributions |

---

## Key Design Patterns

These patterns recur throughout the codebase. Recognizing them makes
reading faster.

### 1. BinderPack — overload resolution as dispatch

Multiple inheritance from typed slots + `using...` to pull `operator[]`
into scope. The compiler's overload resolution replaces hash maps:

```cpp
container[mu]      // → double&       (compile-time dispatch)
container[z_b]     // → span<double>  (different overload)
container[expr]    // → evaluate expr (yet another overload)
```

Shows up in: `BinderPack`, `SymbolicState`, `MutableState`, `Samples`.

### 2. Info-domain operations — avoid intermediate types

Instead of:
```
type A → transform → type B → simplify → type C → ...
```

Reflect to `std::meta::info`, do all work there, splice once:
```
type A → ^^A (info) → manipulate info → [:result:] → final type
```

Shows up in: differentiation, type traits, symbol lookup.

### 3. Effect-extensible leaves

`Atom<Id, Effect>` + new effects per domain = open leaf node set
without changing core infrastructure.

### 4. Two-stage computation

Expression *structure* is static (compile-time types).
Expression *evaluation* is dynamic (runtime data of arbitrary size).
`ReduceOver` is the bridge — its structure is in the type, its loop
bounds come from runtime data.
