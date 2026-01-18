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

We want models to read clearly and express their statistical structure without cryptic operators. The syntax should make the role of each variable obvious—is it a parameter we infer, or data we observe?

```cpp
auto model = Model{
    param(μ) = Normal(0_c, 10_c),
    param(σ) = HalfNormal(5_c),
    data(y)  = Normal(μ, σ)
};
```

This reads naturally: "parameter μ is Normal(0, 10), parameter σ is HalfNormal(5), data y is Normal(μ, σ)."

### Why This Design?

We considered many operator-based approaches to mimic the statistical `~` notation:

| Syntax | Problem |
|--------|---------|
| `μ ~ Normal(...)` | `~` is unary in C++, doesn't parse |
| `μ \| ~Normal(...)` | Works, but looks like line noise |
| `μ % Normal(...)` | `%` suggests modulo, not distribution |
| `μ << Normal(...)` | Suggests bit shifting or streaming |
| `Normal(...) >> μ` | Reversed from how statisticians think |

None of these read as naturally as they should. Operator overloading forces us to repurpose symbols that carry wrong connotations.

**The explicit approach wins because:**

1. `param()` and `data()` make roles unambiguous—no guessing which variables get inferred
2. `=` genuinely means "is defined as"—that's exactly the right semantics
3. No obscure operators to explain to new users
4. Easy to extend with new roles: `latent()`, `hyperprior()`, `transformed()`
5. IDE autocomplete and refactoring work correctly
6. Error messages reference real function names, not cryptic operators

### Implementation

The DSL requires three components: role wrappers, distribution builders, and statements.

```cpp
namespace tempura::bayes::sym {

// ============================================================================
// ROLE WRAPPERS - Capture the variable and its role in the model
// ============================================================================

enum class Role { Parameter, Data, Latent, Hyperprior };

template <Symbolic Var, Role R>
struct RoleSpec {
    // Assignment creates a statement binding variable to distribution
    template <typename Dist>
    constexpr auto operator=(Dist dist) const {
        return Statement<Var, Dist, R>{};
    }
};

// Role wrapper functions - these are the user-facing API
template <Symbolic Var>
constexpr auto param(Var) {
    return RoleSpec<Var, Role::Parameter>{};
}

template <Symbolic Var>
constexpr auto data(Var) {
    return RoleSpec<Var, Role::Data>{};
}

template <Symbolic Var>
constexpr auto latent(Var) {
    return RoleSpec<Var, Role::Latent>{};
}

// ============================================================================
// DISTRIBUTION BUILDERS - Lightweight types that construct log-prob expressions
// ============================================================================

template <Symbolic Mu, Symbolic Sigma>
struct NormalDist {
    [[no_unique_address]] Mu μ;
    [[no_unique_address]] Sigma σ;

    // Generate log-probability expression for variable x
    template <Symbolic X>
    constexpr auto logProbFor(X x) const {
        return logNormal(x, μ, σ);
    }

    // Chain constraints with |
    template <typename Constraint>
    constexpr auto operator|(Constraint c) const {
        return ConstrainedDist{*this, c};
    }
};

template <Symbolic Sigma>
struct HalfNormalDist {
    [[no_unique_address]] Sigma σ;

    template <Symbolic X>
    constexpr auto logProbFor(X x) const {
        return logHalfNormal(x, σ);
    }

    template <typename Constraint>
    constexpr auto operator|(Constraint c) const {
        return ConstrainedDist{*this, c};
    }
};

// Factory functions return distribution builders
template <Symbolic Mu, Symbolic Sigma>
constexpr auto Normal(Mu μ, Sigma σ) {
    return NormalDist<Mu, Sigma>{μ, σ};
}

template <Symbolic Sigma>
constexpr auto HalfNormal(Sigma σ) {
    return HalfNormalDist<Sigma>{σ};
}

template <Symbolic Alpha, Symbolic Beta>
constexpr auto Beta(Alpha α, Beta β) {
    return BetaDist<Alpha, Beta>{α, β};
}

// ============================================================================
// STATEMENT - Binds a variable to a distribution with a role
// ============================================================================

template <Symbolic Var, typename Dist, Role R>
struct Statement {
    using variable = Var;
    using distribution = Dist;
    static constexpr Role role = R;

    // Extract the log-probability expression
    static constexpr auto logProb() {
        return Dist{}.logProbFor(Var{});
    }

    // Check if this is observed data
    static constexpr bool isObserved() {
        return R == Role::Data;
    }

    // Check if this is an inferred parameter
    static constexpr bool isParameter() {
        return R == Role::Parameter || R == Role::Latent;
    }
};

}  // namespace tempura::bayes::sym
```

**Why RoleSpec returns a type with operator=?** Because we need to capture both the variable and its role before we know the distribution. The `param(μ)` call returns a lightweight spec; `= Normal(...)` completes it.

**Why store distribution as a type, not a value?** Because everything happens at compile time. The distribution's parameters (which may be other symbols) are encoded in the type. No runtime storage needed.

### Model Composition

Multiple statements combine into a complete model:

```cpp
namespace tempura::bayes::sym {

template <typename... Statements>
struct Model {
    // Total log-probability is sum of all statement log-probs
    static constexpr auto logProb() {
        return (Statements::logProb() + ...);
    }

    // Count statements by role
    static constexpr std::size_t numParameters() {
        return ((Statements::isParameter() ? 1 : 0) + ...);
    }

    static constexpr std::size_t numObserved() {
        return ((Statements::isObserved() ? 1 : 0) + ...);
    }

    // Extract parameter symbols as a type list
    using parameters = FilterByRole<Role::Parameter, Statements...>;

    // Extract data symbols as a type list
    using observations = FilterByRole<Role::Data, Statements...>;

    // Bind observed data, returning a callable for inference
    template <typename... Bindings>
    static constexpr auto observe(Bindings... bindings) {
        return ModelInstance<Model, Bindings...>{bindings...};
    }
};

// Deduction guide: Model{stmt1, stmt2, ...} deduces Model<Stmt1, Stmt2, ...>
template <typename... Statements>
Model(Statements...) -> Model<Statements...>;

}  // namespace tempura::bayes::sym
```

**Why fold expression `(Statements::logProb() + ...)`?** Because the joint log-probability of independent statements is their sum. The fold expands at compile time to a single symbolic expression representing the entire model's log-density.

**Why `FilterByRole` instead of runtime filtering?** Because we need the parameter types at compile time to compute gradients. The filter is a template metafunction that extracts symbols matching a role.

### Complete Example

```cpp
#include "bayes/symbolic.h"

using namespace tempura::bayes::sym;
using namespace tempura::symbolic3;

int main() {
    // Define symbols
    constexpr Symbol μ, σ, y;

    // Define model with explicit roles
    constexpr auto model = Model{
        param(μ) = Normal(0_c, 10_c),
        param(σ) = HalfNormal(5_c),
        data(y)  = Normal(μ, σ)
    };

    // Model knows its structure at compile time
    static_assert(model.numParameters() == 2);
    static_assert(model.numObserved() == 1);

    // Extract log-probability as a symbolic expression
    constexpr auto log_joint = model.logProb();

    // Compute gradients symbolically (happens at compile time)
    constexpr auto grad_μ = diff_simplified(log_joint, μ);
    constexpr auto grad_σ = diff_simplified(log_joint, σ);

    // Bind observed data
    auto instance = model.observe(y = 2.5);

    // Run inference
    std::mt19937_64 rng{42};
    auto samples = instance.sample<HMC>(
        10000,
        {.step_size = 0.1, .num_leapfrog = 10, .warmup = 1000}
    );

    // Analyze posterior
    auto [μ_mean, μ_std] = summarize(samples, μ);
    auto [σ_mean, σ_std] = summarize(samples, σ);
}
```

### Constraints

Parameters often have constrained domains. We express constraints by chaining with `|`:

```cpp
auto model = Model{
    param(μ) = Normal(0_c, 10_c),
    param(σ) = HalfNormal(5_c) | Positive{},
    param(ρ) = Uniform(-1_c, 1_c) | Bounded{-1, 1},
    param(p) = Beta(1_c, 1_c) | UnitInterval{},
    data(y)  = Normal(μ, σ)
};
```

**Why chain constraints separately?** Because some distributions imply constraints (HalfNormal is positive by definition) while others don't. Explicit constraints let users state their intent clearly. The library can verify consistency: `Normal(...) | Positive{}` triggers a transform, while `HalfNormal(...) | Positive{}` is redundant but valid.

Constraints trigger automatic parameter transforms during inference:

| Constraint | Transform | Jacobian |
|------------|-----------|----------|
| `Positive{}` | exp(x) | x |
| `Bounded{a, b}` | a + (b-a)·sigmoid(x) | log((x-a)(b-x)/(b-a)) |
| `UnitInterval{}` | sigmoid(x) | log(x(1-x)) |
| `Simplex{}` | stick-breaking | sum of log-Jacobians |

The sampler works in unconstrained space; the model evaluates in constrained space with Jacobian corrections.

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

### Phase 1: Core Primitives (Foundation)

**Goal:** Establish log-probability expressions that integrate with symbolic3.

1. **Add log-probability functions** for common distributions:
   - `logNormal`, `logHalfNormal`, `logTruncatedNormal`
   - `logBeta`, `logGamma`, `logExponential`
   - `logBernoulli`, `logBinomial`, `logPoisson`
   - `logUniform`, `logCauchy`, `logStudentT`

2. **Add special function symbols** for normalization constants:
   - `logGammaFn(x)` - log of gamma function
   - `logBetaFn(a, b)` - log of beta function
   - Ensure `diff()` handles these correctly (derivatives exist in closed form)

3. **Verify gradient correctness** with static_assert tests:
   ```cpp
   constexpr Symbol x, μ, σ;
   constexpr auto lp = logNormal(x, μ, σ);
   constexpr auto dlp_dμ = diff_simplified(lp, μ);
   // Should equal (x - μ) / (σ * σ)
   static_assert(/* structural equality check */);
   ```

**Why start here?** Because everything else depends on correct log-probability expressions. If these are wrong, inference produces garbage. We verify correctness at compile time before building anything on top.

### Phase 2: Model Definition (Structure)

**Goal:** Enable users to define models declaratively.

4. **Implement ModelBuilder** for capturing parameter symbols
5. **Implement Model** type with pre-computed gradients
6. **Add parameter ordering** to ensure consistent array indexing

**Why pre-compute gradients in the Model type?** Because we know the parameters at model definition time. Computing `gradient(log_prob, params...)` once and storing the result types avoids redundant work at instantiation time.

### Phase 3: Model Instantiation (Binding)

**Goal:** Connect models to observed data and provide callable interfaces.

7. **Implement ModelInstance** with `logProb()` and `gradient()` methods
8. **Implement parameter packing/unpacking** between arrays and symbolic bindings
9. **Add constraint handling** (e.g., σ > 0 via log-transform)

**Why handle constraints here?** Because many parameters have restricted domains. The sampler works in unconstrained space (all of R^n), but the model uses constrained parameters. The instance layer applies transformations automatically—users specify `σ > 0`, and we internally sample `log(σ)`.

### Phase 4: MCMC Integration (Inference)

**Goal:** Run actual inference on symbolic models.

10. **Implement ChainAdapter** for Metropolis-Hastings
11. **Implement HMCAdapter** with leapfrog integration
12. **Add warmup/thinning/convergence diagnostics**

**Why HMC before simpler methods work?** Because HMC is where symbolic gradients shine. Metropolis-Hastings doesn't use gradients—it works equally well with runtime computation. HMC's performance depends critically on efficient gradient evaluation, so it demonstrates the value of the symbolic approach.

### Phase 5: Conjugacy Detection (Optimization)

**Goal:** Automatically use closed-form posteriors when possible.

13. **Define patterns** for common conjugate pairs:
    - Normal-Normal (known variance)
    - Beta-Binomial, Beta-Bernoulli
    - Gamma-Poisson, Gamma-Exponential
    - Dirichlet-Multinomial

14. **Implement pattern matching** using `extractBindings`
15. **Add closed-form update functions** for each conjugate pair
16. **Wire conjugacy detection into `infer()`** dispatch

**Why defer conjugacy?** Because it's an optimization, not a correctness requirement. MCMC works for all models. Conjugacy makes some models faster. We build the general solution first, then add the fast paths.

### Phase 6: Ergonomics (Polish)

**Goal:** Make the library pleasant to use.

17. **Add model DSL syntax** with `~` operator if feasible
18. **Support vector-valued observations** (multiple data points)
19. **Add model validation** (all parameters have priors, no cycles)
20. **Improve error messages** for common mistakes

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
