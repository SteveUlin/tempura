# Statistical Rethinking Examples

## Purpose

These examples implement homework problems from Richard McElreath's [Statistical Rethinking 2025](https://github.com/rmcelreath/stat_rethinking_2025) course. They serve as **the primary test cases for symbolic4 API completeness**.

The goal: when the symbolic4 API is complete, these examples should become dramatically simpler - replacing manual log-prob/gradient implementations with declarative model specifications.

## Current Status

| Example | Model Complexity | API Usage | Status |
|---------|-----------------|-----------|--------|
| `linear_regression.cpp` | Simple | `infer()` + `plate<Obs>()` | ✅ Fully declarative with automatic gradients |
| `logistic_regression.cpp` | Simple | `infer()` + `plate<Obs>()` | ✅ Fully declarative with automatic gradients |
| `bangladesh_contraception.cpp` | Complex | Helper utilities | 🔧 Manual - needs non-centered parameterization |
| `bmj_weekend_submissions.cpp` | Moderate | Minimal | 🔧 Manual - needs indexed latent parameters |
| `bmj_symbolic.cpp` | Moderate | `plate<>` + `differentiate()` | 🔧 Manual gradients for indexed params |

## Model Patterns Needed

### 1. Linear Regression (linear_regression.cpp) ✅ DONE
```cpp
// ~180 lines with fully automatic gradients and clean syntax!
struct Obs {};  // Dimension tag

auto alpha = normal(0.0, 10.0);
auto beta = normal(0.0, 5.0);
auto sigma = halfNormal(2.0);
auto x = data<Obs>();  // Covariate plate
auto y = plate<Obs>(normal(alpha + beta * x, sigma));  // Direct usage!

auto posterior = infer(alpha, beta, sigma, y)
    .bind(x = indexed(x_data), y = indexed(y_data));

// Direct use with HMC - no manual likelihood or gradient code!
auto hmc = bayes::makeHMC<double, 3>(
    [&](const State& z) { return posterior.logProb(z); },
    [&](const State& z) { return posterior.gradient(z); },
    epsilon, n_steps);

// Or query gradients by symbol:
auto grad = posterior.gradientResult(z);
double d_alpha = grad[alpha];  // Query by RandomVar
```

### 2. Logistic Regression (logistic_regression.cpp) ✅ DONE
```cpp
// ~225 lines with fully automatic gradients!
struct Obs {};

auto alpha = normal(0.0, 5.0);
auto beta = normal(0.0, 2.5);
auto x = data<Obs>();
// logistic(z) = 1 / (1 + exp(-z))
auto y = plate<Obs>(bernoulli(logistic(alpha + beta * x)));  // Direct usage!

auto posterior = infer(alpha, beta, y)
    .bind(x = indexed(x_data), y = indexed(y_data));

// Same pattern - fully automatic
auto hmc = bayes::makeHMC<double, 2>(
    [&](const State& z) { return posterior.logProb(z); },
    [&](const State& z) { return posterior.gradient(z); },
    epsilon, n_steps);
```

### 3. Hierarchical Model (bangladesh_contraception.cpp)
```cpp
// Current: ~600 lines with manual state layout, gradients, transforms
// Target:
auto a_bar = normal(0, 1);
auto sigma_a = exponential(1);
auto z_a = plate<Districts>(normal(0, 1));
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
auto z = plate<Districts>(normal(0, 1));
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

The API is "complete enough" when:

1. **linear_regression.cpp** can be rewritten in <50 lines with no manual gradients
2. **bangladesh_contraception.cpp** can be rewritten in <100 lines with no manual state layout
3. All examples use `infer()` or `model().posterior().build()` without lambdas for log-prob/gradient
4. Parameter transforms and Jacobians are fully automatic
5. Plate summations over data are automatic

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
