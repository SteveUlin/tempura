# symbolic4 - Making MCMC Boring

## Vision

The goal is to make MCMC "boring" - users define models declaratively, and the library handles all the plumbing:
- Automatic parameter transforms (log, logit) based on distribution support
- Automatic Jacobian corrections for change of variables
- Symbolic gradients via automatic differentiation
- Compile-time expression optimization

Users write `auto posterior = infer(y)` and get a working HMC-ready posterior.

## Case Studies: Statistical Rethinking Examples

The `examples/stat_rethinking/` directory contains implementations of homework problems from Richard McElreath's Statistical Rethinking 2025 course. **These examples are the litmus test for API completeness.**

| Example | Status | What It Tests |
|---------|--------|---------------|
| `linear_regression.cpp` | Manual gradients | Basic regression with plates |
| `logistic_regression.cpp` | Manual gradients | Binary outcomes, Bernoulli likelihood |
| `bangladesh_contraception.cpp` | Manual everything | Hierarchical model, varying effects, Dirichlet simplex, ordered monotonic |
| `bmj_weekend_submissions.cpp` | Manual | Poisson regression |
| `bmj_symbolic.cpp` | Symbolic | Simpler model using symbolic4 API |

### Current Pain Points (from examples)

The examples reveal what users currently have to do manually:

```cpp
// User WANTS to write:
auto y = plate<Obs>(normal(alpha + beta * x, sigma));
auto posterior = model(alpha, beta, sigma, y)
    .observe(y = data)
    .build();

// User ACTUALLY writes:
auto full_log_prob = [&](const State& theta) {
  double lp = 0.0;
  // Manual prior computation
  lp += -0.5 * theta[0] * theta[0] / 100.0;
  // Manual likelihood loop
  for (size_t i = 0; i < N; ++i) {
    double mu = theta[0] + theta[1] * x[i];
    lp += -0.5 * pow((y[i] - mu) / theta[2], 2) - log(theta[2]);
  }
  return lp;
};

auto full_gradient = [&](const State& theta) {
  // Hand-derive and implement gradients...
};
```

### What "Complete" Looks Like

When the API is complete, `bangladesh_contraception.cpp` should look like:

```cpp
// Hyperpriors
auto a_bar = normal(0, 1);
auto sigma_a = exponential(1);

// Varying intercepts (non-centered)
auto z_a = plate<Districts>(normal(0, 1));
auto a = a_bar + sigma_a * z_a;  // Deterministic transform

// Ordered monotonic effect
auto delta = dirichlet({2, 2, 2});
auto bK = orderedMonotonic(delta);

// Likelihood
auto y = plate<Obs>(bernoulli(logistic(a[district] + bK * children)));

// One line to get posterior
auto posterior = infer(y).observe(y = data).build();
auto samples = hmc(posterior, 2000, 500);
```

### API Gaps to Close

1. **Data-dependent likelihoods**: `plate<Obs>(normal(mu, sigma))` with external covariates `x[i]`
2. **Automatic gradient derivation**: From symbolic log-prob through data sums
3. **Non-centered parameterization**: Built into plate notation
4. **Ordered monotonic effects**: Simplex → cumulative transform
5. **Varying effects syntax**: `a[district]` indexing into plates
6. **State layout automation**: No manual index management

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     User-Facing APIs                             │
│  model(mu, sigma, y).posterior().observe(y=3.5).build()         │
│  infer(y)  // auto-discovers mu, sigma                          │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                    Probabilistic Layer                           │
│  distributions/   RandomVar<Dist,Id>   plates   collectLogProbs │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      MCMC Layer                                  │
│  transforms   support inference   TransformedPosterior   HMC    │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                   Interpreter Layer                              │
│  eval.h   diff.h   simplify.h   to_string.h                     │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Core Layer                                  │
│  Atom<Id,Effect>   Expression<Op,Args...>   scheme/cata.h       │
└─────────────────────────────────────────────────────────────────┘
```

## Key Design Principle: Types ARE the Representation

Expressions are encoded entirely in C++ types at compile time:
- `x + y` creates `Expression<AddOp, Atom<X,Free>, Atom<Y,Free>>`
- No runtime AST, no heap allocation, no type tags
- Full compiler optimization and constexpr support

## Core Abstractions

### Atoms (leaf nodes)
`Atom<Id, Effect>` with four effects:
- `Free` - Variable needing binding (`Symbol<X>`)
- `Embedded<T>` - Runtime literal value
- `Sample<D>` - Random variable from distribution D
- `Compute<E>` - Deterministic function

### Expressions (internal nodes)
`Expression<Op, Args...>` where operators are callable functors serving dual roles:
- AST construction: `x + y` builds the type
- Evaluation: `AddOp{}(3.0, 4.0)` returns `7.0`

### RandomVar
`RandomVar<Dist, Id>` pairs a distribution with a unique identity, enabling:
- Symbolic log-probability computation
- Automatic parent discovery via expression traversal
- Transform inference from distribution support

## File Organization

| Directory | Purpose |
|-----------|---------|
| `core.h`, `operators.h` | Expression system, type traits |
| `scheme/` | Recursion schemes (fold, para, cata) |
| `distributions/` | Distribution wrappers, RandomVar, log-probs |
| `interpreter/` | eval, diff, simplify, to_string |
| `mcmc/` | Transforms, posteriors, HMC adapter |
| `indexed/` | Plate notation for vectorized parameters |
| `matrix/` | Multivariate support (MVN, LKJ) |

## Usage Patterns

### Current API (working but inconsistent)
```cpp
auto alpha = normal(0.0, 10.0);
auto beta = normal(0.0, 5.0);
auto sigma = halfNormal(2.0);
auto x = data<Obs>();
auto y = plate<Obs>(normal(alpha + beta * x, sigma));

auto posterior = infer(alpha, beta, sigma, y)
    .bind(x = x_data, y = y_data);

// Problem: flat arrays lose parameter identity
using State = std::array<double, 3>;
auto hmc = makeHMC<double, 3>(
    [&](const State& z) { return posterior.logProb(z); },
    [&](const State& z) { return State{...}; },  // Manual unpacking
    epsilon, steps);
```

### Target API (binding-centric)
```cpp
auto alpha = normal(0.0, 10.0);
auto beta = normal(0.0, 5.0);
auto sigma = halfNormal(2.0);
auto x = data<Obs>();
auto y = plate<Obs>(normal(alpha + beta * x, sigma));

auto posterior = infer(alpha, beta, sigma, y)
    .bind(x = x_data, y = y_data);

// Unified binding syntax throughout
auto samples = posterior.sample(
    hmc(epsilon = 0.01, steps = 20),
    warmup = 500,
    draws = 1000,
    init = (alpha = 0.0, beta = 0.0, sigma = 0.0),
    rng);

// Samples queryable by symbol
double mean_alpha = mean(samples[alpha]);
double mean_beta = mean(samples[beta]);
```

### Hierarchical with plates (target)
```cpp
auto alpha = gamma(2.0, 0.1);
auto theta = plate<Groups>(beta(alpha, 1.0));

auto posterior = infer(alpha, theta, y)
    .bind(y = y_data);  // Dimension auto-inferred

auto samples = posterior.sample(
    hmc(epsilon = 0.01, steps = 20),
    init = (alpha = 1.0, theta = std::vector<double>(38, 0.5)),
    rng);

// Access indexed samples
auto theta_samples = samples[theta];  // Matrix: draws × groups
```

## Current Status

Working:
- Core expression system with type-level AST
- 15+ distributions with symbolic log-probs
- Automatic differentiation via symbolic diff
- Parameter transforms with Jacobian corrections
- HMC sampling via bayes/ integration
- Basic plate support with `data<Dim>()` and `plate<Dim>()`
- `infer()` with `.bind()` for data and observations
- `GradientResult` for symbol-queryable gradients

Gaps:
- Simplex/Cholesky transforms not integrated into auto-transform
- MCMC diagnostics (ESS, R-hat) not implemented
- Binding-centric API not complete (see roadmap)

## Roadmap: Binding-Centric API

See `mcmc/BINDING_API_PLAN.md` for detailed implementation plan.

**Goal**: Unify all parameter interactions around binding syntax.

| Step | Feature | Status |
|------|---------|--------|
| 1 | `ParameterState` class | 🔲 Not started |
| 2 | `posterior(alpha = 0.5, ...)` evaluation | 🔲 Not started |
| 3 | `gradientAt(alpha = 0.5, ...)` | 🔲 Not started |
| 4 | `posterior.state(alpha, beta, ...)` | 🔲 Not started |
| 5 | `Samples` container with symbol access | 🔲 Not started |
| 6 | `posterior.sample(hmc(...), ...)` | 🔲 Not started |
| 7 | Update examples to new API | 🔲 Not started |

**Key insight**: The posterior already knows its parameters from `infer(alpha, beta, sigma, y)`. All subsequent operations should use this knowledge rather than requiring flat arrays with implicit ordering.
