# Expression Graph DSL for Probabilistic Programming

## Design Philosophy

**The DSL produces symbolic expressions. That's it.**

Users evaluate and differentiate using existing `symbolic3` primitives:
- `evaluate(expr, BinderPack{mu = 1.0, sigma = 2.0})`
- `diff(expr, mu)`

No special `Posterior` class. No `.infer()` method. No magic. If you want a callable, wrap it in a lambda yourself.

## Core Idea

```cpp
// Define random variables - each creates a node in the expression graph
auto mu = Normal(0, 10);       // RandomVar: log p(mu) = logNormal(mu, 0, 10)
auto sigma = HalfNormal(5);    // RandomVar: log p(sigma) = logHalfNormal(sigma, 5)
auto y = Normal(mu, sigma);    // RandomVar: log p(y|mu,sigma) = logNormal(y, mu, sigma)

// Get the joint log-probability (symbolic expression)
auto log_prob = jointLogProb(y);
// Equivalent to: logNormal(mu.sym(), 0, 10) + logHalfNormal(sigma.sym(), 5)
//                + logNormal(y.sym(), mu.sym(), sigma.sym())

// Evaluate at concrete values - use symbolic3 directly
double lp = evaluate(log_prob, BinderPack{mu = 1.0, sigma = 2.0, y = 2.5});

// Compute gradients - use symbolic3 directly
auto d_mu = diff(log_prob, mu);
auto d_sigma = diff(log_prob, sigma);

double grad_mu = evaluate(d_mu, BinderPack{mu = 1.0, sigma = 2.0, y = 2.5});

// Want a callable? Write a lambda.
auto log_prob_fn = [&](double mu_val, double sigma_val) {
    return evaluate(log_prob, BinderPack{mu = mu_val, sigma = sigma_val, y = 2.5});
};
```

## What the DSL Provides

1. **RandomVar** / **DeterministicVar** - Nodes in the probabilistic graph
2. **Distribution factories** - `Normal(mu, sigma)`, `HalfNormal(sigma)`, etc.
3. **`jointLogProb(rv...)`** - Traverse DAG, sum log-probabilities, return symbolic expression
4. **`rv = value`** - Binding syntax (works with `BinderPack`)
5. **`diff(expr, rv)`** - Differentiate w.r.t. a RandomVar

That's the entire public API. Everything else uses standard symbolic3.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         User Code                            │
│  auto mu = Normal(0, 10);                                   │
│  auto y = Normal(mu, sigma);                                │
│  auto lp = jointLogProb(y);                                 │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              RandomVar<Id, Dist, Parents...>                 │
│  - Unique compile-time identity                             │
│  - Distribution for this node                               │
│  - Parent RandomVars (edges in DAG)                         │
│  - .sym() returns Symbol<Id> for bindings                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    jointLogProb(rv)                          │
│  - Traverse DAG, visit each node once                       │
│  - Sum nodeLogProb() for all nodes                          │
│  - Return: symbolic expression                               │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Symbolic Expression                        │
│  (just a symbolic3 expression - nothing special)            │
│                                                              │
│  Use with existing tools:                                   │
│    evaluate(expr, BinderPack{...})                          │
│    diff(expr, symbol)                                       │
│    simplify(expr)                                           │
└─────────────────────────────────────────────────────────────┘
```

## Implementation

### RandomVar

```cpp
namespace tempura::bayes::graph {

// Type traits
template <typename T>
constexpr bool is_random_var = false;

template <typename T>
constexpr bool is_deterministic_var = false;

// RandomVar: a stochastic node in the probabilistic graph
template <typename Id,           // Unique type (stateless lambda)
          typename Dist,         // Distribution type
          typename... Parents>   // Parent RandomVars
struct RandomVar {
    using id_type = Id;
    using dist_type = Dist;
    using symbol_type = symbolic3::Symbol<Id>;

    [[no_unique_address]] Dist dist_;
    [[no_unique_address]] std::tuple<Parents...> parents_;

    // The symbol for this random variable's value
    static constexpr auto sym() { return symbol_type{}; }

    // Log-probability for THIS node only: log p(this | parents)
    constexpr auto nodeLogProb() const {
        return dist_.logProbFor(sym());
    }

    // Access parents (for DAG traversal)
    constexpr const auto& parents() const { return parents_; }

    // Binding syntax: mu = 1.0 creates a TypeValueBinder
    // This lets users write: BinderPack{mu = 1.0, sigma = 2.0}
    constexpr auto operator=(double value) const {
        return sym() = value;
    }
};

template <typename Id, typename Dist, typename... Parents>
constexpr bool is_random_var<RandomVar<Id, Dist, Parents...>> = true;

// DeterministicVar: a computed node (not sampled)
template <typename Id,
          typename Expr,
          typename... Parents>
struct DeterministicVar {
    using id_type = Id;
    using expr_type = Expr;
    using symbol_type = symbolic3::Symbol<Id>;

    [[no_unique_address]] Expr expr_;
    [[no_unique_address]] std::tuple<Parents...> parents_;

    static constexpr auto sym() { return symbol_type{}; }

    // Deterministic: no log-prob contribution
    constexpr auto nodeLogProb() const { return symbolic3::Literal{0.0}; }

    // The defining expression (for substitution into children)
    constexpr auto expr() const { return expr_; }

    constexpr const auto& parents() const { return parents_; }

    // Binding syntax: y_hat = 5.0
    constexpr auto operator=(double value) const {
        return sym() = value;
    }
};

template <typename Id, typename Expr, typename... Parents>
constexpr bool is_deterministic_var<DeterministicVar<Id, Expr, Parents...>> = true;

// Overload diff() to accept RandomVar/DeterministicVar directly
template <symbolic3::Symbolic Expr, typename Id, typename D, typename... Ps>
constexpr auto diff(Expr expr, const RandomVar<Id, D, Ps...>& rv) {
    return symbolic3::diff(expr, rv.sym());
}

template <symbolic3::Symbolic Expr, typename Id, typename E, typename... Ps>
constexpr auto diff(Expr expr, const DeterministicVar<Id, E, Ps...>& dv) {
    return symbolic3::diff(expr, dv.sym());
}

}  // namespace tempura::bayes::graph
```

### Argument Handling

```cpp
namespace tempura::bayes::graph {

// Convert distribution argument to symbolic form
template <typename T>
constexpr auto toSymbolic(const T& x) {
    if constexpr (is_random_var<T>) {
        return x.sym();
    } else if constexpr (is_deterministic_var<T>) {
        return x.sym();
    } else if constexpr (symbolic3::Symbolic<T>) {
        return x;
    } else {
        return symbolic3::Literal{static_cast<double>(x)};
    }
}

// Collect RandomVar/DeterministicVar parents from distribution arguments
template <typename T>
constexpr auto asParentTuple(const T& x) {
    if constexpr (is_random_var<T> || is_deterministic_var<T>) {
        return std::tuple<T>{x};
    } else {
        return std::tuple<>{};
    }
}

template <typename... Args>
constexpr auto collectParents(const Args&... args) {
    return std::tuple_cat(asParentTuple(args)...);
}

}  // namespace tempura::bayes::graph
```

### Distribution Factories

```cpp
namespace tempura::bayes::graph {

// Distribution wrapper (stores symbolic parameters)
template <symbolic3::Symbolic Mu, symbolic3::Symbolic Sigma>
struct NormalDist {
    [[no_unique_address]] Mu mu_;
    [[no_unique_address]] Sigma sigma_;

    template <symbolic3::Symbolic X>
    constexpr auto logProbFor(X x) const {
        return prob::logNormal(x, mu_, sigma_);
    }
};

template <symbolic3::Symbolic Sigma>
struct HalfNormalDist {
    [[no_unique_address]] Sigma sigma_;

    template <symbolic3::Symbolic X>
    constexpr auto logProbFor(X x) const {
        return prob::logHalfNormal(x, sigma_);
    }
};

// ... other distributions ...

// Factory: Normal(mu, sigma)
template <typename Mu, typename Sigma>
constexpr auto Normal(const Mu& mu, const Sigma& sigma) {
    auto mu_sym = toSymbolic(mu);
    auto sigma_sym = toSymbolic(sigma);

    using Dist = NormalDist<decltype(mu_sym), decltype(sigma_sym)>;
    auto parents = collectParents(mu, sigma);

    constexpr auto id = []{};  // Unique type per instantiation
    using Id = decltype(id);

    return std::apply([&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{
            Dist{mu_sym, sigma_sym},
            std::tuple{ps...}
        };
    }, parents);
}

// Factory: HalfNormal(sigma)
template <typename Sigma>
constexpr auto HalfNormal(const Sigma& sigma) {
    auto sigma_sym = toSymbolic(sigma);

    using Dist = HalfNormalDist<decltype(sigma_sym)>;
    auto parents = collectParents(sigma);

    constexpr auto id = []{};
    using Id = decltype(id);

    return std::apply([&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{
            Dist{sigma_sym},
            std::tuple{ps...}
        };
    }, parents);
}

// Factory: Beta(alpha, beta)
template <typename Alpha, typename Beta>
constexpr auto Beta(const Alpha& alpha, const Beta& beta) { /* ... */ }

// Factory: Exponential(lambda)
template <typename Lambda>
constexpr auto Exponential(const Lambda& lambda) { /* ... */ }

// etc.

}  // namespace tempura::bayes::graph
```

### Arithmetic on RandomVars (Deterministic Nodes)

```cpp
namespace tempura::bayes::graph {

// RandomVar + RandomVar -> DeterministicVar
template <typename Id1, typename D1, typename... P1,
          typename Id2, typename D2, typename... P2>
constexpr auto operator+(const RandomVar<Id1, D1, P1...>& a,
                         const RandomVar<Id2, D2, P2...>& b) {
    auto expr = a.sym() + b.sym();
    constexpr auto id = []{};

    return DeterministicVar<decltype(id), decltype(expr),
                            RandomVar<Id1, D1, P1...>,
                            RandomVar<Id2, D2, P2...>>{
        expr,
        std::tuple{a, b}
    };
}

// RandomVar * scalar -> DeterministicVar
template <typename Id, typename D, typename... Ps, typename T>
    requires std::is_arithmetic_v<T>
constexpr auto operator*(const RandomVar<Id, D, Ps...>& rv, T scalar) {
    auto expr = rv.sym() * symbolic3::Literal{static_cast<double>(scalar)};
    constexpr auto id = []{};

    return DeterministicVar<decltype(id), decltype(expr),
                            RandomVar<Id, D, Ps...>>{
        expr,
        std::tuple{rv}
    };
}

// scalar * RandomVar
template <typename T, typename Id, typename D, typename... Ps>
    requires std::is_arithmetic_v<T>
constexpr auto operator*(T scalar, const RandomVar<Id, D, Ps...>& rv) {
    return rv * scalar;
}

// Similar for -, /, and combinations with DeterministicVar

}  // namespace tempura::bayes::graph
```

### DAG Traversal

```cpp
namespace tempura::bayes::graph {

// Type-level set of visited IDs
template <typename... Ids>
struct IdSet {
    template <typename Id>
    static constexpr bool contains = (std::is_same_v<Id, Ids> || ...);

    template <typename Id>
    using insert = std::conditional_t<contains<Id>, IdSet, IdSet<Id, Ids...>>;
};

// Traverse DAG, accumulating log-prob, deduplicating nodes
template <typename Node, typename Visited>
struct Traverse {
    static constexpr bool skip = Visited::template contains<typename Node::id_type>;
    using NewVisited = typename Visited::template insert<typename Node::id_type>;

    static constexpr auto apply(const Node& node, Visited) {
        if constexpr (skip) {
            return std::pair{symbolic3::Literal{0.0}, Visited{}};
        } else {
            // Traverse parents first
            auto [parent_lp, after_parents] = traverseParents(node.parents(), NewVisited{});
            // Add this node's contribution
            auto total = parent_lp + node.nodeLogProb();
            return std::pair{total, after_parents};
        }
    }

private:
    template <typename... Ps, typename V>
    static constexpr auto traverseParents(const std::tuple<Ps...>& parents, V visited) {
        return fold<0>(parents, visited, symbolic3::Literal{0.0});
    }

    template <std::size_t I, typename Parents, typename V, typename Acc>
    static constexpr auto fold(const Parents& parents, V visited, Acc acc) {
        if constexpr (I >= std::tuple_size_v<Parents>) {
            return std::pair{acc, visited};
        } else {
            const auto& parent = std::get<I>(parents);
            using Parent = std::remove_cvref_t<decltype(parent)>;
            auto [lp, new_visited] = Traverse<Parent, V>::apply(parent, visited);
            return fold<I + 1>(parents, new_visited, acc + lp);
        }
    }
};

// Public API: get joint log-prob from a single root
template <typename Node>
constexpr auto jointLogProb(const Node& root) {
    return Traverse<Node, IdSet<>>::apply(root, IdSet<>{}).first;
}

// Public API: get joint log-prob from multiple roots
template <typename... Nodes>
constexpr auto jointLogProb(const Nodes&... roots) {
    // Traverse each root, threading visited set through
    return jointLogProbMulti(IdSet<>{}, symbolic3::Literal{0.0}, roots...);
}

template <typename Visited, typename Acc>
constexpr auto jointLogProbMulti(Visited, Acc acc) {
    return acc;
}

template <typename Visited, typename Acc, typename Node, typename... Rest>
constexpr auto jointLogProbMulti(Visited visited, Acc acc,
                                  const Node& node, const Rest&... rest) {
    auto [lp, new_visited] = Traverse<Node, Visited>::apply(node, visited);
    return jointLogProbMulti(new_visited, acc + lp, rest...);
}

}  // namespace tempura::bayes::graph
```

### Deterministic Substitution

When a DeterministicVar is used in a child distribution, its defining expression should be substituted. This happens in `toSymbolic`:

```cpp
// For DeterministicVar, we could either:
// 1. Return its symbol (and substitute later)
// 2. Return its expression directly (inline substitution)

// Option 1: Keep symbols, substitute at evaluation time
// This is simpler and matches how we handle RandomVars

// Option 2: Inline substitution (what the current dsl.h does)
// This might produce more complex expressions but avoids extra symbols
```

For now, we keep symbols and rely on the user to bind them (or future simplification to substitute).

## Complete Examples

### Example 1: Simple Model

```cpp
#include "bayes/graph/dsl.h"

using namespace tempura::bayes::graph;
using namespace tempura::symbolic3;

int main() {
    // Define model
    auto mu = Normal(0.0, 10.0);
    auto sigma = HalfNormal(5.0);
    auto y = Normal(mu, sigma);

    // Get symbolic log-prob
    auto lp = jointLogProb(y);

    // Evaluate
    double val = evaluate(lp, BinderPack{mu = 1.0, sigma = 2.0, y = 2.5});

    // Gradient
    auto dlp_dmu = diff(lp, mu);
    double grad_mu = evaluate(dlp_dmu, BinderPack{mu = 1.0, sigma = 2.0, y = 2.5});

    std::println("logProb = {}", val);
    std::println("d(logProb)/d(mu) = {}", grad_mu);
}
```

### Example 2: Wrapping in a Lambda (for MCMC)

```cpp
#include "bayes/graph/dsl.h"
#include "bayes/mcmc.h"

using namespace tempura::bayes::graph;
using namespace tempura::symbolic3;

int main() {
    auto mu = Normal(0.0, 10.0);
    auto sigma = HalfNormal(5.0);
    auto y = Normal(mu, sigma);

    auto lp = jointLogProb(y);
    auto dlp_dmu = diff(lp, mu);
    auto dlp_dsigma = diff(lp, sigma);

    double y_observed = 2.5;

    // Wrap in callable for MCMC
    auto log_prob = [&](std::span<const double> params) {
        return evaluate(lp, BinderPack{mu = params[0], sigma = params[1], y = y_observed});
    };

    auto gradient = [&](std::span<const double> params, std::span<double> grad) {
        auto bindings = BinderPack{mu = params[0], sigma = params[1], y = y_observed};
        grad[0] = evaluate(dlp_dmu, bindings);
        grad[1] = evaluate(dlp_dsigma, bindings);
    };

    // Use with HMC
    std::mt19937_64 rng{42};
    auto samples = hmc(log_prob, gradient, rng, {0.0, 1.0}, 10000,
                       {.step_size = 0.1, .num_leapfrog = 10});
}
```

### Example 3: Multiple Observations

```cpp
auto mu = Normal(0.0, 10.0);
auto sigma = HalfNormal(5.0);
auto y1 = Normal(mu, sigma);
auto y2 = Normal(mu, sigma);

// Both observations share mu and sigma (deduplicated)
auto lp = jointLogProb(y1, y2);

double val = evaluate(lp, BinderPack{mu = 1.0, sigma = 2.0, y1 = 2.5, y2 = 3.0});
```

### Example 4: IID Observations (Runtime Loop)

For N iid observations, we don't want N separate RandomVars. Instead, use the single RandomVar's nodeLogProb in a loop:

```cpp
auto mu = Normal(0.0, 10.0);
auto sigma = HalfNormal(5.0);
auto y = Normal(mu, sigma);

// Prior log-prob (mu and sigma only, not y)
auto prior_lp = jointLogProb(mu) + jointLogProb(sigma);

// Single observation log-prob
auto obs_lp = y.nodeLogProb();  // logNormal(y, mu, sigma)

std::vector<double> data = {1.2, 2.5, 1.8, 3.1, 2.2};

auto log_prob = [&](double mu_val, double sigma_val) {
    double total = evaluate(prior_lp, BinderPack{mu = mu_val, sigma = sigma_val});
    for (double yi : data) {
        total += evaluate(obs_lp, BinderPack{mu = mu_val, sigma = sigma_val, y = yi});
    }
    return total;
};
```

### Example 5: Linear Regression with Deterministic Node

```cpp
auto alpha = Normal(0.0, 10.0);
auto beta = Normal(0.0, 10.0);
auto sigma = HalfNormal(5.0);

double x = 2.0;  // Covariate (data)
auto y_hat = alpha + beta * x;  // DeterministicVar
auto y = Normal(y_hat, sigma);

auto lp = jointLogProb(y);
// lp includes: logNormal(alpha) + logNormal(beta) + logHalfNormal(sigma)
//              + logNormal(y, y_hat, sigma)

// Note: y_hat is a separate symbol that needs binding
// OR we inline it during traversal (design choice)

double val = evaluate(lp, BinderPack{
    alpha = 1.0,
    beta = 2.0,
    sigma = 1.0,
    y_hat = 5.0,  // = alpha + beta * x = 1 + 2*2 = 5
    y = 5.5
});

// Alternative: substitute y_hat's expression into lp
// (future simplification pass)
```

## File Structure

```
src/bayes/graph/
├── core.h           # RandomVar, DeterministicVar, type traits
├── distributions.h  # Normal, HalfNormal, Beta, etc.
├── traversal.h      # jointLogProb, DAG traversal
├── operators.h      # Arithmetic on RandomVars -> DeterministicVars
└── dsl.h            # Convenience header (includes all above)
```

## What This Design Does NOT Provide

1. **No `.infer()` method** - Use symbolic3 bindings directly
2. **No `Posterior` class** - It's just a symbolic expression
3. **No built-in IID handling** - Write a loop (it's 3 lines)
4. **No automatic parameter ordering** - You decide the order in your lambda
5. **No constraint transforms** - Handle in your wrapper (or use a separate utility)

## What This Design DOES Provide

1. **Expression graph encoded in types** - Full DAG structure at compile time
2. **Automatic deduplication** - Shared parents counted once
3. **Composable building blocks** - RandomVars compose naturally
4. **Symbolic expressions** - Work with existing symbolic3 tools
5. **Zero overhead** - No runtime allocation for model structure
6. **Differentiation agnostic** - Use symbolic diff, or wrap for AD backends

## Future Extensions

### Symbolic Simplification

```cpp
auto lp = jointLogProb(y);
auto simplified = simplify(lp);  // Algebraic simplification
```

### IID Recognition

```cpp
// Recognize pattern: sum of identical terms with different bindings
// Optimize evaluation: precompute common subexpressions
auto optimized = optimizeIID(lp, y.sym(), data);
```

### Conjugacy Detection

```cpp
// Pattern match Normal-Normal, Beta-Bernoulli, etc.
if constexpr (isConjugate<decltype(lp)>) {
    auto posterior_params = conjugateUpdate(prior, likelihood, data);
}
```

### Helper for Common Patterns

If the lambda boilerplate becomes tedious, provide optional utilities:

```cpp
// Optional convenience (not in core DSL)
auto [log_prob_fn, gradient_fn] = makeCallables(
    lp,
    /*free=*/ {mu, sigma},
    /*observed=*/ BinderPack{y = 2.5}
);
```

But this is a separate utility, not part of the core expression graph.
