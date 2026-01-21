# Design: symbolic4 — AST-as-Types with Pluggable Interpreters

## The Problem

We want to write mathematical expressions in C++ and do multiple things with them:

```cpp
auto expr = x * x + 2 * x + 1;

// Different operations on the SAME expression:
double value = evaluate(expr, {x = 3.0});     // → 16.0
auto deriv = diff(expr, x);                    // → 2*x + 2
auto simple = simplify(expr);                  // → (x + 1)²
std::string s = toString(expr);                // → "x² + 2x + 1"
double lp = logProb(model, trace);             // → sum of log-probabilities
```

The naive approach — implementing each operation with hand-written recursion — leads to:
- Duplicated traversal logic across evaluate, diff, simplify, toString, etc.
- No way to compose operations (simplify then evaluate)
- No way to validate that an expression is well-formed for a specific DSL
- Bugs from inconsistent handling of edge cases

**Goal**: Factor out the common traversal patterns so adding a new operation is just defining "what to do at each node", not "how to walk the tree".

---

## The Core Insight: Atoms as Effectful Placeholders

The deepest abstraction underlying this design:

> **Atoms are effectful placeholders. The binding strategy determines how to resolve them. Interpreters are effect handlers.**

An atom is a leaf node in an expression tree — something that can't be broken down further. But different atoms have different *effects* when evaluated:

| Atom | Identity | Effect | Resolution Strategy |
|------|----------|--------|---------------------|
| `Symbol<Id>` | Yes | **Lookup** | Look up `Id` in bindings |
| `Literal<T>` | No | **Intrinsic** | Return the embedded value |
| `Constant<V>` | No | **Intrinsic** | Return the compile-time value |
| `RandomVar<Id, D>` | Yes | **Sample** | Draw from distribution `D`, bind to `Id` |
| `DeterministicVar<Id, E>` | Yes | **Compute** | Evaluate expression `E`, bind to `Id` |

The pattern: `Atom<Identity, BindingStrategy>`

```cpp
// The unified abstraction
template <typename Id, typename Strategy>
struct Atom : SymbolicTag {
  [[no_unique_address]] Strategy strategy_;
};

// Binding strategies (the "effects")
struct Lookup {};                                   // Look up in environment
template <typename T> struct Intrinsic { T value; };// Value embedded
template <typename D> struct Sample { D dist_; };   // Sample from distribution
template <typename E> struct Compute { E expr_; };  // Evaluate expression

// The atoms we use are instantiations of this unified type:
template <typename Id>
using Symbol = Atom<Id, Lookup>;

template <typename T>
using Literal = Atom<void, Intrinsic<T>>;  // void = no identity

template <typename Id, typename D>
using RandomVar = Atom<Id, Sample<D>>;

template <typename Id, typename E>
using DeterministicVar = Atom<Id, Compute<E>>;
```

### Identity vs Anonymous

The fundamental split in atom types:

| Has Identity (`Id` type) | Anonymous (`void` or no Id) |
|--------------------------|------------------------------|
| Can be shared in DAG | Computed inline |
| Appears in trace | No trace entry |
| Can be deduplicated | Always re-evaluated |
| `Symbol`, `RandomVar`, `DeterministicVar` | `Literal`, `Constant`, `Fraction` |

**Why identity matters:**

1. **DAG sharing**: When `mu` appears in multiple places, we want ONE sample, not many
2. **Trace inspection**: We can query `trace[mu]` to see sampled values
3. **Deduplication**: `foldUnique` skips already-visited identities

**Why anonymous matters:**

1. **No overhead**: `Literal{3.14}` doesn't need tracking machinery
2. **Value IS identity**: Two `Literal{3.14}` are interchangeable
3. **Inline computation**: No indirection through trace lookup

### Interpreters as Effect Handlers

An interpreter handles each binding strategy:

```cpp
struct Evaluator {
  // Handle Lookup: look up in bindings
  template <typename Id>
  static auto handleAtom(Atom<Id, Lookup>, auto& ctx) {
    return ctx.bindings[Id{}];
  }

  // Handle Intrinsic: return the embedded value
  template <typename T>
  static auto handleAtom(Atom<void, Intrinsic<T>> atom, auto&) {
    return atom.strategy_.value;
  }

  // Handle Compute: evaluate the expression
  template <typename Id, typename E>
  static auto handleAtom(Atom<Id, Compute<E>> atom, auto& ctx) {
    return evaluate(atom.strategy_.expr_, ctx);
  }
};

struct Sampler {
  // Handle Sample: sample from distribution, bind result
  template <typename Id, typename D>
  static auto handleAtom(Atom<Id, Sample<D>> atom, auto& ctx) {
    auto params = evaluateParams(atom.strategy_.dist_, ctx.trace);
    auto value = sample(atom.strategy_.dist_, params, ctx.rng);
    ctx.trace.bind(Id{}, value);
    return value;
  }

  // Handle Compute: evaluate expression, bind result
  template <typename Id, typename E>
  static auto handleAtom(Atom<Id, Compute<E>> atom, auto& ctx) {
    auto value = evaluate(atom.strategy_.expr_, ctx.trace);
    ctx.trace.bind(Id{}, value);
    return value;
  }
};

struct LogProbAccumulator {
  // Handle Sample: contribute log-prob
  template <typename Id, typename D>
  static auto handleAtom(Atom<Id, Sample<D>> atom, auto& ctx) {
    return atom.strategy_.dist_.unnormalizedLogProb(Symbol<Id>{});
  }

  // Handle Compute: contribute nothing (deterministic)
  template <typename Id, typename E>
  static auto handleAtom(Atom<Id, Compute<E>>, auto&) {
    return Literal{0.0};
  }
};
```

**Effect polymorphism**: The same `RandomVar` behaves differently under different interpreters:
- Under `Sampler`: draws a value
- Under `LogProbAccumulator`: returns symbolic log-prob expression
- Under `Evaluator`: looks up in trace

This is the connection between symbolic computation and algebraic effects. The expression tree declares what effects it needs; the interpreter (effect handler) decides how to provide them.

### No Explicit Parent Tracking

Dependencies are **implicit in the expression structure**. When we need to traverse the DAG:

```cpp
// Extract all Symbol<Id> types referenced in an expression
template <Symbolic E>
using referenced_ids = /* type-level set of Ids found in E */;

// For RandomVar<Id, Dist<Normal, Symbol<Id_mu>, Symbol<Id_sigma>>>:
// referenced_ids = IdSet<Id_mu, Id_sigma>
```

The interpreter can compute dependencies by inspecting which `Symbol<Id>`s appear in an atom's strategy:

- `Sample<Dist<Normal, Symbol<Id_mu>, Literal<1.0>>>` depends on `Id_mu`
- `Compute<Expression<AddOp, Symbol<Id_a>, Symbol<Id_b>>>` depends on `Id_a`, `Id_b`

This is cleaner than threading parent tuples everywhere. The DAG structure is **derived from types**, not stored.

### Giving Identity to Expressions: `let`

Sometimes you have an anonymous expression but want to give it identity:

```cpp
// Anonymous: just math, no identity
auto expr = mu.sym() / sigma.sym();

// With identity: now it's a DAG node
auto z = let(expr);  // DeterministicVar with fresh Id
```

Implementation:

```cpp
// Give identity to any symbolic expression
template <typename Id = decltype([] {}), Symbolic Expr>
constexpr auto let(Expr expr) {
  return Atom<Id, Compute<Expr>>{{expr}};
}

// Usage examples:
auto z_score = let((x - mu) / sigma);     // Named intermediate
auto log_odds = let(log(p / (1 - p)));    // Can appear in trace
auto shared = let(expensive_computation); // Computed once, shared
```

---

## Expression Templates: Types-Only Representation

### Why Types-Only?

We encode expressions **entirely in the type system**:

```cpp
// Expression IS a type, not a value
using Expr = Expression<AddOp,
    Expression<MulOp, Symbol<X>, Symbol<X>>,  // x * x
    Expression<MulOp, Literal<2>, Symbol<X>>  // 2 * x
>;

// No runtime storage needed — the structure IS the type
constexpr Expr expr{};  // Empty object, all info in type
```

**Pros**: Zero runtime overhead, full constexpr, compile-time optimization
**Cons**: Can't build expressions at runtime (rarely needed for our use cases)

For **symbolic differentiation** and **Bayesian inference**, expressions are:
1. Known at compile time (model definitions, not user input)
2. Traversed multiple times (evaluate, differentiate, accumulate log-prob)
3. Performance-critical (inner loops of MCMC samplers)

Type-level representation means:
- **Zero storage**: `sizeof(Expression<...>) == 1` (empty class optimization)
- **Zero dispatch**: No virtual calls, no variant visits — just `if constexpr`
- **Full constexpr**: Differentiation and simplification happen at compile time
- **Optimizer visibility**: Compiler sees the entire expression structure

### Contrast: Runtime Values (Boost.YAP style)

YAP stores expression trees as **runtime data** in tuples:

```cpp
// YAP: expression is a runtime value
template <expr_kind Kind, typename Tuple>
struct expression {
    static const expr_kind kind = Kind;
    Tuple elements;                       // Children stored in hana::tuple
};
```

**Pros**: Familiar value semantics, can store runtime-computed expressions
**Cons**: Runtime overhead (tuple storage, pointer chasing), limited constexpr

---

## Theoretical Foundations

### Free Algebras

A **free algebra** T(Σ,X) over signature Σ is all well-formed terms with no equations imposed — pure syntax. Our type-level AST is exactly this: `Expression<AddOp, A, B>` represents "A + B" without any simplification.

The **universal property**: any assignment of variables extends uniquely to a homomorphism. This is why interpreters work — once you define what to do for each symbol, the interpretation of the whole expression is determined.

### F-Algebras and Catamorphisms

For functor F, an **F-algebra** is a pair (A, α : F(A) → A). The **initial algebra** is the recursive type itself. A **catamorphism** is the unique homomorphism from the initial algebra — our generic fold.

This isn't just theory — it tells us that `fold` is **the** canonical way to consume a recursive structure when you only need child results.

### Finally-Tagless Style

In Haskell, **finally-tagless** represents DSL terms as type class methods:

```haskell
class Symantics repr where
  lit :: Int -> repr Int
  add :: repr Int -> repr Int -> repr Int
```

Different interpreters = different instances. Our `Interpreter` structs are the same pattern:

```cpp
struct Eval {
  static double terminal(Literal<int> l, auto&) { return l.value; }
  static double combine(AddOp, double a, double b, auto&) { return a + b; }
};
```

This solves the **expression problem** for operations — add new interpreters without modifying expression types.

### Recursion Schemes

Different operations need different amounts of information during traversal:

#### Catamorphism (fold): "I only need child results"

```cpp
// Evaluating x * y:
eval(x * y) = eval(x) * eval(y)
//            ↑         ↑
//       just numbers, don't need original x and y
```

Most operations are like this: evaluate, toString, countNodes, etc.

#### Paramorphism (para): "I need both originals AND results"

```cpp
// Differentiating x * y (product rule):
diff(x * y) = x * diff(y) + diff(x) * y
//            ↑              ↑
//       need original x  need original y
```

The product rule needs the **original subexpressions**, not just their derivatives.

#### Scheme Summary

| Scheme | Algebra Gets | Use Case |
|--------|--------------|----------|
| **Catamorphism** | Child results only | eval, accum, print |
| **Paramorphism** | Child + result pairs | diff (needs originals) |
| **Anamorphism** | Build up structure | unfold, generate |
| **Hylomorphism** | Unfold then fold | compile (build IR, then emit) |
| **Zygomorphism** | Two folds in parallel | forward-mode AD (diff + eval) |

---

## Proposed Architecture

### Layer 0: Core Types

```cpp
namespace tempura::symbolic4 {

// =============================================================================
// Binding Strategies (the "effects" that atoms can have)
// =============================================================================

struct Lookup {};  // Look up in environment

template <typename T>
struct Intrinsic { T value; };  // Value embedded in the atom

template <typename D>
struct Sample { [[no_unique_address]] D dist_; };  // Sample from distribution

template <typename E>
struct Compute { [[no_unique_address]] E expr_; };  // Evaluate expression

// =============================================================================
// The Unified Atom Type
// =============================================================================

template <typename Id, typename Strategy>
struct Atom : SymbolicTag {
  using id_type = Id;
  using strategy_type = Strategy;

  [[no_unique_address]] Strategy strategy_;

  // For atoms with identity, the symbolic variable representing this node
  static constexpr auto sym() requires (!std::is_void_v<Id>) {
    return Atom<Id, Lookup>{};
  }
};

// =============================================================================
// Atom Aliases (the user-facing vocabulary)
// =============================================================================

// Named variable: look up in bindings
template <typename Id = decltype([] {})>
using Symbol = Atom<Id, Lookup>;

// Runtime value embedded in expression
template <typename T>
using Literal = Atom<void, Intrinsic<T>>;

// Compile-time constant
template <auto V>
struct Constant : SymbolicTag {
  static constexpr auto value = V;
};

// Compile-time rational (avoids floating-point in types)
template <long long N, long long D = 1>
struct Fraction : SymbolicTag {
  static constexpr double value = static_cast<double>(N) / D;
};

// Random variable: sample from distribution
template <typename Id, typename DistT>
using RandomVar = Atom<Id, Sample<DistT>>;

// Deterministic variable: compute from expression
template <typename Id, typename ExprT>
using DeterministicVar = Atom<Id, Compute<ExprT>>;

// =============================================================================
// Compound Expressions (internal nodes)
// =============================================================================

template <typename Op, Symbolic... Args>
struct Expression : SymbolicTag {
  using op_type = Op;
  static constexpr std::size_t arity = sizeof...(Args);

  [[no_unique_address]] CompressedTuple<Args...> args_;

  template <std::size_t I>
  constexpr auto arg() const { return get<I>(args_); }
};

// =============================================================================
// Type Traits — derive "kind" from structure
// =============================================================================

template <typename T> struct is_atom : std::false_type {};
template <typename Id, typename S> struct is_atom<Atom<Id, S>> : std::true_type {};

template <typename T> struct is_expression : std::false_type {};
template <typename Op, typename... Args>
struct is_expression<Expression<Op, Args...>> : std::true_type {};

template <typename T> struct has_identity : std::false_type {};
template <typename Id, typename S>
struct has_identity<Atom<Id, S>> : std::bool_constant<!std::is_void_v<Id>> {};

// Terminal = any leaf node (Atom, Constant, Fraction)
template <typename T>
constexpr bool is_terminal_v = !is_expression<T>::value;

// =============================================================================
// The Symbolic Concept
// =============================================================================

template <typename T>
concept Symbolic =
  is_atom<T>::value ||
  std::is_same_v<T, Constant<T::value>> ||
  requires { T::value; } ||  // Fraction
  is_expression<T>::value;

}
```

**Key insight**: We don't need explicit `TerminalTag`, `UnaryTag`, `BinaryTag` — the type structure itself tells us everything:
- `Atom<Id, Lookup>` is a terminal with identity
- `Atom<void, Intrinsic<T>>` is an anonymous terminal
- `Expression<SinOp, A>` is unary (`arity` = 1)
- `Expression<AddOp, A, B>` is binary (`arity` = 2)

### Layer 1: Recursion Schemes

```cpp
namespace tempura::symbolic4 {

// Compressed pair for zero-overhead storage
template <typename A, typename B>
struct Pair {
  [[no_unique_address]] A first;
  [[no_unique_address]] B second;
};

// Interpreter concept
template <typename I>
concept Interpreter = requires {
  typename I::result_type;
};

// =============================================================================
// Catamorphism (fold): child results only
// =============================================================================

template <Interpreter I, Symbolic E, typename Ctx>
constexpr auto fold(E expr, Ctx& ctx) -> typename I::result_type {
  if constexpr (is_terminal_v<E>) {
    return I::terminal(expr, ctx);
  } else {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return I::combine(
        typename E::op_type{},
        fold<I>(expr.template arg<Is>(), ctx)...,
        ctx
      );
    }(std::make_index_sequence<E::arity>{});
  }
}

// =============================================================================
// Paramorphism (para): original children + their results
// =============================================================================

template <Interpreter I, Symbolic E, typename Ctx>
constexpr auto para(E expr, Ctx& ctx) -> typename I::result_type {
  if constexpr (is_terminal_v<E>) {
    return I::terminal(expr, ctx);
  } else {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return I::combine(
        typename E::op_type{},
        Pair{expr.template arg<Is>(), para<I>(expr.template arg<Is>(), ctx)}...,
        ctx
      );
    }(std::make_index_sequence<E::arity>{});
  }
}

}
```

### Layer 2: Interpreter Implementations

Each interpreter handles atoms according to their binding strategy:

```cpp
namespace tempura::symbolic4 {

// =============================================================================
// Evaluation Interpreter (uses FOLD)
// =============================================================================

template <typename Binders>
struct Eval {
  using result_type = double;

  template <typename E>
  static constexpr auto terminal(E expr, Binders& b) {
    if constexpr (is_atom<E>::value) {
      using Strategy = typename E::strategy_type;
      if constexpr (std::is_same_v<Strategy, Lookup>) {
        return b[expr];  // Look up in bindings
      } else if constexpr (requires { expr.strategy_.value; }) {
        return static_cast<double>(expr.strategy_.value);  // Intrinsic
      } else if constexpr (requires { expr.strategy_.expr_; }) {
        return fold<Eval>(expr.strategy_.expr_, b);  // Compute
      }
    } else {
      return static_cast<double>(E::value);  // Constant, Fraction
    }
  }

  template <typename Op, typename... Rs>
  static constexpr auto combine(Op, Rs... results, Binders&) {
    return Op{}(results...);
  }
};

// =============================================================================
// Differentiation Interpreter (uses PARA)
// =============================================================================

template <typename Var>
struct Diff {
  using result_type = Symbolic auto;

  template <typename E>
  static constexpr auto terminal(E expr, auto&) {
    if constexpr (is_atom<E>::value && std::is_same_v<typename E::strategy_type, Lookup>) {
      if constexpr (std::same_as<typename E::id_type, typename Var::id_type>) {
        return Constant<1>{};  // d/dx(x) = 1
      } else {
        return Constant<0>{};  // d/dx(y) = 0
      }
    } else {
      return Constant<0>{};  // d/dx(c) = 0
    }
  }

  // Sum rule: d/dx(A + B) = dA + dB
  template <typename A, typename DA, typename B, typename DB>
  static constexpr auto combine(AddOp, Pair<A, DA> left, Pair<B, DB> right, auto&) {
    return DA{} + DB{};
  }

  // Product rule: d/dx(A * B) = A * dB + dA * B
  template <typename A, typename DA, typename B, typename DB>
  static constexpr auto combine(MulOp, Pair<A, DA> left, Pair<B, DB> right, auto&) {
    return left.first * right.second + left.second * right.first;
  }

  // Chain rule: d/dx(sin(A)) = cos(A) * dA
  template <typename A, typename DA>
  static constexpr auto combine(SinOp, Pair<A, DA> arg, auto&) {
    return cos(arg.first) * arg.second;
  }
};

}
```

### Layer 3: Deduplicating Fold (for DAGs)

When accumulating over a DAG, each unique identity should contribute once:

```cpp
namespace tempura::symbolic4 {

// Type-level visited set (tracks Id types)
template <typename... Ids>
struct IdSet {
  template <typename Id>
  static constexpr bool contains = (std::same_as<Id, Ids> || ...);

  template <typename Id>
  using insert = std::conditional_t<contains<Id>, IdSet, IdSet<Id, Ids...>>;
};

// Returns Pair{result, new_visited_set}
template <Interpreter I, Symbolic E, typename Visited, typename Ctx>
constexpr auto foldUnique(E expr, Visited, Ctx& ctx) {
  if constexpr (is_atom<E>::value && has_identity<E>::value) {
    using Id = typename E::id_type;
    if constexpr (Visited::template contains<Id>) {
      return Pair{I::identity(ctx), Visited{}};  // Already visited
    } else {
      using NewVisited = typename Visited::template insert<Id>;
      return Pair{I::terminal(expr, ctx), NewVisited{}};
    }
  } else if constexpr (is_terminal_v<E>) {
    return Pair{I::terminal(expr, ctx), Visited{}};  // Anonymous: always process
  } else {
    return foldChildrenUnique<I>(expr, Visited{}, ctx);
  }
}

}
```

### Layer 4: Grammars (Optional)

Use C++20 concepts for grammar validation:

```cpp
// A simple calculator grammar: only arithmetic on numbers and symbols
template <typename T>
concept CalcExpr =
  (is_atom<T>::value && std::is_same_v<typename T::strategy_type, Lookup>) ||
  (is_atom<T>::value && requires { T{}.strategy_.value; }) ||  // Literal
  (is_expression<T>::value &&
   (std::same_as<typename T::op_type, AddOp> ||
    std::same_as<typename T::op_type, MulOp>) &&
   CalcExpr</* children... */>);

// Invalid expressions fail at call site with readable error
template <CalcExpr E>
constexpr auto evalCalc(E expr, auto& ctx) {
  return fold<Eval<decltype(ctx)>>(expr, ctx);
}
```

---

## Integration with bayes3

### Why bayes3 Needs symbolic4

A Bayesian model is a **directed acyclic graph (DAG)** of random variables:

```cpp
// A hierarchical model:
//   mu ~ Normal(0, 10)      // prior on mean
//   sigma ~ HalfNormal(1)   // prior on std dev
//   y ~ Normal(mu, sigma)   // likelihood
//
// DAG structure:
//   mu -----+
//           +---> y
//   sigma --+

auto mu = normal(0.0, 10.0);
auto sigma = halfNormal(1.0);
auto y = normal(mu, sigma);
```

We need **multiple operations** on this DAG:

1. **Sample**: Draw values for mu, sigma, y in topological order
2. **Compute log-prob**: Sum log p(mu) + log p(sigma) + log p(y|mu,sigma)
3. **Differentiate log-prob**: For gradient-based inference (HMC, ADVI)
4. **Condition on data**: Fix y = observed, infer mu and sigma

These are all **folds over the same DAG** — exactly what symbolic4 provides.

### Models ARE Atoms

A `RandomVar` is just an `Atom` with a `Sample` strategy:

```cpp
// What the user writes:
auto mu = normal(0.0, 10.0);

// What this creates:
Atom<Id_mu, Sample<Dist<Normal, Literal<0.0>, Literal<10.0>>>>
//   ^                    ^
//   identity             binding strategy (sample from this distribution)

// Equivalent to:
RandomVar<Id_mu, Dist<Normal, Literal<0.0>, Literal<10.0>>>
```

When `mu` appears in another distribution's parameters:

```cpp
auto y = normal(mu, sigma);

// Creates:
Atom<Id_y, Sample<Dist<Normal, Symbol<Id_mu>, Symbol<Id_sigma>>>>
//                              ^              ^
//                        dependencies are in the expression!
```

**Key insight**: No explicit parent tracking needed. The dependencies are **implicit in the distribution's parameter expressions**. We can extract `{Id_mu, Id_sigma}` by inspecting which `Symbol<Id>` types appear in the `Dist`.

### Distributions as Symbolic Log-Prob Formulas

Each distribution family knows how to compute its log-prob **symbolically**:

```cpp
template <template <typename...> class D>
struct LogProbFormula;

// Normal: log p(x|mu,sigma) propTo -0.5*((x-mu)/sigma)^2
template <>
struct LogProbFormula<Normal> {
  template <Symbolic X, Symbolic Mu, Symbolic Sigma>
  static constexpr auto apply(X x, Mu mu, Sigma sigma) {
    auto z = (x - mu) / sigma;
    return Fraction<-1, 2>{} * z * z;
  }
};

// Beta: log p(x|alpha,beta) propTo (alpha-1)*log(x) + (beta-1)*log(1-x)
template <>
struct LogProbFormula<Beta> {
  template <Symbolic X, Symbolic Alpha, Symbolic Beta>
  static constexpr auto apply(X x, Alpha alpha, Beta beta) {
    return (alpha - Literal{1}) * log(x) + (beta - Literal{1}) * log(Literal{1} - x);
  }
};
```

**Why symbolic?** Because we need to:
1. **Differentiate** for HMC: `diff(logProb, mu)` gives the gradient
2. **Simplify** before evaluation: Constant folding, algebraic simplification
3. **Inspect** the model structure: Debugging, visualization

### Why foldUnique? The DAG Problem

Consider a model where two variables share a parent:

```cpp
auto mu = normal(0.0, 1.0);
auto y1 = normal(mu, 1.0);
auto y2 = normal(mu, 1.0);

// DAG:
//        +---> y1
//   mu --+
//        +---> y2
```

If we compute `logProb(y1) + logProb(y2)` naively, we'd count `log p(mu)` **twice**. The DAG is not a tree — nodes can be shared.

`foldUnique` solves this by tracking visited **identities**:

```cpp
// When we encounter an Id we've seen, return identity (0 for sum, 1 for product)
// Anonymous atoms (Literal, Constant) have no Id — always processed
```

### The Three Interpreters

All three operations are folds over the DAG — they differ only in what they do at each node:

#### 1. SamplingInterpreter: DAG -> Trace

```cpp
template <typename Rng>
struct SamplingInterpreter {
  using result_type = Trace<...>;

  template <typename Id, typename D>
  static auto handleAtom(Atom<Id, Sample<D>> atom, Rng& rng, auto parent_trace) {
    auto params = evaluate(atom.strategy_.dist_.params(), parent_trace);
    double value = atom.strategy_.dist_.sample(params, rng);
    return merge(parent_trace, bind(Symbol<Id>{}, value));
  }
};
```

#### 2. SymbolicLogProbInterpreter: DAG -> Expression

```cpp
struct SymbolicLogProbInterpreter {
  using result_type = Symbolic;

  static constexpr auto identity(auto&) { return Constant<0>{}; }

  template <typename Id, typename D>
  static constexpr auto handleAtom(Atom<Id, Sample<D>> atom, auto&, auto parent_lp) {
    return parent_lp + atom.strategy_.dist_.unnormalizedLogProb(Symbol<Id>{});
  }
};
```

#### 3. NumericLogProbInterpreter: (DAG, Trace) -> double

```cpp
template <typename TraceT>
struct NumericLogProbInterpreter {
  using result_type = double;

  static constexpr double identity(const TraceT&) { return 0.0; }

  template <typename Id, typename D>
  static double handleAtom(Atom<Id, Sample<D>> atom, const TraceT& trace, double parent_lp) {
    return parent_lp + evaluate(atom.strategy_.dist_.unnormalizedLogProb(Symbol<Id>{}), trace);
  }
};
```

### Putting It Together: HMC Inference

```cpp
// Build the model once
auto mu = normal(0.0, 10.0);
auto sigma = halfNormal(1.0);
auto y = normal(mu, sigma);

// Get symbolic log-prob (computed once, reused)
auto logProbExpr = jointLogProb(y);

// Get symbolic gradients (computed once, reused)
auto gradMu = diff(logProbExpr, mu.sym());
auto gradSigma = diff(logProbExpr, sigma.sym());

// In HMC inner loop (called thousands of times):
double lp = evaluate(logProbExpr, trace);
double dMu = evaluate(gradMu, trace);
double dSigma = evaluate(gradSigma, trace);
```

**The key win**: Differentiation happens **once** at compile time. The HMC inner loop just evaluates pre-computed expressions — no autodiff overhead per iteration.

### Summary: bayes3 as a Thin Layer

| Component | What it is | symbolic4 foundation |
|-----------|------------|---------------------|
| `RandomVar<Id, Dist>` | `Atom<Id, Sample<Dist>>` | Atom with Sample strategy |
| `DeterministicVar<Id, Expr>` | `Atom<Id, Compute<Expr>>` | Atom with Compute strategy |
| `let(expr)` | Give identity to expression | Creates `Atom<Id, Compute<Expr>>` |
| `Dist<Family, Params...>` | Distribution with symbolic params | Params are symbolic expressions |
| `LogProbFormula<Family>` | Symbolic log-prob formula | symbolic4 operators |
| `Trace<Bindings...>` | Maps Ids to values | symbolic4 `BinderPack` |
| `foldUnique<Sampler>` | Sample the DAG | symbolic4 `foldUnique` |
| `foldUnique<LogProbAccum>` | Build log-prob expression | symbolic4 `foldUnique` |
| `diff(logProb, var)` | Gradient for HMC | symbolic4 `para<Diff>` |

---

## Transform Composition

Interpreters compose naturally:

```cpp
namespace tempura::symbolic4 {

// Sequential: run I1, then I2 on result
template <Interpreter I1, Interpreter I2>
struct Compose {
  using result_type = typename I2::result_type;

  template <Symbolic E, typename Ctx>
  static constexpr auto run(E expr, Ctx& ctx) {
    auto intermediate = fold<I1>(expr, ctx);
    return fold<I2>(intermediate, ctx);
  }
};

// Parallel: run both, return pair (zygomorphism)
template <Interpreter I1, Interpreter I2>
struct Parallel {
  using result_type = Pair<typename I1::result_type, typename I2::result_type>;

  template <Symbolic E, typename Ctx>
  static constexpr auto run(E expr, Ctx& ctx) {
    return Pair{fold<I1>(expr, ctx), fold<I2>(expr, ctx)};
  }
};

// Zygomorphism: diff + eval in single traversal (forward-mode AD)
template <typename Var, typename Binders>
struct ForwardAD {
  using result_type = Pair<double, double>;  // (value, derivative)
  // ... implementation runs eval and diff in parallel
};

}
```

---

## What We Learn from YAP

Despite choosing types-only storage, YAP's **design patterns** are valuable:

### 1. Minimal Expression Concept

YAP expressions need only two things:
```cpp
E::kind      // What kind of node
e.elements   // The children
```

We adopt this minimalism but encode `kind` implicitly in the type structure.

### 2. Transform = Callable with Overloads

YAP transforms are just functions that pattern-match on expressions. We adopt this: an **Interpreter** is a struct with static methods that handle each case.

### 3. Compiler Pipeline

YAP frames expression processing as: **capture -> transform -> evaluate**

```cpp
auto expr = capture(a + b * c);           // Build expression tree
auto opt = transform(expr, optimize);      // Optimization pass
auto result = evaluate(opt, {a=1, b=2});  // Final evaluation
```

### Why Static Methods Instead of operator()

**YAP's transform approach** (runtime dispatch):
```cpp
struct my_transform {
    auto operator()(terminal_tag, double value) { return value; }
    auto operator()(plus_tag, double l, double r) { return l + r; }
};
```

**Our interpreter approach** (compile-time dispatch):
```cpp
struct Eval {
    static auto terminal(Literal<double> lit, auto&) { return lit.value; }
    static auto combine(AddOp, double l, double r, auto&) { return l + r; }
};
```

**Why the difference?**

1. **No runtime dispatch**: YAP's `operator()` overloads are resolved at runtime. Our static methods with `if constexpr` are resolved at compile time.
2. **Explicit recursion scheme**: `fold<Eval>` vs `para<Diff>` makes it clear what information the interpreter receives.
3. **Composability**: Interpreters are types, so `Compose<Simplify, Eval>` works naturally.
4. **No callable objects**: YAP transforms are values; our interpreters are types. Types compose better at compile time.

---

## Simplification: A Different Beast

Simplification is the odd one out. While `evaluate`, `diff`, and `toString` are clean recursion schemes — you define what to do at each node, and the scheme handles traversal — simplification is fundamentally different. It's not a single pass but an **iterated rewrite system** with its own set of challenges.

### Why Simplification Doesn't Fit the Recursion Scheme Model

Consider what happens when you simplify `0 * (x + y)`:

```cpp
// With a bottom-up fold, you'd first simplify children:
//   x -> x  (no change)
//   y -> y  (no change)
//   x + y -> x + y  (no change)
// Then simplify parent:
//   0 * (x + y) -> 0  (apply zero-annihilation rule)
```

That works. But what about `x + y + x`?

```cpp
// Bottom-up (assuming left-associative):
//   (x + y) + x
//   First pass:
//     x -> x
//     y -> y
//     x + y -> x + y  (no like terms)
//     (x + y) + x -> ???
```

To collect like terms here, we need to **reassociate** the tree — but that requires seeing the whole structure and making global decisions about how to reorder terms. A single bottom-up pass can't do it.

**The fundamental issue**: Simplification involves pattern matching and rewriting, not just consuming children and producing results. The "right" simplification may require:

1. **Multiple passes**: First canonicalize, then collect, then simplify
2. **Non-local decisions**: Reordering terms requires seeing siblings
3. **Fixpoint iteration**: Keep applying rules until nothing changes
4. **Strategic choices**: Sometimes expand, sometimes factor — depends on context

This is why simplification needs its own theory: **term rewriting systems**.

### Term Rewriting Systems (TRS)

A term rewriting system is a set of **rules** that transform expressions:

```
Rule         = Pattern -> Replacement
x + 0        -> x                    (additive identity)
x * 1        -> x                    (multiplicative identity)
x * 0        -> 0                    (zero annihilation)
x + x        -> 2 * x                (like terms)
x^a * x^b    -> x^(a+b)              (power combination)
```

**Key terminology**:

| Term | Meaning |
|------|---------|
| **Redex** | A subexpression matching a rule's pattern ("reducible expression") |
| **Normal form** | An expression with no redexes (no rules apply) |
| **Reduction** | Applying a rule to transform a redex |
| **Derivation** | A sequence of reductions |

### The Two Critical Properties

For a TRS to be useful as a simplifier, we want two properties:

#### 1. Termination

> Every sequence of rewrites eventually stops.

Without termination, simplification can loop forever:

```cpp
// Bad rule pair:
x * (a + b) -> x*a + x*b    // distribution
x*a + x*b   -> x * (a + b)  // factoring

// Result: infinite oscillation!
x * (a + b) -> x*a + x*b -> x*(a+b) -> x*a + x*b -> ...
```

**Techniques to ensure termination**:

- **Reduction orderings**: Rules must always make expressions "smaller" in some well-founded order
- **Lexicographic path orderings (LPO)**: A specific ordering that works well for algebraic expressions
- **Polynomial interpretations**: Assign polynomial weights to constructors
- **Conditional rules**: Only apply rules when a predicate holds

Our current approach uses **predicates** to prevent oscillation:

```cpp
// Good: directional ordering rule
constexpr auto Ordering = Rewrite{
    y_ + x_, x_ + y_,
    [](auto ctx) {
      return compareAdditionTerms(get(ctx, x_), get(ctx, y_)) == Ordering::Less;
    }};
```

#### 2. Confluence (Church-Rosser Property)

> If an expression can be rewritten to two different expressions, both can be further rewritten to a common expression.

```
     a
    / \
   b   c      Confluence: exists d such that b ->* d and c ->* d
    \ /
     d
```

Confluence matters because it guarantees **unique normal forms**. Without it, the order you apply rules affects the result.

**Newman's Lemma**: A terminating TRS is confluent if and only if it is **locally confluent** — all critical pairs converge.

### Rewriting Strategies

Even with a good rule set, we must choose a **strategy** — the order in which to find and apply rules:

| Strategy | Description | Trade-offs |
|----------|-------------|------------|
| **Innermost** | Reduce innermost (deepest) redexes first | Guarantees children simplified before parent |
| **Outermost** | Reduce outermost (top) redexes first | Can short-circuit (0*bigexpr -> 0) |
| **Leftmost-innermost** | Innermost, but prefer leftmost | Deterministic |
| **Specific/strategic** | Custom ordering based on rule priorities | Most control; most complex |

Our current implementation uses a **two-phase strategy**:

```cpp
// Phase 1 (Descent/Top-Down): Quick patterns before recursing
constexpr auto descent_phase = topdown(quick_patterns | descent_rules);
// Short-circuits: 0 * complex_expr -> 0 (without simplifying complex_expr)

// Phase 2 (Ascent/Bottom-Up): Collection and canonicalization after children simplified
constexpr auto ascent_phase = bottomup(ascent_rules);
// Requires simplified children: x + x -> 2*x (needs to see both x's)

// Combined with fixpoint:
constexpr auto simplify = FixPoint{descent_phase >> ascent_phase};
```

### E-Graphs and Equality Saturation

**E-graphs** (equivalence graphs) offer an elegant solution to TRS limitations. Instead of destructively rewriting to *a* normal form, e-graphs represent *all* equivalent forms simultaneously.

#### What is an E-Graph?

An e-graph compactly represents an equivalence relation over expressions:

```
E-graph for "a*2" = "2*a" = "a+a":

  +-------------------------+
  |    Equivalence Class    |
  |  +---+  +---+  +---+   |
  |  |a*2|  |2*a|  |a+a|   |
  |  +-+-+  +-+-+  +-+-+   |
  +----+------+------+-----+
       |      |      |
       v      v      v
   [a] [2]  [2][a]  [a][a]
```

Each node belongs to an **e-class** (equivalence class). The e-graph can represent exponentially many equivalent expressions in polynomial space.

#### Equality Saturation

Instead of choosing one rule to apply, equality saturation applies **all** rules simultaneously:

```
1. Start with e-graph containing just the input expression
2. Repeat until saturated (no new equivalences discovered):
   - Match all rules against all e-classes
   - Add new equivalent expressions to the e-graph
   - Merge e-classes when expressions are proven equal
3. Extract the "best" expression according to a cost function
```

**Key insight**: By deferring the choice of which rewrite to keep, equality saturation finds the **globally optimal** result (according to your cost function).

#### Can We Do E-Graphs at Compile Time?

The honest answer: **probably not, at least not fully**.

E-graphs have inherent challenges for type-level computation:

1. **Exponential Blowup**: E-graphs can grow exponentially with AC rules
2. **Dynamic Structure**: Adding nodes, merging e-classes is awkward at compile time
3. **Extraction is NP-Hard**: Finding optimal extraction is expensive

### Practical Approach for symbolic4

Given the constraints, we propose a **layered architecture**:

#### Layer 1: Fast Canonicalization (Always On)

Cheap, always-beneficial transformations:

```cpp
// Constant folding
2 + 3 -> 5

// Identity elimination
x + 0 -> x
x * 1 -> x

// Zero annihilation
x * 0 -> 0

// Inverse cancellation
exp(log(x)) -> x
```

#### Layer 2: Algebraic Normalization (Default)

Standard algebraic simplifications with canonical forms:

```cpp
// Canonical ordering (with predicates to prevent oscillation)
y + x -> x + y    when x < y

// Like-term collection
x + x -> 2*x
```

#### Layer 3: Domain-Specific Simplification (Opt-In)

Rules that may or may not be beneficial depending on context:

```cpp
// These are NOT in the default pipeline
sin(2*x) -> 2*sin(x)*cos(x)  // Sometimes helpful, sometimes not
x*(a+b) <-> x*a + x*b        // Which direction depends on goal!
```

### Handling Non-Termination

Even with careful rule design, we need safety nets:

```cpp
template <Strategy S, std::size_t MaxDepth = 20>
struct FixPoint {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    if constexpr (ctx.depth >= MaxDepth) {
      return expr;  // Safety limit reached
    } else {
      auto result = strategy.apply(expr, ctx);
      constexpr bool unchanged = isSame<decltype(result), Expr>;
      if constexpr (unchanged) {
        return expr;  // Fixed point reached
      } else {
        return apply(result, ctx.template increment_depth<1>());
      }
    }
  }
};
```

---

## File Organization

```
symbolic4/
+-- core.h              # Atom, Expression, binding strategies
+-- operators.h         # AddOp, MulOp, SinOp, etc.
+-- compressed.h        # CompressedTuple, Pair with [[no_unique_address]]
|
+-- scheme/
|   +-- fold.h          # Catamorphism
|   +-- para.h          # Paramorphism
|   +-- fold_unique.h   # Deduplicating fold for DAGs
|   +-- id_set.h        # Type-level visited set
|
+-- interpreter/
|   +-- eval.h          # Evaluation (fold)
|   +-- diff.h          # Differentiation (para)
|   +-- to_string.h     # Pretty-printing (fold)
|   +-- compose.h       # Sequential, Parallel, Zygomorphism
|
+-- grammar/            # Concepts as grammars
|   +-- core.h          # Terminal, NumericLiteral, etc.
|
+-- evaluate.h          # Convenience wrapper
+-- derivative.h        # Convenience wrapper
+-- simplify.h          # Rewrite system (NOT a recursion scheme)
+-- traversal.h         # Strategies for simplification
```

---

## Design Principles

1. **Types ARE the representation**
   - No runtime AST, no pointer chasing, no variant dispatch
   - `sizeof(Expression<...>) == 1` (or size of any embedded Literal values)

2. **Atoms are effectful placeholders**
   - `Atom<Id, Strategy>` is the fundamental building block
   - Strategy determines the effect; interpreter handles it

3. **Derive, don't declare**
   - No explicit tags — `is_terminal_v<T>` computed from type structure
   - Dependencies derived from expression structure, not stored

4. **Explicit recursion schemes**
   - `fold<I>` when you only need child results (eval, toString)
   - `para<I>` when you need originals too (differentiation)

5. **Interpreters are types, not values**
   - Static methods, not `operator()` overloads
   - Compose at compile time: `Compose<I1, I2>`

6. **Zero overhead where it matters**
   - `[[no_unique_address]]` for empty types
   - Full constexpr — compute at compile time when possible

---

## Future Atom Types

The `Atom<Id, Strategy>` abstraction is extensible:

### Probabilistic Extensions

| Atom | Strategy | Effect | Use Case |
|------|----------|--------|----------|
| `Observed<Id, D>` | `Conditioned<D, value>` | Fixed to data | Bayesian inference |
| `Transformed<Id, E, J>` | `Reparameterized<E, J>` | Change-of-variables | Log-normal via exp(z) |
| `Mixture<Id, K, D...>` | `Switch<K, D...>` | One of several distributions | Mixture models |

### Optimization / Variational Inference

| Atom | Strategy | Effect | Use Case |
|------|----------|--------|----------|
| `Param<Id>` | `Optimized` | Learnable parameter | MLE, MAP estimation |
| `Variational<Id, Q>` | `Approximate<Q>` | Variational approximation | ADVI |

### Indexed / Plate Notation

| Atom | Strategy | Effect | Use Case |
|------|----------|--------|----------|
| `Indexed<Id, I, E>` | `ForEach<I, E>` | Plate notation | y[i] ~ Normal(mu, sigma) |
| `Reduced<Id, Op, E>` | `Aggregate<Op, E>` | Sum/product over index | sum(x[i]) |

### Priority for Implementation

1. **`Observed`** — Essential for real Bayesian inference
2. **`Indexed`** — Plate notation is critical for realistic models
3. **`Transformed`** — Reparameterization with automatic Jacobian
4. **`Param`** — For optimization-based inference (MLE, MAP, VI)

---

## Migration Path

### Phase 1: Core infrastructure
1. Add `compressed.h` with `CompressedTuple`, `Pair`
2. Add `scheme/fold.h` with generic `fold<Interpreter>`
3. Add `scheme/para.h` with generic `para<Interpreter>`
4. Add `scheme/id_set.h` and `scheme/fold_unique.h`

### Phase 2: Refactor interpreters
1. Rewrite `evaluate.h` using `fold<Eval>`
2. Rewrite `derivative.h` using `para<Diff>`
3. Keep `simplify.h` with current approach (separate concern)

### Phase 3: Composition & grammars
1. Add `interpreter/compose.h` with `Compose`, `Parallel`
2. Add `grammar/core.h` with concept-based grammars

### Phase 4: bayes3 integration
1. Refactor bayes3 to use `foldUnique<LogProbInterpreter>`
2. Delete hand-written traversals

---

## Brainstorming: Beyond Symbolic Math

The `Atom<Id, Strategy>` abstraction is surprisingly general. Here are speculative applications beyond symbolic math and probabilistic programming.

### Heterogeneous Compute Graphs

**Atoms as compute locations:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `HostVar<Id, T>` | `OnCPU<T>` | Value lives in CPU memory |
| `DeviceVar<Id, T>` | `OnGPU<T, DeviceId>` | Value lives in GPU memory |
| `RemoteVar<Id, T>` | `OnNode<T, NodeId>` | Value lives on remote machine |
| `Pinned<Id, T>` | `Pinned<T>` | Page-locked memory for async transfer |

**Interpreters:**
- `PlacementInterpreter` — decide where computation happens
- `TransferInterpreter` — insert memory copies at boundaries
- `FusionInterpreter` — fuse ops on same device

```cpp
// Define computation across devices
auto weights = deviceVar<Matrix>(gpu0, loadWeights());   // On GPU 0
auto input = hostVar<Vector>(cpuData);                   // On CPU
auto bias = deviceVar<Vector>(gpu0, loadBias());         // On GPU 0

// Expression: y = weights * input + bias
auto y = matmul(weights, input) + bias;

// PlacementInterpreter analyzes locations:
struct PlacementInterpreter {
  using result_type = Location;

  // GPU + CPU → need transfer, result on GPU (bigger operand wins)
  static auto combine(MatMulOp, Location a, Location b, auto&) {
    if (a != b) return Location::GPU;  // Prefer GPU
    return a;
  }
};

// TransferInterpreter inserts copies:
// y = matmul(weights, copyToGPU(input)) + bias
//                     ^^^^^^^^^^^^^ inserted automatically

// ExecuteInterpreter generates kernel launches:
struct ExecuteInterpreter {
  template <typename Id, typename T>
  static auto handleAtom(Atom<Id, OnGPU<T>> atom, auto& ctx) {
    return ctx.getGPUBuffer(Id{});
  }

  static auto combine(MatMulOp, auto a, auto b, auto& ctx) {
    return launchKernel<MatMulKernel>(a, b, ctx.stream);
  }
};
```

### Task Graphs / Parallelism

**Atoms as tasks:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `Task<Id, F>` | `Spawn<F>` | Async task returning F's result |
| `Future<Id, T>` | `Await<T>` | Block until result ready |
| `Channel<Id, T>` | `Stream<T>` | Producer/consumer channel |
| `Barrier<Id>` | `Sync` | Synchronization point |

**Interpreters:**
- `ScheduleInterpreter` — topological sort, find parallelism
- `ExecuteInterpreter` — actually run tasks
- `VisualizerInterpreter` — generate DAG diagram

```cpp
// Define a task DAG for ML pipeline
auto rawData = task([] { return readCSV("data.csv"); });
auto cleaned = task([](auto d) { return cleanData(d); }, rawData);
auto features = task([](auto d) { return extractFeatures(d); }, cleaned);
auto model = task([] { return loadModel("model.bin"); });  // Independent!
auto predictions = task([](auto f, auto m) { return predict(f, m); }, features, model);
auto metrics = task([](auto p) { return evaluate(p); }, predictions);

// ScheduleInterpreter finds parallelism:
struct ScheduleInterpreter {
  using result_type = TaskGroup;  // Set of tasks that can run together

  template <typename Id, typename F>
  static auto handleAtom(Atom<Id, Spawn<F>>, auto& ctx) {
    return TaskGroup{{Id{}}};  // Single task
  }

  // Dependencies merge; independent tasks stay parallel
  static auto combine(auto op, TaskGroup a, TaskGroup b, auto&) {
    return TaskGroup::sequential(TaskGroup::parallel(a, b), op);
  }
};

// Result:
// Level 0: [rawData, model]     ← run in parallel!
// Level 1: [cleaned]
// Level 2: [features]
// Level 3: [predictions]        ← waits for both features AND model
// Level 4: [metrics]

// ExecuteInterpreter runs with thread pool:
struct ExecuteInterpreter {
  template <typename Id, typename F>
  static auto handleAtom(Atom<Id, Spawn<F>> atom, auto& ctx) {
    return ctx.threadPool.submit([&] {
      return atom.strategy_.func_();
    });
  }

  static auto combine(auto op, auto futureA, auto futureB, auto& ctx) {
    return ctx.threadPool.submit([=] {
      return op(futureA.get(), futureB.get());
    });
  }
};
```

### Dataflow / Stream Processing

**Atoms as stream operators:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `Source<Id, T>` | `Produce<T>` | Emits values |
| `Sink<Id, T>` | `Consume<T>` | Receives values |
| `Transform<Id, F>` | `Map<F>` | Transform each element |
| `Window<Id, W>` | `Batch<W>` | Collect into windows |
| `Join<Id, L, R>` | `Merge<L, R>` | Combine two streams |

```cpp
// Real-time analytics pipeline
auto clicks = source<ClickEvent>("kafka://clicks");
auto purchases = source<PurchaseEvent>("kafka://purchases");

// Process clicks: filter → extract → window → count
auto validClicks = filter(clicks, [](auto e) { return e.userId != 0; });
auto userIds = map(validClicks, [](auto e) { return e.userId; });
auto clickWindows = window(userIds, SlidingWindow{minutes(5), minutes(1)});
auto clickCounts = aggregate(clickWindows, countDistinct());

// Join with purchases
auto userPurchases = map(purchases, [](auto e) { return Pair{e.userId, e.amount}; });
auto joined = join(clickCounts, userPurchases, /*key=*/userId);

// Output
auto metrics = sink(joined, "kafka://user-metrics");

// FusionInterpreter combines adjacent maps:
struct FusionInterpreter {
  // map(map(x, f), g) → map(x, compose(g, f))
  template <typename Id1, typename F, typename Id2, typename G>
  static auto combine(MapOp,
                      Expression<MapOp, Atom<Id1, Map<F>>, X> inner,
                      Atom<Id2, Map<G>>,
                      auto&) {
    return map(inner.arg<1>(), compose(G{}, F{}));  // Fused!
  }
};

// PartitionInterpreter assigns operators to workers:
struct PartitionInterpreter {
  using result_type = Pair<WorkerId, OperatorGraph>;

  // Stateful operators (window, aggregate) are partition boundaries
  template <typename Id, typename W>
  static auto handleAtom(Atom<Id, Batch<W>>, auto& ctx) {
    return Pair{ctx.assignWorker(), PartitionBoundary{}};
  }

  // Stateless operators can be replicated
  template <typename Id, typename F>
  static auto handleAtom(Atom<Id, Map<F>>, auto& ctx) {
    return Pair{ctx.currentWorker(), Replicable{}};
  }
};

// DeployInterpreter generates Flink/Kafka Streams code:
// source("kafka://clicks")
//   .filter(e -> e.userId != 0)
//   .map(e -> e.userId)
//   .window(SlidingWindows.of(minutes(5), minutes(1)))
//   .aggregate(new CountDistinct())
//   .join(purchases.map(...))
//   .sink("kafka://user-metrics")
```

### Neural Network Layers

**Atoms as layers:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `Input<Id, Shape>` | `Placeholder<Shape>` | Network input |
| `Dense<Id, In, Out>` | `Linear<In, Out>` | Fully connected |
| `Conv<Id, K, S>` | `Convolution<K, S>` | Convolution layer |
| `Activation<Id, F>` | `Pointwise<F>` | Element-wise activation |
| `Weight<Id, Shape>` | `Trainable<Shape>` | Learnable parameter |

**Interpreters:**
- `ForwardInterpreter` — compute forward pass
- `BackwardInterpreter` — compute gradients (reverse-mode AD)
- `ShapeInferInterpreter` — propagate tensor shapes
- `FusionInterpreter` — fuse adjacent ops

```cpp
// Define a simple CNN
auto x = input<Shape<28, 28, 1>>();                    // MNIST input
auto conv1 = conv2d<32, 3, 3>(x);                      // 32 filters, 3x3
auto relu1 = relu(conv1);
auto pool1 = maxPool<2, 2>(relu1);
auto conv2 = conv2d<64, 3, 3>(pool1);
auto relu2 = relu(conv2);
auto pool2 = maxPool<2, 2>(relu2);
auto flat = flatten(pool2);
auto dense1 = dense<128>(flat);
auto relu3 = relu(dense1);
auto logits = dense<10>(relu3);
auto output = softmax(logits);

// ShapeInferInterpreter propagates shapes at compile time:
struct ShapeInferInterpreter {
  using result_type = TensorShape;

  template <typename Id, std::size_t... Dims>
  static auto handleAtom(Atom<Id, Placeholder<Shape<Dims...>>>, auto&) {
    return Shape<Dims...>{};
  }

  template <std::size_t Filters, std::size_t K1, std::size_t K2>
  static auto combine(Conv2DOp<Filters, K1, K2>, auto inputShape, auto&) {
    // Output: (H - K + 1, W - K + 1, Filters) for valid padding
    return Shape<inputShape.H - K1 + 1, inputShape.W - K2 + 1, Filters>{};
  }
};

// Compile-time shape check!
static_assert(fold<ShapeInferInterpreter>(output, ctx) == Shape<10>{});

// ForwardInterpreter executes the network:
struct ForwardInterpreter {
  using result_type = Tensor;

  template <typename Id, typename S>
  static auto handleAtom(Atom<Id, Placeholder<S>>, auto& ctx) {
    return ctx.inputs[Id{}];  // Get input tensor
  }

  template <typename Id, typename S>
  static auto handleAtom(Atom<Id, Trainable<S>>, auto& ctx) {
    return ctx.weights[Id{}];  // Get learned weights
  }

  static auto combine(ReluOp, Tensor x, auto&) {
    return x.cwiseMax(0);  // Element-wise max(0, x)
  }

  template <std::size_t F, std::size_t K1, std::size_t K2>
  static auto combine(Conv2DOp<F, K1, K2>, Tensor x, auto& ctx) {
    return conv2d(x, ctx.weights[/*kernel*/], F, K1, K2);
  }
};

// GradientInterpreter uses para<> for reverse-mode AD:
// Each node gets (original, forward_result) pair
// Returns gradient w.r.t. each weight
```

### Query Plans / Relational Algebra

**Atoms as query operators:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `Table<Id, Schema>` | `Scan<Schema>` | Read from table |
| `Filter<Id, P>` | `Select<P>` | Filter rows |
| `Project<Id, Cols>` | `Project<Cols>` | Select columns |
| `Join<Id, L, R, K>` | `HashJoin<K>` | Join tables |
| `Aggregate<Id, G, F>` | `GroupBy<G, F>` | Group and aggregate |

**Interpreters:**
- `CostEstimator` — estimate cardinality and cost
- `Optimizer` — reorder joins, push down predicates
- `Executor` — actually run the query
- `Explainer` — generate query plan diagram

```cpp
// SQL: SELECT users.name, SUM(orders.amount)
//      FROM users JOIN orders ON users.id = orders.user_id
//      WHERE orders.amount > 100
//      GROUP BY users.name

auto users = table<UsersSchema>("users");
auto orders = table<OrdersSchema>("orders");

auto bigOrders = filter(orders, [](auto o) { return o.amount > 100; });
auto joined = join(users, bigOrders, /*on=*/users.id == orders.userId);
auto projected = project<"name", "amount">(joined);
auto result = aggregate(projected, groupBy<"name">(), sum<"amount">());

// CostEstimator folds to compute expected cardinality:
struct CostEstimator {
  using result_type = QueryStats;  // {cardinality, cost, ...}

  template <typename Id, typename Schema>
  static auto handleAtom(Atom<Id, Scan<Schema>>, auto& ctx) {
    return QueryStats{
      .cardinality = ctx.tableStats[Id{}].rowCount,
      .cost = ctx.tableStats[Id{}].rowCount * kSeqScanCost
    };
  }

  template <typename P>
  static auto combine(SelectOp<P>, QueryStats input, auto& ctx) {
    double selectivity = estimateSelectivity(P{}, ctx);
    return QueryStats{
      .cardinality = input.cardinality * selectivity,
      .cost = input.cost + input.cardinality * kFilterCost
    };
  }

  static auto combine(HashJoinOp, QueryStats left, QueryStats right, auto&) {
    return QueryStats{
      .cardinality = left.cardinality * right.cardinality * kJoinSelectivity,
      .cost = left.cost + right.cost +
              left.cardinality * kHashBuildCost +
              right.cardinality * kHashProbeCost
    };
  }
};

// Optimizer rewrites the plan:
// - Push filter below join: filter(join(users, orders)) → join(users, filter(orders))
// - Reorder joins by cost: ((A ⋈ B) ⋈ C) vs (A ⋈ (B ⋈ C))

struct PushdownOptimizer {
  // filter(join(A, B), P) → join(A, filter(B, P)) if P only references B
  template <typename P, typename L, typename R, typename K>
  static auto combine(SelectOp<P>,
                      Expression<HashJoinOp<K>, L, R>,
                      auto& ctx) {
    if constexpr (referencesOnly<P, R>()) {
      return join<K>(L{}, filter<P>(R{}));  // Pushed down!
    } else {
      return filter<P>(join<K>(L{}, R{}));  // Can't push
    }
  }
};

// Explainer generates visual plan:
// Aggregate [GROUP BY name, SUM(amount)]
//   └─ Project [name, amount]
//        └─ HashJoin [users.id = orders.user_id]
//             ├─ TableScan [users]
//             └─ Filter [amount > 100]
//                  └─ TableScan [orders]
```

### Circuit / Hardware Description

**Atoms as signals:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `Wire<Id, W>` | `Signal<W>` | W-bit wire |
| `Reg<Id, W>` | `Register<W>` | Clocked register |
| `Input<Id, W>` | `Port<In, W>` | Input port |
| `Output<Id, W>` | `Port<Out, W>` | Output port |

**Interpreters:**
- `SimulateInterpreter` — cycle-accurate simulation
- `SynthesizeInterpreter` — generate Verilog/VHDL
- `TimingInterpreter` — analyze critical paths

```cpp
// A simple 8-bit counter with enable and reset
auto clk = input<1>("clk");
auto rst = input<1>("rst");
auto en = input<1>("en");

auto count = reg<8>("count");
auto next_count = mux(rst,
                      constant<8>(0),           // if rst: 0
                      mux(en,
                          count + constant<8>(1), // if en: count + 1
                          count));                // else: hold

auto overflow = count == constant<8>(255);
auto out = output<8>("count_out", count);
auto ovf = output<1>("overflow", overflow);

// Connect register to its next value
connect(count, next_count, clk);  // count <= next_count on posedge clk

// SimulateInterpreter runs cycle-accurate simulation:
struct SimulateInterpreter {
  using result_type = BitVector;

  template <typename Id, std::size_t W>
  static auto handleAtom(Atom<Id, Port<In, W>>, auto& ctx) {
    return ctx.inputs[Id{}];  // Get current input value
  }

  template <typename Id, std::size_t W>
  static auto handleAtom(Atom<Id, Register<W>>, auto& ctx) {
    return ctx.regState[Id{}];  // Get current register value
  }

  static auto combine(AddOp, BitVector a, BitVector b, auto&) {
    return a + b;  // Wrapping add
  }

  static auto combine(MuxOp, BitVector sel, BitVector t, BitVector f, auto&) {
    return sel[0] ? t : f;
  }
};

// SynthesizeInterpreter generates Verilog:
struct SynthesizeInterpreter {
  using result_type = VerilogExpr;

  template <typename Id, std::size_t W>
  static auto handleAtom(Atom<Id, Port<In, W>>, auto&) {
    return VerilogExpr{Id::name};  // Just the signal name
  }

  template <typename Id, std::size_t W>
  static auto handleAtom(Atom<Id, Register<W>>, auto&) {
    return VerilogExpr{Id::name};
  }

  static auto combine(AddOp, VerilogExpr a, VerilogExpr b, auto&) {
    return VerilogExpr{fmt::format("({} + {})", a.code, b.code)};
  }

  static auto combine(MuxOp, VerilogExpr s, VerilogExpr t, VerilogExpr f, auto&) {
    return VerilogExpr{fmt::format("({} ? {} : {})", s.code, t.code, f.code)};
  }
};

// Output:
// module counter(
//   input wire clk,
//   input wire rst,
//   input wire en,
//   output wire [7:0] count_out,
//   output wire overflow
// );
//   reg [7:0] count;
//   always @(posedge clk)
//     count <= rst ? 8'b0 : (en ? (count + 8'b1) : count);
//   assign count_out = count;
//   assign overflow = (count == 8'hff);
// endmodule

// TimingInterpreter computes critical path delay:
struct TimingInterpreter {
  using result_type = Nanoseconds;

  template <typename Id, std::size_t W>
  static auto handleAtom(Atom<Id, Register<W>>, auto&) {
    return 0ns;  // Registers are timing boundaries
  }

  static auto combine(AddOp, Nanoseconds a, Nanoseconds b, auto&) {
    return std::max(a, b) + kAdderDelay;
  }

  static auto combine(MuxOp, Nanoseconds s, Nanoseconds t, Nanoseconds f, auto&) {
    return std::max({s, t, f}) + kMuxDelay;
  }
};
```

### More Abstract: Beyond Compute Graphs

The previous examples are all "computation" flavored. Here are some weirder applications showing the abstraction is truly general.

#### Access Control / Permissions

**Atoms as permission sources:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `User<Id>` | `Principal<Id>` | Who's asking |
| `Role<Id>` | `Grant<Perms>` | Role grants permissions |
| `Resource<Id>` | `Protected<Policy>` | What's being accessed |
| `Context<Id>` | `Environment<Props>` | Time, location, device |

```cpp
// Define permission structure
auto alice = user("alice");
auto admin = role("admin", {read, write, delete});
auto editor = role("editor", {read, write});
auto viewer = role("viewer", {read});

auto aliceRoles = hasRole(alice, admin) | hasRole(alice, editor);
auto doc = resource("secret-doc", requiresRole(admin));
auto time = context(duringBusinessHours);

auto canAccess = check(alice, doc, read, time);

// PolicyInterpreter evaluates access:
struct PolicyInterpreter {
  using result_type = Decision;  // Allow | Deny | NeedMoreInfo

  template <typename Id>
  static auto handleAtom(Atom<Id, Principal<Id>>, auto& ctx) {
    return ctx.lookupUser(Id{});
  }

  static auto combine(CheckOp, UserInfo u, ResourcePolicy p, Permission perm, auto&) {
    if (p.requires(perm) && u.hasPermission(perm)) return Allow;
    return Deny;
  }

  // OR is permissive: any Allow wins
  static auto combine(OrOp, Decision a, Decision b, auto&) {
    if (a == Allow || b == Allow) return Allow;
    return Deny;
  }
};

// AuditInterpreter logs access attempts:
// "alice requested read on secret-doc via admin role: ALLOWED"

// ExplainInterpreter shows WHY:
// "Access granted because:
//   - alice has role 'admin'
//   - admin grants 'read' permission
//   - request is during business hours"
```

#### Parser Combinators

**Atoms as grammar rules:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `Char<C>` | `Literal<C>` | Match exact character |
| `Range<A, B>` | `CharClass<A, B>` | Match range [A-B] |
| `Ref<Id>` | `RuleRef<Id>` | Reference another rule |
| `Capture<Id, P>` | `Named<Id, P>` | Capture match as Id |

```cpp
// JSON number grammar
auto digit = range<'0', '9'>();
auto digits = many1(digit);
auto sign = optional(oneOf<'+', '-'>());
auto integer = seq(sign, digits);
auto fraction = seq(char_<'.'>(), digits);
auto exponent = seq(oneOf<'e', 'E'>(), sign, digits);
auto number = capture<"number">(seq(integer, optional(fraction), optional(exponent)));

// MatchInterpreter parses input:
struct MatchInterpreter {
  using result_type = ParseResult;  // {success, remaining, captures}

  template <char C>
  static auto handleAtom(Atom<void, Literal<C>>, auto& ctx) {
    if (ctx.input.empty() || ctx.input[0] != C) return Fail{};
    return Success{ctx.input.substr(1)};
  }

  static auto combine(SeqOp, ParseResult a, ParseResult b, auto&) {
    if (!a.success) return a;
    return b;  // Continue with remaining input
  }

  static auto combine(AltOp, ParseResult a, ParseResult b, auto&) {
    if (a.success) return a;
    return b;  // Try alternative
  }
};

// CompileInterpreter generates state machine or recursive descent

// FirstSetInterpreter computes FIRST sets for LL parsing

// LeftFactorInterpreter rewrites grammar to remove left recursion
```

#### Music / Sound Synthesis

**Atoms as audio sources:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `Osc<Id, Wave>` | `Oscillator<Wave>` | Sine, saw, square wave |
| `Env<Id, ADSR>` | `Envelope<ADSR>` | Attack-decay-sustain-release |
| `Sample<Id, File>` | `AudioFile<File>` | Sampled audio |
| `Param<Id, T>` | `Control<T>` | Modulatable parameter |

```cpp
// A simple synthesizer patch
auto freq = param<"freq">(440.0);  // A4
auto amp = param<"amp">(0.5);

auto osc1 = sine(freq);
auto osc2 = saw(freq * 2.01);  // Slight detune for richness
auto mixed = osc1 * 0.7 + osc2 * 0.3;

auto env = adsr(attack(0.01), decay(0.1), sustain(0.7), release(0.3));
auto shaped = mixed * env;

auto filtered = lowpass(shaped, cutoff(1000.0), resonance(0.5));
auto output = filtered * amp;

// RenderInterpreter generates audio samples:
struct RenderInterpreter {
  using result_type = SampleBuffer;

  template <typename Id, typename Wave>
  static auto handleAtom(Atom<Id, Oscillator<Wave>>, auto& ctx) {
    return Wave::generate(ctx.sampleRate, ctx.frequency, ctx.phase);
  }

  static auto combine(MulOp, SampleBuffer a, SampleBuffer b, auto&) {
    return a.pointwiseMultiply(b);  // Amplitude modulation
  }

  static auto combine(LowpassOp, SampleBuffer in, float cutoff, float res, auto&) {
    return biquadFilter(in, cutoff, res);
  }
};

// GraphInterpreter generates modular synth patch diagram:
//  ┌─────────┐    ┌─────────┐
//  │ sine    │───▶│         │
//  │ (freq)  │    │  mixer  │───▶ envelope ───▶ lowpass ───▶ out
//  ├─────────┤    │ 0.7/0.3 │
//  │ saw     │───▶│         │
//  │ (freq*2)│    └─────────┘
//  └─────────┘

// MIDIInterpreter maps note events to parameter changes
```

#### Type Systems / Inference

**Atoms as types:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `TVar<Id>` | `Unknown<Id>` | Type variable to solve |
| `TCon<Name>` | `Concrete<Name>` | Concrete type (Int, String) |
| `TApp<F, Args>` | `Applied<F, Args>` | Type application |

```cpp
// Represent: map : (a -> b) -> [a] -> [b]
auto a = tvar<"a">();
auto b = tvar<"b">();
auto mapType = forall({a, b},
  arrow(arrow(a, b), arrow(list(a), list(b))));

// UnifyInterpreter solves type equations:
struct UnifyInterpreter {
  using result_type = Substitution;  // TVar -> Type mapping

  template <typename Id>
  static auto handleAtom(Atom<Id, Unknown<Id>>, auto& ctx) {
    // Return current binding or self
    return ctx.substitution.lookup(Id{}).value_or(TVar<Id>{});
  }

  static auto combine(ArrowOp, Type a, Type b, Type c, Type d, auto& ctx) {
    // (a -> b) unifies with (c -> d) if a~c and b~d
    auto s1 = unify(a, c, ctx);
    auto s2 = unify(apply(s1, b), apply(s1, d), ctx);
    return compose(s2, s1);
  }
};

// InferInterpreter assigns types to expressions:
// let f = \x -> x + 1
// infer(f) → Int -> Int

// PrettyInterpreter renders types:
// "(a -> b) -> [a] -> [b]"
```

#### Finite State Machines / Protocols

**Atoms as states:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `State<Id>` | `Location<Id>` | A state in the machine |
| `Trans<From, To, E>` | `Edge<E>` | Transition on event E |
| `Guard<P>` | `Condition<P>` | Transition guard |
| `Action<F>` | `Effect<F>` | Side effect on transition |

```cpp
// TCP connection state machine (simplified)
auto closed = state<"CLOSED">();
auto listen = state<"LISTEN">();
auto synSent = state<"SYN_SENT">();
auto synRecv = state<"SYN_RECV">();
auto established = state<"ESTABLISHED">();

auto machine = fsm(
  closed,  // initial state
  transition(closed, listen, on<PassiveOpen>()),
  transition(closed, synSent, on<ActiveOpen>(), action(sendSyn)),
  transition(listen, synRecv, on<RecvSyn>(), action(sendSynAck)),
  transition(synSent, established, on<RecvSynAck>(), action(sendAck)),
  transition(synRecv, established, on<RecvAck>()),
  transition(established, closed, on<Close>(), action(sendFin))
);

// SimulateInterpreter runs the FSM:
struct SimulateInterpreter {
  using result_type = State;

  template <typename Id>
  static auto handleAtom(Atom<Id, Location<Id>>, auto& ctx) {
    return ctx.currentState;
  }

  static auto combine(TransitionOp, State from, State to, Event e, auto& ctx) {
    if (ctx.currentState == from && ctx.currentEvent == e) {
      return to;  // Take transition
    }
    return from;  // Stay
  }
};

// VerifyInterpreter checks properties:
// "Can we reach ESTABLISHED from CLOSED?" → Yes
// "Is there a deadlock?" → No
// "Does every path from SYN_SENT lead to ESTABLISHED or CLOSED?" → Yes

// DotInterpreter generates Graphviz:
// digraph TCP {
//   CLOSED -> LISTEN [label="passive open"]
//   CLOSED -> SYN_SENT [label="active open / send SYN"]
//   ...
// }
```

#### Recipes / Dependency Graphs

**Atoms as ingredients/steps:**

| Atom | Strategy | Effect |
|------|----------|--------|
| `Ingredient<Id, Q>` | `Material<Q>` | Raw ingredient with quantity |
| `Step<Id, F>` | `Process<F>` | Cooking step |
| `Tool<Id>` | `Equipment<Id>` | Required equipment |

```cpp
// Chocolate chip cookies
auto flour = ingredient<"flour">(cups(2.25));
auto sugar = ingredient<"sugar">(cups(0.75));
auto butter = ingredient<"butter">(cups(1), softened);
auto eggs = ingredient<"eggs">(2);
auto chips = ingredient<"chocolate chips">(cups(2));

auto dry = step<"mix dry">(combine(flour, bakingSoda, salt));
auto wet = step<"cream">(beat(butter, sugar), minutes(3));
auto wetWithEggs = step<"add eggs">(mix(wet, eggs));
auto dough = step<"combine">(fold(wetWithEggs, dry));
auto withChips = step<"add chips">(fold(dough, chips));
auto shaped = step<"shape">(scoop(withChips, tablespoon));
auto baked = step<"bake">(bake(shaped, degrees(375), minutes(10)));

// ScaleInterpreter adjusts quantities:
struct ScaleInterpreter {
  using result_type = ScaledRecipe;

  template <typename Id, typename Q>
  static auto handleAtom(Atom<Id, Material<Q>>, auto& ctx) {
    return Q{} * ctx.scaleFactor;
  }
};

// SubstituteInterpreter replaces ingredients:
// "Replace butter with coconut oil" → different recipe

// TimingInterpreter computes total time:
// "Prep: 20 min, Cook: 10 min, Total: 30 min"

// ShoppingListInterpreter aggregates ingredients:
// "flour: 2.25 cups, sugar: 0.75 cups, ..."

// NutritionInterpreter computes macros:
// "Per cookie: 150 cal, 8g fat, 20g carbs, 2g protein"
```

### The Common Pattern

All these domains share the same structure:

1. **Atoms** = nodes with identity and a "what am I" strategy
2. **Expressions** = composition via operators
3. **Interpreters** = "what to do" with each strategy
4. **Recursion schemes** = how to traverse

The insight: **computation graphs are everywhere**. symbolic4's contribution is making the graph structure explicit in types, with zero runtime overhead and pluggable semantics.

### What Makes This Different from TensorFlow/PyTorch?

Those frameworks build graphs at **runtime** (eager or traced). We build graphs at **compile time** in the type system:

| Aspect | TensorFlow/PyTorch | symbolic4 |
|--------|-------------------|-----------|
| Graph construction | Runtime | Compile time |
| Type safety | Runtime errors | Compile errors |
| Overhead | Graph objects, dispatch | Zero (types only) |
| Optimization | JIT, XLA | constexpr, template metaprogramming |
| Flexibility | Dynamic shapes, control flow | Static structure (mostly) |

The trade-off: we sacrifice dynamic flexibility for static guarantees and zero overhead. This is ideal for **fixed computation patterns** that run many times (MCMC inner loops, DSP pipelines, neural network inference).

---

## Open Questions

1. **E-graphs for simplification**: Worth the complexity? Current fixpoint approach is simpler but less principled.

2. **Zygomorphism for forward-mode AD**: Run diff + eval in single pass. Worth implementing?

3. **Context threading**: Mutable `Ctx&` vs state monad `-> Pair<Result, Ctx>`. Current: mutable for simplicity.

4. **Phase-specific metadata**: "Trees That Grow" pattern — type families for different compilation phases. Needed?

5. **C++26 reflection**: Auto-generate interpreter dispatch? Wait for standardization.

---

## Success Criteria

- [ ] `fold<Eval>(expr, ctx)` matches current `evaluate()` behavior
- [ ] `para<Diff>(expr, ctx)` matches current `diff()` behavior
- [ ] `foldUnique<Accum>` replaces bayes3 hand-written traversals
- [ ] New fold interpreter: ~15 lines
- [ ] New para interpreter: ~25 lines
- [ ] Composition works: `Compose<Simplify, Eval>`
- [ ] Concept grammars catch invalid DSL at compile time
- [ ] No performance regression
- [ ] Core concepts explainable in 5 minutes

---

## References

### Expression Templates & Interpreters
- [Boost.YAP Manual](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_yap/manual.html) — transform patterns
- [Finally Tagless (Okmij)](https://okmij.org/ftp/tagless-final/) — interpreter pattern theory
- [Recursion Schemes](https://www.schoolofhaskell.com/user/edwardk/recursion-schemes/catamorphisms) — cata/para/zygo
- [Trees That Grow](https://arxiv.org/abs/1610.04799) — phase-specific AST metadata

### Term Rewriting Systems
- [Confluence (Wikipedia)](https://en.wikipedia.org/wiki/Confluence_(abstract_rewriting)) — Church-Rosser property, local vs global confluence
- [Church-Rosser theorem (Wikipedia)](https://en.wikipedia.org/wiki/Church%E2%80%93Rosser_theorem) — theoretical foundations
- [Term Rewriting Systems: A Tutorial (CWI)](https://ir.cwi.nl/pub/6129/6129D.pdf) — comprehensive introduction
- [Rewriting (Wikipedia)](https://en.wikipedia.org/wiki/Rewriting) — general overview and strategies

### E-Graphs & Equality Saturation
- [egg: Fast and Extensible Equality Saturation](https://egraphs-good.github.io/) — canonical Rust implementation
- [egg Paper (arXiv)](https://arxiv.org/abs/2004.03082) — POPL 2021 Distinguished Paper
- [SIGPLAN Blog: Equality Saturation with egg](https://blog.sigplan.org/2021/04/06/equality-saturation-with-egg/) — accessible introduction
- [Metatheory.jl](https://github.com/JuliaSymbolics/Metatheory.jl) — Julia e-graphs and term rewriting
- [SymbolicUtils.jl](https://github.com/JuliaSymbolics/SymbolicUtils.jl) — Julia symbolic rewriting rules
- [JuliaSymbolics](https://juliasymbolics.org/) — Julia symbolic programming ecosystem

### Algebraic Simplification
- [Algebraic Simplification (Springer)](https://link.springer.com/chapter/10.1007/978-3-7091-3406-1_2) — canonical forms and algorithms
- [GNU Emacs Calc: Algebraic Properties of Rewrite Rules](https://www.gnu.org/software/emacs/manual/html_node/calc/Algebraic-Properties-of-Rewrite-Rules.html) — practical rule design
