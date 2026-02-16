# symbolic5 Design

## Expr

One type. Every symbolic expression — a lone variable, a matrix multiply, a full
Bayesian model — is an `Expr`.

```cpp
struct Expr {
  std::meta::info tree;   // expression structure as reflected type-level AST
  std::meta::info meta;   // annotation map, travels with the expression
};
```

Both fields are `std::meta::info` — consteval-only values. Expressions exist at
compile time. At the runtime boundary, the tree splices to a type-level AST for
zero-cost evaluation. The `Expr` itself never touches runtime.

**Why one type:** In symbolic4, `App<AddOp, Var<X>, Var<Y>>` and
`App<MulOp, Var<Z>, Lit<3>>` are different types. A model with 50 parameters and
nested plates instantiates thousands of unique expression types. With `Expr`, the
number of C++ type instantiations is bounded by the number of leaf and operator
*kinds* (Symbol, Constant, AddOp, SinOp...), not the number of expression *instances*.
All the structural variation lives in consteval values, not types.

**Why annotations travel with the expression:** If graph and metadata are separate
objects, users must track both and pass both to every operation. That's friction.
Annotations propagate through composition automatically — when you write
`annotated_sigma * x`, the result carries sigma's annotations forward.

### Construction

```cpp
auto x = sym("x");                       // leaf: Var<λ₁>
auto f = x * x + sin(x);                 // Expr (one type for everything)
auto g = 2_c * x;                        // Expr (Lit<2.0> composed with Var)

auto sigma = sym("σ") | positive();      // Expr with annotation
auto h = log(sigma);                      // carries sigma's annotation
```

Operator overloads build the tree incrementally. `x + y` calls `substitute` to
construct `^^App<AddOp, Var<X>, Var<Y>>` in the info domain and returns an `Expr`
holding that info. Chained operations nest:

```cpp
auto a = x + y;    // Expr{tree: ^^App<AddOp, Var<X>, Var<Y>>}
auto b = a * x;    // Expr{tree: ^^App<MulOp, App<AddOp, Var<X>, Var<Y>>, Var<X>>}
```

The full type-level AST exists as an `info` value — never instantiated as a C++
type until the splice boundary.

### The Symbolic concept

```cpp
template <typename T>
concept Symbolic = is_var<T> || is_lit<T> || std::same_as<T, Expr>;

template <Symbolic L, Symbolic R>
consteval Expr operator+(L lhs, R rhs) {
  return Expr{
    substitute(^^App, {^^AddOp, toInfo(lhs), toInfo(rhs)}),
    mergeAnnotations(metaOf(lhs), metaOf(rhs))
  };
}
```

`toInfo` converts any symbolic value to its tree info:
- `Var<Tag>{}` → `^^Var<Tag>`
- `Lit<V>{}` → `^^Lit<V>`
- `Expr e` → `e.tree`

`metaOf` extracts annotations:
- `Var<Tag>{}` → empty (unannotated leaf)
- `Expr e` → `e.meta`

---

## Leaves

A leaf is any type that appears at the terminal positions of the tree. The core
defines two. Domains add their own. The core never enumerates all possible leaves —
it handles them structurally.

### Core leaves

```cpp
// Named variable — identity from unique lambda type, display name from string
template <typename Tag>
struct Symbol {
  static constexpr std::string_view name = /* from Tag */;

  template <typename V>
  constexpr auto operator=(V value) const;  // creates Binder for BinderPack
};

// Compile-time constant value
template <auto V>
struct Constant {};

// Rational constant (exact arithmetic, auto-reduces)
template <auto N, auto D>
struct Rational {};
```

Factory:

```cpp
template <typename Tag = decltype([] {})>
consteval auto sym(std::string_view name) {
  return Symbol<Tag>{name};
}

// User-defined literal: 42_c → Constant<42.0>
```

### Domain leaves

Any template can serve as a leaf. The core recognizes leaves by the absence of
children — if `template_arguments_of(info)` yields types that aren't expression
nodes (or the type isn't an `App`), it's a leaf.

```cpp
// Bayesian domain — random variable leaf
template <typename Dist>
struct Sample {};

// Named subexpression — for explicit sharing / let-binding
template <typename Tag, typename DefExpr>
struct NamedExpr {};
```

The core doesn't import these. It handles them generically:

| Core operation | Behavior on unknown leaf |
|---------------|-------------------------|
| **Traversal** | Stop (leaf has no children) |
| **Differentiation** | Return 1 if leaf matches variable, 0 otherwise |
| **Evaluation** | Look up in bindings via splice + BinderPack dispatch |
| **Stringify** | Print the type name (or query annotation for display name) |

Domain-specific behavior (auto-discovery, log-prob collection, transform selection)
lives in domain interpreters that pattern-match on leaf types in the info domain:

```cpp
consteval bool isSample(std::meta::info leaf) {
  return is_specialization_of(leaf, ^^Sample);
}
```

---

## Operators

An operator is a type that defines a computation over its children. Operators live
in `App<Op, Children...>` nodes.

### Structure

```cpp
// Binary
template <typename Op, typename Lhs, typename Rhs>
struct App;  // general: App<Op, Args...>

// Operators are stateless types with associated behavior:
struct AddOp {
  static constexpr std::size_t arity = 2;
  static constexpr std::string_view symbol = "+";

  // Runtime evaluation
  static constexpr auto apply(auto lhs, auto rhs) { return lhs + rhs; }
};
```

### Fold operators — reductions as ops

Folds are operators parameterized by a dimension tag and a monoidal combine rule.
The body expression is the single child. The dimension lives in the operator type,
not as a child node — because a dimension isn't an expression you evaluate; it's
a parameter controlling the fold's iteration.

```cpp
template <typename DimTag>
struct SumFoldOp {
  static constexpr std::size_t arity = 1;  // body only
  static constexpr auto identity = 0.0;

  static constexpr auto combine(auto a, auto b) { return a + b; }
};

template <typename DimTag>
struct ProdFoldOp {
  static constexpr std::size_t arity = 1;
  static constexpr auto identity = 1.0;

  static constexpr auto combine(auto a, auto b) { return a * b; }
};
```

Usage:

```cpp
auto obs = dim("obs");   // Dim<λ_obs>
auto lp = sumOver(obs, logNormal(y, mu, sigma));
// tree: ^^App<SumFoldOp<ObsTag>, App<LogNormalOp, Symbol<Y>, Symbol<Mu>, Symbol<Sigma>>>
```

The evaluator handles fold ops by looping over the dimension's runtime size:

```cpp
// Pseudocode in the generated eval function:
if constexpr (is_fold_op_v<Op>) {
  auto accum = Op::identity;
  for (std::size_t i = 0; i < dimSize; ++i)
    accum = Op::combine(accum, evalBody(i));
  return accum;
}
```

Nested folds nest naturally:

```cpp
sumOver(obs, sumOver(features, body))
// tree: ^^App<SumFoldOp<Obs>, App<SumFoldOp<Features>, Body>>
```

### Differentiation rules on operators

The core differentiator knows rules for built-in math ops (add, mul, sin, exp, log,
pow). For fold ops and domain ops, the operator type provides its own rule:

```cpp
template <typename DimTag>
struct SumFoldOp {
  // ...

  // ∂(Σ_d f)/∂x = Σ_d (∂f/∂x) — linearity of summation
  static consteval auto diff(std::meta::info body, std::meta::info var) {
    return substitute(^^App, {^^SumFoldOp<DimTag>, diffInfo(body, var)});
  }
};
```

The differentiator dispatches:
1. Check built-in rules (AddOp, MulOp, SinOp, ...)
2. Check if the op type has a `diff` static member (via reflection: `has_member(op, "diff")`)
3. If neither: error — undifferentiable operator

This makes the differentiation system open. A domain defines an op with a custom diff
rule, and the core differentiator picks it up without modification.

### Domain operators

```cpp
// Matrix domain
struct MatMulOp {
  static constexpr std::size_t arity = 2;
  static constexpr auto apply(auto A, auto B) { return multiply(A, B); }

  // ∂(A·B)/∂A = ... (matrix calculus rule)
  static consteval auto diff(std::meta::info children, std::meta::info var) { ... }
};

// Neural network domain
struct ReLUOp {
  static constexpr std::size_t arity = 1;
  static constexpr auto apply(auto x) { return x > 0 ? x : 0; }

  // ∂ReLU(f)/∂x = (f > 0) · ∂f/∂x
  static consteval auto diff(std::meta::info body, std::meta::info var) { ... }
};
```

---

## Annotations

Annotations are a compile-time map from `(node identity, key)` → `value`, stored
as a reflected type in `Expr::meta`. They propagate through expression composition
via merge.

### Attaching annotations

The `|` operator annotates a symbol or expression:

```cpp
auto sigma = sym("σ") | positive();
auto mu = sym("μ") | prior(normal(0_c, 10_c)) | bounds(0_c, 100_c);
```

Each `|` produces an `Expr` with the annotation added to `meta`. Multiple `|` chain.

### Propagation through composition

When expressions compose, their annotations merge:

```cpp
auto f = mu * mu + log(sigma);
// f.meta = merge(mu's annotations, sigma's annotations)
// f carries: μ has Prior(Normal(0,10)), σ is Positive
```

The merge is by symbol identity. Two annotations for the same `(symbol, key)` pair
from different sources is a conflict — the library reports an error at consteval time.

### Annotation keys

Keys are ordinary types. The core defines none — even `Positive` and `Prior` are
domain-provided. This keeps the core free of domain vocabulary.

Example domain keys:

```cpp
// General-purpose (could be a shared utility, not core)
struct Bounds {};       // value: Positive, Unit, Bounded<lo, hi>
struct DisplayName {};  // value: string_view wrapper

// Bayesian domain
struct Prior {};        // value: a distribution expression (info)
struct Observed {};     // value: true_type / false_type
struct Support {};      // value: Real, PositiveReal, UnitInterval

// Matrix domain
struct Shape {};        // value: Shape<Dim1, Dim2, ...>
struct Structure {};    // value: Symmetric, PositiveDefinite, Triangular

// Neural network domain
struct Trainable {};    // value: true_type
struct Dtype {};        // value: Float16, Float32, BFloat16
```

### Querying annotations

Consteval functions inspect annotations during manipulation:

```cpp
consteval bool isPositive(Expr e, std::meta::info symbol) {
  return hasAnnotation(e.meta, symbol, ^^Bounds, ^^Positive);
}
```

The differentiator can use this for smarter simplification:
- `abs(sigma)` → `sigma` when sigma has `Positive` bound
- `sqrt(x²)` → `x` when x is non-negative

### Expression-level annotations

Most annotations attach to symbols (leaves). For interior nodes, use `assume`:

```cpp
auto f = assume(complex_expr, positive());
// f.meta gains an entry keyed by the root node's info
```

Interpreters check the root node's annotations. `propagate(expr)` can derive
expression-level properties from leaf properties when the math supports it (positive
× positive = positive, exp(anything) = positive, etc.).

### Representation

`meta` reflects a type encoding the annotation map:

```
meta = ^^AnnotationList<
  Entry<Symbol<Mu>, Prior, NormalDist<Constant<0>, Constant<10>>>,
  Entry<Symbol<Sigma>, Bounds, Positive>
>
```

Operations on the map (`merge`, `add`, `query`, `has`) use `template_arguments_of`
to iterate entries and `substitute` to build new lists. All consteval, no allocation.

---

## Dimensions

Dimensions are compile-time tags identifying axes of indexed data. They are not
expression nodes — they're parameters of fold operators and metadata on symbols.

### Construction

```cpp
auto obs = dim("obs");               // Dim<λ₁> (runtime-sized)
auto features = dim("features");     // Dim<λ₂>
auto hidden = dim("hidden", 256);    // Dim<λ₃, 256> (compile-time-sized)

template <typename Tag = decltype([] {}), auto Size = kDynamic>
consteval auto dim(std::string_view name) {
  return Dim<Tag, Size>{name};
}
```

### Where dimensions appear

1. **Fold operators** — `SumFoldOp<DimTag>` parameterized by the dimension tag
2. **Symbol annotations** — `sym("θ") | shape(countries)` means θ varies over countries
3. **Bindings** — `bind(y = data_span)` provides the runtime size for y's dimension

Dimensions are NOT tree nodes. You never evaluate a dimension. They control
iteration (in folds) and describe structure (in annotations).

### Indexed access (gather / composition)

If `theta : Countries → ℝ` and `idx : Obs → Countries`, then `theta[idx]` is
function composition: `theta ∘ idx : Obs → ℝ`.

```cpp
auto theta = sym("θ") | shape(countries);
auto district_idx = sym("idx") | shape(obs);

auto theta_i = gather(theta, district_idx);
// tree: ^^App<GatherOp, Symbol<Theta>, Symbol<Idx>>
```

`GatherOp` is a binary operator: first child is the indexed expression, second is
the index. The evaluator implements it as `theta[idx[i]]` in the inner loop.

### Matrix multiply as fold + gather

Matrix multiply emerges from the dimension system:

```
C(i,j) = Σ_k A(i,k) · B(k,j)
```

```cpp
auto k = dim("k");
auto C = sumOver(k, A * B);  // broadcasting handles the dimension alignment
```

The dimension system unifies plates (statistical), contractions (linear algebra), and
reductions (general). One mechanism.

---

## The Splice Boundary

Expressions live at compile time. Evaluation happens at runtime. The splice boundary
converts an `Expr`'s info-domain tree into a type-level AST that drives zero-cost
`if constexpr` dispatch.

### How it works

```cpp
constexpr auto f = x*x + sin(y);  // Expr (compile-time value)

// At the evaluation boundary:
using F = [:f.tree:];
// F = App<AddOp, App<MulOp, Symbol<X>, Symbol<X>>, App<SinOp, Symbol<Y>>>

auto result = evalType(F{}, bind(x = 3.0, y = 1.5));
// evalType is symbolic4-style if constexpr recursion — zero-cost at runtime
```

The splice `[:f.tree:]` instantiates the full type-level AST *once*. The type drives
the evaluator, which the compiler optimizes to pure arithmetic.

```
compile time:  Expr value ─── manipulate ──→ Expr value
                   │                             │
                   │ [:splice:]                  │ [:splice:]
                   ▼                             ▼
runtime:       Type AST ─── if constexpr ──→ double
```

### eval wraps the boundary

Users don't write splices. `eval` handles it:

```cpp
template <Symbolic E, typename... Bindings>
constexpr auto eval(E expr, Bindings... bindings) {
  constexpr auto tree = toInfo(expr);
  using Tree = [:tree:];
  return evalType(Tree{}, BinderPack{bindings...});
}
```

### Manipulation stays in info domain

All structural operations — differentiation, simplification, substitution,
discovery — operate on `Expr` values. No reflect step. No intermediate type
instantiations.

```cpp
constexpr auto f = x*x + sin(x);
constexpr auto df = diff(f, x);          // Expr → Expr (consteval)
constexpr auto df2 = simplify(df);       // Expr → Expr (consteval)
auto result = eval(df2, bind(x = 3.0));  // splice + runtime eval
```

Only the final expression that reaches `eval` triggers type instantiation. All
intermediate work is consteval computation on info values.

---

## Extensibility

The core provides traversal, composition, the splice boundary, and built-in
differentiation rules for standard math ops. Everything else is extensible.

### Adding a new leaf type

Define a template. That's it.

```cpp
// In your domain header (not core):
template <typename Dist>
struct Sample {};
```

The core handles it generically. Your domain interpreter recognizes it:

```cpp
// Domain-specific discovery:
consteval auto collectSamples(Expr e) -> std::vector<std::meta::info> {
  std::vector<std::meta::info> result;
  walkLeaves(e.tree, [&](std::meta::info leaf) {
    if (is_specialization_of(leaf, ^^Sample))
      result.push_back(leaf);
  });
  return result;
}
```

### Adding a new operator

Define a type with `arity`, `apply`, and optionally `diff`:

```cpp
struct SoftmaxOp {
  static constexpr std::size_t arity = 1;

  static auto apply(auto x) { return softmax(x); }

  static consteval auto diff(std::meta::info body, std::meta::info var) {
    // Jacobian-vector product rule for softmax
    // ...
  }
};

// Factory:
consteval Expr softmax(Expr e) {
  return Expr{
    substitute(^^App, {^^SoftmaxOp, e.tree}),
    e.meta  // annotations pass through
  };
}
```

### Adding new annotation keys

Define types:

```cpp
struct Activation {};  // key
struct GELU {};        // value

// Usage:
auto h = sym("h") | annotate<Activation, GELU>();
```

### Adding a new interpreter

Write a consteval function over `std::meta::info`:

```cpp
// Code generation interpreter:
consteval std::string codegen_cuda(Expr e) {
  auto tree = e.tree;
  if (is_specialization_of(tree, ^^App)) {
    auto op = template_arguments_of(tree)[0];
    auto children = /* rest */;
    if (op == ^^AddOp) return codegen_cuda(children[0]) + " + " + codegen_cuda(children[1]);
    // ...
  }
  if (is_specialization_of(tree, ^^Symbol)) {
    return /* variable name from annotation or type */;
  }
  // ...
}
```

---

## Worked Example: Sample Without Touching Core

### Step 1: Define the leaf

```cpp
// bayesian/sample.h
namespace tempura::bayesian {

template <typename Dist>
struct Sample {};

}  // namespace tempura::bayesian
```

### Step 2: Define the factory

```cpp
// bayesian/random_var.h
namespace tempura::bayesian {

// sample(dist) creates an Expr with a Sample leaf
// The dist's parameters (themselves Exprs) are captured in the Sample type
template <typename Dist>
consteval Expr sample(Dist dist) {
  // Dist is something like NormalDist<Symbol<Mu>, Symbol<Sigma>>
  // We need a unique identity for this random variable
  using Tag = decltype([] {});
  using SampleLeaf = Sample<Tag, Dist>;

  return Expr{
    ^^SampleLeaf,
    // Annotations: mark this as a sample, record the distribution
    makeAnnotation(^^SampleLeaf, ^^IsSample, ^^Dist)
  };
}

}  // namespace tempura::bayesian
```

### Step 3: Define domain operations

```cpp
// bayesian/discover.h
namespace tempura::bayesian {

// Collect all Sample leaves from an expression
consteval auto discoverSamples(Expr e) {
  std::vector<std::meta::info> samples;
  walkLeaves(e.tree, [&](std::meta::info leaf) {
    if (is_specialization_of(leaf, ^^Sample))
      samples.push_back(leaf);
  });
  return samples;
}

// Collect joint log-probability
consteval Expr collectLogProbs(Expr observed, Expr model_expr) {
  auto samples = discoverSamples(model_expr);
  auto joint = model_expr;
  for (auto sample_info : samples) {
    auto dist_info = template_arguments_of(sample_info)[1];  // extract Dist
    auto lp = logProbExpr(dist_info, sample_info);           // domain function
    joint = joint + lp;
  }
  return joint;
}

}  // namespace tempura::bayesian
```

### Step 4: Use in a model

```cpp
auto mu = sym("μ") | prior(normal(0_c, 10_c));
auto sigma = sym("σ") | prior(halfNormal(5_c)) | positive();
auto obs = dim("obs");

auto y = sample(normal(mu, sigma)) | shape(obs) | observed();

// collectLogProbs traverses y's tree, finds Sample<Normal<mu, sigma>>,
// then recursively finds mu's and sigma's priors from annotations
auto joint_lp = collectLogProbs(y);

// Differentiate — core operation, domain-unaware
auto grad_mu = diff(joint_lp, mu);

// Evaluate — splices to type at the boundary
eval(grad_mu, bind(mu = 3.0, sigma = 1.0, y = data_span, obs = n));
```

**The core never imported `Sample`, `Prior`, or any Bayesian concept.** It traversed
the tree structurally, differentiated using standard calculus rules (Sample leaves
are just variables — `∂/∂x` returns 1 or 0 based on identity match), and spliced to
a type for evaluation. The Bayesian domain provided `sample()`, `collectLogProbs()`,
`prior()`, and `positive()` — all built on the core's extensibility points.

---

## Worked Example: Full Model

```cpp
// Symbols
auto mu = sym("μ") | prior(normal(0_c, 10_c));
auto sigma = sym("σ") | prior(exponential(1_c)) | positive();
auto theta = sym("θ") | prior(normal(0_c, 1_c));

// Dimensions
auto districts = dim("districts");
auto obs = dim("obs");

// Indexed symbols (dimension in annotation, not in symbol type)
auto z = sym("z") | shape(districts) | prior(normal(0_c, 1_c));
auto district_idx = sym("idx") | shape(obs);

// Non-centered parameterization: a = mu + sigma * z
auto a = mu + sigma * z;

// Likelihood
auto y_hat = gather(a, district_idx);  // a[district_idx[i]] for each observation
auto y = sample(bernoulli(logistic(y_hat))) | shape(obs) | observed();

// Log-probability: auto-discovered from expression tree + annotations
auto joint_lp = sumOver(obs, logProb(y))
              + sumOver(districts, logProb(z))
              + logProb(mu) + logProb(sigma);

// Or: let the domain auto-collect everything
auto joint_lp = collectLogProbs(y);

// Gradients
auto g = grad(joint_lp);
g[mu]           // scalar gradient expression
g[sigma]        // scalar gradient expression (through log transform)
g[z]            // indexed gradient (family over districts)

// Evaluate
eval(g[mu], bind(mu = 0.0, sigma = 1.0, z = z_vec,
                  district_idx = idx_data, y = y_data,
                  districts = n_districts, obs = n_obs));
```

---

## Open Design Questions

### 1. Does `substitute` trigger template instantiation?

The entire design depends on `substitute(^^App, {^^AddOp, ...})` creating an info
value WITHOUT fully instantiating `App<AddOp, ...>` as a C++ type. If substitute
eagerly instantiates, the compile-time savings vanish.

P2996 implementations vary. Bloomberg's Clang fork likely defers instantiation until
the type is used (splice, sizeof, member access). Needs verification.

**Fallback:** If substitute is expensive, use a custom consteval AST node type
(`ExprNode` with op + children vector) for manipulation, and only call `substitute`
at the splice boundary to build the final type.

### 2. Annotation map representation

`meta` reflects a type like `AnnotationList<Entry<K1,V1>, Entry<K2,V2>, ...>`. Every
annotation add/merge builds a new type via `substitute`. For large annotation maps,
this could be expensive.

**Alternative:** Store annotations as a flat consteval structure (consteval-only
struct with `std::vector<AnnotationEntry>`) rather than as a reflected type.
Requires non-transient constexpr allocation (C++26 P2738). Check compiler support.

### 3. How does NamedExpr (let-binding) work?

`NamedExpr<Tag, DefExpr>` encodes "this subexpression has identity `Tag` and
definition `DefExpr`." Multiple references to the same `Tag` share the computation.

**Open question:** Should the definition live in the tree type (`NamedExpr<Tag, Def>`)
or in annotations (`NamedExpr<Tag>` + annotation mapping Tag → definition)?

Tree: self-contained, but changes the tree type every time the definition changes.
Annotations: cleaner tree, but the definition is indirect.

Both work in the info-native model (neither affects the `Expr` C++ type). Leaning
toward tree — it's simpler and the info-native approach means the tree type is
internal anyway.

### 4. How do bindings provide dimension sizes?

When `eval` encounters a `SumFoldOp<ObsTag>`, it needs the runtime size of the
`obs` dimension. Where does that come from?

**Option A:** Explicit dim bindings: `bind(obs = 100, y = data_span)`

**Option B:** Inferred from data: `bind(y = data_span)` implies
`obs = data_span.size()` because y's annotation says `shape(obs)`.

Option B is more ergonomic but requires the evaluator to read annotations. Option A
is simpler. Both should be supported — B as sugar over A.

### 5. Annotation conflict resolution

When composing `f = a + b` where `a` annotates symbol X with `Prior(Normal(0,1))`
and `b` annotates symbol X with `Prior(Normal(0,10))`, what happens?

Current stance: **consteval error** on conflict. Annotations must agree. This
prevents silent overwrites.

**Alternative:** Last-writer-wins. Simpler, but hides bugs.

**Alternative:** Explicit override syntax:
`f | override(x, prior(normal(0_c, 10_c)))` for intentional changes.

### 6. Op arity for variadic functions

Some operations have variable arity: `max(a, b, c)`, `polynomial(coeffs...)`. The
current model assumes fixed arity in the Op type.

**Option:** Variadic ops have `arity = kVariadic`. The tree node
`App<MaxOp, A, B, C>` has three template arguments after the Op. `substitute` builds
it with a variable-length argument list.

### 7. How does the evaluator handle domain leaves it doesn't know?

The splice boundary converts the info tree to a type-level AST. The evaluator
dispatches via `if constexpr`. Unknown leaves hit the `else` branch.

**Default behavior:** Look up in bindings. `bindings[UnknownLeaf{}]` resolves via
BinderPack overload resolution. If the user provided a binding for that leaf type,
it works. If not, compile error.

This means domain leaves like `Sample<Dist>` just need bindings:
```cpp
eval(expr, bind(sample_leaf = 3.5));  // user provides the value
```

For MCMC, the sampler provides bindings for all Sample leaves automatically.
