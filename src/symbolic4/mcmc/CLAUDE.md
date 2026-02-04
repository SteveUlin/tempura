# mcmc/ - Making MCMC Boring

## Purpose

This directory implements the "boring MCMC" vision: automatic parameter transforms, Jacobian corrections, and gradient chain rules so users just define models.

## Key Files

| File | Purpose |
|------|---------|
| `transforms.h` | Parameter transform types (log, logit, Cholesky, simplex) |
| `support.h` | Distribution → support → transform inference (`autoTransform`) |
| `plate_transforms.h` | `PlateTransformedPosterior` — unified posterior for scalar + indexed params |
| `posterior.h` | Simple posterior wrapper |
| `non_centered.h` | Hierarchical model reparameterization |
| `likelihoods.h` | Numerically stable logistic functions |

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

## PlateTransformedPosterior

The unified posterior class for HMC sampling, handling both scalar and indexed params:

```cpp
// Via infer():
auto posterior = infer(y).bind(x = data, y = obs);

// Via model():
auto posterior = model(mu, sigma, y).posterior().observe(y = 3.5).build();

// Via explicit construction:
auto posterior = makePlateTransformedPosterior(joint_lp, mu, sigma).observe(y = 3.5);
```

Operations:
- `logProb(z)` - Evaluates in unconstrained space, includes Jacobian correction
- `gradient(z)` - Chain-ruled gradients w.r.t. unconstrained parameters
- `transform(z)` - Convert unconstrained → constrained
- `inverse(x)` - Convert constrained → unconstrained
- `logProbAt(BinderPack{mu=1.0, sigma=2.0})` - Symbolic lookup evaluation
- `stateDim()` - Runtime state dimension (supports indexed params)

### Jacobian Correction

In `logProb()`:
```cpp
auto x = transform_pack_.transform(z);  // z → x (per-param transforms)
double lp = evaluate(log_prob_expr_, bindings);
double log_jacobian = transform_pack_.logJacobian(z);
return lp + log_jacobian;
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

### Current

`PlateTransformedPosterior` integrates with `bayes::makeHMCDynamic` via lambdas:

```cpp
auto posterior = makePlateTransformedPosterior(joint_lp, mu, sigma).observe(y = 3.5);
auto hmc = bayes::makeHMCDynamic(
    [&](std::span<const double> z) { return posterior.logProb(z); },
    [&](std::span<const double> z) { return posterior.gradient(z); },
    epsilon, n_steps, posterior.stateDim());
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

## Posterior Types

| Type | Space | Jacobian | Use case |
|------|-------|----------|----------|
| `PlateTransformedPosterior` | Unconstrained (HMC) | Automatic | Production MCMC — handles both scalar and indexed params |
| `RawPosterior` (in `infer.h`) | Constrained only | None | Testing/verification only (`inferRaw()`) |

**Note**: The old `TransformedPosterior` (scalar-only, fixed-size `std::array` state) has been deleted. `PlateTransformedPosterior` is now the sole production posterior type, used by both `infer()`, `inferExplicit()`, and `model().posterior().build()`.

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

3. Add `chainRuleGrad()` method to the transform struct:
   ```cpp
   auto chainRuleGrad(double grad_x, double z) const -> double {
     double dx_dz = ...;  // Derivative of forward transform
     double dlogJ_dz = ...;  // Derivative of log-Jacobian
     return grad_x * dx_dz + dlogJ_dz;
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
