# Statistical Rethinking Examples

## Purpose

These examples implement homework problems from Richard McElreath's [Statistical Rethinking 2025](https://github.com/rmcelreath/stat_rethinking_2025) course. They serve as **the primary test cases for symbolic4 API completeness**.

The goal: when the symbolic4 API is complete, these examples should become dramatically simpler - replacing manual log-prob/gradient implementations with declarative model specifications.

## Current Status

| Example | Model Complexity | API Usage | Status |
|---------|-----------------|-----------|--------|
| `linear_regression.cpp` | Simple | `infer()` + `plate<Obs>()` + `sample()` | ✅ ~115 lines, fully automatic |
| `logistic_regression.cpp` | Simple | `infer()` + `plate<Obs>()` + `sample()` | ✅ ~115 lines, fully automatic |
| `random_allocation_game.cpp` | Simple | `beta()`, `normal()`, `sample()` | ✅ Teaching example for basic sampling |
| `bmj_symbolic.cpp` | Moderate | `plate<>` + `differentiate()` | 🔧 Uses symbolic4 but manual HMC (indexed latent params) |
| `bmj_weekend_submissions.cpp` | Moderate | Manual | 🔧 Pedagogical - compares no-pooling vs partial-pooling |
| `bangladesh_contraception.cpp` | Complex | Helper utilities | 🔧 Manual - needs indexed latent params, non-centered |

### Current Limitations

The `sample()` method only supports **scalar latent parameters**. Models with indexed latent params (like `b[i] ~ Normal(0, sigma)` in `bmj_symbolic.cpp`) require runtime-sized HMC which is not yet implemented. These examples must use manual HMC setup.

## Model Patterns Needed

### 1. Linear Regression (linear_regression.cpp) ✅ DONE
```cpp
// ~115 lines with fully automatic sampling!
struct Obs {};  // Dimension tag

auto alpha = normal(0_c, 10_c);
auto beta = normal(0_c, 5_c);
auto sigma = halfNormal(2_c);
auto x = data<Obs>();  // Covariate plate
auto y = plate<Obs>(normal(alpha + beta * x, sigma));

// infer(y) auto-discovers alpha, beta, sigma from y's expression tree
auto posterior = infer(y)
    .bind(x = indexed(x_data), y = indexed(y_data));

// One-line sampling with binding-centric API
auto samples = posterior.sample(
    HmcConfig{.epsilon = 0.01, .steps = 20, .warmup = 500, .draws = 1000},
    BinderPack{alpha = 0.0, beta = 0.0, sigma = 0.0},  // init
    rng);

// Access by symbol
std::cout << "alpha: " << mean(samples[alpha]) << "\n";
std::cout << "beta:  " << mean(samples[beta]) << "\n";
std::cout << "sigma: " << mean(samples[sigma]) << "\n";
```

### 2. Logistic Regression (logistic_regression.cpp) ✅ DONE
```cpp
// ~115 lines with fully automatic sampling!
struct Obs {};

auto alpha = normal(0_c, 5_c);
auto beta = normal(0_c, 2.5_c);
auto x = data<Obs>();
auto y = plate<Obs>(bernoulli(logistic(alpha + beta * x)));

auto posterior = infer(y)
    .bind(x = indexed(x_data), y = indexed(y_data));

auto samples = posterior.sample(
    HmcConfig{.epsilon = 0.02, .steps = 20, .warmup = 500, .draws = 1000},
    BinderPack{alpha = 0.0, beta = 0.0},
    rng);

std::cout << "alpha: " << mean(samples[alpha]) << "\n";
std::cout << "beta:  " << mean(samples[beta]) << "\n";
```

### 3. Hierarchical Model (bangladesh_contraception.cpp)
```cpp
// Current: ~600 lines with manual state layout, gradients, transforms
// Target:
auto a_bar = normal(0_c, 1_c);
auto sigma_a = exponential(1_c);
auto z_a = plate<Districts>(normal(0_c, 1_c));
auto a = a_bar + sigma_a * z_a;  // Non-centered

auto delta = dirichlet({2, 2, 2});  // Ordered monotonic
auto y = plate<Obs>(bernoulli(logistic(
    a[district] + bU[district] * urban + bA * age + bK[district] * cumsum(delta)[kids]
)));

auto posterior = infer(y)
    .withDimension<Districts>(60)
    .withData(district = district_idx, urban = urban_data, age = age_data, kids = kids_data)
    .observe(y = contraception_data)
    .build();
```

## Key Features Needed

### Data Covariates
```cpp
// x is observed data, not a parameter
auto y = plate<Obs>(normal(alpha + beta * x, sigma));
//                                       ^ external covariate
```

### Indexing into Plates
```cpp
auto a = plate<Districts>(normal(mu, sigma));
// Later:
a[district]  // district is data, indexes into the plate
```

### Non-Centered Parameterization
```cpp
// Centered (bad geometry):
auto a = plate<Districts>(normal(a_bar, sigma_a));

// Non-centered (good geometry):
auto z = plate<Districts>(normal(0_c, 1_c));
auto a = a_bar + sigma_a * z;  // Deterministic transform
```

### Ordered Monotonic Effects
```cpp
auto delta = dirichlet({2, 2, 2});  // 3 increments for 4 levels
auto effect = orderedMonotonic(delta);
// effect[1] = 0
// effect[2] = delta[0]
// effect[3] = delta[0] + delta[1]
// effect[4] = delta[0] + delta[1] + delta[2] = 1
```

## Success Criteria

### Achieved ✅
1. **linear_regression.cpp** - ~115 lines, fully automatic (was ~180 lines)
2. **logistic_regression.cpp** - ~115 lines, fully automatic
3. Scalar parameter transforms and Jacobians are fully automatic
4. Plate summations over observed data are automatic
5. Symbol-based sample access: `samples[alpha]`, `mean(samples[beta])`

### In Progress 🔧
1. **Indexed latent parameters** - `sample()` only supports scalar params; indexed latent params (like `b[i]`) need runtime-sized HMC
2. **Non-centered parameterization** - Helper utilities exist but not integrated into `infer()` API
3. **bangladesh_contraception.cpp** - Target: <100 lines with no manual state layout

## Weekly Topics Covered

From the course schedule, these examples touch:

- **Week 2**: Geocentric models (linear regression)
- **Week 4**: MCMC
- **Week 5**: Modeling events (logistic, Poisson)
- **Week 6**: Multilevel models (bangladesh)
- **Week 7**: Correlated features (varying effects)

Future examples could cover:
- **Week 7**: LKJ correlation priors (needs matrix/ integration)
- **Week 8**: Gaussian processes
- **Week 9**: Measurement error, missing data
