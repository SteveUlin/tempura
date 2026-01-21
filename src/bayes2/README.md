# bayes2 - Compile-Time Probabilistic Programming

A refined probabilistic programming DSL with compile-time DAG construction and
first-class MCMC integration. Built on the same foundations as `bayes/graph/`
but with a cleaner, unified API.

## Architecture

### Type-Encoded DAGs

The entire probabilistic graph is encoded in types at compile time:

```cpp
auto mu = normal(0.0, 10.0);     // RandomVar<λ₁, normalDist<Lit, Lit>>
auto sigma = halfNormal(5.0);    // RandomVar<λ₂, halfNormalDist<Lit>>
auto y = normal(mu, sigma);      // RandomVar<λ₃, normalDist<Sym, Sym>, μ_type, σ_type>
```

Each node carries:
- **Unique type identity** via `decltype([] {})` - distinct type per call site
- **Distribution** with symbolic parameters
- **Parent tuple** - other `RandomVar`s this node depends on

The DAG structure exists purely in types. No runtime graph container needed.

### Symbolic Expressions

Distribution parameters and computations use `symbolic3`:

```cpp
// mu.sym() returns Symbol<λ₁> - a unique symbolic variable
// sigma.sym() returns Symbol<λ₂>
// y's distribution stores normalDist{mu.sym(), sigma.sym()}
```

This enables:
- **Compile-time log-prob expressions**: `jointLogProb(y)` returns a symbolic expression
- **Automatic differentiation**: `diff(expr, mu)` computes symbolic gradients
- **Evaluation**: `evaluate(expr, BinderPack{mu = 1.0, sigma = 2.0, y = 2.5})`

### DAG Traversal

`jointLogProb(node)` traverses parents recursively at compile time:

```cpp
// For y = normal(mu, sigma):
// jointLogProb(y) expands to:
//   lognormal(y.sym(), mu.sym(), sigma.sym())   // y's contribution
// + lognormal(mu.sym(), 0, 10)                   // mu's contribution
// + loghalfNormal(sigma.sym(), 5)                // sigma's contribution
```

Nodes are deduplicated via `IdSet` (type-level set) to handle diamond dependencies.

## Goals for bayes2

### 1. Unified DSL

Consolidate `bayes/graph/` and `bayes/sym/` into one coherent API:

```cpp
// Define model
auto mu = normal(0.0, 10.0);
auto sigma = halfNormal(5.0);
auto y = normal(mu, sigma);

// Build posterior (observations + inference targets)
auto posterior = Posterior(y)
    .observe(y = 2.5)
    .params(mu, sigma);

// Use with MCMC
double lp = posterior.logProb(1.0, 2.0);
auto grad = posterior.gradient(1.0, 2.0);
```

### 2. First-Class MCMC Integration

The `Posterior` type directly satisfies MCMC requirements:

```cpp
concept MCMCTarget = requires(T t, std::span<const double> params) {
    { t.numParams() } -> std::same_as<size_t>;
    { t.logProb(params) } -> std::same_as<double>;
    { t.gradient(params) } -> convertible_to<std::array<double, N>>;
};
```

No manual wiring:

```cpp
auto chain = HMC(posterior, {.step_size = 0.1, .num_steps = 10});
for (auto& sample : chain | take(1000)) {
    // sample[0] = mu, sample[1] = sigma
}
```

### 3. Constraint Transforms

Distributions imply constraints; transforms happen automatically:

```cpp
auto sigma = halfNormal(5.0);  // Positive constraint
auto p = beta(2.0, 2.0);       // UnitInterval constraint
```

The `Posterior` handles:
- **Transform**: constrained → unconstrained (for HMC)
- **Inverse**: unconstrained → constrained (for evaluation)
- **Jacobian**: adjustment to log-prob

### 4. Vectorized Observations

Handle multiple data points cleanly:

```cpp
auto mu = normal(0.0, 10.0);
auto sigma = halfNormal(5.0);
auto y = normalObs(mu, sigma, data);  // data is std::span<const double>

// jointLogProb(y) sums over all observations
```

## Core Types

### RandomVar

```cpp
template <typename Id, typename Dist, typename... Parents>
struct RandomVar {
    Dist dist_;
    std::tuple<Parents...> parents_;

    static constexpr auto sym();        // Unique symbol for this variable
    constexpr auto nodeLogProb() const; // log p(this | parents)
    constexpr auto operator=(double) const; // Binding syntax
};
```

### DeterministicVar

```cpp
template <typename Id, typename Expr, typename... Parents>
struct DeterministicVar {
    Expr expr_;
    std::tuple<Parents...> parents_;

    static constexpr auto sym();
    constexpr auto nodeLogProb() const { return Literal{0.0}; }
    constexpr auto expr() const;
};
```

### Posterior

New type that bridges DSL and MCMC:

```cpp
template <typename Joint, typename Observations, typename... Params>
struct Posterior {
    // Compile-time: symbolic log-prob expression with observations substituted
    // Runtime: evaluate at parameter values

    static constexpr size_t numParams() { return sizeof...(Params); }

    double logProb(std::span<const double> params) const;
    auto gradient(std::span<const double> params) const;

    // For constrained parameters
    double logProbUnconstrained(std::span<const double> unconstrained) const;
    auto gradientUnconstrained(std::span<const double> unconstrained) const;
};
```

## Implementation Phases

### Phase 1: Core (MVP)
- [ ] `RandomVar` and `DeterministicVar` (port from graph/)
- [ ] Distribution factories (normal, halfNormal, beta, Gamma, etc.)
- [ ] Operators (+, -, *, /)
- [ ] `jointLogProb` traversal
- [ ] Basic tests

### Phase 2: Posterior
- [ ] `Posterior` type with observe/params
- [ ] `logProb` and `gradient` methods
- [ ] Parameter ordering

### Phase 3: Constraints
- [ ] Constraint types (Positive, UnitInterval, Bounded)
- [ ] Distribution → constraint inference
- [ ] Transform/inverse/Jacobian in Posterior

### Phase 4: MCMC Bridge
- [ ] `MCMCTarget` concept
- [ ] Integration with existing HMC
- [ ] Chain as input_range

### Phase 5: Vectorization
- [ ] `normalObs`, `BernoulliObs`, etc. for vectorized likelihood
- [ ] Efficient summation over observations
