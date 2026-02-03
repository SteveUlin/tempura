# mcmc/ - Making MCMC Boring

## Purpose

This directory implements the "boring MCMC" vision: automatic parameter transforms, Jacobian corrections, and gradient chain rules so users just define models.

## Key Files

| File | Purpose |
|------|---------|
| `transforms.h` | Parameter transforms (log, logit, Cholesky, simplex) |
| `support.h` | Distribution → support → transform inference |
| `posterior.h` | Simple posterior wrapper |
| `hmc_adapter.h` | Integration with bayes/hmc.h |
| `plate_transforms.h` | Transforms for indexed parameters, `PlateTransformedPosterior` |
| `non_centered.h` | Hierarchical model reparameterization |
| `likelihoods.h` | Numerically stable logistic functions |
| `BINDING_API_PLAN.md` | Roadmap for binding-centric API |

### Planned Files (binding-centric API)

| File | Purpose |
|------|---------|
| `state.h` | `ParameterState` - symbol-indexed parameter container |
| `samples.h` | `Samples` - symbol-indexed sample container |
| `sampler.h` | `posterior.sample()` integration |

## Transform System

### Core Transforms

| Transform | Forward | Inverse | Log-Jacobian |
|-----------|---------|---------|--------------|
| `Unconstrained` | z | z | 0 |
| `Positive` | exp(z) | log(x) | z |
| `UnitInterval` | sigmoid(z) | logit(x) | log(x) + log(1-x) |
| `LowerBounded(a)` | a + exp(z) | log(x-a) | z |
| `UpperBounded(b)` | b - exp(z) | log(b-x) | z |
| `Interval(a,b)` | a + (b-a)*sigmoid(z) | scaled logit | log(b-a) + log(s) + log(1-s) |

### Specialized Transforms

**`CholeskyTransform`** - For covariance matrices:
- Maps K(K+1)/2 unconstrained → K×K lower triangular
- Diagonal: `L_ii = exp(z)` (ensures positivity)
- Off-diagonal: `L_ij = z` (unconstrained)
- Log-Jacobian: `Σ z_diag[i]`

**`SimplexTransform<K>`** - For probability vectors:
- Maps K-1 unconstrained → K-simplex via stick-breaking
- Provides `logJacobianGrad()` and `chainRuleGrad()` for HMC

## Support Inference (autoTransform)

Automatically selects transforms based on distribution support:

```cpp
auto sigma = halfNormal(2.0);
auto transform = autoTransform(sigma);  // Returns Positive<sigma>
```

Mapping:
- `RealSupport` → `Unconstrained`
- `PositiveSupport` → `Positive`
- `UnitIntervalSupport` → `UnitInterval`
- `SimplexSupport` → `Unconstrained` (TODO: full integration)

## TransformedPosterior

The key class for HMC sampling:

```cpp
auto posterior = makeTransformedPosterior(
    logProb(mu, sigma, y),
    unconstrained(mu),
    positive(sigma)
).observe(y = 3.5);
```

Operations:
- `logProb(z)` - Evaluates in unconstrained space, includes Jacobian correction
- `gradient(z)` - Chain-ruled gradients w.r.t. unconstrained parameters
- `transform(z)` - Convert unconstrained → constrained
- `inverse(x)` - Convert constrained → unconstrained

### Jacobian Correction

In `logProb()`:
```cpp
StateArray x = transform(z);           // z → x
double lp = evaluate(log_prob_expr_, bindings);  // Model log-prob
double log_jacobian = (transforms_[I].logJacobian(z[I]) + ...);
return lp + log_jacobian;  // Add correction
```

### Gradient Chain Rule

For each parameter, applies:
```
grad_z[i] = grad_x[i] * (dx/dz) + d(log|J|)/dz
```

Example for Positive transform (`x = exp(z)`):
```cpp
grad_z = grad_x * x + 1.0;  // dx/dz = x, d(log|J|)/dz = 1
```

## HMC Integration

### Current (flat arrays, inconsistent)

`HMCAdapter` wraps `TransformedPosterior` for use with `bayes::makeHMC`:

```cpp
auto adapter = makeHMCFromPosterior<double>(posterior, epsilon, n_steps);
auto samples = adapter.sample(initial, n_samples, n_warmup, rng);
// Returns samples in constrained space - but as flat arrays
```

### Target (binding-aware, unified)

HMC should work directly with bindings, not flat arrays:

```cpp
// Posterior already knows its parameters
auto posterior = infer(alpha, beta, sigma, y).bind(x = data, y = obs);

// Sample with binding syntax
auto samples = posterior.sample(
    hmc(epsilon = 0.01, steps = 20),
    init = (alpha = 0.0, beta = 0.0, sigma = 0.0),
    warmup = 500,
    draws = 1000,
    rng);

// Access by symbol
for (auto& draw : samples) {
    double a = draw[alpha];
    double b = draw[beta];
}
```

The binding syntax is unified across:
- Data binding: `bind(x = x_data)`
- Observation binding: `bind(y = y_data)`
- Parameter evaluation: `logProb(alpha = 0.5, beta = 1.0, ...)`
- Gradient queries: `grad[alpha]`
- Initial state: `init = (alpha = 0.0, ...)`
- Sample access: `draw[alpha]`

## Non-Centered Parameterization

For hierarchical models with funnel geometry:

```cpp
// Instead of: theta[j] ~ Normal(mu, sigma)
// Use: z[j] ~ Normal(0, 1), theta[j] = mu + sigma * z[j]

auto ncp = nonCenteredParam<N>(mu_idx, log_sigma_idx, z_start_idx);
double theta_j = ncp.param(state, j);
ncp.addParamGrad(grad, j, d_theta, state);
ncp.addZPriorGrad(grad, j, state);  // N(0,1) prior contribution
```

## SimplePosterior vs TransformedPosterior

| Aspect | SimplePosterior | TransformedPosterior |
|--------|-----------------|----------------------|
| Space | Constrained only | Unconstrained (HMC) |
| Jacobian | None | Automatic |
| Use case | Verification, prior sampling | Production MCMC |
| From | `infer()` | `model().posterior().build()` |

**Note**: `infer()` now returns `PlateTransformedPosteriorBuilder` which includes transforms. The `SimplePosterior` is only used by `inferRaw()` for testing.

## Adding New Transforms

1. Define transform struct:
   ```cpp
   template <typename Param>
   struct MyTransform {
     Param param;
     auto transform(double z) const -> double;
     auto inverse(double x) const -> double;
     auto logJacobian(double z) const -> double;
   };
   ```

2. Add type traits in `transforms.h`:
   ```cpp
   template <typename T>
   struct IsMyTransform : std::false_type {};
   template <typename P>
   struct IsMyTransform<MyTransform<P>> : std::true_type {};
   ```

3. Add chain rule case in `TransformedPosterior::chainRuleGrad()`:
   ```cpp
   } else if constexpr (is_my_transform_v<Transform>) {
     // Your gradient formula
   }
   ```

4. If support-based, add mapping in `support.h`:
   ```cpp
   template <typename Param>
   struct SupportToTransform<MySupportTag, Param> {
     static constexpr auto apply(Param p) { return MyTransform{p}; }
   };
   ```

## Next Steps

See `BINDING_API_PLAN.md` for the detailed implementation roadmap.

Priority order:
1. **ParameterState** - Foundation for binding-based operations
2. **Binding-based evaluation** - `posterior(alpha = 0.5, ...)`
3. **Samples container** - Symbol-indexed results
4. **Integrated sampler** - `posterior.sample(hmc(...), ...)`
