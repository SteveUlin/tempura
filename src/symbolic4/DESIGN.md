# symbolic4: Compile-Time Computation Graph Library

## 1. Background

### 1.1 Problem Statement

symbolic4 is a **compile-time AST metaprogramming library** for C++26. It represents
computation graphs as C++ types, enabling zero-overhead graph construction,
analysis, optimization, and code generation—all at compile time.

**Core idea**: Describe computations declaratively as type-level graphs, then
transform and schedule them at compile time to emit efficient runtime code.

### 1.2 Use Cases

**Current focus: Bayesian modeling and MCMC**

Bayesian inference is the primary application driving development. It requires:
- Complex expression graphs (log-probability of hierarchical models)
- Symbolic differentiation for HMC gradients
- Aggressive simplification (gradient expressions can be huge)
- Efficient evaluation (inner loop of MCMC sampler)

This demands robust symbolic mathematics: expression construction, differentiation,
simplification, and evaluation. Getting these right is the current priority.

**Example future applications** (not currently planned):

The same compile-time AST infrastructure could support other domains:

- *ML computation graphs*: Forward/backward pass, operator fusion
- *Matrix chain optimization*: Optimal parenthesization via compile-time DP
- *Resource-aware scheduling*: Static execution plans for parallel/GPU execution
- *Compiler IRs*: Optimization passes as rewrite rules

These illustrate the generality of the approach but are not on the roadmap.

### 1.3 Current State

The existing implementation is already quite sophisticated:

**Core representation** (`core.h`):
- `Atom<Id, Effect>` - Leaf nodes with identity and behavior
- `Expression<Op, Args...>` - Internal nodes with operator and children
- `Constant<V>`, `Fraction<N,D>` - Compile-time constants

**The Effect system** distinguishes leaf node behavior:
- `Free` - Symbol needing binding at evaluation (`Symbol<X>`)
- `Embedded<T>` - Runtime value baked in (`Literal<double>`)
- `Sample<D>` - Random variable from distribution (Bayesian)
- `Compute<E>` - Deterministic transformation of other nodes

**Traversal** (`scheme/`):
- `cata<I, Mode>` - Unified catamorphism/paramorphism with mode parameter
- `fold` - Alias for `cata` with `ChildMode::ResultOnly`
- `para` - Alias for `cata` with `ChildMode::WithOriginal`
- `fold_unique` - DAG-aware traversal that deduplicates shared subexpressions

**Interpreters** (`interpreter/`):
- `eval` - Numeric evaluation with bindings
- `diff` - Symbolic differentiation (uses para for chain rule)
- `simplify` - Algebraic simplification with canonicalization
- `to_string` - Pretty printing with precedence

**Indexed expressions** (`indexed/`):
- `Index<Dim>` - Index variable over a dimension
- `IndexedSymbol<Base, Indices...>` - Subscripted symbols (xᵢ)
- `sum_over<N>` - Summation over index range
- Evaluation loops over indices automatically

**Distributions** (`distributions/`):
- Log-probability functions returning symbolic expressions
- Distribution wrappers storing symbolic parameters
- RandomVar combining distribution + identity
- Plate notation for i.i.d. replication

**MCMC** (`mcmc/`):
- HMC sampler using symbolic gradients
- Step size adaptation
- Basic diagnostics

**What works well**:
- Zero-overhead type-level representation
- Same expression interpretable multiple ways (eval, diff, simplify)
- Distributions and plates enable Bayesian model specification
- HMC runs on non-trivial hierarchical models

**Prior art: symbolic3**

The `symbolic3/` directory contains a previous iteration with mature
implementations of pattern matching and strategies:

- `pattern_matching.h`: PatternVar, BindingContext, Rewrite, RewriteSystem
- `strategy.h`: Strategy concept, Identity, Fail, Sequence, Choice, Try, FixPoint
- `traversal.h`: Fold, Unfold, TopDown, BottomUp, Innermost, Outermost

These need adaptation for symbolic4's `Atom<Id, Effect>` model.

**Gaps to address**:

1. **Adapt pattern matching for Atom/Effect**: symbolic3's pattern matching
   handles Symbol/Constant; we need matching on Effect types.

2. **Port strategy combinators**: Should work with minimal changes.

3. **Top-down short-circuiting**: Current simplification is bottom-up only.
   `0 * complex_expr` still traverses the subtree when it could return 0
   immediately.

### 1.4 Goals

1. **Compile-time first**: All graph manipulation happens at compile time
2. **Robust symbolic math**: Differentiation, simplification, evaluation
3. **Declarative rules**: Express transformations as pattern → replacement
4. **Composable strategies**: Control how, where, and how often rules apply
5. **General-purpose foundation**: Infrastructure that could support other domains

### 1.5 Non-Goals

- Runtime graph construction (graphs are types, fixed at compile time)
- Runtime scheduling (we generate static schedules)
- Numerical solvers (focus is graph manipulation, not numerics)

### 1.6 Naming Alternatives

The core type is currently called `Expression`. Alternatives considered:
- `Node` - graph terminology, but less evocative
- `Term` - term rewriting terminology
- `Expr` - shorter, common in compilers
- `AST` - too implementation-focused

Keeping `Expression` for now—it's general enough and familiar.

---

## 2. Core Ideas

### 2.1 Graphs as Types

**Goal**: Represent computation graphs with zero runtime overhead, full type
safety, and compile-time manipulation.

We want to describe computations declaratively—"multiply A and B, then add C"—and
have the compiler analyze, optimize, and generate efficient code. The graph
structure should exist only during compilation, with no runtime representation
cost.

**Approach in C++**: Two node categories.

```cpp
// Leaf nodes: things that don't contain subexpressions
struct Symbol;     // Variable needing binding at eval time
struct Constant;   // Compile-time value (in the type)
struct Literal;    // Runtime value (stored in object)

// Internal nodes: operations with children
template <typename Op, Symbolic... Args>
struct Expression;
```

**Example**: The expression `(x + 1) * 2` becomes:
```cpp
Expression<MulOp,
  Expression<AddOp, Symbol<x_tag>, Constant<1>>,
  Constant<2>>
```

**Memory layout**: `[[no_unique_address]]` eliminates storage for empty types.
Most symbolic expressions are composed of empty tag types (Symbol, Constant,
operators), so deeply nested expressions can have sizeof == 1.

**What makes something a leaf?**

The `Symbolic` concept defines what can appear in an expression. Leaves are
types that satisfy `Symbolic` but aren't `Expression<...>`. This matters for
pattern matching: rules need to know when to stop recursing.

```cpp
template <typename T>
concept Symbolic = /* is Expression, Symbol, Constant, Literal, or PatternVar */;

template <typename T>
concept Leaf = Symbolic<T> && !is_expression_v<T>;
```

With pattern matching as foundation (§2.4), every rule system needs a catch-all
"do nothing" case for leaves it doesn't care about. The key constraint is just
distinguishing leaves from internal nodes.

**Current state: Atom<Id, Effect>**

The current implementation uses a unified `Atom<Id, Effect>` type for all leaves:

| Type | Atom encoding |
|------|---------------|
| `Symbol<X>` | `Atom<X, Free>` |
| `Literal<T>` | `Atom<void, Embedded<T>>` |
| `RandomVar<W,D>` | `Atom<W, Sample<D>>` |

**Is Atom worth the abstraction?**

Arguments for:
- Single `is_atom_v<T>` trait for all leaves
- Uniform memory layout with `[[no_unique_address]] Effect effect_`
- Adding new leaf kinds means adding one Effect, not a new type

Arguments against:
- With pattern matching, we need catch-all rules anyway
- Separate types are simpler to understand
- The "interpreter explosion" argument is weak—pattern matching handles dispatch
- Most code uses the aliases (`Symbol`, `Literal`) not `Atom` directly

**Decision**: Keep `Atom<Id, Effect>` as implemented. The abstraction is already
in use throughout the codebase and works well enough. The Effect system provides
value for Bayesian modeling where we genuinely have multiple leaf kinds (symbols,
literals, random variables, observed data). Pattern matching will add a catch-all
rule regardless, so the abstraction cost is minimal.

If we find the Effect system creates friction, we can always add type aliases
that hide it further. The migration path is: keep Atom internally, expose
simpler types externally.

**Literal syntax**

`lit(0.0)` is verbose. Better options:

```cpp
// Option 1: User-defined literal (preferred)
auto expr = x + 0.0_c;  // _c for "constant"

// Option 2: Implicit conversion in operators
auto expr = x + 0.0;    // operator+(Symbol, double) wraps the double

// Option 3: CTAD helper
auto expr = x + c(0.0); // shorter than lit()
```

The `_c` suffix is cleanest—explicit, short, and doesn't require operator
overloads for every type combination.

**Alternatives considered**:
- *Runtime AST (heap-allocated nodes)*: Flexible but has allocation overhead,
  no compile-time optimization, requires manual memory management.
- *Expression templates (lazy evaluation)*: Common for linear algebra but
  limited to evaluation—can't inspect or transform the expression.
- *constexpr values*: Harder to pattern match and transform compared to
  type-level encoding.

### 2.2 Two Kinds of Operations

**Goal**: Provide a unified way to process graphs, whether computing a value
or producing a transformed graph.

Every useful operation on a graph involves traversing it and combining results.
We want one set of traversal machinery that works for all operations, with the
operation itself parameterized.

**Approach in C++**: Template the traversal over a "handler" that defines what
to do at each node. The handler's return type determines the result.

| Category | Output | Examples |
|----------|--------|----------|
| **Interpretation** | Value (type depends on graph) | eval→numeric, toString→string, cost→size_t |
| **Transformation** | New graph | simplify, differentiate, optimize |

```cpp
// Interpretation: collapses graph to a value
template<typename E, typename Bindings>
constexpr auto eval(E, Bindings) -> /* numeric type from E */;

// Transformation: produces a new graph type
template<typename E, typename Var>
using Differentiate = /* new Expression type */;
```

Interpretation return types are determined by the graph—an expression over
integers evaluates to int, over floats to float. The traversal machinery is
generic over the result type.

**Alternatives considered**:
- *Separate implementations per operation*: Works but leads to code duplication
  and inconsistent traversal behavior.
- *Visitor pattern*: Common in OOP but awkward with type-level ASTs; we use
  template specialization instead, which is the compile-time equivalent.

### 2.3 Traversal Patterns

**Goal**: Provide reusable building blocks for graph operations that handle
the recursion, letting users focus on the per-node logic.

Writing recursive traversals by hand is error-prone and repetitive. Different
operations need different traversal orders—evaluation needs children processed
first (bottom-up), while short-circuit optimizations want to check the parent
first (top-down).

**Current state** (`scheme/`):

symbolic4 currently has `cata`/`fold`/`para` with an Interpreter interface:

```cpp
// Current: Interpreter defines terminal() and combine<Op>()
struct Eval {
  static auto terminal(Constant<V>, ctx) { return V; }
  static auto terminal(Symbol<X>, ctx) { return ctx[X]; }
  template<typename Op> static auto combine(ctx, auto... children);
};
auto result = fold<Eval>(expr, ctx);  // returns double
```

**Proposed unification** (Milestone 1):

Replace the dual system with a single Strategy concept. Traversals become
Strategy wrappers:

| Traversal | Order | Use case |
|-----------|-------|----------|
| `bottomUp(s)` | Children → parent | Evaluation, term collection |
| `topDown(s)` | Parent → children | Short-circuit (0 * x → 0) |
| `innermost(s)` | Bottom-up + fixpoint | Exhaustive simplification |

```cpp
// Unified: everything is a Strategy with apply()
auto result = bottomUp(Eval{}).apply(expr, ctx);      // double
auto result = topDown(QuickRules{}).apply(expr, ctx); // Expr
auto pipeline = topDown(quick) >> bottomUp(collect);  // composable
```

The Interpreter interface (`terminal`/`combine`) becomes an implementation
detail for building bottom-up strategies, not a separate public concept.

Top-down traversal enables short-circuiting: `0 * big_expr` returns 0 without
traversing `big_expr`.

**Alternatives considered**:
- *Manual recursion everywhere*: Works but duplicates boilerplate.
- *Single traversal pattern*: Simpler but inefficient—top-down short-circuiting
  is impossible with pure bottom-up traversal.

### 2.4 Pattern Matching as Foundation

**Goal**: Make pattern matching the primary mechanism for expressing graph
transformations. Rules like `0 * x → 0` and `d/dx(x*y) → (d/dx x)*y + x*(d/dx y)`
should be expressible directly.

Pattern matching is not just a convenience—it's the foundation. Both simplification
and differentiation can be expressed as rewrite rules. This unifies the codebase
around one transformation mechanism.

**Pattern variables**

Pattern variables match any subexpression and capture it for use in the replacement.
They use a lambda trick for unique type identity:

```cpp
// Each PatternVar gets a unique type via lambda
template <typename Id = decltype([] {})>
struct PatternVar : SymbolicTag {
  static constexpr auto id = kMeta<PatternVar<Id>>;
};

// Create pattern variables (each call to PatternVar{} has unique type)
inline constexpr PatternVar x_;
inline constexpr PatternVar y_;
inline constexpr PatternVar z_;
```

**Why aren't PatternVars part of Expression?**

In symbolic3, PatternVar satisfied `Symbolic` and could appear in expressions.
This felt unified but caused issues:
- Every interpreter had to handle PatternVar (even though it should never appear
  in a "real" expression being evaluated)
- Pattern expressions and real expressions had the same type, making it easy to
  accidentally evaluate a pattern

PatternVar is *metadata about* expressions, not *part of* expressions. It exists
only during compile-time matching to capture subexpressions. Keeping it separate
makes the distinction clear.

**Matching and substitution**

Matching walks pattern and expression in parallel:
- `PatternVar` → match succeeds, bind the expression to that variable
- Same operator with matching children → recurse
- Same constant/symbol → match succeeds
- Mismatch → match fails

```cpp
// Match x_ + Constant<1> against (a * b) + Constant<1>
// Result: {x_ → (a * b)}

// Substitute x_ * Constant<2> with bindings {x_ → (a * b)}
// Result: (a * b) * Constant<2>
```

**Rules**

A rule combines a pattern, a replacement, and an optional predicate:

```cpp
template <typename Pattern, typename Replacement, typename Pred = AlwaysTrue>
struct Rewrite {
  Pattern pattern;
  Replacement replacement;
  Pred predicate;

  template <Symbolic Expr, typename Ctx>
  constexpr auto apply(Expr e, Ctx ctx) const {
    if (auto bindings = match(pattern, e); bindings && predicate(bindings)) {
      return substitute(replacement, bindings);
    }
    return e;  // No match, return unchanged
  }
};
```

**Example: Simplification rules**

```cpp
constexpr auto zero_mul = Rewrite{x_ * zero, zero};
constexpr auto add_zero = Rewrite{x_ + zero, x_};
constexpr auto sub_self = Rewrite{x_ - x_, zero};
constexpr auto exp_log  = Rewrite{exp(log(x_)), x_};

// Ordering rule with predicate to prevent oscillation
constexpr auto comm_add = Rewrite{y_ + x_, x_ + y_,
    [](auto ctx) { return lessThan(get(ctx, x_), get(ctx, y_)); }};
```

**Example: Differentiation as rewrite rules**

Differentiation is naturally a rewrite system. The key insight is that `diff(expr, var)`
can be represented as wrapping `expr` in a `Diff<Var>` operator, then applying
differentiation rules to eliminate the `Diff` nodes:

```cpp
// Diff<X> is an operator meaning "differentiate with respect to X"
template <typename Var>
struct Diff;

// Differentiation rules
constexpr auto diff_const = Rewrite{Diff<v_>(c_), zero,
    [](auto ctx) { return is_constant(get(ctx, c_)); }};

constexpr auto diff_var = Rewrite{Diff<v_>(v_), one};   // d/dx(x) = 1

constexpr auto diff_other_var = Rewrite{Diff<v_>(w_), zero,
    [](auto ctx) { return is_symbol(get(ctx, w_)) &&
                          get(ctx, v_) != get(ctx, w_); }};

// Product rule: d/dx(f*g) = (d/dx f)*g + f*(d/dx g)
constexpr auto diff_mul = Rewrite{
    Diff<v_>(x_ * y_),
    Diff<v_>(x_) * y_ + x_ * Diff<v_>(y_)
};

// Chain rule: d/dx(f(g)) = f'(g) * (d/dx g)
constexpr auto diff_exp = Rewrite{
    Diff<v_>(exp(x_)),
    exp(x_) * Diff<v_>(x_)
};

constexpr auto diff_log = Rewrite{
    Diff<v_>(log(x_)),
    Diff<v_>(x_) / x_
};
```

The `diff` function then becomes:

```cpp
template <typename Var, typename Expr>
constexpr auto diff(Expr e) {
  // 1. Wrap expression in Diff operator
  auto wrapped = Expression<Diff<Var>, Expr>{};

  // 2. Apply differentiation rules until no Diff nodes remain
  return innermost(diff_rules).apply(wrapped, ctx);
}
```

This is elegant: differentiation is just rewriting, using the same machinery as
simplification. The `Diff<Var>` operator is a *deferred computation* marker—it
says "differentiate this" without doing it yet. Rules then propagate and eliminate
these markers.

**Literal values in pattern matching**

Patterns are simple (typically 2-5 nodes), so literal value handling is manageable.
When a pattern contains a `Literal<T>` (runtime value), matching must compare values:

```cpp
// Pattern with literal: match x_ + Literal<double>
// Must check: rhs.value() == pattern_literal.value()
```

This works because:
1. Patterns are small—deep patterns with many literals are rare
2. Literals in patterns are usually constants like 0.0, 1.0
3. The match happens at compile time for type structure, runtime for values

For pure compile-time matching (no runtime literals), the entire match is resolved
by the compiler. This is the common case for symbolic math.

**Catch-all rules**

Every rule system needs a default: if no rules match, return unchanged. This is
automatic when using `Try` or `Choice` combinators:

```cpp
// Try: apply rule, or return unchanged if no match
auto safe_rule = try_strategy(zero_mul);

// Choice: try rules in order, first match wins
auto rules = zero_mul | add_zero | sub_self | identity;
```

**Alternatives considered**:
- *Template specialization per rule*: Current approach in simplify.h. Works but
  rules aren't first-class, can't compose or reorder them.
- *Runtime pattern matching*: Flexible but defeats compile-time goal.

**Do rules make sense for Bayesian modeling?**

Rules are clearly useful for simplification and differentiation (as shown above).
For Bayesian modeling specifically, rules could also express:

- **Log-prob identities**: `log(exp(x)) → x`, `log(a*b) → log(a) + log(b)`
- **Distribution transformations**: Folding constants into distribution parameters
- **Model normalization**: Canonicalizing model structure

However, the core Bayesian operations (constructing log-prob, composing models,
conditioning on data) are better expressed as direct computation than as rewrite
rules. Rules shine for *simplifying* the resulting expressions, not for
constructing them in the first place.

**Verdict**: Rules are essential for the symbolic math that underlies Bayesian
modeling (differentiation, simplification), but model construction itself
remains procedural. This is the right separation of concerns.

### 2.5 Separate Rules from Strategy

**Goal**: Decouple *what* transformations are valid from *how* they're applied,
enabling reuse and control over optimization behavior.

A rule like `x * 0 → 0` is valid anywhere it matches, but *where* and *when* to
apply it affects efficiency. Applying it top-down before recursing avoids
processing the subtree `x`. Applying it bottom-up after simplifying `x` might
enable more matches. We want to specify rules once and choose strategies separately.

**Approach in C++**: Rules are types describing pattern/replacement pairs.
Strategies are types describing how to traverse and apply rules. Combine them
at the call site.

```cpp
// Rules (WHAT)
using IdentityRules = RuleSet<AddZero, MulOne, ...>;
using InverseRules = RuleSet<ExpLog, LogExp, NegNeg, ...>;

// Strategies (HOW)
using QuickPass = TopDown<IdentityRules>;      // Short-circuit early
using CleanupPass = BottomUp<InverseRules>;    // Clean up after other transforms
using FullSimplify = Innermost<AllRules>;      // Exhaustive simplification
```

This separation enables:
- Reusable rule sets across different strategies
- Domain-specific optimization pipelines
- Control over traversal for efficiency

**Alternatives considered**:
- *Hardcoded traversal per rule set*: Simpler but inflexible—can't reuse rules
  with different strategies.
- *Runtime strategy selection*: Possible but we prefer compile-time composition
  for zero overhead.

### 2.6 Strategy Combinators

**Goal**: Build complex traversal strategies from simple, composable pieces.

Rather than implementing each strategy (bottom-up, top-down, innermost, etc.)
separately, we want a small set of primitives that combine to express any
strategy. This is the approach from Stratego and other term rewriting systems.

**Approach in C++**: Strategies are types with an `apply` method. Combinators
are templates that compose strategies.

| Combinator | Meaning |
|------------|---------|
| `Id` | Identity (no change) |
| `Fail` | Always fails (signals no match) |
| `Seq<S1, S2>` | Sequence: apply S1, then S2 to result |
| `Choice<S1, S2>` | Try S1; if fails/unchanged, try S2 |
| `All<S>` | Apply S to all children |
| `Try<S>` | Apply S, or identity if fails |

Traversals are derived from these primitives:
```cpp
// Bottom-up: recurse on children, then apply strategy at root
template<Strategy S>
using BottomUp = Seq<All<BottomUp<S>>, S>;

// Top-down: apply strategy at root, then recurse on children
template<Strategy S>
using TopDown = Seq<S, All<TopDown<S>>>;

// Innermost: exhaustively apply bottom-up until no more matches
template<Strategy S>
using Innermost = BottomUp<Try<Seq<S, Innermost<S>>>>;
```

**Alternatives considered**:
- *Monolithic traversal implementations*: Simpler to understand individually
  but harder to extend and verify. Combinators give algebraic guarantees.
- *Runtime combinator objects*: More flexible but incurs overhead. Type-level
  combinators are resolved entirely at compile time.

### 2.7 Confluence and Termination

**Goal**: Ensure that rewriting produces predictable, unique results regardless
of rule application order, and always terminates.

If rules can apply in different orders with different results, optimization
becomes unpredictable. If rewriting can loop forever, compilation hangs. We
need guarantees.

**Properties we want**:
- **Terminating**: No infinite rewrite sequences
- **Confluent**: All rule application orders reach the same final result

**Approach**: Design rules carefully and add safety limits.

1. *Size-decreasing rules*: Most rules reduce expression size (x+0 → x removes
   a node). These can't loop.

2. *Guarded non-decreasing rules*: Rules like `y+x → x+y` (canonical ordering)
   don't decrease size. Add predicates so they fire at most once (only when
   x < y in some ordering).

3. *Iteration limits*: As a safety net, limit rewriting iterations. If the
   limit is hit, something is wrong with the rule set.

```cpp
template<Strategy S, size_t MaxIter = 100>
struct Innermost {
  // After MaxIter iterations, stop even if not at fixpoint
};
```

**Alternatives considered**:
- *Prove confluence formally*: Ideal but requires significant tooling (Knuth-
  Bendix completion, critical pair analysis). Pragmatic approach is careful
  rule design plus iteration limits.
- *No guarantees*: Unacceptable—silent infinite loops or nondeterministic
  results are worse than compile errors.

### 2.8 Evaluation and Bindings

**Goal**: Evaluate expressions to numeric values given variable bindings.

Evaluation is interpretation that collapses an expression tree to a value.
The result type depends on the expression—integers evaluate to int, floats
to double, etc.

**Bindings**

Bindings map symbols to values. They're a compile-time heterogeneous map:

```cpp
// Bindings as type-value pairs
template <typename... Pairs>
struct Bindings {
  std::tuple<typename Pairs::value_type...> values_;

  template <typename Sym>
  constexpr auto get() const {
    // Find Sym in Pairs..., return corresponding value
  }
};

// Usage
auto bindings = bind<x>(3.0).bind<y>(2.0);
auto result = eval(x * y + 1.0_c, bindings);  // 7.0
```

**How eval works**

Evaluation is a bottom-up traversal (fold/cata):

1. **Leaves**: Look up symbols in bindings, return constants directly
2. **Internal nodes**: Apply operator to evaluated children

```cpp
struct Eval {
  // Constant: value is in the type
  template <auto V>
  static constexpr auto terminal(Constant<V>, auto) { return V; }

  // Literal: value is stored in the object
  template <typename T>
  static constexpr auto terminal(Literal<T> lit, auto) { return lit.value(); }

  // Symbol: look up in bindings
  template <typename Id>
  static constexpr auto terminal(Symbol<Id>, auto ctx) {
    return ctx.bindings.template get<Symbol<Id>>();
  }

  // Combine: apply operator to child results
  template <typename Op, typename... Results>
  static constexpr auto combine(auto, Results... children) {
    return Op::apply(children...);
  }
};

template <typename Expr, typename Bindings>
constexpr auto eval(Expr e, Bindings b) {
  return fold<Eval>(e, Context{.bindings = b});
}
```

**Context**

Context carries information through traversal:

```cpp
template <typename Bindings = EmptyBindings, size_t Depth = 0>
struct Context {
  Bindings bindings;
  static constexpr size_t depth = Depth;

  // Increment depth (for recursion limits)
  template <size_t N = 1>
  constexpr auto increment_depth() const {
    return Context<Bindings, Depth + N>{bindings};
  }
};
```

The context is passed to every node during traversal. Different operations
use it differently:
- `eval`: Uses bindings to look up symbol values
- `diff`: Uses it to know which variable to differentiate with respect to
- `simplify`: May use it for mode flags (aggressive vs. conservative)

**Memoization**

For transformations where the same subexpression appears multiple times,
we could memoize results. However, since our expressions are types, the
compiler already does this via template instantiation caching. Each unique
expression type is only processed once.

This is a key benefit of type-level representation: memoization is free.

### 2.9 Distributions, Plates, and Sampling

**Goal**: Express probabilistic models where variables are drawn from distributions.

Bayesian modeling requires:
1. **Distributions**: Normal, Beta, Gamma, etc. with symbolic parameters
2. **Random variables**: Named samples from distributions
3. **Plates**: I.i.d. replication (N observations from same distribution)
4. **Log-probability**: Symbolic expressions for log p(data | params)

**Distributions**

A distribution is a function returning a symbolic log-probability expression:

```cpp
// Normal distribution: log p(x | μ, σ)
template <typename Mean, typename Stddev>
struct NormalDist {
  Mean mean;
  Stddev stddev;

  template <typename X>
  constexpr auto logProb(X x) const {
    // -0.5 * log(2π) - log(σ) - 0.5 * ((x - μ) / σ)²
    auto z = (x - mean) / stddev;
    return -0.5_c * log(2.0_c * pi) - log(stddev) - 0.5_c * z * z;
  }
};

// Helper function
template <typename M, typename S>
constexpr auto normal(M mean, S stddev) {
  return NormalDist<M, S>{mean, stddev};
}
```

The key: `logProb` returns a *symbolic expression*, not a number. This
enables symbolic differentiation for gradients.

**Random variables**

A random variable combines identity (a name) with a distribution:

```cpp
template <typename Id, typename Dist>
struct RandomVar {
  Dist distribution;

  // Create the log-prob expression: log p(this | params)
  constexpr auto logProb() const {
    return distribution.logProb(Symbol<Id>{});
  }
};

// Usage
struct mu_tag;
auto mu = RandomVar<mu_tag, NormalDist<...>>{normal(0.0_c, 10.0_c)};
```

**Plates**

Plate notation represents i.i.d. replication. `plate<N>(dist)` means N
independent draws from the same distribution:

```cpp
template <typename Tag, typename Dist>
struct Plate {
  Dist distribution;
  // N is determined at model instantiation time

  // Log-prob sums over all N observations
  template <size_t N>
  constexpr auto logProb() const {
    // Σᵢ log p(xᵢ | params) for i = 0..N-1
    return sum_over<N>([&](auto i) {
      return distribution.logProb(IndexedSymbol<Tag, i>{});
    });
  }
};

// Usage
auto observations = plate<obs_tag>(normal(mu, sigma));
```

**Model composition**

A model collects random variables and computes total log-probability:

```cpp
auto mu = normal(0.0_c, 10.0_c);         // Prior: μ ~ N(0, 10)
auto sigma = halfNormal(5.0_c);          // Prior: σ ~ HalfNormal(5)
auto y = plate<Y>(normal(mu, sigma));    // Likelihood: yᵢ ~ N(μ, σ)

auto m = model(mu, sigma, y);
auto posterior = m.posterior()
    .withDimension<Y>(100)               // 100 observations
    .observe(y = data)                   // Condition on data
    .build();

// posterior.logProb() returns symbolic expression for log p(μ, σ | data)
// posterior.gradLogProb<mu>() returns symbolic gradient
```

**Why symbolic?**

The log-probability and its gradient are symbolic expressions. This means:
1. **Exact gradients**: No finite differences, no autodiff tape
2. **Simplification**: Redundant terms eliminated before evaluation
3. **Compile-time optimization**: The entire expression is known at compile time
4. **Zero runtime overhead**: Evaluating the gradient is just arithmetic

### 2.10 Index Variables and Tensors

**Goal**: Support indexed expressions like `Σᵢ xᵢ * wᵢ` for plates and
eventually matrix/tensor operations.

**Current implementation** (`indexed/`):

Index variables and summation are implemented and used by plates:

```cpp
// Index variable ranging over dimension D
template <typename D>
struct Index;

// Indexed symbol: xᵢ (x subscript i)
template <typename Base, typename... Indices>
struct IndexedSymbol;

// Summation: evaluates body for each index value and sums
template <size_t N, typename F>
constexpr auto sum_over(F body);
```

**How plates use indexing**:

When you write `plate<Y>(normal(mu, sigma))`, the log-probability becomes:
```cpp
// Σᵢ log p(yᵢ | μ, σ) for i = 0..N-1
sum_over<N>([&](auto i) {
  return normal(mu, sigma).logProb(IndexedSymbol<Y, decltype(i)>{});
});
```

Evaluation loops over the index range, looking up `y[i]` from bindings.

**Tensors (future)**:

Multi-indexed symbols for matrices:

```cpp
// Matrix multiplication: Cᵢₖ = Σⱼ Aᵢⱼ * Bⱼₖ
using j = Index<K>;
auto matmul = Sum<j, A[i][j] * B[j][k]>{};
```

**Open questions**:
- Pattern matching for indexed expressions (how to match `Σᵢ (aᵢ + bᵢ)`?)
- Dimension checking at compile time
- Efficient lowering to vectorized code
- Differentiation through sums (chain rule: d/dx Σᵢ f(x,i) = Σᵢ d/dx f(x,i))

---

## 3. Detailed Design

This section describes the proposed extensions to the existing codebase. The
current implementation already has solid foundations (core types, traversals,
interpreters). The main addition is declarative rewriting via pattern matching
and strategies.

### 3.1 What Exists vs. What's Proposed

**Already implemented** (keep and build on):
- `core.h`: Expression types, Symbolic concept
- `scheme/`: `cata`, `fold`, `para` traversal machinery
- `interpreter/`: `eval`, `diff`, `simplify`, `to_string`
- `distributions/`: Log-prob functions, wrappers, RandomVar, plates
- `model.h`: Model definition and posterior construction

**Proposed additions**:
- `strategy/`: Pattern matching and composable rewriting (§3.2)

The existing interpreters work well but are inflexible—each is a monolithic
implementation. The strategy system makes rewriting composable and controllable.
Longer term, `diff` and `simplify` should be reimplemented using rewrite rules.

### 3.2 Strategy System

**Status**: Implemented in `symbolic3/`, needs porting to symbolic4.

See §2.4-§2.6 for the conceptual foundation. This section focuses on
implementation details specific to the port.

**Source files to port from symbolic3**:
- `pattern_matching.h`: PatternVar, BindingContext, Rewrite, RewriteSystem
- `strategy.h`: Strategy concept, combinators (Sequence, Choice, Try, FixPoint)
- `traversal.h`: TopDown, BottomUp, Innermost, Outermost

**Unifying cata and strategies**:

symbolic4 already has `cata<Interpreter, Mode>` for bottom-up traversal.
The Strategy system from symbolic3 uses a different interface (`apply` method).

**Proposed unification**: Everything is a Strategy. The Interpreter interface
(`terminal`, `combine`) becomes a helper for implementing bottom-up strategies,
not a separate concept:

```cpp
// Strategy concept: anything with apply()
template <typename S>
concept Strategy = requires(S s, SomeExpr e, SomeCtx ctx) {
  { s.apply(e, ctx) };
};

// Eval is a strategy returning double
double result = bottomUp(Eval{}).apply(expr, ctx);

// Simplify is a strategy returning Expr
auto simplified = innermost(SimplifyRules{}).apply(expr, ctx);
```

This unification means:
- `cata`/`fold`/`para` remain as low-level machinery
- `bottomUp`, `topDown`, etc. become the public API
- Both evaluation and rewriting use the same Strategy interface

**Porting to symbolic4**:

Main change is handling `Atom<Id, Effect>`:
- Add matching cases for different Effects
- Decide how pattern variables interact with Effects
- Integrate with or replace existing `interpreter/simplify.h`

### 3.3 Error Handling and Diagnostics

**Compile-time errors**:

Most errors surface at compile time as template instantiation failures:
- Missing binding for a symbol → `get<Symbol<X>>` fails to find type in Bindings
- Type mismatch → operator overload resolution fails
- Invalid pattern → match function won't compile

These produce compiler errors with type information. The error messages can be
cryptic (deep template stacks), but the errors are caught early.

**Runtime errors**:

Few runtime errors are possible since most validation happens at compile time.
Potential runtime issues:
- Domain errors (log of negative, sqrt of negative) → undefined behavior or NaN
- Numeric overflow/underflow → normal floating-point behavior
- Division by zero → inf or NaN

The library doesn't add runtime checks for these—it assumes the user provides
valid inputs. For Bayesian modeling, the MCMC sampler may encounter bad regions
of parameter space; the sampler handles this by rejecting proposals.

**Debugging**:

For debugging complex expressions:
- `to_string` produces human-readable output
- Compile-time: use `static_assert` with type traits
- Runtime: print intermediate values during evaluation

**Compile-time costs**:

Expression types can grow large. Each operation creates a new type:
```cpp
// x + y + z has type:
Expression<Add, Expression<Add, Symbol<X>, Symbol<Y>>, Symbol<Z>>
```

Deep nesting increases:
- Template instantiation depth (compiler limits, typically 500-1000)
- Compile time (roughly linear in expression size for simple traversals)
- Error message length

Mitigations:
- `fold_unique` deduplicates shared subexpressions
- Simplification reduces expression size before further processing
- Avoid deeply nested expressions where possible

In practice, Bayesian models with ~100 parameters compile in reasonable time.
Gradient expressions can be large (1000s of nodes) but simplify dramatically.

### 3.4 Directory Organization

```
symbolic4/
├── core.h, operators.h, ...     # Existing: expression types
├── scheme/                       # Existing: traversal machinery
├── interpreter/                  # Existing: eval, diff, simplify, to_string
├── distributions/                # Existing: probability distributions
├── model.h                       # Existing: Bayesian model definition
│
└── strategy/                     # NEW: composable rewriting (port from symbolic3)
    ├── pattern.h                 # Pattern variables, matching, substitution
    ├── rule.h                    # Rule definition, RuleSet
    ├── combinator.h              # Id, Fail, Seq, Choice, Try
    └── traversal.h               # All, BottomUp, TopDown, Innermost
```

---

## 4. Implementation Roadmap

This roadmap is organized by milestones. Each milestone delivers concrete
capabilities that can be used independently. Later milestones build on earlier
ones but the system is usable after each milestone completes.

### Current State (Milestone 0)

**What we have today**:
- Expression types: `Atom<Id, Effect>`, `Expression<Op, Args...>`
- Traversals: `fold`, `para` for bottom-up processing
- Interpreters: `eval`, `diff`, `simplify`, `to_string`
- Distributions: 15+ distributions with log-prob, RandomVar, plates
- Model building: `model()`, `.posterior()`, `.observe()`
- MCMC: Basic HMC sampler

**What we can do**:
- Define Bayesian models with priors and likelihoods
- Automatically compute log-probability expressions
- Symbolically differentiate for HMC gradients
- Run basic inference on scalar and hierarchical models

See `examples/hmc_symbolic4_example.cc` for a complete working example.

**Limitations**:
- Simplification is monolithic (can't control traversal order)
- No short-circuit optimization (0 * big_expr still traverses big_expr)
- Can't easily add new simplification rules

---

### Milestone 1: Declarative Rewriting

**Goal**: Port pattern matching and strategy system from symbolic3 to symbolic4,
adapting for the Atom/Effect model. Enable declarative rule definition and
flexible traversal control.

**Prior art**: This is largely implemented in `symbolic3/`:
- `pattern_matching.h`: PatternVar, BindingContext, Rewrite, RewriteSystem
- `strategy.h`: Strategy concept, combinators (Sequence, Choice, Try, FixPoint)
- `traversal.h`: Fold, Unfold, TopDown, BottomUp, Innermost, Outermost

**What needs adaptation**:
- Pattern matching for `Atom<Id, Effect>` (currently handles Symbol/Constant)
- Matching on Effect types (Free, Embedded, Sample, Compute)
- Integration with symbolic4's existing simplify.h

**What this enables**:
- Write rules like `Rewrite{x_ + zero, x_}` instead of template specializations
- Combine rules into RewriteSystem
- Choose traversal: `topdown(rules)` vs `bottomup(rules)`
- Compose strategies: `strategy1 >> strategy2`, `strategy1 | strategy2`

**Key deliverables**:
1. Port `PatternVar`, `BindingContext`, `extractBindings`, `substitute`
2. Port `Rewrite`, `RewriteSystem` with predicate support
3. Port strategy combinators: `Identity`, `Fail`, `Sequence`, `Choice`, `Try`
4. Port traversals: `Fold`, `Unfold`, `TopDown`, `BottomUp`, `Innermost`
5. Integrate with existing simplify.h or replace it

**Success criteria**:
- Can define algebraic simplification rules declaratively
- Can apply same rules with different traversal orders
- `topdown(zero_mul_rule)` on `0 * (big expression)` returns 0 without
  traversing the subtree
- Existing simplify behavior reproducible with new system

**Example usage after this milestone** (based on symbolic3 API):
```cpp
// Define rules declaratively (symbolic3 syntax)
constexpr auto zero_mul = Rewrite{zero * x_, zero};
constexpr auto mul_zero = Rewrite{x_ * zero, zero};
constexpr auto add_zero = Rewrite{x_ + zero, x_};

// Combine into rule system
constexpr auto quick_rules = RewriteSystem{zero_mul, mul_zero, add_zero};

// Apply top-down for short-circuit behavior
auto simplified = topdown(quick_rules).apply(complex_expr, ctx);

// Or compose strategies
auto pipeline = topdown(quick_rules) >> bottomup(collection_rules);
auto result = FixPoint{pipeline}.apply(expr, ctx);
```

---

### Milestone 2: Enhanced Bayesian Inference

**Goal**: Improve the Bayesian modeling workflow with better simplification,
more distributions, and diagnostic tools.

**What this enables**:
- Cleaner gradient expressions (fewer redundant terms)
- Support for more model structures (mixture models, GPs)
- Diagnostic output (effective sample size, R-hat)
- Prior/posterior predictive checks

**Key deliverables**:
1. **Improved simplification pipeline** using Milestone 1 strategies
2. **Additional distributions**: Dirichlet, Multinomial, LKJ, Wishart
3. **Sampler diagnostics**: ESS, R-hat, trace plots
4. **Predictive generation**: Prior predictive, posterior predictive

**Success criteria**:
- Gradient expressions 30%+ smaller after simplification
- Can fit hierarchical models with 100+ parameters
- Diagnostic output matches Stan/PyMC quality

**Example usage after this milestone**:
```cpp
// Define hierarchical model
auto mu = normal(0.0_c, 10.0_c);
auto sigma = halfNormal(5.0_c);
auto y = plate<Observations>(normal(mu, sigma));

auto m = model(mu, sigma, y);
auto posterior = m.posterior()
    .withDimension<Observations>(100)
    .observe(y = data)
    .build();

// Run inference with diagnostics
auto [samples, diagnostics] = posterior.sample(
    HMC{.num_samples = 1000, .warmup = 500}
);

// Check convergence
assert(diagnostics.rhat < 1.01);
assert(diagnostics.min_ess > 400);

// Posterior predictive
auto predictions = posterior.predictive(y, samples);
```

---

## 5. Appendix: Theoretical Background

### A.1 Term Rewriting Systems

A term rewriting system (TRS) consists of:
- A set of terms (our Expression types)
- A set of rewrite rules: `l → r` where l, r are term patterns
- A rewrite relation: `t₁ →* t₂` if t₁ can be transformed to t₂ by rules

**Key properties**:
- **Terminating**: No infinite rewrite chains (guaranteed by size-decreasing rules)
- **Confluent**: All paths lead to same result (guaranteed by non-overlapping rules)
- **Normal form**: A term that cannot be rewritten further

### A.2 Strategy Combinators (Stratego)

Strategies control rule application:
- `id`: Identity (no change)
- `fail`: Always fails
- `s₁ ; s₂`: Sequence
- `s₁ <+ s₂`: Choice (try s₁, else s₂)
- `all(s)`: Apply s to all children
- `try(s)`: s or identity

Traversals are derived:
```
bottomup(s) = all(bottomup(s)) ; s
topdown(s)  = s ; all(topdown(s))
innermost(s) = bottomup(try(s ; innermost(s)))
```

### A.3 Bayesian Inference and MCMC

**Bayesian modeling**: Express beliefs as probability distributions.
- Prior: p(θ) — beliefs before seeing data
- Likelihood: p(data|θ) — how likely data is given parameters
- Posterior: p(θ|data) ∝ p(data|θ)p(θ) — updated beliefs

**MCMC**: Draw samples from posterior when direct sampling is intractable.
- Markov chain whose stationary distribution is the target posterior
- Samples approximate the posterior distribution

**Hamiltonian Monte Carlo (HMC)**:
- Uses gradient of log-posterior for efficient proposals
- Requires: log p(θ|data) and ∇θ log p(θ|data)
- Symbolic differentiation gives exact gradients (no finite differences)

**Why compile-time symbolic?**
- Model structure is known at compile time
- Log-prob and gradient can be derived symbolically
- Simplification eliminates redundant computation
- Generated code is as efficient as hand-written

### A.4 Recursion Schemes (Reference)

For completeness, the formal names for traversal patterns:

| Name | Pattern | Description |
|------|---------|-------------|
| Catamorphism (fold) | Bottom-up | Process children, then combine |
| Anamorphism (unfold) | Top-down | Generate structure from seed |
| Paramorphism | Bottom-up + context | Access original subtree during fold |
| Hylomorphism | Unfold then fold | Composition of ana and cata |

These come from category theory (F-algebras) but the practical intuition is
just "different ways to traverse a tree."

---

## 6. References

**Bayesian Inference & MCMC**:
1. Gelman et al. "Bayesian Data Analysis" (3rd ed, 2013)
2. Neal. "MCMC using Hamiltonian dynamics" (2011) - HMC tutorial
3. Hoffman & Gelman. "The No-U-Turn Sampler" (2014) - NUTS algorithm

**Term Rewriting**:
4. Baader & Nipkow. "Term Rewriting and All That" (1998)
5. Visser. "Stratego: A Language for Program Transformation" (2001)

**Scheduling**:
6. Coffman. "Computer and Job-Shop Scheduling Theory" (1976)
7. Kwok & Ahmad. "Static Scheduling Algorithms for Allocating DAGs" (1999)

**Matrix Algorithms**:
8. Cormen et al. "Introduction to Algorithms" - Ch. 15 (Matrix Chain)

**Recursion Schemes** (optional background):
9. Meijer et al. "Bananas, Lenses, Envelopes and Barbed Wire" (1991)
