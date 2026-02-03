# Phase 7: Decouple Infrastructure from Domain

## Goal

Separate the domain-independent expression/evaluation/binding infrastructure from
probabilistic-specific code. Unify all containers on the compressed-tuple symbolic
lookup pattern. Eliminate the scalar/indexed asymmetry that currently forces two
parallel implementations of everything.

## Prerequisites

- Phases 1–5 (done)
- Independent of phase 6 (can proceed in parallel)
- Independent of matrix algebra tracks

## Core Insight: Three Layers, Not Two

Currently the codebase mixes transforms into evaluation. The evaluator knows about
constraint transforms (for `Sample` atoms), and the posterior knows about constraint
transforms (pre-applied for indexed params). Same concern, two locations, different
behavior.

The fix: **three clean layers** with strict dependency direction.

```
Domain Layer  →  defines effects, transforms, expressions
Transform Layer  →  maps between representation spaces
Binding Layer  →  symbol → typed value (heterogeneous, via overload dispatch)
Evaluation Layer  →  traverse expression, loop over dimensions, apply ops
Expression Algebra  →  Atom, Expression, ReduceOver, Grad
```

Arrows only point down. The evaluation layer has zero domain knowledge.

## Principle: Compressed Tuples with Symbolic Lookup

The BinderPack pattern (multiple inheritance + `using Binders::operator[]...`) is
the canonical container. Each symbol maps to its own typed storage slot:

```cpp
container[mu]       // → double&        (scalar)
container[z_a]      // → span<double>   (indexed, N elements)
container[Sigma]    // → DynamicDense&  (matrix, K×K)
```

No flat arrays, no manual offset computation, no type erasure. The return type is
heterogeneous — determined by the symbol, not homogenized to `double`.

**Why this matters for matrix params:** A `MatrixSymbol<Id, Dim, Dim, PositiveDefinite>`
needs to map to a `DynamicDense<double>`. You can't fit that into a `double` slot in
a flat array. The compressed-tuple approach handles this naturally — the slot for
`Sigma` simply stores a `DynamicDense<double>`.

**The flat HMC state vector is an internal detail.** HMC needs a contiguous `double*`
for leapfrog integration. The symbolic state provides typed views into regions of this
flat vector. The user never sees offsets; they see `state[mu]` and `state[z_a]`.

## Task 7.0: SymbolicSlot and SymbolicState

**Files to create:**
- `src/symbolic4/symbolic_state.h`

A mutable compressed tuple with symbol-dispatched access.

```cpp
// A single typed slot in a symbolic state
template <typename Sym, typename Storage>
struct SymbolicSlot {
    [[no_unique_address]] Sym sym_;
    Storage storage_;

    constexpr auto& operator[](Sym) { return storage_; }
    constexpr auto const& operator[](Sym) const { return storage_; }
};

// The full state: inherits from all slots
template <typename... Slots>
struct SymbolicState : Slots... {
    using Slots::operator[]...;

    // Construct from a flat vector + layout metadata
    // Each slot gets a view (double&, span, DynamicDense ref) into the flat storage
    explicit SymbolicState(std::span<double> flat, Layout<Slots...> layout);
};
```

**Key properties:**
- `state[mu]` returns `double&` — mutable reference to scalar param
- `state[z_a]` returns `std::span<double>` — mutable view of indexed param
- `state[Sigma]` returns a matrix view — mutable view of matrix param
- Zero overhead for symbol dispatch (compile-time overload resolution)
- Backed by a flat `vector<double>` for HMC compatibility

**Layout:** A compile-time metadata struct that knows the offset and size of each slot
in the flat vector. Constructed once from dimension sizes.

```cpp
template <typename... Slots>
struct Layout {
    std::array<SizeT, sizeof...(Slots)> offsets;
    SizeT total_size;

    // Inferred from dimension metadata
    template <typename DimSizes>
    static constexpr auto fromDimensions(DimSizes dims) -> Layout;
};
```

### Relation to existing types

| Current type | Becomes |
|-------------|---------|
| `ParameterState<Params...>` | `SymbolicState<ScalarSlot<P>...>` |
| `GradientResult` in plate_transforms.h | `SymbolicState<GradSlot<P>...>` |
| Flat `vector<double>` in PlateTransformedPosterior | Internal backing of SymbolicState |

ParameterState and GradientResult's type-to-index + flat array patterns are replaced
by SymbolicSlot's direct overload dispatch. The flat array still exists (for HMC),
but it's internal — users interact with typed slots.

## Task 7.1: Open Effect System

**Files to modify:**
- `src/symbolic4/core.h` — remove `Sample<D>` and `IndexedSample<D, DimsList>`
- `src/symbolic4/distributions/effects.h` — new, defines probabilistic effects

### Move domain effects out of core

Core defines the effect framework and domain-independent effects:

```cpp
// core.h — domain-independent
struct Free {};
template <typename T> struct Embedded { T value; };
template <typename E> struct Compute { E expr; };

template <typename Id, typename Effect>
struct Atom : SymbolicTag { ... };
```

The probabilistic domain defines its own effects:

```cpp
// distributions/effects.h — probabilistic domain
template <typename D> struct Sample { D dist_; };
template <typename D, typename DimsList> struct IndexedSample { D dist_; };

template <typename T> constexpr bool is_sample_effect_v = /* ... */;
template <typename T> constexpr bool is_indexed_sample_effect_v = /* ... */;
// ... all probabilistic type traits move here
```

### What stays in core.h

The `Atom<Id, Effect>` template itself stays — it's the universal leaf node.
The effect parameter is unconstrained (`typename Effect`), making the system
open to any domain's effect types.

The type traits `is_atom_v`, `same_atom_id_v`, etc. stay in core.h since they
work on the generic Atom shape.

### What about auto-discovery?

`collectLogProbs` and `discoverParams` traverse expression trees looking for
`Sample` atoms. This traversal logic stays in `distributions/` where it belongs.
It uses `is_sample_effect_v` (now from `distributions/effects.h`) to recognize
its own domain's atoms. Other domains would write their own traversal using
their own effect traits.

## Task 7.2: Extensible Evaluator Terminal Dispatch

**Files to modify:**
- `src/symbolic4/indexed/indexed_eval.h` — parameterize on terminal handler
- `src/symbolic4/interpreter/eval.h` — thin wrapper over indexed eval

### Terminal handler policy

The evaluator takes a terminal handler as a template parameter:

```cpp
// Base terminals — domain-independent, handles all core types
struct BaseTerminals {
    template <typename T, typename Ctx>
    static constexpr double eval(T expr, Ctx& ctx) {
        if constexpr (is_constant_v<T>)           return T::value;
        else if constexpr (is_fraction_v<T>)      return T::value;
        else if constexpr (is_literal_v<T>)       return expr.effect_.value;
        else if constexpr (is_atom_v<T>)          return ctx.scalars[expr];
        else if constexpr (is_indexed_symbol_v<T>) return lookupIndexed(expr, ctx);
        else static_assert(false, "Unknown terminal type");
    }
};

// Probabilistic terminals — extends base with Sample/IndexedSample handling
struct ProbTerminals {
    template <typename T, typename Ctx>
    static constexpr double eval(T expr, Ctx& ctx) {
        if constexpr (is_random_var_atom_v<T>) {
            auto z = lookupByAtomId(expr, ctx.scalars);
            return constraints::applyNumeric<support_t<T>>(z);
        } else if constexpr (is_indexed_random_var_atom_v<T>) {
            return lookupIndexedRV(expr, ctx);
        } else {
            return BaseTerminals::eval(expr, ctx);
        }
    }
};
```

### Unified evaluator

```cpp
// One evaluator function, parameterized on terminal handler
template <typename Terminals = BaseTerminals>
constexpr auto evaluate(auto expr, auto... bindings);

// In distributions/ or mcmc/, a convenience alias:
template <typename... Args>
constexpr auto evaluateProb(auto expr, Args&&... bindings) {
    return evaluate<ProbTerminals>(expr, std::forward<Args>(bindings)...);
}
```

### Merging scalar and indexed eval

Scalar evaluation is indexed evaluation with no ReduceOver nodes. The merged
evaluator handles both — if the expression has no ReduceOver nodes and no indexed
bindings, the ReduceOver loop code is dead and the compiler eliminates it.

The existing `eval.h` becomes a thin wrapper that calls the unified evaluator
with `BaseTerminals` (or `ProbTerminals` for backward compat).

## Task 7.3: Transforms as a Binding Concern

**Files to create:**
- `src/symbolic4/mcmc/transform_pack.h`

**Files to modify:**
- `src/symbolic4/mcmc/plate_transforms.h` — use TransformPack instead of inline transforms
- `src/symbolic4/mcmc/transforms.h` — same

### The problem

Currently, constraint transforms are applied in two different places:
- **Scalar params:** Inside the evaluator's terminal dispatch (when `Sample<D>` atom
  is encountered, evaluator calls `constraints::applyNumeric<Support>(z)`)
- **Indexed params:** In the posterior's binding construction (pre-transforms `z → x`
  before creating bindings, then expression uses raw `IndexedSymbol` with no transform)

This asymmetry forces two separate posteriors, two gradient paths, and the
`lookupByAtomId` hack in the evaluator.

### The fix: TransformPack

Transforms are a separate layer between the state vector and the bindings.
Applied uniformly for all params, before evaluation.

```cpp
// A transform for one parameter
template <typename Sym, typename Transform>
struct ParamTransform {
    using symbol_type = Sym;
    using transform_type = Transform;

    // z → x (constrained)
    constexpr auto forward(double z) const -> double;

    // x → z (unconstrained)
    constexpr auto inverse(double x) const -> double;

    // d(log|J|)/dz
    constexpr auto logJacobianGrad(double z) const -> double;
};

// A pack of transforms, one per parameter
template <typename... Transforms>
struct TransformPack {
    // Apply all transforms: SymbolicState<unconstrained> → BinderPack<constrained>
    constexpr auto apply(const SymbolicState<...>& state) const;
};
```

### How logProb changes

Before (current PlateTransformedPosterior):
```
logProb(z):
    for each scalar param: bind z directly (transform at eval via Sample atom)
    for each indexed param: transform z → x, bind x (no transform at eval)
    evaluateIndexed(expr, mixed_bindings)
```

After:
```
logProb(z):
    state = SymbolicState(z, layout)         // typed views into flat z
    bindings = transforms.apply(state)        // uniform z → x for ALL params
    evaluate(expr, bindings)                  // no Sample atoms, no transforms in eval
    + sum(transforms.logJacobians(state))     // Jacobian as separate term
```

The evaluator sees only `Free` atoms and `IndexedSymbol` — all already
constrained. No domain knowledge needed.

### How gradient changes

Before:
```
gradient(z):
    for scalar: evaluate grad_expr (chain rule in expression via Sample atom)
    for indexed: evaluateAtIndex per element, then manual chainRuleGrad()
```

After:
```
gradient(z):
    bindings = transforms.apply(state)
    for each param P:
        grad_x = evaluate(grad_expr[P], bindings)    // gradient w.r.t. constrained x
        grad_z = transforms.chainRule<P>(grad_x, z)  // uniform chain rule
```

One code path for both scalar and indexed. The expression only knows about
constrained values. The transform layer handles the unconstrained ↔ constrained
mapping uniformly.

## Task 7.4: Unify Posteriors

**Files to modify:**
- `src/symbolic4/mcmc/plate_transforms.h` — becomes the single posterior
- `src/symbolic4/mcmc/transforms.h` — deprecate TransformedPosterior

### One posterior type

With SymbolicState and TransformPack, the scalar vs indexed distinction disappears
from the posterior's perspective. One type handles everything:

```cpp
template <typename LogProbExpr, typename GradExprs, typename TransformPack,
          typename ObsBindings, typename DataBindings, typename... ParamSlots>
class Posterior {
    // logProb: transform → bind → evaluate
    auto logProb(std::span<const double> z) const -> double;

    // gradient: transform → bind → evaluate grads → chain rule
    auto gradient(std::span<const double> z) const -> GradientResult;

    // Both use the same code path regardless of parameter types
};
```

**GradientResult** becomes a SymbolicState with typed access:
```cpp
auto g = posterior.gradient(z);
g[mu]       // double
g[z_a]      // span<const double>
g[Sigma]    // DynamicDense<double> view
```

**Samples** becomes a SymbolicState per draw:
```cpp
samples[mu]         // vector<double> (one per draw)
samples[z_a]        // DynamicDense (draws × N)
samples[Sigma]      // vector<DynamicDense> (one matrix per draw)
samples[expr]       // evaluate expr at each draw's bindings
```

## Task 7.5: Remove lookupByAtomId

**Files to modify:**
- `src/symbolic4/indexed/indexed_eval.h` — remove lookupByAtomId and related machinery

### Why it exists

`Atom<Id, Sample<D>>` and `Atom<Id, Free>` are different types with the same `Id`.
When the expression contains a `Sample` atom but bindings use a `Free` atom (because
the user binds `x = 5.0` not `Sample<NormalDist>{} = 5.0`), the evaluator can't find
the binding via direct overload resolution. `lookupByAtomId` scans all binders and
matches on `Id` regardless of effect type.

### Why it goes away

After task 7.3, expressions don't contain `Sample` atoms at runtime. The expression
tree uses `Free` atoms (scalar) and `IndexedSymbol` (indexed) — the same types
that bindings use. Direct overload resolution works. No fallback scan needed.

**Note:** `Sample` atoms still exist in the symbolic world for auto-discovery
(`collectLogProbs`, `discoverParams`). But by evaluation time, the transform layer
has already converted to constrained bindings keyed by `Free`/`IndexedSymbol` types.

## Task 7.6: Differentiation Cleanup

**Files to modify:**
- `src/symbolic4/strategy/diff.h` — remove Sample atom interception

### Current state

`DiffRecursive` intercepts `Sample<D>` atoms before delegating to pattern rules,
because `rule(Var{}, 1_c)` matches `Atom<Id, Free>` but not `Atom<Id, Sample<D>>`.

### After phase 7

If the expression being differentiated uses `Free` atoms (because transforms are
applied at the binding level), the standard rules handle everything:

```cpp
rule(Var{}, 1_c)          // d/dx[x] = 1  — matches Atom<Id, Free> directly
rule(AnySymbol{}, 0_c)    // d/dx[y] = 0  — matches other Free atoms
```

The `Sample` interception in DiffRecursive becomes dead code and can be removed.

**Important subtlety:** The gradient expression is differentiated w.r.t. the
*constrained* parameter (since the expression uses constrained values). The
chain rule through the constraint transform is applied by the TransformPack,
not by the symbolic differentiator. This is actually simpler — the differentiator
doesn't need to know about constraints at all.

## Ordering

```
7.0 SymbolicSlot / SymbolicState        (independent, new file)
7.1 Open effect system                   (independent, refactor core.h)
7.2 Extensible evaluator terminals       (depends on 7.1)
7.3 TransformPack                        (independent, new file + refactor posteriors)
7.4 Unify posteriors                     (depends on 7.0, 7.2, 7.3)
7.5 Remove lookupByAtomId               (depends on 7.2, 7.3)
7.6 Differentiation cleanup             (depends on 7.3)
```

Tasks 7.0, 7.1, and 7.3 can start in parallel.

## Acceptance Criteria

- [ ] `core.h` defines only `Free`, `Embedded<T>`, `Compute<E>` effects
- [ ] `indexed_eval.h` does not include any file from `distributions/` or `constraints.h`
- [ ] One evaluator handles both scalar-only and indexed expressions
- [ ] One posterior type handles scalar, indexed, and (eventually) matrix params
- [ ] `state[sym]` returns the natural type for that symbol (double, span, matrix)
- [ ] `gradient[sym]` returns the natural type (double, span, matrix)
- [ ] All existing tests pass with no behavioral changes
- [ ] Constraint transforms applied uniformly in TransformPack, not in evaluator
- [ ] `diff.h` has no `Sample` or `IndexedSample` handling

## Build & Test

```bash
cmake --build build && ctest --test-dir build -R symbolic4
```
