# mcmc/ - Making MCMC Boring

## Purpose

This directory implements the "boring MCMC" vision: automatic parameter transforms, Jacobian corrections, and gradient chain rules so users just define models.

## Key Files

| File | Purpose |
|------|---------|
| `transforms.h` | Parameter transform types (log, logit, Cholesky, simplex) |
| `support.h` | Distribution → support → transform inference (`autoTransform`) |
| `plate_transforms.h` | `PlateTransformedPosterior` — sole production posterior with inline typed HMC |
| `mutable_state.h` | `MutableState` — per-slot typed container for HMC position/momentum/gradient |
| `samples.h` | `Samples` — unified MCMC draw container with symbol-indexed access |
| `sampler.h` | `HmcConfig` struct |
| `non_centered.h` | Hierarchical model reparameterization |
| `likelihoods.h` | Numerically stable logistic functions |

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
auto sigma = halfNormal(2_c);
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
- `logProbAt(BinderPack{mu=1.0, sigma=2.0})` - Log-prob at constrained values
- `gradientAt(BinderPack{mu=1.0, sigma=2.0})` - Gradient (returns `MutableState`, queryable by symbol)
- `sample(HmcConfig, BinderPack{...}, rng)` - Run HMC and return `Samples`
- `debugGradientCheck(BinderPack{...})` - Verify analytic vs finite-diff gradient
- `stateDim()` - Runtime state dimension (supports indexed params)
- `logProb(span)` / `gradient(span)` - Legacy span-based API (backward compat)

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

HMC is **inline** — the posterior performs leapfrog integration directly on typed
state. No external library, no flat-vector wrapping. The computation pipeline
operates on `MutableState` (symbol-dispatched per-slot container) end-to-end;
the only flat vector is the serialization boundary into `Samples` storage.

### MutableState — typed HMC container

`MutableState<ScalarSlot<α>, ScalarSlot<σ>, IndexedSlot<z>>` stores each parameter
in its own typed slot. Overload resolution dispatches `state[alpha]` → `double&`,
`state[z_b]` → `span<double>`. No flat arrays, no offset arithmetic.

Leapfrog, gradient chain-rule, and momentum sampling all dispatch by symbol via
fold expressions over the slot types.

### Sampling API (binding-aware, unified)

`posterior.sample()` accepts `BinderPack` init values and returns a `Samples` object
with full symbol-indexed access:

```cpp
auto posterior = makePlateTransformedPosterior(joint_lp, alpha, sigma, z_b)
                     .withDimension<Districts>(N)
                     .build();

auto samples = posterior.sample(
    HmcConfig{.epsilon = 0.1, .steps = 10, .warmup = 500, .draws = 1000},
    BinderPack{alpha = 0.0, sigma = 1.0, z_b = std::vector<double>(N, 0.0)},
    rng);

// Symbol-indexed access
mean(samples[alpha]);           // posterior mean (scalar)
samples[z_b];                   // DynamicDense matrix (draws × districts)
samples[logistic(alpha + sigma * z_b)];  // derived expression across draws

// Per-draw access
auto draw = samples[std::size_t{0}];
double a_val = draw[alpha];      // scalar lookup
auto z_span = draw[z_b];        // span<const double> (indexed)

// Range-for iteration
for (const auto& draw : samples) {
    double a = draw[alpha];
}
```

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
| `PlateTransformedPosterior` | Unconstrained (HMC) | Automatic | Production MCMC — sole posterior type for scalar + indexed params |
| `RawPosterior` (in `infer.h`) | Constrained only | None | Testing/verification only (`inferRaw()`) |

`PlateTransformedPosterior` is the sole production posterior, used by `infer()`, `inferExplicit()`, `model().posterior().build()`, and direct `makePlateTransformedPosterior()` construction.

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

2. Add type trait in `transforms.h` using reflection:
   ```cpp
   template <typename T>
   constexpr bool is_my_transform_v = core_traits_detail::isSpecOf<T, MyTransform>();
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

## Remaining Gaps

- **Constrained init values** — users currently pass unconstrained init (`sigma = std::log(0.5)`). The posterior has `inverse()` — apply it to init values automatically.
- ~~**Indexed gradient spans**~~ — ✓ Solved. `gradientAt()` returns `MutableState`: `grad[alpha]` → `double`, `grad[z_b]` → `span<double>`.
- **MCMC diagnostics** — ESS, R-hat, divergences. Currently not tracked.
- **Unify SymbolicState with MutableState** — Samples uses `SymbolicState` (flat-vector-backed) for per-draw access. MutableState is a better fit but requires a non-owning DrawView or per-draw copy.
