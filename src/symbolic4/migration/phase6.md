# Phase 6: Symbol-Forward MCMC

## Goal

Extend the symbol-lookup pattern through the full MCMC pipeline so that
model definition, sampling, and post-processing all use the same
`container[expr]` idiom. Eliminate manual post-processing loops.

## Prerequisites

None — this phase is **independent** of phases 1–5. Can be done in parallel
with the strategy migration.

The standalone operator fix (Task 6.0) has zero dependencies and should be
done first since it simplifies all subsequent model-building code.

## Task 6.0: Relax Operator `requires` Clause (standalone)

**File to modify:** `src/symbolic4/operators.h`

The binary operators at lines 155–185 have:

```cpp
template <SymbolicLike L, SymbolicLike R>
  requires(Symbolic<L> || Symbolic<R>)  // overly restrictive
constexpr auto operator+(L lhs, R rhs) { ... }
```

The `requires(Symbolic<L> || Symbolic<R>)` prevents two SymbolicLike-but-not-Symbolic
operands from combining. For example, `sigma * z_b` where both are RandomVar/IndexedRandomVar
fails because neither satisfies `Symbolic` directly.

**Fix:** Remove the extra `requires` clause from all four binary operators (+, -, *, /)
and unary negation. The `SymbolicLike L, SymbolicLike R` template parameters already
exclude `int`/`double` (which don't satisfy SymbolicLike), so there's no ambiguity risk.

```cpp
// BEFORE:
template <SymbolicLike L, SymbolicLike R>
  requires(Symbolic<L> || Symbolic<R>)
constexpr auto operator+(L lhs, R rhs) { ... }

// AFTER:
template <SymbolicLike L, SymbolicLike R>
constexpr auto operator+(L lhs, R rhs) { ... }
```

**Impact:** Every model definition gets simpler:
```cpp
// Before:
auto p = 1_c / (1_c + exp(-(a.constrainedExpr() + sigma.constrainedExpr() * z_b.sym())));

// After:
auto p = logistic(a + sigma * z_b);
```

**Test:** Update `bmj_symbolic.cpp` to use the simplified syntax. All existing tests
must still pass (asSymbolic() already does the right conversion internally).

**Risk:** Low. The `asSymbolic()` function already handles all SymbolicLike types
correctly (RandomVar → constrainedExpr(), IndexedData → sym()). We're just
removing the gate that prevented it from being called.

Check for edge cases:
- `RandomVar + RandomVar` → both asSymbolic → Expression of two Sample atoms ✓
- `RandomVar * IndexedRandomVar` → Sample atom * IndexedSymbol ✓
- `IndexedData + IndexedRandomVar` → both go through sym() ✓
- `2.0 + RandomVar` → double not SymbolicLike, won't match, user uses lit(2.0) ✓

## Task 6.1: `samples[expr]` — Evaluate Derived Quantities Across Draws

**Files to modify:**
- `src/symbolic4/mcmc/samples.h` — add `operator[](Expr)` to both `Samples` and `DynamicSamples`

### Design

A symbolic expression is a function of its free symbols. A Samples container
binds those symbols to values (one binding per draw). So `samples[expr]` means
"evaluate this expression at each draw's parameter values."

For `Samples<Params...>` (scalar-only params):

```cpp
// Evaluate a scalar expression across all draws
template <Symbolic Expr>
auto operator[](Expr expr) const -> std::vector<double> {
    std::vector<double> result;
    result.reserve(draws_.size());
    for (const auto& draw : draws_) {
        // Build bindings from this draw's values
        auto bindings = buildBindings(draw, std::index_sequence_for<Params...>{});
        result.push_back(evaluate(expr, bindings));
    }
    return result;
}
```

For `DynamicSamples<SymbolsTuple, SpecsTuple>` (indexed params):

```cpp
// Evaluate a scalar expression across all draws
template <Symbolic Expr>
auto operator[](Expr expr) const -> std::vector<double> { ... }

// Evaluate an indexed expression across all draws → matrix (draws × dim)
template <Symbolic Expr>
  requires is_indexed_expr_v<Expr>
auto operator[](Expr expr) const -> matrix3::DynamicDense<double> { ... }
```

### Challenges

1. **Building bindings from draw values**: For scalar params, straightforward
   (`param_sym = draw[offset]`). For indexed params, need to reconstruct
   IndexedBinding from the draw's slice of values.

2. **Detecting scalar vs indexed result**: The expression might be scalar
   (e.g., `a + sigma`) or indexed (e.g., `sigma * z_b`). The return type
   should differ. Use `if constexpr` with expression traits to dispatch.

3. **Constrained vs unconstrained**: Samples store CONSTRAINED values (after
   transform). But expressions built from RandomVar use Sample atoms that
   expect unconstrained values. Need to either:
   - Store both spaces in Samples, or
   - Inverse-transform before evaluating, or
   - Have expressions that work in constrained space directly

   **Recommended:** Add a `constrainedEval` path that evaluates expressions
   with constrained values bound directly to the free symbols (bypassing
   the Sample atom's constraint transform).

### Example

```cpp
auto a = normal(-2, 1);
auto sigma = exponential(1);
auto z_b = plate<Countries>(normal(0, 1));

// Model defines derived quantity
auto p = logistic(a + sigma * z_b);

// After sampling...
auto samples = posterior.sample(config, init, rng);

// Symbol lookup on raw params
auto a_draws = samples[a];           // vector<double>: constrained a values
auto z_b_draws = samples[z_b];       // matrix: draws × countries

// Symbol lookup on derived expression — no manual loops!
auto p_draws = samples[p];           // matrix: draws × countries
mean(samples[a + sigma * z_b]);      // scalar: mean of linear predictor
```

## Task 6.2: `grad[indexed_param]` Returns Span

**File to modify:** `src/symbolic4/mcmc/plate_transforms.h`
(GradientResult class, lines 103–172)

Currently, `operator[]` always returns `double`:

```cpp
template <typename RV>
auto operator[](const RV&) const -> double {
    return grad_[offsets_[idx]];
}
```

**Fix:** Dispatch based on whether the param spec is scalar or indexed:

```cpp
template <typename RV>
auto operator[](const RV&) const {
    constexpr std::size_t idx = findSymbolIndex<RV>();
    if constexpr (is_scalar_param_spec_v<std::tuple_element_t<idx, SpecsTuple>>) {
        return grad_[offsets_[idx]];  // double
    } else {
        std::size_t n = std::get<idx>(specs_).size();
        return std::span<const double>{grad_.data() + offsets_[idx], n};  // span
    }
}
```

This mirrors how `DynamicSamples::operator[]` already dispatches
(returns `vector<double>` for scalar, `DynamicDense` for indexed).

## Task 6.3: Init Values in Constrained Space

**File to modify:** `src/symbolic4/mcmc/plate_transforms.h`
(PlateTransformedPosterior::sample and init methods)

Currently, `sample()` accepts `BinderPack{sigma = log(0.5)}` — unconstrained values.

**Fix:** Apply `inverse()` from each param spec's transform inside `initStateFromBindings`:

```cpp
template <std::size_t I, typename... Binders, std::size_t N>
void extractBindingValue(BinderPack<Binders...> bindings,
                         std::array<double, N>& z, std::size_t& offset) const {
    // ...
    double constrained_value = static_cast<double>(bindings[SymType{}]);
    // NEW: convert constrained → unconstrained
    z[offset] = std::get<I>(specs_).transform.inverse(constrained_value);
    offset += 1;
}
```

For indexed params, apply inverse element-wise.

**Breaking change:** Existing code that passes unconstrained init values will break.
Either:
- Option A: Change the semantics and update all call sites (cleaner)
- Option B: Add a `sampleConstrained()` method alongside existing `sample()` (safer)
- Option C: Use a wrapper type `constrained(value)` vs bare value (explicit)

**Recommended:** Option A. The number of call sites is small (examples + tests).
The current behavior is a foot-gun that should be fixed.

## Acceptance Criteria

- [ ] `a + sigma * z_b` compiles without `.constrainedExpr()`/`.sym()`
- [ ] `samples[expr]` evaluates scalar expressions across draws
- [ ] `samples[indexed_expr]` returns matrix for indexed expressions
- [ ] `grad[indexed_param]` returns `std::span<const double>`
- [ ] Init values are in constrained space; posterior applies inverse internally
- [ ] `bmj_symbolic.cpp` simplified to use new syntax
- [ ] All existing tests pass

## Build & Test

```bash
cmake --build build && ctest --test-dir build -R symbolic4
```
