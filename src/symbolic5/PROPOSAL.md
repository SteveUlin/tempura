# symbolic5: A Reflection-First Compile-Time Expression System

## Why symbolic5

symbolic4 proved the core thesis: encode expression graphs as C++ types, manipulate
them at compile time, evaluate at runtime with zero overhead. The type-level AST,
info-domain differentiation, and BinderPack dispatch pattern are genuine wins.

But symbolic4 was *migrated* from template metaprogramming to reflection — a
renovation, not a new build. Three structural problems persist because they're
baked into the foundation, not fixable with incremental changes.

### 1. Metadata is homeless

symbolic4 cannot annotate symbols or expressions. "σ is always positive" has no
representation. "A is a 3×3 matrix" has no representation. The `Effect` system
(`Sample<D>`, `Free`) partially addresses this by encoding *how a leaf gets its
value*, but it conflates graph structure with domain semantics:

- Effects are structural (part of the expression type), so adding an annotation
  means changing the `Atom` type, which changes every expression containing it
- The Effect vocabulary is fixed at `core.h` — domains can't extend it without
  modifying infrastructure
- Auto-discovery works by pattern-matching on `Sample<D>` atoms — tight coupling
  between the graph traversal mechanism and the Bayesian domain

### 2. Two worlds with no clear boundary

symbolic4 uses type-level ASTs for construction and evaluation, info-domain values
for differentiation and simplification. This split happened organically —
differentiation migrated to the info domain because type-level differentiation
caused ~200+ intermediate template instantiations per derivative. But the boundary
is ad-hoc: some operations live in types, others in info, with no principle
governing which belongs where.

### 3. Indexing was bolted on

`ReduceOver`, `IndexedSymbol`, `IndexedSample`, `DimIndexMap` — these were added to
handle plates (statistical arrays). But they're extensions rather than first-class
concepts. The result: two evaluators (`eval` and `evaluateIndexed`), two atom effects
for the same concept (`Sample` and `IndexedSample`), and runtime RTTI in the only
place in the codebase that uses `std::type_index`.

### What symbolic5 is

A **general-purpose compile-time expression system** — not a probabilistic
programming library with general aspirations, but a genuine compute graph framework
that happens to support Bayesian modeling as one domain among many.

Target domains:
1. **Symbolic mathematics** — differentiation, simplification, algebraic manipulation
2. **Bayesian MCMC** — distribution modeling, posterior inference, HMC sampling
3. **Matrix algebra** — symbolic linear algebra with shape tracking
4. **Neural networks** — pipeline definition, automatic differentiation, code generation
5. **Anything with a term algebra** — query optimization, circuit synthesis, policy engines

---

## Core Architecture

### The separation principle

An expression graph IS a graph — nodes connected by edges. It is not a probability
model, not a neural network, not a matrix equation. It becomes those things when
paired with *metadata* (a context) and *interpretation* (an evaluator,
differentiator, code generator).

> **Meaning = Graph × Context × Interpreter**

The graph carries structure. The context carries domain knowledge. The interpreter
produces a result. Changing the context changes what the graph *means* without
changing the graph itself.

This separation eliminates symbolic4's metadata problem. Distribution priors,
positivity constraints, tensor shapes, activation functions — all live in contexts,
not in the graph. Different contexts can annotate the same graph differently for
different purposes.

### The three computation domains

| Domain | Representation | Purpose |
|--------|---------------|---------|
| **Type** | C++ types (`App<AddOp, Var<X>, Var<Y>>`) | Construction, evaluation, zero-cost dispatch |
| **Info** | `std::meta::info` values | All structural manipulation: diff, simplify, rewrite, discover |
| **Value** | `double`, `span<double>`, `DynamicDense` | Runtime evaluation results |

**The principle:** types for construction and execution, info for manipulation, values
for results. Types flow *downward* (user writes expressions, compiler generates code).
Info flows *laterally* (reflection inspects, transforms, and re-splices types). Values
flow at runtime.

symbolic4 discovered the info domain through painful experience (the differentiation
migration). symbolic5 makes it the explicit architecture from day one.

---

## The Expression Graph

### Node types

Four node types encode all expression graphs. Everything else is metadata.

```
Var<Tag>                          Leaf — a named variable
App<Op, Args...>                  Interior — operator applied to children
Lit<V>                            Leaf — compile-time constant value
Fold<ReduceOp, Dim, Body>        Reduction — monoidal fold over a dimension
```

```cpp
// Variable — a named leaf
template <typename Tag>
struct Var {
  static constexpr std::string_view name = Tag::name;

  template <typename V>
  constexpr auto operator=(V value) const;  // creates a Binder for BinderPack
};

// Application — operator applied to children
template <typename Op, typename... Args>
struct App;

// Literal — compile-time constant
template <auto V>
struct Lit {
  static constexpr auto value = V;
};

// Fold — monoidal reduction over a named dimension
template <typename ReduceOp, typename Dim, typename Body>
struct Fold;
```

**Changes from symbolic4:**

| symbolic4 | symbolic5 | Why |
|-----------|-----------|-----|
| `Atom<Id, Effect>` | `Var<Tag>` | No effect — effects are metadata. Tag provides identity |
| `Expression<Op, Args...>` | `App<Op, Args...>` | Shorter, emphasizes function application |
| `Constant<V>` | `Lit<V>` | Shorter |
| `ReduceOver<ROp, Dim, Body>` | `Fold<ROp, Dim, Body>` | Same semantics, shorter name |
| `Fraction<N, D>` | `Lit<Rational{N, D}>` | Rational arithmetic as NTTP value, not special template |
| `IndexedSymbol<Id, Dims...>` | Deleted | A `Var` with dimension metadata in the context |
| `LetNode<Id, Def, Body>` | Deleted | Sharing handled by CSE in the info domain |

**What's deleted from the graph:** The `Effect` system (`Free`, `Compute`, `Sample`,
`IndexedSample`) is gone. These were domain-specific annotations masquerading as
structural nodes. In symbolic5, a variable is a variable. Whether it's "free",
"observed", "sampled from Normal(0,1)", or "a 256×256 weight matrix" is context, not
structure.

### Why delete Effects from the graph

The Effect system in symbolic4 served two purposes:
1. **Dispatch during evaluation** — `Free` atoms look up values, `Compute` atoms
   evaluate subexpressions, `Sample` atoms look up transformed values
2. **Auto-discovery** — traversal finds `Sample<D>` atoms to collect log-probabilities

Both purposes survive in symbolic5, but through the context:

```cpp
// symbolic4: distribution embedded in graph
auto mu_atom = Atom<MuId, Sample<NormalDist<Constant<0>, Constant<10>>>>{};

// symbolic5: graph is clean, context carries the distribution
auto mu = sym("μ");
auto ctx = context(mu | prior(normal(0_c, 10_c)));
auto params = discover(expr, ctx, Prior{});  // traverse graph, query context
```

The second approach is strictly more general — you can discover symbols with *any*
metadata key, not just `Sample` atoms. And changing the prior doesn't change the
expression type.

### Symbol construction

```cpp
// Factory with auto-unique type (preferred)
auto mu = sym("μ");       // Var<λ₁> with display name "μ"
auto sigma = sym("σ");    // Var<λ₂> with display name "σ"
auto x = sym("x");

// Under the hood:
template <typename Tag = decltype([] {})>
constexpr auto sym(std::string_view name) {
  return Var<Tag>{name};
}
```

Each call to `sym()` at a different source location produces a unique type because
each lambda expression has a unique type. The string provides human-readable display;
the lambda type provides compile-time identity.

**Alternative considered: `Symbol<"mu">` with NTTP strings.** The string serves as
both identity and display name. Rejected — two symbols named `"mu"` in different
scopes would be the same type, which violates the principle that each declaration
creates a unique identity. Names are for humans; types are for the compiler.

**Alternative considered: `Symbol<struct Mu>` (symbolic4 style).** Works, but
requires forward-declaring a struct for every symbol. Verbose for models with many
parameters.

### Expression construction

Same natural syntax as symbolic4 — operator overloads on `Symbolic` types build
`App` nodes:

```cpp
auto f = x*x + sin(y);
// Type: App<AddOp, App<MulOp, Var<X>, Var<X>>, App<SinOp, Var<Y>>>

auto g = 2_c * x + 3_c;
// Type: App<AddOp, App<MulOp, Lit<2.0>, Var<X>>, Lit<3.0>>

auto h = fold<SumReduce>(obs, logNormal(y, mu, sigma));
// Type: Fold<SumReduce, Obs, App<LogNormalOp, Var<Y>, Var<Mu>, Var<Sigma>>>
```

Operators return expression types — they build structure, not values. No
simplification at construction time. Simplification is an explicit interpretation
pass.

---

## The Context System

### Contexts as compile-time maps

A context maps `(Symbol, Key) → Value` at compile time. It uses the BinderPack
pattern from symbolic4 — overload resolution as dispatch — generalized to
composite keys.

```cpp
auto ctx = context(
  mu    | prior(normal(0_c, 10_c)),
  sigma | prior(halfNormal(5_c)) | support(Positive{}),
  A     | shape(rows, cols)      | property(PositiveDefinite{})
);
```

Internally:

```cpp
template <typename... Entries>
struct Context : Entries... {
  using Entries::operator[]...;
};

template <typename Tag, typename Key, typename Value>
struct Entry {
  constexpr auto operator[](MetaKey<Tag, Key>) const -> Value { return {}; }
};

// Lookup:
ctx[MetaKey<Mu, Prior>{}]     // → NormalDist<Lit<0.0>, Lit<10.0>>{}
ctx[MetaKey<Sigma, Support>{}] // → Positive{}
```

**Three properties make this work** (same as BinderPack):
1. `[[no_unique_address]]` — all entry types are empty, zero storage cost
2. Overload resolution — compile-time dispatch, no runtime map or hash
3. Heterogeneous values — each entry can return a different type

### Has-query for optional metadata

Not every symbol has every annotation. The context provides a compile-time
`has<Tag, Key>()` query:

```cpp
if constexpr (ctx.has<Sigma, Support>()) {
  // sigma has a support annotation — use it
  auto s = ctx[MetaKey<Sigma, Support>{}];
}
```

Implemented via `requires` on a constrained overload or SFINAE on the info domain.

### Domain-defined keys

The core provides the lookup mechanism. Domains define the vocabulary.

**Domain-independent (core):**

| Key | Value type | Meaning |
|-----|-----------|---------|
| `Name` | `string_view` | Display name for pretty-printing |
| `Bounds` | `Positive`, `Unit`, `Bounded<lo, hi>` | Value domain |
| `Shape` | `Shape<Dims...>` | Dimensional shape |

**Bayesian domain:**

| Key | Value type | Meaning |
|-----|-----------|---------|
| `Prior` | A distribution expression | Prior distribution |
| `Support` | `Real`, `Positive`, `Unit`, etc. | Distribution support |
| `Observed` | `bool_constant` | Whether observed data |
| `Transform` | `Exp`, `Logistic`, etc. | Constraint transform |

**Matrix domain:**

| Key | Value type | Meaning |
|-----|-----------|---------|
| `MatrixShape` | `Shape<Rows, Cols>` | Matrix dimensions |
| `Structure` | `Symmetric`, `PosDef`, `Triangular` | Matrix structure |
| `Decomposition` | `Cholesky`, `QR`, `SVD` | Preferred decomposition |

**Neural network domain:**

| Key | Value type | Meaning |
|-----|-----------|---------|
| `Trainable` | `bool_constant` | Learnable parameter |
| `Dtype` | `Float16`, `Float32`, `BFloat16` | Numeric precision |
| `Activation` | `ReLU`, `Sigmoid`, `GELU` | Activation function |

**The point:** no domain needs to modify the core to add metadata keys. A new domain
creates its own key types and value types. The Context mechanism is completely generic.

### Context composition

Contexts compose by merging — later entries override earlier ones for the same
(symbol, key) pair:

```cpp
auto base_ctx = context(
  mu | support(Real{})
);
auto model_ctx = context(
  mu | prior(normal(0_c, 10_c)) | support(Real{})
);

auto full_ctx = base_ctx + model_ctx;  // model_ctx entries take precedence
```

This enables layered contexts: a base context for the domain, a model context for
the specific problem, an optimization context for the specific solver.

### Expression-level metadata

Most metadata attaches to symbols (leaves). But sometimes you need to annotate an
interior node: "this subexpression is always positive" or "this product has shape
(N, M)."

**Primary mechanism: inference.** A `propagate(expr, ctx)` function derives
expression-level metadata from leaf metadata. If `sigma` is `Positive` and the
expression is `exp(sigma)`, the result is positive by composition. This works for
most cases.

**Escape hatch: explicit annotation.** When inference can't derive a property
(because it depends on domain knowledge the compiler can't see), an explicit
annotation node marks the claim:

```cpp
auto x = assume(complex_expr, Positive{});
// Type: Annotated<ComplexExpr, Positive>
// Interpreters trust the annotation without re-deriving it
```

`Annotated` is a fifth node type — but it's an *assertion*, not structure. Stripping
all annotations from a graph doesn't change its evaluation, only the optimizations
interpreters can apply.

**Alternative considered: all metadata via annotation nodes.** Rejected — this makes
the graph type dependent on metadata, recreating symbolic4's problem. The context
approach keeps the graph clean; `Annotated` is for rare exceptions.

---

## Named Dimensions

### All indexing is function application

This insight from symbolic4's roadmap becomes first-class in symbolic5. A plate
variable `z` isn't "an array indexed by g" — it's a function
`z : Groups → ℝ`. This unification means:

- **Define:** `plate(dist, dim)` creates `f : Dim → sample(dist)`
- **Compose:** `z[idx]` is `z ∘ idx`. If `z : Groups → ℝ` and `idx : Obs → Groups`,
  then `z ∘ idx : Obs → ℝ`
- **Integrate:** `fold(expr, dim, op)` eliminates an argument by folding over its
  domain. Plate log-prob, matrix contraction, and marginalization are the same
  operation

Broadcasting is implicit domain extension — `x : Features → ℝ` lifts to
`(Obs, Features) → ℝ` by ignoring the argument it doesn't depend on.

### Dimension construction

```cpp
auto obs = dim("obs");              // runtime-sized
auto features = dim("features");    // runtime-sized
auto hidden = dim("hidden", 256);   // compile-time-sized

// Under the hood:
template <typename Tag = decltype([] {}), auto Size = kDynamic>
constexpr auto dim(std::string_view name) {
  return Dim<Tag, Size>{name};
}
```

Like symbols, each call to `dim()` produces a unique type. The size is either
`kDynamic` (determined at evaluation time from bound data) or a compile-time
constant.

### Dimensions in the context

```cpp
auto theta = sym("θ");
auto W = sym("W");

auto ctx = context(
  theta | shape(countries),                    // θ : Countries → ℝ
  W     | shape(features, hidden),             // W : Features × Hidden → ℝ
  y     | shape(obs) | observed()              // y : Obs → ℝ (data)
);
```

Shape metadata drives:
- **Type checking** — `W @ x` requires compatible inner dimensions
- **Evaluation** — the evaluator binds dimension sizes from data
- **Code generation** — loop bounds, memory layout, tiling

### Matrix multiply as compose + fold

```
C(i,j) = Σ_k A(i,k) · B(k,j)
```

In symbolic5: `A @ B` desugars to `fold<SumReduce>(k, A * B)` where `k` is the
shared inner dimension. The elementwise product broadcasts `A : (I, K) → ℝ` and
`B : (K, J) → ℝ` to `(I, J, K) → ℝ`, then the fold over `K` yields
`(I, J) → ℝ`. One mechanism covers matrix multiply, dot products, convolutions, and
attention.

### One evaluator

symbolic4 has `evaluate` (scalar) and `evaluateIndexed` (with dimensions). symbolic5
has one evaluator. A scalar expression is a nullary function (takes no dimension
arguments). A `Fold` node drives a loop over its dimension's runtime size. Dimension
sizes come from the bindings, not a separate `DimIndexMap`:

```cpp
auto result = eval(expr, bind(
  mu = 3.0,
  sigma = 1.0,
  y = span_of(data),         // binds data + its dimension size
  theta = span_of(theta_vec)  // binds data + its dimension size
));
```

---

## The Interpretation API

Interpreters are functions that take a graph and produce a result. The graph is
always a type. The context and bindings are optional arguments that provide
domain knowledge and runtime values.

### Evaluation

```cpp
// Pure structural evaluation — no context needed
eval(f, bind(x = 3.0, y = 0.5));

// Context-aware evaluation — context enables optimization
// (e.g., skip abs() for known-positive values)
eval(f, ctx, bind(x = 3.0, y = 0.5));
```

Implementation: direct `if constexpr` recursion, same as symbolic4. No catamorphism,
no interpreter pattern. The compiled function is pure arithmetic.

### Differentiation

```cpp
auto df_dx = diff(f, x);           // structural differentiation
auto df_dx = diff(f, x, ctx);      // context enables stronger simplifications

auto g = grad(f);                   // rank-1 covector
g[x]                                // ∂f/∂x
g[y]                                // ∂f/∂y

auto H = grad(grad(f));            // rank-2 tensor
H[x, y]                            // ∂²f/∂x∂y  (C++26 multidim operator[])
```

All differentiation happens in the info domain. The type-level result appears only
after all manipulation is complete:

```cpp
template <typename Expr, typename Var>
constexpr auto diff(Expr, Var) {
  constexpr auto result = [] consteval {
    auto d = diffInfo(^^Expr, ^^Var);
    return simplifyInfo(d);
  }();
  return [:result:]{};  // splice once — one type instantiation
}
```

### Discovery

Context-driven traversal replaces effect-based pattern matching:

```cpp
// Find all symbols with Prior metadata
auto params = discover(expr, ctx, Prior{});

// Find all symbols with Shape metadata
auto tensors = discover(expr, ctx, Shape{});

// Collect log-probabilities (Bayesian domain)
auto joint_lp = collectLogProbs(expr, ctx);
```

`discover` walks the expression tree (in the info domain), queries the context for
each `Var`, and collects matches. Generic over any metadata key.

### Pretty-printing

```cpp
stringify(f);             // "x² + sin(y)"  (default names from sym())
stringify(f, ctx);        // "μ² + sin(σ)"  (context overrides display names)
```

### Code generation (future)

```cpp
auto kernel = codegen<CUDA>(f, ctx);     // generates CUDA kernel
auto source = codegen<CPP>(f, ctx);      // generates standalone C++ function
auto ir = codegen<LLVM_IR>(f, ctx);      // generates LLVM IR
```

Code generation is a natural interpretation — it walks the graph and emits code
instead of computing values. The context provides types, shapes, and optimization
hints.

---

## Info-Domain as the Default

### Why info beats types for manipulation

In symbolic4, type-level differentiation of `f * g` creates:

```
Expression<MulOp, DF, G>              ← d(f) * g
Expression<MulOp, F, DG>              ← f * d(g)
Expression<AddOp, ..., ...>           ← sum
Expression<AddOp, SimplifiedLHS, RHS> ← after simplifying left
Expression<AddOp, ..., SimplifiedRHS> ← after simplifying right
... ~200+ intermediate types
```

In the info domain, the same operation manipulates `std::meta::info` values — no
intermediate type instantiations until the final splice. This matters because:

- **Compile time scales linearly** with expression size, not quadratically
- **Deep expression trees** (full MCMC models) become tractable
- **Simplification** can run aggressively without paying per-step type costs

### The universal workflow

```
              User writes                    Compiler sees
              ──────────                     ─────────────
Construction: x * x + sin(y)           →    App<AddOp, App<MulOp, ...>, ...>
                    │
                    ▼ ^^T (reflect)
              ┌─────────────────────────────────────────────────┐
Manipulation: │  std::meta::info values                         │
              │  diffInfo()  simplifyInfo()  rewriteInfo()      │
              │  discoverInfo()  propagateInfo()                 │
              │                                                  │
              │  All manipulation here. No intermediate types.   │
              └──────────────────────────────────┬──────────────┘
                                                  │ [:result:] (splice)
                                                  ▼
Execution:    FinalExprType{}  →  eval(expr, bind(...))  →  double
```

Every structural operation follows this flow. Differentiation, simplification,
pattern matching, common subexpression elimination, metadata propagation — all live
in the info domain. The type domain is for *entry* (user syntax) and *exit* (final
compiled evaluation).

### Rewriting without strategy types

symbolic4 uses Stratego-style strategy types (`Seq`, `Choice`, `Try`, `All`,
`Innermost`) for composable rewriting. These are elegant but create intermediate
expression types during rewriting.

In symbolic5, rewriting uses **consteval functions** in the info domain:

```cpp
consteval auto simplifyInfo(std::meta::info e) -> std::meta::info {
  if (isApp(e, ^^AddOp)) {
    auto [lhs, rhs] = children(e);
    lhs = simplifyInfo(lhs);
    rhs = simplifyInfo(rhs);

    if (isZero(lhs)) return rhs;         // 0 + x → x
    if (isZero(rhs)) return lhs;         // x + 0 → x
    if (same(lhs, rhs))
      return app(^^MulOp, lit(2), lhs);  // x + x → 2x

    return app(^^AddOp, lhs, rhs);
  }
  // ... more patterns
}

// Composability via function composition:
consteval auto innermost(auto rule, std::meta::info e) -> std::meta::info {
  auto result = applyBottomUp(rule, e);
  while (result != e) {
    e = result;
    result = applyBottomUp(rule, e);
  }
  return result;
}

// Usage:
consteval auto optimize(std::meta::info e) -> std::meta::info {
  return innermost(simplifyInfo, e);
}
```

Strategy composition comes from function composition — simpler, more direct, and
zero type-level cost.

**What's preserved:** The *ideas* from symbolic4's strategies (bottom-up, top-down,
innermost fixpoint, choice) carry over as consteval function combinators. The
*implementation* changes from type-level strategy objects to info-domain functions.

**Alternative considered: keep Stratego-style strategy types for user-facing DSL.**
Worth revisiting once the core stabilizes — a strategy DSL could *compile to*
info-domain operations, giving users a declarative rewriting language without paying
type instantiation costs. But it's not needed for v1.

---

## Domain Sketches

### 1. Symbolic Mathematics

```cpp
auto x = sym("x");
auto y = sym("y");

auto f = x*x + sin(y);

// Differentiation
auto df_dx = diff(f, x);           // → 2*x
auto df_dy = diff(f, y);           // → cos(y)
auto d2f = diff(diff(f, x), x);    // → 2

// Evaluation
eval(f, bind(x = 3.0, y = pi/2));  // 9.0 + 1.0 = 10.0

// Pretty-printing
stringify(df_dx);                    // "2·x"
```

No context needed for pure mathematics — the graph alone suffices.

### 2. Bayesian MCMC

```cpp
auto mu = sym("μ");
auto sigma = sym("σ");
auto y = sym("y");
auto obs = dim("obs");

// Context carries the probabilistic model
auto ctx = context(
  mu    | prior(normal(0_c, 10_c)),
  sigma | prior(halfNormal(5_c)) | support(Positive{}),
  y     | shape(obs) | observed()
);

// Expression: per-observation log-likelihood
auto lp = fold<SumReduce>(obs, logNormal(y, mu, sigma));

// Auto-discover all parameters with priors
auto joint_lp = collectLogProbs(lp, ctx);

// Inference
auto posterior = infer(joint_lp, ctx).bind(y = indexed(data));
auto samples = posterior.sample(config, rng);

// Symbol lookup on draws
mean(samples[mu]);         // posterior mean of μ
samples[sigma];            // all σ draws
samples[exp(mu)];          // derived quantity, evaluated per-draw
```

The same expression graph (`lp`) could be reused with a different context
(different priors, different constraints) without rebuilding the graph.

### 3. Matrix Algebra

```cpp
auto rows = dim("rows");
auto cols = dim("cols");
auto A = sym("A");
auto b = sym("b");

auto ctx = context(
  A | shape(rows, cols) | property(PositiveDefinite{}),
  b | shape(cols)
);

// Matrix-vector multiply
auto y = A @ b;                     // shape: (rows)

// Symbolic matrix expressions
auto trace_A = fold<SumReduce>(rows, diag(A));
auto det_A = det(A);                // opaque node — calls dense routine

// Differentiation respects matrix structure
auto dL_dA = diff(logDet(A), A);    // → A⁻ᵀ (context knows A is pos-def)
```

Shape metadata enables compile-time dimension checking. Structure metadata
(`PositiveDefinite`) enables algebraic simplifications that require domain knowledge.

### 4. Neural Network Pipeline

```cpp
auto batch = dim("batch");
auto input_d = dim("input", 784);
auto hidden_d = dim("hidden", 256);
auto output_d = dim("output", 10);

auto x = sym("x");
auto W1 = sym("W1");
auto b1 = sym("b1");
auto W2 = sym("W2");
auto b2 = sym("b2");
auto labels = sym("labels");

auto ctx = context(
  x      | shape(batch, input_d),
  W1     | shape(input_d, hidden_d)  | trainable(),
  b1     | shape(hidden_d)           | trainable(),
  W2     | shape(hidden_d, output_d) | trainable(),
  b2     | shape(output_d)           | trainable(),
  labels | shape(batch, output_d)    | observed()
);

// Compute graph
auto h1 = relu(x @ W1 + b1);
auto h2 = softmax(h1 @ W2 + b2);
auto loss = fold<SumReduce>(batch, crossEntropy(h2, labels));

// Symbolic gradients for all trainable parameters
auto g = grad(loss, ctx);
g[W1]     // gradient expression w.r.t. W1
g[b1]     // gradient expression w.r.t. b1

// Context-aware optimization
auto opt_loss = optimize(loss, ctx);  // fuse ops, constant fold, etc.

// Code generation
auto kernel = codegen<CUDA>(opt_loss, ctx);
```

The SAME expression graph and differentiation mechanism serves Bayesian inference
and neural network training. The difference is the context (prior distributions vs
trainable flags) and the interpreter (MCMC sampler vs gradient descent, evaluation vs
code generation).

### 5. Cross-Domain: Bayesian Neural Network

The separation principle shines when domains overlap:

```cpp
auto ctx = context(
  W1 | shape(input_d, hidden_d) | trainable() | prior(normal(0_c, 1_c)),
  b1 | shape(hidden_d) | trainable() | prior(normal(0_c, 1_c)),
  // ... BNN = NN structure + Bayesian priors
);

// Same NN graph, but now inferrable via MCMC
auto posterior = infer(loss, ctx).bind(x = data_x, labels = data_y);
auto weight_samples = posterior.sample(config, rng);
```

No new machinery — the context carries both NN metadata and Bayesian metadata, and
the interpreters use whichever keys they understand.

---

## What Carries Over from symbolic4

### Preserved entirely

| Component | Why it works |
|-----------|-------------|
| **BinderPack pattern** | Overload resolution as dispatch. Zero-cost, type-safe, extensible. Becomes the mechanism for *both* value bindings and metadata contexts |
| **CompressedTuple** | `[[no_unique_address]]` for zero-cost storage of empty types. Foundation for both expression nodes and context entries |
| **Info-domain differentiation** | `diffInfo()` + `simplifyInfo()` pipeline. The breakthrough of symbolic4, promoted to universal mechanism in symbolic5 |
| **Operator overloads** | `x + y` builds `App<AddOp, X, Y>`. Natural syntax for expression construction |
| **`_c` literals** | `42_c` → `Lit<42.0>`. Essential for readable model code |
| **Monoidal reductions** | `SumReduce`, `ProdReduce`, `LogSumExpReduce`. Sound algebraic foundation for all fold operations |
| **Direct recursion evaluation** | `if constexpr` dispatch. Simple, extensible, no framework overhead |
| **Grad with typed rank** | `grad(grad(f))` = rank 2. Rank encoded in type nesting, not argument count |

### Preserved in spirit, reimplemented

| Component | What changes |
|-----------|-------------|
| **Effect system** | `Atom<Id, Effect>` → context metadata. Same *purpose* (dispatch, discovery), different *mechanism* |
| **ReduceOver** | → `Fold`. Same semantics, integrated with dimension system from start |
| **Strategy combinators** | → consteval functions. Same composability, no intermediate types |
| **Auto-discovery** | Context traversal replaces `Sample<D>` pattern matching. More general |
| **Distribution wrappers** | Reimplemented as expression-returning functions, not CRTP classes. `normal(mu, sigma)` returns a struct with a `.logProb(x)` that produces a symbolic expression |

### Deleted

| Component | Why |
|-----------|-----|
| `IndexedSymbol` / `IndexedSample` | Dimensions are context metadata, not node types |
| `DimIndexMap` with `std::type_index` | Dimension sizes come from bindings, not a runtime RTTI map |
| `evaluateIndexed` | One evaluator handles all expressions |
| `LetNode` | CSE is an info-domain rewrite pass, not a graph node |
| `cata.h` / Interpreter pattern | Already dead in symbolic4, confirmed dead in symbolic5 |

---

## Key Decisions & Alternatives

### Decision 1: Metadata in context, not in graph

**Chosen:** `Graph × Context` — separate structures.

**Alternative A: Metadata in the type (`Atom<Id, Effect, Meta...>`).** Every
annotation changes the expression type. Adding "positive" to sigma changes every
expression containing sigma. Compile-time cost scales with annotations × expressions.
Rejected — this is symbolic4's core pain point.

**Alternative B: `Annotated<Expr, MetaMap>` wrapper on every expression.** The graph
and metadata travel together. Cleaner than putting metadata in leaves, but still
couples the graph type to its annotations. Changing metadata changes the wrapper type.
Rejected for the same reason.

**Alternative C: Runtime metadata map.** A `std::unordered_map<TypeId, Metadata>` at
runtime. Maximum flexibility, but breaks compile-time manipulation. Rejected — the
whole point is compile-time optimization.

**Why the chosen approach wins:** The graph type is stable — it changes only when the
*computation* changes, not when annotations change. Contexts are separate types that
compose independently. Different interpreters can use different contexts for the same
graph.

### Decision 2: Info-domain as primary manipulation domain

**Chosen:** All structural manipulation (diff, simplify, rewrite, discover) happens
on `std::meta::info` values. Types are for entry (construction) and exit (evaluation).

**Alternative: Type-level manipulation with reflection as accelerant.** Keep
type-level strategies but use reflection for traits and dispatch. This is what
symbolic4 does today. Rejected — it's a half-measure. Every type-level operation
still pays template instantiation costs.

**Risk:** P2996 `consteval` has limitations — recursion depth, compile-time memory,
error messages. If info-domain manipulation hits compiler limits, we may need to fall
back to type-level operations for some passes. The architecture should accommodate
this gracefully (any manipulation that produces a valid `std::meta::info` can be
spliced, regardless of how it was computed).

### Decision 3: Dimensions as first-class concepts

**Chosen:** `dim()` objects from the start, shapes as context metadata, one evaluator.

**Alternative: Bolt on later (symbolic4's path).** Start with scalar expressions,
add indexing as needed. Rejected — symbolic4 proved this leads to two parallel
systems that never fully unify.

### Decision 4: Delete Effects from the graph

**Chosen:** `Var<Tag>` with no effect. All dispatch information lives in the context.

**Alternative: Keep a minimal effect system.** Just `Free` and `Bound`, to
distinguish "needs a value" from "has a value." This is tempting because evaluation
*does* need to know whether a variable is looked up or computed. But the evaluator
can get this information from the context or from the bindings (if a variable has a
binding, it's "free"; if it's not in the bindings, check the context for a `Compute`
entry). The graph stays clean.

**Risk:** Evaluation might be slightly less efficient without structural dispatch on
the effect. If this matters, we can add a minimal `Free`/`Computed` tag back to
`Var` — but it should be a performance optimization, not a semantic distinction.

### Decision 5: Rename node types

**Chosen:** `Var`, `App`, `Lit`, `Fold` — shorter, more conventional.

**Alternative: Keep symbolic4 names** (`Atom`, `Expression`, `Constant`,
`ReduceOver`). These are established in the codebase and have clear documentation.
Renaming creates churn for anyone familiar with symbolic4.

**Why rename:** symbolic5 is a new system, not a refactor. New names signal new
semantics (especially `Var` vs `Atom` — no more Effect parameter). "App" avoids
confusion with the overloaded word "Expression" (the whole tree is an expression).
"Fold" is standard in functional programming for monoidal reductions.

---

## Open Questions

### 1. How does the context interact with evaluation?

The simplest model: the evaluator takes only `(graph, bindings)` and ignores the
context. This is clean but can't optimize based on metadata (e.g., skip `abs()` for
known-positive values).

The richer model: the evaluator takes `(graph, context, bindings)` and uses metadata
for optimization. But this means compiled evaluation code depends on the context,
not just the graph — changing metadata might change the generated code.

**Tentative answer:** Two signatures. `eval(graph, bindings)` for context-free
evaluation. `eval(graph, ctx, bindings)` for context-aware evaluation. The latter
applies metadata-based optimizations (constant propagation from context, dead code
elimination for known-zero terms, etc.) as an info-domain pass *before* generating
the evaluation function.

### 2. Should `Fold` carry its dimension, or should dimensions be context-only?

The proposal puts dimensions in `Fold<SumReduce, Dim, Body>` — the dimension is part
of the graph. This is necessary because the fold *structure* depends on which
dimension is being reduced. But it means dimension types appear in expression types,
which creates a coupling.

**Alternative:** Dimensions are always context. `Fold<SumReduce, Body>` reduces over
"whatever dimension the context says." But this is ambiguous when an expression has
multiple reducible dimensions.

**Tentative answer:** Keep dimensions in `Fold`. The dimension *is* structural — it
determines loop nesting order and expression semantics. Shape metadata in the context
is for type-checking and optimization, not for determining *what* the expression
computes.

### 3. How do opaque operations work?

Matrix decompositions (Cholesky, QR), special functions (Bessel, erf), and
platform-specific operations (cuBLAS matmul) can't be expressed as composition of
elementary operators. They need "opaque" nodes that the expression graph treats as
black boxes.

**Tentative design:** Opaque nodes are `App` nodes with special operator types:

```cpp
template <typename MatExpr>
struct CholeskyOp {
  // Evaluation: call dense routine
  auto operator()(DynamicDense& A) const { return cholesky(A); }

  // Differentiation: known rule, implemented in info domain
  // d(chol(A)) requires the Cholesky-specific derivative formula
};
```

The operator type carries both the evaluation function and the differentiation rule.
The info-domain differentiator queries the operator for its rule, just as it queries
`AddOp` for the sum rule.

### 4. Can the context carry runtime values?

The proposal describes contexts as compile-time maps. But some metadata is inherently
runtime: "the dimension size is 1000" isn't known until data is loaded.

**Tentative answer:** Contexts are compile-time for structural metadata (types,
shapes, properties). Runtime values flow through bindings, not contexts. Dimension
*sizes* are runtime values bound at evaluation time, but dimension *identities* are
compile-time types in the context. This maintains the two-stage computation model:
compile-time structure, runtime data.

### 5. What's the migration path from symbolic4?

symbolic5 doesn't replace symbolic4 overnight. The practical path:
1. Build the core expression graph (`Var`, `App`, `Lit`, `Fold`) and context system
2. Port info-domain differentiation from symbolic4 (minimal changes)
3. Build the unified evaluator with dimension support
4. Port Bayesian domain (distributions, discovery, MCMC) as the first context
5. Deprecate symbolic4 once symbolic5 covers all its functionality

The two systems can coexist during migration — they share no types.

---

## Summary

| Aspect | symbolic4 | symbolic5 |
|--------|-----------|-----------|
| **Graph nodes** | `Atom<Id, Effect>`, `Expression`, `Constant`, `ReduceOver` | `Var<Tag>`, `App`, `Lit`, `Fold` |
| **Metadata** | Embedded in Effect (homeless) | Separate Context system |
| **Manipulation domain** | Mixed type/info | Info-first |
| **Dimensions** | Bolted on (`IndexedSymbol`, `IndexedSample`, `DimIndexMap`) | First-class (`dim()`, shape context, one evaluator) |
| **Strategies** | Type-level Stratego combinators | consteval function combinators |
| **Discovery** | Pattern-match on `Sample<D>` atoms | Context traversal with any metadata key |
| **Evaluators** | Two (`eval`, `evaluateIndexed`) | One |
| **Domain coupling** | Core knows about distributions | Core is domain-independent |

The fundamental bet: **separating graph structure from domain metadata, with C++26
reflection as the manipulation substrate, produces a system that is both more general
and simpler than symbolic4.** The generality comes from the Context mechanism — any
domain can annotate any graph without modifying the core. The simplicity comes from
the info domain — one manipulation substrate replaces the type/info split.
