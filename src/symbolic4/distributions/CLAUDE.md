# distributions/ - Probabilistic Building Blocks

## Purpose

This directory provides the probabilistic layer: distributions, random variables, and automatic log-probability collection.

## Key Files

| File | Purpose |
|------|---------|
| `wrappers.h` | Distribution structs (NormalDist, BetaDist, etc.) |
| `log_prob.h` | Symbolic log-probability functions |
| `random_var.h` | RandomVar abstraction and factory functions |
| `collect_log_prob.h` | Auto-discovery of parent RVs |
| `joint.h` | Manual log-prob composition |
| `sample.h` | Sampling utilities |

## Distribution Pattern

Each distribution is a struct with:
```cpp
template <Symbolic Mu, Symbolic Sigma>
struct NormalDist {
  using support_type = support::Real;  // or PositiveReal, Unit
  Mu mu_;
  Sigma sigma_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logNormal(x, mu_, sigma_);
  }
};
```

Support types drive automatic transform selection:
- `Real` → unconstrained
- `PositiveReal` → log transform
- `Unit` → logit transform

## Available Distributions

| Distribution | Support | Parameters |
|--------------|---------|------------|
| Normal | Real | mu, sigma |
| HalfNormal | Positive | sigma |
| Beta | Unit | alpha, beta |
| Gamma | Positive | alpha, beta (rate) |
| Exponential | Positive | lambda |
| Uniform | Real | a, b |
| Cauchy | Real | x0, gamma |
| StudentT | Real | nu, mu, sigma |
| Bernoulli | Binary | p |
| Binomial | Discrete | n, p |
| Poisson | Discrete | lambda |
| Dirichlet | Simplex | alpha (vector) |
| Categorical | Discrete | log_probs |

## RandomVar Abstraction

`RandomVar<Dist, Id>` wraps a distribution with:
- Unique identity `Id` (generated via `decltype([] {})`)
- `sym()` - Returns `Atom<Id, Sample<Dist>>` carrying distribution info
- `freeSym()` - Returns `Symbol<Id>` for bindings
- `logProb()` - Symbolic log-probability expression

Factory functions generate unique IDs per call site:
```cpp
auto mu = normal(0.0, 10.0);   // Each call gets unique Id
auto sigma = halfNormal(5.0);
auto y = normal(mu, sigma);    // mu.sym() and sigma.sym() appear in y's dist
```

## Auto-Discovery (collectLogProbs)

The key innovation: traverse expression trees to find parent RVs.

```cpp
// Given: y = normal(mu, sigma)
auto joint = collectLogProbs(y);
// Returns: logProb(y) + logProb(mu) + logProb(sigma)
```

How it works:
1. Traverse `y.logProb()` expression
2. Find `Atom<Id, Sample<Dist>>` nodes (random variable atoms)
3. For each, call `dist.logProbFor()` to get its log-prob
4. Track visited IDs via `IdSet<...>` to prevent double-counting
5. Sum all contributions

This enables hierarchical models without manual dependency listing.

## Sampling

Three modes:
```cpp
// Single RV with parent bindings
auto mu_val = sample(mu, rng);
auto y_val = sample(y, rng, mu = mu_val);

// Full trace in dependency order
auto trace = sampleAll(rng, mu, sigma, y);
double y_val = trace[y];

// IID replicates
auto samples = samplePlate(y, 1000, rng, mu = 0.5, sigma = 1.0);
```

## Adding New Distributions

1. Add wrapper struct to `wrappers.h`:
   ```cpp
   template <Symbolic Shape, Symbolic Rate>
   struct InverseGammaDist {
     using support_type = support::PositiveReal;
     Shape shape_;
     Rate rate_;

     template <Symbolic X>
     constexpr auto logProbFor(X x) const {
       return logInverseGamma(x, shape_, rate_);
     }
   };
   ```

2. Add log-prob function to `log_prob.h`:
   ```cpp
   template <Symbolic X, Symbolic Shape, Symbolic Rate>
   constexpr auto logInverseGamma(X x, Shape shape, Rate rate) {
     // Return symbolic expression
   }
   ```

3. Add factory to `random_var.h`:
   ```cpp
   template <typename Id = decltype([] {}), typename Shape, typename Rate>
   constexpr auto inverseGamma(Shape shape, Rate rate) {
     auto dist = InverseGammaDist{toSymbolic(shape), toSymbolic(rate)};
     return RandomVar<decltype(dist), Id>{dist};
   }
   ```

4. Add support mapping to `mcmc/support.h`:
   ```cpp
   template <Symbolic Shape, Symbolic Rate>
   struct DistributionSupport<InverseGammaDist<Shape, Rate>> {
     using type = PositiveSupport;
   };
   ```
