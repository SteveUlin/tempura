# Symbolic Bayesian Inference Library

## Overview

This document proposes a Bayesian inference library built on top of `symbolic3`. The core idea: represent probabilistic models as compile-time symbolic expressions, then leverage automatic differentiation and pattern matching for inference.

**Why symbolic?** Traditional probabilistic programming libraries (Stan, PyMC) parse model definitions at runtime, build computation graphs, and perform automatic differentiation during sampling. This works, but carries overhead. We can eliminate this overhead by moving the entire model representation to the C++ type system. The compiler becomes our model compiler.

**Why build on symbolic3?** The library already provides the exact primitives we need:
- `Symbol<unique>` gives us named random variables
- `Expression<Op, Args...>` represents mathematical relationships
- `diff()` computes symbolic gradients for HMC/NUTS
- `evaluate()` substitutes concrete values
- Pattern matching enables conjugacy detection

We avoid building a second symbolic system by reusing what exists.

---

## Motivation

### The Problem with Runtime Probabilistic Programming

Consider what happens when you write a model in Stan:

```stan
parameters { real mu; real<lower=0> sigma; }
model {
  mu ~ normal(0, 10);
  sigma ~ half_normal(5);
  y ~ normal(mu, sigma);
}
```

Stan's compiler parses this text, builds an AST, generates C++ code, compiles that code, then runs it. Each sampling iteration evaluates the log-probability and its gradient by traversing the computation graph.

This architecture made sense in 2012. Modern C++ offers a better path: encode the model structure directly in the type system. The "compilation" step happens during C++ compilation. The "graph traversal" becomes template instantiation. The resulting binary contains specialized code for exactly this model.

### What We Gain

**Zero-overhead model definition.** A model like `Normal(μ, σ)` with `μ ~ Normal(0, 10)` compiles to the same machine code you would write by hand. No runtime graph, no virtual dispatch, no memory allocation during sampling.

**Compile-time gradient computation.** We compute symbolic derivatives once, during compilation. The sampler evaluates pre-simplified gradient expressions. Compare this to reverse-mode AD, which builds tape during forward pass and replays during backward pass—every single iteration.

**Static model validation.** The type system catches errors like "you forgot to specify a prior for θ" or "you conditioned on a variable that isn't in the model." These become compile errors, not runtime crashes 10,000 iterations into your MCMC run.

**Conjugacy as an optimization.** Pattern matching detects conjugate prior-likelihood pairs at compile time. When we recognize Normal-Normal or Beta-Binomial structure, we skip MCMC entirely and compute the closed-form posterior. The user writes the same model; the library chooses the best inference algorithm.

---

## Design Principles

### 1. Models Are Types

A probabilistic model is a type, not a value. This sounds restrictive but provides powerful guarantees.

**Why types?** Because the C++ type system already handles everything we need: composition (tuple of expressions), transformation (template metafunctions), and validation (concepts and static_assert). We get these for free instead of reimplementing them at runtime.

The model `μ ~ Normal(0, 10), x | μ ~ Normal(μ, 1)` becomes something like:
```cpp
Expression<AddOp,
    Expression<LogNormalOp, Symbol<μ_id>, Constant<0>, Constant<10>>,
    Expression<LogNormalOp, Symbol<x_id>, Symbol<μ_id>, Constant<1>>>
```

This type fully describes the model's dependency structure. Template metaprogramming can analyze it, transform it, and generate efficient evaluation code.

### 2. Log-Probability as the Universal Interface

Every distribution exposes one thing: its log-probability density (or mass) function. We represent models as sums of log-probabilities.

**Why log-probability?** Three reasons:

First, MCMC algorithms work with log-probabilities. Metropolis-Hastings computes acceptance ratios as differences of log-probabilities. HMC uses gradients of log-probability. By representing everything in log-space, we match the sampler's native interface.

Second, log-space handles extreme probabilities gracefully. A probability of 10^-300 underflows to zero in linear space. In log-space, it's just -690—a normal floating-point number. Bayesian inference routinely encounters such values in the tails.

Third, products become sums. Bayes' theorem says posterior ∝ likelihood × prior. In log-space: log-posterior = log-likelihood + log-prior + constant. Addition is simpler than multiplication, and we avoid numerical issues from multiplying many small probabilities.

### 3. Gradients Come Free

We compute gradients symbolically using `symbolic3::diff()`. The gradient expressions exist at compile time, alongside the log-probability expressions.

**Why symbolic gradients?** Performance and correctness.

For performance: reverse-mode automatic differentiation (what PyTorch and JAX use) builds a "tape" of operations during the forward pass, then walks backward to compute gradients. This tape consumes memory proportional to the computation depth, and traversing it adds overhead. Symbolic differentiation produces a closed-form expression. Evaluating that expression costs the same as evaluating any other expression—no tape, no backward pass, no overhead.

For correctness: symbolic gradients are exact. We can verify them at compile time using `static_assert`. Numerical gradients (finite differences) suffer from truncation and rounding errors. AD implementations can have bugs that only manifest for specific expression structures. Symbolic differentiation produces an expression we can inspect and test.

### 4. Inference Algorithms Are Strategies

We separate model definition from inference. A model describes a probability distribution; an inference algorithm produces samples from (or approximations to) that distribution.

**Why separate?** Because the same model might warrant different inference approaches depending on context. A small dataset might use MCMC for exact inference. A large dataset might use variational inference for speed. A conjugate model might use closed-form updates. By decoupling model and inference, users choose the right tool for their problem without rewriting the model.

The existing `bayes::Chain` infrastructure already embodies this philosophy. A `Chain` takes a log-probability function and a proposal distribution, then produces samples. We plug symbolic models into this interface.

---

## Architecture

### Layer 1: Log-Probability Primitives

The foundation consists of functions that construct symbolic log-probability expressions.

```cpp
namespace tempura::bayes::sym {

// Normal: log p(x|μ,σ) = -½log(2π) - log(σ) - (x-μ)²/(2σ²)
template <Symbolic X, Symbolic Mu, Symbolic Sigma>
constexpr auto logNormal(X x, Mu μ, Sigma σ) {
    // We break this into pieces for clarity and to enable simplification
    auto z = (x - μ) / σ;                    // Standardized value
    auto log_norm = log(σ) + kLogSqrt2Pi;    // Normalization term
    return Fraction<-1,2>{} * z * z - log_norm;
}

// Beta: log p(x|α,β) = (α-1)log(x) + (β-1)log(1-x) - logB(α,β)
template <Symbolic X, Symbolic Alpha, Symbolic Beta>
constexpr auto logBeta(X x, Alpha α, Beta β) {
    return (α - 1_c) * log(x) + (β - 1_c) * log(1_c - x) - logBetaFn(α, β);
}

// Gamma: log p(x|α,β) = α·log(β) + (α-1)log(x) - β·x - logΓ(α)
template <Symbolic X, Symbolic Alpha, Symbolic Beta>
constexpr auto logGamma(X x, Alpha α, Beta β) {
    return α * log(β) + (α - 1_c) * log(x) - β * x - logGammaFn(α);
}

}  // namespace tempura::bayes::sym
```

**Why functions instead of classes?** Because we want to compose log-probabilities with ordinary arithmetic. Writing `logNormal(x, μ, σ) + logNormal(μ, 0_c, 10_c)` feels natural. A class-based design would require special composition operators.

**Why include normalization constants?** For most MCMC purposes, we could drop them—they cancel in acceptance ratios. But we include them because:
1. Model comparison (Bayes factors) requires proper normalization
2. Variational inference uses absolute log-probability values
3. Debugging is easier when log-probabilities match textbook formulas

### Layer 2: Model Definition

A model combines log-probability terms and distinguishes parameters from observations.

```cpp
namespace tempura::bayes::sym {

// A Model bundles the log-probability expression with metadata about
// which symbols are parameters (to be inferred) vs data (observed).
template <Symbolic LogProb, typename ParamSymbols, typename DataSymbols>
struct Model {
    using log_prob_type = LogProb;
    using param_symbols = ParamSymbols;  // TypeList<Symbol<...>, ...>
    using data_symbols = DataSymbols;    // TypeList<Symbol<...>, ...>

    // Number of parameters to infer
    static constexpr std::size_t num_params = Length<ParamSymbols>;

    // Pre-computed gradient expressions (one per parameter)
    using gradient_type = /* tuple of diff(LogProb, param) for each param */;
};

// Builder for ergonomic model construction
template <Symbolic... Params>
struct ModelBuilder {
    template <Symbolic LogProb>
    static constexpr auto withLogProb(LogProb) {
        return Model<LogProb, TypeList<Params...>, TypeList<>>{};
    }
};

// Usage:
constexpr Symbol μ, σ, x;

constexpr auto log_prob =
    logNormal(μ, 0_c, 10_c) +      // prior: μ ~ Normal(0, 10)
    logHalfNormal(σ, 5_c) +        // prior: σ ~ HalfNormal(5)
    logNormal(x, μ, σ);            // likelihood: x ~ Normal(μ, σ)

using MyModel = decltype(ModelBuilder<μ, σ>::withLogProb(log_prob));

}  // namespace tempura::bayes::sym
```

**Why distinguish parameters from data?** Because inference treats them differently. Parameters are unknowns we sample; data are knowns we condition on. The model needs this distinction to:
1. Know how many dimensions the parameter space has
2. Compute gradients only with respect to parameters
3. Construct the right type of state vector for MCMC

**Why a builder pattern?** Because we need to specify which symbols are parameters before we can compute gradients. The builder captures this information, then `withLogProb` uses it to construct the full model type with pre-computed gradient expressions.

### Layer 3: Model Instantiation

A model instance binds observed data and provides callable interfaces for inference.

```cpp
namespace tempura::bayes::sym {

template <typename Model, typename DataBindings>
class ModelInstance {
public:
    constexpr ModelInstance(DataBindings data) : data_{data} {}

    // Evaluate log-probability at given parameter values
    // This is what MCMC chains call repeatedly
    template <typename ParamValues>
    constexpr auto logProb(const ParamValues& params) const {
        auto bindings = merge(data_, packParams<Model>(params));
        return evaluate(typename Model::log_prob_type{}, bindings);
    }

    // Evaluate gradient at given parameter values
    // HMC calls this alongside logProb
    template <typename ParamValues>
    constexpr auto gradient(const ParamValues& params) const {
        auto bindings = merge(data_, packParams<Model>(params));
        return evaluateGradient<Model>(bindings);
    }

    // Create a proposal-aware version for HMC
    auto forHMC(double step_size, int num_leapfrog) const {
        return HMCAdapter{*this, step_size, num_leapfrog};
    }

private:
    DataBindings data_;
};

// Convenience function
template <typename Model>
constexpr auto instantiate(Model, auto... data_bindings) {
    return ModelInstance<Model, decltype(BinderPack{data_bindings...})>{
        BinderPack{data_bindings...}
    };
}

// Usage:
constexpr Symbol μ, σ, x;
// ... define model ...

auto model = instantiate(MyModel{}, x = observed_value);
double lp = model.logProb(std::array{0.5, 1.2});  // μ=0.5, σ=1.2
auto grad = model.gradient(std::array{0.5, 1.2});

}  // namespace tempura::bayes::sym
```

**Why a separate instance type?** Because the same model structure can apply to different datasets. The model (a type) describes the probability distribution family. The instance (a value) binds specific observations. This mirrors how we use distributions: `Normal` is a family; `Normal(0, 1)` is a specific distribution.

**Why return arrays for gradient?** Because MCMC algorithms treat the parameter space as a flat vector. Even if our symbolic representation has named parameters, the sampler sees `std::array<double, N>`. The gradient returns the same shape.

### Layer 4: Conjugacy Detection

Pattern matching identifies conjugate prior-likelihood pairs and triggers closed-form updates.

```cpp
namespace tempura::bayes::sym {

// Pattern for Normal likelihood with Normal prior on mean
// log p(x|μ,σ) + log p(μ|μ₀,σ₀) where σ is known
struct NormalNormalPattern {
    static constexpr PatternVar x_, μ_, σ_, μ0_, σ0_;

    static constexpr auto pattern =
        logNormal(x_, μ_, σ_) + logNormal(μ_, μ0_, σ0_);

    // Check if an expression matches this conjugate structure
    template <Symbolic E>
    static constexpr bool matches() {
        auto bindings = extractBindings(pattern, E{});
        if constexpr (detail::isBindingFailure<decltype(bindings)>()) {
            return false;
        } else {
            // Additional check: σ_ must be a constant (known variance)
            return is_constant<decltype(get(bindings, σ_))>;
        }
    }

    // Compute closed-form posterior given data
    template <Symbolic E>
    static auto posteriorParams(E, std::span<const double> data) {
        auto bindings = extractBindings(pattern, E{});

        // Extract prior parameters
        double μ0 = evaluateConstant(get(bindings, μ0_));
        double σ0 = evaluateConstant(get(bindings, σ0_));
        double σ = evaluateConstant(get(bindings, σ_));

        // Conjugate update formulas
        double prior_precision = 1.0 / (σ0 * σ0);
        double data_precision = data.size() / (σ * σ);
        double post_precision = prior_precision + data_precision;

        double data_mean = std::accumulate(data.begin(), data.end(), 0.0)
                         / data.size();
        double post_mean = (prior_precision * μ0 + data_precision * data_mean)
                         / post_precision;
        double post_σ = 1.0 / std::sqrt(post_precision);

        return std::pair{post_mean, post_σ};
    }
};

// Dispatch to closed-form or MCMC based on model structure
template <typename Model>
auto infer(ModelInstance<Model> instance, auto data, auto inference_config) {
    if constexpr (NormalNormalPattern::matches<typename Model::log_prob_type>()) {
        // Conjugate case: return exact posterior
        auto [μ_post, σ_post] = NormalNormalPattern::posteriorParams(
            typename Model::log_prob_type{}, data);
        return Normal{μ_post, σ_post};
    }
    else if constexpr (BetaBinomialPattern::matches<typename Model::log_prob_type>()) {
        // Another conjugate case
        return betaBinomialUpdate(instance, data);
    }
    else {
        // General case: use MCMC
        return mcmcInfer(instance, inference_config);
    }
}

}  // namespace tempura::bayes::sym
```

**Why detect conjugacy?** Because closed-form posteriors are exact and instantaneous. MCMC produces approximate samples and takes time. When conjugacy exists, we should exploit it automatically. Users shouldn't need to know which prior-likelihood pairs are conjugate—the library figures it out.

**Why pattern matching?** Because conjugacy depends on structural properties of the model, not runtime values. The expression `logNormal(x, μ, σ) + logNormal(μ, 0, 10)` is conjugate regardless of what `σ` equals. Pattern matching extracts this structure at compile time.

**Why the additional σ_ constant check?** Because Normal-Normal conjugacy requires known variance. If σ is also a parameter (unknown), the conjugacy breaks. The pattern matches the syntactic form; the predicate checks the semantic requirement.

### Layer 5: MCMC Integration

Connect symbolic models to the existing `bayes::Chain` infrastructure.

```cpp
namespace tempura::bayes::sym {

// Adapter that makes a ModelInstance callable for Chain
template <typename ModelInstance>
struct ChainAdapter {
    ModelInstance model;

    template <typename State>
    auto operator()(const State& params) const {
        return model.logProb(params);
    }
};

// HMC-specific adapter that provides leapfrog integration
template <typename ModelInstance>
class HMCAdapter {
public:
    HMCAdapter(ModelInstance model, double step_size, int num_steps)
        : model_{model}, step_size_{step_size}, num_steps_{num_steps} {}

    template <typename State, typename RNG>
    auto operator()(const State& position, RNG& rng) const {
        // Sample momentum from standard normal
        auto momentum = sampleMomentum<State::size()>(rng);

        // Leapfrog integration
        auto [new_pos, new_mom] = leapfrog(position, momentum);

        return new_pos;
    }

private:
    template <typename State>
    auto leapfrog(State q, State p) const {
        auto grad = model_.gradient(q);

        // Half step for momentum
        for (std::size_t i = 0; i < q.size(); ++i) {
            p[i] += 0.5 * step_size_ * grad[i];
        }

        // Full steps
        for (int step = 0; step < num_steps_; ++step) {
            // Full step for position
            for (std::size_t i = 0; i < q.size(); ++i) {
                q[i] += step_size_ * p[i];
            }

            // Full step for momentum (except at end)
            if (step < num_steps_ - 1) {
                grad = model_.gradient(q);
                for (std::size_t i = 0; i < q.size(); ++i) {
                    p[i] += step_size_ * grad[i];
                }
            }
        }

        // Half step for momentum
        grad = model_.gradient(q);
        for (std::size_t i = 0; i < q.size(); ++i) {
            p[i] += 0.5 * step_size_ * grad[i];
        }

        return std::pair{q, p};
    }

    ModelInstance model_;
    double step_size_;
    int num_steps_;
};

// Top-level inference function
template <typename Model>
auto sample(ModelInstance<Model> model,
            std::size_t num_samples,
            auto initial_params,
            auto rng,
            InferenceConfig config = {}) {

    using State = decltype(initial_params);

    if (config.algorithm == Algorithm::MetropolisHastings) {
        auto proposal = GaussianWalkND<double, Model::num_params>(config.step_size);
        auto log_prob = ChainAdapter{model};

        return chain<State>(log_prob, proposal, rng, initial_params)
            | std::views::drop(config.warmup)
            | std::views::stride(config.thin)
            | std::views::take(num_samples)
            | std::views::transform([](auto s) { return s.value; })
            | std::ranges::to<std::vector>();
    }
    else if (config.algorithm == Algorithm::HMC) {
        auto hmc_proposal = model.forHMC(config.step_size, config.num_leapfrog);
        auto log_prob = ChainAdapter{model};

        return chain<State>(log_prob, hmc_proposal, rng, initial_params)
            | std::views::drop(config.warmup)
            | std::views::take(num_samples)
            | std::views::transform([](auto s) { return s.value; })
            | std::ranges::to<std::vector>();
    }
}

}  // namespace tempura::bayes::sym
```

**Why adapt rather than rewrite Chain?** Because `Chain` already handles the core MCMC logic correctly: proposal, acceptance, iteration. We gain nothing by duplicating that code. The adapter pattern lets symbolic models plug into existing infrastructure with minimal glue.

**Why include HMC?** Because symbolic gradients make HMC practical. HMC requires gradient evaluations at every leapfrog step—potentially dozens per sample. With runtime autodiff, this overhead dominates. With symbolic gradients, it's just evaluating a mathematical expression. The cost drops dramatically.

---

## Model DSL

### The Goal

We want models to read clearly and express their statistical structure. The syntax should:
1. Be valid C++ without macro tricks
2. Visually resemble statistical notation
3. Support flexible role assignment (same variable can be observed OR inferred depending on context)
4. Encode constraints that are intrinsic to variables

### Design Philosophy: Roles Are Contextual, Not Intrinsic

After studying how other PPLs handle variable roles, we adopt a key insight:

**A random variable's role (observed vs inferred) is a property of how the model is USED, not an intrinsic property of the variable itself.**

The same variable `y` might be:
- **Observed** in `model.condition(y = data)` for posterior inference
- **Sampled** in `model.priorPredictive()` for prior checks
- **Partially observed** in `model.condition(y[known_idx] = partial_data)` for missing data

Making this a compile-time type distinction (like `Param<T>` vs `Data<T>`) reduces flexibility without proportional safety benefits. This is why:
- **PyMC** uses runtime `observed=` argument
- **Edward/TFP** binds variables to data at inference time
- **Turing.jl** determines roles when the model function is called
- **Stan's** block separation is a language construct, not a type distinction

### What SHOULD Be Types

However, some things genuinely deserve type-level encoding:

| Property | Type-Level? | Why |
|----------|-------------|-----|
| **Constraint** (σ > 0) | ✓ Yes | Intrinsic to the variable—σ is ALWAYS positive |
| **Dimensionality** | ✓ Yes | Intrinsic—β is ALWAYS a 3-vector |
| **Deterministic vs Stochastic** | ✓ Yes | Derived quantities are computed, never sampled |
| **Observed vs Inferred** | ✗ No | Contextual—depends on usage |

### Syntax: The `|` Operator

We use `|` (pipe) to associate variables with distributions:

```cpp
constexpr Symbol mu;
constexpr Symbol sigma;
constexpr Symbol y;

auto model = Model{
    mu    | Normal(0.0, 10.0),     // mu has Normal prior
    sigma | HalfNormal(5.0),       // sigma has HalfNormal prior
    y     | Normal(mu, sigma)      // y has Normal likelihood
};
```

**Why `|`?**
- `~` is unary in C++, so `a ~ b` doesn't parse
- `|` is binary and visually suggests conditioning: p(mu | ...)
- Same solution used by [AutoPPL](https://github.com/JamesYang007/autoppl) with `|=`
- Reads naturally: "mu given Normal(0, 10)"

### Role Assignment at Usage Time

```cpp
// Define model structure (no roles yet)
auto model = Model{
    mu    | Normal(0.0, 10.0),
    sigma | HalfNormal(5.0),
    y     | Normal(mu, sigma)
};

// Usage 1: Standard posterior inference
// y is observed, (mu, sigma) are inferred
auto posterior = model
    .observe(y = data)
    .infer(mu, sigma);

// Usage 2: Prior predictive (all variables sampled)
auto prior_pred = model
    .infer(mu, sigma, y);  // No observations, sample everything

// Usage 3: Missing data imputation
// Some y values observed, some inferred
auto imputation = model
    .observe(y[known_idx] = known_data)
    .infer(mu, sigma, y[missing_idx]);

// Usage 4: Semi-supervised learning
// Some labels observed, others inferred
auto semisup = model
    .observe(y[labeled_idx] = labels)
    .infer(mu, sigma, y[unlabeled_idx]);
```

This flexibility is impossible with rigid `Param<T>` vs `Data<T>` types.

### Constraint Types

Constraints ARE intrinsic to variables and deserve type-level encoding. A variable `sigma` that represents a standard deviation is ALWAYS positive, regardless of whether it's observed or inferred.

```cpp
namespace tempura::bayes::sym {

// =============================================================================
// CONSTRAINT TYPES - Encode domain restrictions with automatic transforms
// =============================================================================

// Unconstrained (default)
struct Unconstrained {
    static constexpr auto transform(double x) { return x; }
    static constexpr auto inverse(double y) { return y; }
    static constexpr auto logJacobian(double) { return 0.0; }
};

// Positive reals: R → R+ via exp
struct Positive {
    static constexpr auto transform(double x) { return std::exp(x); }
    static constexpr auto inverse(double y) { return std::log(y); }
    static constexpr auto logJacobian(double x) { return x; }  // log|d(exp(x))/dx| = x
};

// Unit interval: R → [0,1] via logistic
struct UnitInterval {
    static constexpr auto transform(double x) {
        return 1.0 / (1.0 + std::exp(-x));
    }
    static constexpr auto inverse(double y) {
        return std::log(y / (1.0 - y));
    }
    static constexpr auto logJacobian(double x) {
        double p = transform(x);
        return std::log(p * (1.0 - p));
    }
};

// Bounded interval: R → [a,b]
template <auto Lower, auto Upper>
struct Bounded {
    static constexpr double a = Lower;
    static constexpr double b = Upper;

    static constexpr auto transform(double x) {
        double p = 1.0 / (1.0 + std::exp(-x));
        return a + (b - a) * p;
    }
    static constexpr auto inverse(double y) {
        double p = (y - a) / (b - a);
        return std::log(p / (1.0 - p));
    }
    static constexpr auto logJacobian(double x) {
        double p = 1.0 / (1.0 + std::exp(-x));
        return std::log((b - a) * p * (1.0 - p));
    }
};

// K-simplex: R^(K-1) → Δ^K via stick-breaking
template <size_t K>
struct Simplex {
    // Transform from K-1 unconstrained to K simplex
    static auto transform(std::span<const double, K-1> x) -> std::array<double, K>;
    static auto inverse(std::span<const double, K> y) -> std::array<double, K-1>;
    static auto logJacobian(std::span<const double, K-1> x) -> double;
};

// Correlation matrix: R^(D*(D-1)/2) → Corr(D) via Cholesky
template <size_t D>
struct CorrMatrix {
    static constexpr size_t num_unconstrained = D * (D - 1) / 2;
    // LKJ-style parameterization
};

}  // namespace tempura::bayes::sym
```

### Constrained Symbols

Constraints attach to symbols, creating new types:

```cpp
// Unconstrained symbol (default)
constexpr Symbol mu;

// Constrained symbols - constraint is part of the type
constexpr Symbol<Positive> sigma;        // σ > 0 always
constexpr Symbol<UnitInterval> p;        // p ∈ [0,1] always
constexpr Symbol<Bounded<-1, 1>> rho;    // ρ ∈ [-1,1] always
constexpr Symbol<Simplex<3>> probs;      // Σprobs = 1 always

auto model = Model{
    mu    | Normal(0, 10),
    sigma | HalfNormal(5),       // HalfNormal is consistent with Positive
    p     | Beta(1, 1),          // Beta is consistent with UnitInterval
    y     | Normal(mu, sigma)
};

// During inference:
// - Sampler operates on unconstrained parameters
// - Model transforms to constrained space
// - Jacobian adjustment added to log-prob automatically
```

**Why constraint on Symbol, not on Distribution?**

Because the constraint belongs to the variable, not the prior. Consider:
- `sigma | HalfNormal(5)` — HalfNormal implies positivity
- `sigma | Gamma(2, 1)` — Gamma also implies positivity
- `sigma | TruncatedNormal(0, 5, lower=0)` — Also positive

The constraint `σ > 0` is a property of σ, not of which prior we choose. The library verifies consistency: assigning `Normal(0, 1)` to a `Symbol<Positive>` would require truncation or error.

### Deterministic Nodes (Derived Quantities)

Deterministic quantities are fundamentally different from random variables—they're computed, not sampled. This deserves a distinct type:

```cpp
// Derived<Expr> - a deterministic function of other variables
template <typename Expr>
struct Derived {
    Expr expr;

    // Cannot be observed (it's computed, not random)
    // Cannot be sampled (it's deterministic)
    // CAN appear in other expressions
};

// Usage: different operator to distinguish from stochastic
constexpr Symbol alpha, beta, x, y;
constexpr Symbol<Positive> sigma;

auto model = Model{
    alpha | Normal(0, 10),
    beta  | Normal(0, 10),
    sigma | HalfNormal(5),

    // Deterministic: y_hat is computed, not sampled
    y_hat << alpha + beta * x,     // << for deterministic assignment

    y     | Normal(y_hat, sigma)   // Likelihood uses derived quantity
};

// y_hat cannot be in .observe() or .infer() — it's not a random variable
auto posterior = model
    .observe(x = x_data, y = y_data)
    .infer(alpha, beta, sigma);   // y_hat is NOT listed
```

**Why `<<` for deterministic?**

We need to distinguish:
- `y | Normal(mu, sigma)` — y is a random variable with this distribution
- `y_hat << alpha + beta * x` — y_hat is a computed value

Using `<<` for deterministic:
- Visually distinct from `|`
- Suggests "assignment" or "flows from"
- `<<` is left-associative binary operator in C++

### Implementation Architecture

```cpp
namespace tempura::bayes::sym {

// =============================================================================
// STATEMENT - Variable with distribution (no role encoded)
// =============================================================================

template <Symbolic Var, typename Dist>
struct Statement {
    using variable = Var;
    using distribution = Dist;

    [[no_unique_address]] Dist dist;

    constexpr auto logProb() const {
        return dist.logProbFor(Var{});
    }
};

// Operator | creates statements
template <Symbolic Var, typename Dist>
constexpr auto operator|(Var, Dist dist) {
    return Statement<Var, Dist>{dist};
}

// =============================================================================
// DETERMINISTIC STATEMENT - Variable computed from expression
// =============================================================================

template <Symbolic Var, Symbolic Expr>
struct DeterministicStatement {
    using variable = Var;
    using expression = Expr;

    [[no_unique_address]] Expr expr;

    // No logProb contribution — deterministic nodes don't add to joint
    constexpr auto logProb() const { return Constant<0>{}; }
};

// Operator << creates deterministic statements
template <Symbolic Var, Symbolic Expr>
constexpr auto operator<<(Var, Expr expr) {
    return DeterministicStatement<Var, Expr>{expr};
}

// =============================================================================
// MODEL - Collection of statements
// =============================================================================

template <typename... Statements>
struct Model {
    std::tuple<Statements...> statements;

    // Joint log-probability
    constexpr auto logProb() const {
        return std::apply(
            [](auto&... stmt) { return (stmt.logProb() + ...); },
            statements
        );
    }

    // Condition on observations
    template <typename... Binders>
    constexpr auto observe(Binders... binders) const {
        return ConditionedModel{*this, binders...};
    }
};

// =============================================================================
// CONDITIONED MODEL - Ready for inference
// =============================================================================

template <typename ModelT, typename... Binders>
struct ConditionedModel {
    ModelT model;
    std::tuple<Binders...> observations;

    // Specify inference targets and their order
    template <Symbolic... Params>
    constexpr auto infer(Params...) const {
        return Posterior<ModelT, std::tuple<Binders...>, Params...>{
            model, observations
        };
    }
};

}  // namespace tempura::bayes::sym
```

### Vectorized Observations

Real models have N data points, not just one. We need runtime-sized observation support:

```cpp
constexpr Symbol mu;
constexpr Symbol<Positive> sigma;
constexpr Symbol y;  // Will be vectorized

auto model = Model{
    mu    | Normal(0, 10),
    sigma | HalfNormal(5),
    y     | Normal(mu, sigma)   // Single statement, but y can be vector
};

// Option 1: Pass vector of observations
std::vector<double> data = loadData();  // N observations
auto posterior = model
    .observe(y = data)      // y binds to entire vector
    .infer(mu, sigma);

// logProb internally computes: Σᵢ logNormal(data[i], mu, sigma)
```

**Implementation approach:**

When `y = vector_data` is passed to `.observe()`:
1. Detect that the bound value is a range/container
2. For statements involving `y`, sum the log-prob over all elements
3. Gradient computation sums the per-element gradients

```cpp
// Pseudo-implementation
template <typename ModelT, typename... Binders>
struct ConditionedModel {
    // ...

    template <typename ParamValues>
    auto logProb(ParamValues params) const {
        double total = 0.0;

        // For each observation in vectorized data
        for (size_t i = 0; i < data_size_; ++i) {
            auto bindings = makeBindings(params, observations_, i);
            total += evaluate(model_.logProb(), bindings);
        }

        return total;
    }
};
```

### Partial Observation (Missing Data)

Support observing only some indices:

```cpp
constexpr Symbol y;
std::vector<double> known_values = {1.2, 2.5, 3.1};
std::vector<size_t> known_idx = {0, 2, 5};
std::vector<size_t> missing_idx = {1, 3, 4};

auto posterior = model
    .observe(y[known_idx] = known_values)     // Observe some
    .infer(mu, sigma, y[missing_idx]);        // Infer others + params

// This enables:
// - Missing data imputation
// - Semi-supervised learning
// - Selective conditioning
```

### Complete Example (Updated)

```cpp
#include "bayes/sym/dsl.h"

using namespace tempura::bayes::sym;
using namespace tempura::symbolic3;

int main() {
    // Define symbols with constraints
    constexpr Symbol mu;
    constexpr Symbol<Positive> sigma;  // Constraint is type-level
    constexpr Symbol y;

    // Define model structure
    auto model = Model{
        mu    | Normal(0.0, 10.0),
        sigma | HalfNormal(5.0),
        y     | Normal(mu, sigma)
    };

    // Load data
    std::vector<double> observations = {1.2, 2.5, 1.8, 3.1, 2.2};

    // Condition and specify inference targets
    auto posterior = model
        .observe(y = observations)
        .infer(mu, sigma);

    // Evaluate at a point (internally handles constraint transforms)
    double lp = posterior.logProb(2.0, 1.5);  // mu=2.0, sigma=1.5
    auto grad = posterior.gradient(2.0, 1.5);

    // Run HMC (operates in unconstrained space automatically)
    std::mt19937_64 rng{42};
    auto samples = posterior.sample<HMC>(
        rng,
        10000,
        {.warmup = 1000, .step_size = 0.1, .num_leapfrog = 10}
    );

    // Samples are in constrained space (sigma > 0 guaranteed)
    for (auto& [mu_val, sigma_val] : samples) {
        assert(sigma_val > 0);  // Always true
    }
}
```

### Hierarchical Models

Real models often have grouped structure. We support this with indexed symbols and plates:

```cpp
constexpr Symbol μ_pop, σ_pop, σ_obs;
constexpr SymbolArray<10> μ_group;
constexpr SymbolArray<100> y;

auto model = Model{
    // Population-level hyperpriors
    param(μ_pop) = Normal(0_c, 100_c),
    param(σ_pop) = HalfNormal(10_c),
    param(σ_obs) = HalfNormal(1_c),

    // Group-level parameters (10 groups)
    plate<10>([](auto i) {
        return param(μ_group[i]) = Normal(μ_pop, σ_pop);
    }),

    // Observations (100 data points, each belongs to a group)
    plate<100>([](auto i) {
        constexpr auto group = i / 10;  // 10 obs per group
        return data(y[i]) = Normal(μ_group[group], σ_obs);
    })
};
```

**Why `plate<N>` with a lambda?** Because plates expand to N statements at compile time. The lambda receives a compile-time index (`std::integral_constant<std::size_t, i>`), allowing each statement to reference different array elements. This preserves full type-level structure while avoiding manual repetition.

**Why not runtime loops?** Because we need the complete model structure at compile time for gradient computation. A runtime loop would require dynamic graph construction, sacrificing the zero-overhead guarantee.

### Mixture and Conditional Models

Some models have discrete structure or conditional dependencies:

```cpp
constexpr Symbol z, μ0, μ1, σ, y;

auto model = Model{
    // Mixture component indicator
    latent(z) = Bernoulli(0.5_c),

    // Component-specific parameters
    param(μ0) = Normal(0_c, 10_c),
    param(μ1) = Normal(50_c, 10_c),
    param(σ)  = HalfNormal(5_c),

    // Observation depends on latent class
    data(y) = Mixture{
        z,
        Normal(μ0, σ),  // if z == 0
        Normal(μ1, σ)   // if z == 1
    }
};
```

**Why `Mixture` instead of conditional statements?** Because mixture models marginalize over the discrete variable. The `Mixture` type knows to compute `log(p·f₀(y) + (1-p)·f₁(y))` rather than selecting one branch. This enables gradient-based inference even with discrete latent variables.

For models where we condition on discrete values:

```cpp
auto model = Model{
    param(μ) = Normal(0_c, 10_c) | when(group == 0_c),
    param(μ) = Normal(100_c, 10_c) | when(group == 1_c),
    data(y)  = Normal(μ, σ)
};
```

The `when` clause activates statements conditionally. During inference, only active statements contribute to the log-probability.

### Validation and Error Messages

The model validates its structure at compile time:

```cpp
// Error: μ used but never defined
auto bad1 = Model{
    data(y) = Normal(μ, 1_c)
};
// static_assert failure: "Symbol μ appears in distribution but has no prior"

// Error: circular dependency
auto bad2 = Model{
    param(μ) = Normal(σ, 1_c),
    param(σ) = Normal(μ, 1_c)
};
// static_assert failure: "Circular dependency detected: μ → σ → μ"

// Error: data used as distribution parameter
auto bad3 = Model{
    data(x)  = Normal(0_c, 1_c),
    param(μ) = Normal(x, 1_c)  // x is data, can't be a prior parameter
};
// static_assert failure: "Data symbol x cannot appear in prior distribution"

// Warning: redundant constraint
auto redundant = Model{
    param(σ) = HalfNormal(5_c) | Positive{}
};
// Compiles, but produces: "Note: HalfNormal already implies Positive constraint"
```

**Why invest in error messages?** Because probabilistic models fail subtly. A missing prior doesn't crash—it produces garbage inference. A circular dependency might compile but never converge. Catching these at compile time, with clear messages, saves hours of debugging.

### Comparison with Other Approaches

| Feature | Our DSL | Stan | PyMC |
|---------|---------|------|------|
| **Syntax** | `param(μ) = Normal(...)` | `μ ~ normal(...)` | `μ = pm.Normal(...)` |
| **Type safety** | Compile-time | Runtime | Runtime |
| **Gradients** | Symbolic, compile-time | AD, runtime | AD, runtime |
| **Conjugacy** | Pattern matching | Manual | Some automatic |
| **Error timing** | Compile | Compile+Runtime | Runtime |
| **Extensibility** | Add new types | Limited | Good |

Our DSL trades the aesthetic `~` operator for explicit roles and compile-time guarantees. Users know exactly what each symbol represents, the compiler catches structural errors, and gradients are computed symbolically before the program runs.

---

## Example: Complete Workflow

```cpp
#include "bayes/symbolic.h"

using namespace tempura::bayes::sym;
using namespace tempura::symbolic3;

int main() {
    // Step 1: Define symbols
    constexpr Symbol μ, σ, y;

    // Step 2: Build log-probability expression
    constexpr auto log_prob =
        logNormal(μ, 0_c, 10_c) +       // μ ~ Normal(0, 10)
        logHalfNormal(σ, 5_c) +         // σ ~ HalfNormal(5)
        logNormal(y, μ, σ);             // y ~ Normal(μ, σ)

    // Step 3: Create model (parameters are μ and σ)
    using Model = decltype(ModelBuilder<decltype(μ), decltype(σ)>
        ::withLogProb(log_prob));

    // Step 4: Instantiate with observed data
    double observed_y = 2.5;
    auto model = instantiate(Model{}, y = observed_y);

    // Step 5: Run inference
    std::mt19937_64 rng{42};
    auto samples = sample(model,
        10000,                           // num_samples
        std::array{0.0, 1.0},           // initial [μ, σ]
        rng,
        {.algorithm = Algorithm::HMC,
         .step_size = 0.1,
         .num_leapfrog = 10,
         .warmup = 1000});

    // Step 6: Analyze results
    auto μ_samples = samples | std::views::transform([](auto s) { return s[0]; });
    auto σ_samples = samples | std::views::transform([](auto s) { return s[1]; });

    double μ_mean = mean(μ_samples);
    double σ_mean = mean(σ_samples);

    std::println("Posterior mean of μ: {}", μ_mean);
    std::println("Posterior mean of σ: {}", σ_mean);
}
```

---

## Implementation Plan

### Current State

The basic DSL infrastructure exists in `bayes/sym/dsl.h`:
- ✅ `Symbol` type with unique identity via stateless lambdas
- ✅ `Statement<Var, Dist>` binding variables to distributions
- ✅ `Joint<Statements...>` combining statements
- ✅ `Posterior` with `logProb()` and `gradient()` methods
- ✅ Basic distribution wrappers (Normal, HalfNormal, Beta, etc.)
- ✅ Integration with `symbolic3` for symbolic differentiation

### Phase 1: Constraint System (High Priority)

**Goal:** Enable automatic parameter transforms for constrained variables.

1. **Implement constraint types:**
   - `Unconstrained` (identity transform)
   - `Positive` (exp transform)
   - `UnitInterval` (logistic transform)
   - `Bounded<Lower, Upper>` (scaled logistic)

2. **Extend Symbol to accept constraint:**
   - `Symbol<Positive>` carries constraint in type
   - Default: `Symbol<Unconstrained>` or just `Symbol`

3. **Add Jacobian handling in Posterior:**
   - `logProb()` adds Jacobian adjustment for constrained params
   - `gradient()` includes Jacobian derivative
   - Sampler operates in unconstrained space

4. **Verify consistency:**
   - `Symbol<Positive>` with `HalfNormal` → OK (consistent)
   - `Symbol<Positive>` with `Normal` → Error or auto-truncate

**Deliverables:**
- `src/bayes/sym/constraints.h` - Constraint types
- Updated `dsl.h` with constraint-aware Symbol
- Tests verifying transform correctness

### Phase 2: Deterministic Nodes (Medium Priority)

**Goal:** Support computed quantities that aren't random variables.

5. **Implement `DeterministicStatement`:**
   - `y_hat << expr` creates deterministic binding
   - No log-prob contribution (returns 0)
   - Expression substituted when evaluating likelihood

6. **Add `<<` operator for deterministic assignment:**
   - Syntactically distinct from `|` for stochastic

7. **Prevent invalid operations:**
   - Cannot `.observe()` a deterministic node
   - Cannot `.infer()` a deterministic node
   - Static assertion with clear error message

**Deliverables:**
- `DeterministicStatement` in `dsl.h`
- `operator<<` for Symbols
- Tests for linear regression model with `y_hat`

### Phase 3: Vectorized Observations (High Priority)

**Goal:** Handle N data points efficiently.

8. **Support vector binding in `.observe()`:**
   - `y = std::vector<double>` binds to entire dataset
   - Detect container/range types at compile time

9. **Sum log-prob over observations:**
   - For statements involving vectorized symbols
   - Efficient evaluation (avoid re-evaluating param log-probs)

10. **Sum gradients correctly:**
    - Gradient of sum = sum of gradients
    - Only data-dependent terms contribute per observation

**Deliverables:**
- Updated `Posterior::logProb()` with vectorized support
- Updated `Posterior::gradient()` with vectorized support
- Benchmark comparing to manual loop

### Phase 4: Algorithm Integration (Medium Priority)

**Goal:** Seamless connection between DSL models and inference algorithms.

11. **Add `.toHMC()` method on Posterior:**
    ```cpp
    auto hmc = posterior.toHMC({.epsilon = 0.1, .L = 20});
    auto samples = hmc.sample(rng, 5000);
    ```

12. **Add `.toADVI()` method:**
    ```cpp
    auto advi = posterior.toADVI();
    auto approx = advi.fit(rng, 1000);
    ```

13. **Handle constraint transforms in algorithms:**
    - HMC operates on unconstrained params
    - Samples returned in constrained space
    - Mass matrix adapts to unconstrained scale

**Deliverables:**
- `Posterior::toHMC()` factory method
- `Posterior::toADVI()` factory method
- Example showing full workflow

### Phase 5: Partial Observation (Lower Priority)

**Goal:** Support missing data and semi-supervised learning.

14. **Indexed symbol binding:**
    - `y[indices] = values` binds subset
    - `y[other_indices]` remains unbound (inferred)

15. **Mixed observation/inference:**
    - Some elements of a symbol observed
    - Other elements become parameters

16. **Data augmentation MCMC:**
    - Sample missing values alongside parameters

**Deliverables:**
- Indexed binding syntax
- Test with missing data imputation

### Phase 6: Advanced Features (Future)

17. **Simplex and CorrMatrix constraints**
18. **Plate notation for hierarchical models**
19. **Conjugacy detection via pattern matching**
20. **Prior/posterior predictive derived from model**

---

## Summary of Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Param vs Data typing | Roles contextual | Enables missing data, semi-supervised, model reuse |
| Constraint typing | Type-level | Intrinsic property, enables auto-transform |
| Stochastic operator | `\|` (pipe) | Valid C++, suggests conditioning |
| Deterministic operator | `<<` | Visually distinct, valid C++ |
| Vectorized data | Runtime detection | N unknown at compile time |
| Algorithm integration | Factory methods | Clean API, encapsulates details |

---

## Open Questions

### 1. How should we handle vector parameters?

A hierarchical model might have 100 group-level means. Creating 100 distinct `Symbol` types is unwieldy. Options:

**A. Indexed symbols:** `Symbol<"μ", 0>`, `Symbol<"μ", 1>`, etc. The index becomes part of the type.

**B. Array parameters:** A special `SymbolArray<N>` type that represents N related parameters. Internally expands to N symbols.

**C. Plate notation:** Follow probabilistic programming conventions with explicit loops over indices.

The right choice depends on how the expressions compose. Option A is simplest but verbose. Option B hides complexity but may complicate gradient computation. Option C is familiar to users but requires more infrastructure.

### 2. Should we support runtime model construction?

The current design assumes compile-time model definition. But sometimes models come from configuration files or user input. Options:

**A. Compile-time only:** Accept this limitation. Users who need runtime models use a different library.

**B. Hybrid mode:** Provide a runtime `ExpressionTree` type alongside compile-time expressions. Pay the overhead only when needed.

**C. Code generation:** Generate C++ source code from runtime model description, compile it as a shared library, load dynamically.

Option A fits the library's philosophy but limits applicability. Option B adds complexity and dual APIs. Option C works but has operational overhead.

### 3. How do we handle missing data?

Bayesian models naturally handle missing observations by integrating them out. But our symbolic framework assumes all symbols have values during evaluation. Options:

**A. Explicit marginalization:** User writes `∫ p(x|θ) p(θ) dθ` symbolically. We simplify when possible.

**B. Data augmentation:** Treat missing values as additional parameters. MCMC samples them alongside model parameters.

**C. Preprocessing:** Require complete data. Missing value imputation happens before model fitting.

Option B is most general and fits naturally into the MCMC framework. Option C is simplest but less principled.

---

## Appendix: Symbolic Expression Reference

### Distribution Log-Probabilities

| Distribution | Log-probability expression |
|--------------|---------------------------|
| Normal(μ,σ) | -½log(2π) - log(σ) - (x-μ)²/(2σ²) |
| HalfNormal(σ) | log(2) - ½log(2π) - log(σ) - x²/(2σ²) for x≥0 |
| Beta(α,β) | (α-1)log(x) + (β-1)log(1-x) - logB(α,β) |
| Gamma(α,β) | α·log(β) + (α-1)log(x) - β·x - logΓ(α) |
| Exponential(λ) | log(λ) - λ·x |
| Poisson(λ) | x·log(λ) - λ - logΓ(x+1) |
| Bernoulli(p) | x·log(p) + (1-x)log(1-p) |
| Binomial(n,p) | logC(n,x) + x·log(p) + (n-x)log(1-p) |

### Conjugate Pairs

| Prior | Likelihood | Posterior |
|-------|------------|-----------|
| Normal(μ₀,σ₀) | Normal(μ,σ) [σ known] | Normal(μ',σ') |
| Beta(α,β) | Bernoulli(p) | Beta(α+x, β+1-x) |
| Beta(α,β) | Binomial(n,p) | Beta(α+x, β+n-x) |
| Gamma(α,β) | Poisson(λ) | Gamma(α+Σxᵢ, β+n) |
| Gamma(α,β) | Exponential(λ) | Gamma(α+n, β+Σxᵢ) |
| Dirichlet(α) | Multinomial(p) | Dirichlet(α+x) |
