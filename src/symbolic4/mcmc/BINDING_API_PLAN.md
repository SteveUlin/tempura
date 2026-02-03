# Binding-Centric API Implementation Plan

## Goal

Eliminate implicit parameter ordering. `ParameterState` is the universal glue - constructed from bindings, queryable by symbol, used throughout.

**Current problem**:
```cpp
auto grad = posterior.gradient(z);
return State{grad[0], grad[1], grad[2]};  // Which is alpha? beta? sigma?
```

**Target**:
```cpp
auto samples = posterior.sample(config, (alpha = 0.0, beta = 0.0, sigma = 1.0), rng);
double mean_alpha = mean(samples[alpha]);  // Explicit everywhere
```

## Core Insight

The posterior already knows its parameters from `infer(alpha, beta, sigma, y)`. All operations should leverage this - no redundant parameter specifications.

## Target API

```cpp
// Model definition (unchanged)
auto alpha = normal(0.0, 10.0);
auto beta = normal(0.0, 5.0);
auto sigma = halfNormal(2.0);
auto x = data<Obs>();
auto y = plate<Obs>(normal(alpha + beta * x, sigma));

auto posterior = infer(alpha, beta, sigma, y)
    .bind(x = x_data, y = y_data);

// NEW: Sample with binding-based init
auto samples = posterior.sample(
    HmcConfig{.epsilon = 0.01, .steps = 20, .warmup = 500, .draws = 1000},
    (alpha = 0.0, beta = 0.0, sigma = 1.0),  // init as bindings
    rng);

// Access by symbol
double mean_alpha = mean(samples[alpha]);
for (const auto& draw : samples) {
    std::cout << draw[alpha] << ", " << draw[sigma] << "\n";
}
```

## Implementation Plan

### Step 1: `ParameterState<Params...>` - The Central Glue

**File**: `src/symbolic4/mcmc/state.h` (new)

`ParameterState` is the universal currency between all operations:
- Constructed from bindings (type-safe, explicit)
- Queryable by symbol (`state[alpha]`)
- Convertible to/from arrays for HMC internals

```cpp
template <typename... Params>
class ParameterState {
public:
  static constexpr std::size_t N = sizeof...(Params);
  using ArrayType = std::array<double, N>;

  // Default construction
  constexpr ParameterState() = default;

  // Construction from array (internal use)
  constexpr explicit ParameterState(ArrayType values) : values_{values} {}

  // Construction from bindings - the main API
  template <typename... Binders>
  constexpr ParameterState(BinderPack<Binders...> bindings) {
    initFromBindings(bindings);
  }

  // Symbol-based access (compile-time lookup)
  template <typename P>
    requires IsRandomVar<P>
  constexpr double& operator[](const P&) {
    constexpr std::size_t idx = indexOfParam<P>();
    return values_[idx];
  }

  template <typename P>
    requires IsRandomVar<P>
  constexpr double operator[](const P&) const {
    constexpr std::size_t idx = indexOfParam<P>();
    return values_[idx];
  }

  // Array access for HMC internals
  constexpr auto& array() { return values_; }
  constexpr const auto& array() const { return values_; }

private:
  ArrayType values_{};

  template <typename P>
  static constexpr std::size_t indexOfParam() {
    // Match by unconstrained_symbol_type (same as GradientResult)
    return indexInPack<typename P::unconstrained_symbol_type,
                       typename Params::unconstrained_symbol_type...>();
  }

  template <typename... Binders>
  constexpr void initFromBindings(BinderPack<Binders...> bindings) {
    // Extract values from bindings in correct order
    ((values_[indexOfParam</* param for binder */>] = bindings.get<...>()), ...);
  }
};
```

**Key design**: Uses `unconstrained_symbol_type` for lookup, matching `GradientResult` pattern.

### Step 2: `Samples<Params...>` Container

**File**: `src/symbolic4/mcmc/samples.h` (new)

```cpp
template <typename... Params>
class Samples {
public:
  static constexpr std::size_t N = sizeof...(Params);
  using StateType = ParameterState<Params...>;

  void push_back(const std::array<double, N>& draw) {
    draws_.push_back(draw);
  }

  void reserve(std::size_t n) { draws_.reserve(n); }
  std::size_t size() const { return draws_.size(); }

  // Access all values for one parameter (for statistics)
  template <typename P>
    requires IsRandomVar<P>
  auto operator[](const P&) const -> std::vector<double> {
    constexpr std::size_t idx = StateType::template indexOfParam<P>();
    std::vector<double> result;
    result.reserve(draws_.size());
    for (const auto& d : draws_) {
      result.push_back(d[idx]);
    }
    return result;
  }

  // Single draw as ParameterState
  auto operator[](std::size_t i) const -> StateType {
    return StateType{draws_[i]};
  }

  // Iteration over draws
  auto begin() const { return Iterator{draws_.begin()}; }
  auto end() const { return Iterator{draws_.end()}; }

private:
  std::vector<std::array<double, N>> draws_;

  // Iterator that wraps array as ParameterState
  struct Iterator { ... };
};

// Statistics helpers - work with vector<double> from samples[param]
inline double mean(const std::vector<double>& values) {
  return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

inline double sd(const std::vector<double>& values) {
  double m = mean(values);
  double sum_sq = 0.0;
  for (double v : values) sum_sq += (v - m) * (v - m);
  return std::sqrt(sum_sq / values.size());
}
```

### Step 3: `HmcConfig` and `posterior.sample()`

**File**: `src/symbolic4/mcmc/sampler.h` (new)

```cpp
struct HmcConfig {
  double epsilon = 0.01;
  std::size_t steps = 20;
  std::size_t warmup = 500;
  std::size_t draws = 1000;
};
```

**File**: `src/symbolic4/mcmc/plate_transforms.h` (add method)

```cpp
// In PlateTransformedPosterior class:

// Extract Params... from ParamSymbolsTuple for Samples type
template <typename... Binders, std::uniform_random_bit_generator RNG>
auto sample(HmcConfig config, BinderPack<Binders...> init, RNG& rng) const {
  using SamplesType = /* Samples<Params...> extracted from this posterior */;
  constexpr std::size_t N = stateDim();

  // Convert init bindings to array
  auto state = makeStateFromBindings(init);
  std::array<double, N> z = state.array();

  // Create HMC kernel
  auto log_prob = [this](const auto& z) { return logProb(z); };
  auto gradient = [this](const auto& z) {
    auto g = this->gradient(z);
    std::array<double, N> result;
    std::copy(g.begin(), g.end(), result.begin());
    return result;
  };

  auto hmc = bayes::makeHMC<double, N>(log_prob, gradient, config.epsilon, config.steps);
  double lp = log_prob(z);

  // Warmup
  for (std::size_t i = 0; i < config.warmup; ++i) {
    auto [next_z, next_lp, accepted] = hmc.step(z, lp, rng);
    z = next_z;
    lp = next_lp;
  }

  // Collect samples (transformed to constrained space)
  SamplesType samples;
  samples.reserve(config.draws);
  for (std::size_t i = 0; i < config.draws; ++i) {
    auto [next_z, next_lp, accepted] = hmc.step(z, lp, rng);
    z = next_z;
    lp = next_lp;

    auto constrained = transform(z);
    std::array<double, N> x;
    std::copy(constrained.begin(), constrained.end(), x.begin());
    samples.push_back(x);
  }

  return samples;
}
```

### Step 4: Update Examples

**Before** (~50 lines of HMC boilerplate):
```cpp
using State = std::array<double, 3>;
auto log_prob_fn = [&](const State& z) { return posterior.logProb(z); };
auto gradient_fn = [&](const State& z) {
  auto grad = posterior.gradient(z);
  return State{grad[0], grad[1], grad[2]};  // Manual unpacking!
};
auto hmc = bayes::makeHMC<double, 3>(log_prob_fn, gradient_fn, epsilon, steps);

State z{0.0, 0.0, 0.0};
double lp = log_prob_fn(z);

// Manual warmup loop...
// Manual sampling loop...
// Manual statistics computation...
```

**After** (~10 lines):
```cpp
auto samples = posterior.sample(
    HmcConfig{.epsilon = 0.01, .steps = 20, .warmup = 500, .draws = 1000},
    (alpha = 0.0, beta = 0.0, sigma = 1.0),
    rng);

std::cout << "alpha: " << mean(samples[alpha]) << " +/- " << sd(samples[alpha]) << "\n";
std::cout << "beta:  " << mean(samples[beta]) << " +/- " << sd(samples[beta]) << "\n";
std::cout << "sigma: " << mean(samples[sigma]) << " +/- " << sd(samples[sigma]) << "\n";
```

## Files to Create/Modify

| File | Action | Purpose |
|------|--------|---------|
| `src/symbolic4/mcmc/state.h` | Create | `ParameterState<Params...>` |
| `src/symbolic4/mcmc/samples.h` | Create | `Samples<Params...>` + statistics |
| `src/symbolic4/mcmc/sampler.h` | Create | `HmcConfig` |
| `src/symbolic4/mcmc/plate_transforms.h` | Modify | Add `sample()` method |
| `examples/stat_rethinking/linear_regression.cpp` | Modify | Use new API |
| `examples/stat_rethinking/logistic_regression.cpp` | Modify | Use new API |

## Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| `ParameterState<Params...>` | ✅ Done | Symbol lookup, binding construction |
| `Samples<Params...>` | ✅ Done | Symbol access, statistics helpers |
| `HmcConfig` | ✅ Done | Configuration struct |
| `posterior.sample()` | ✅ Done | Binding-centric sampling |
| `posterior.logProbAt()` | ✅ Done | Binding-based evaluation |
| `posterior.gradientAt()` | ✅ Done | Binding-based gradient |
| `linear_regression.cpp` | ✅ Done | Updated to new API |
| `logistic_regression.cpp` | ✅ Done | Updated to new API |
| Indexed param support | 🔲 Deferred | Requires runtime-sized HMC |

## Implementation Order

1. **state.h** - `ParameterState` with symbol lookup and binding construction ✅
2. **samples.h** - `Samples` container + `mean`/`sd` helpers ✅
3. **sampler.h** - `HmcConfig` struct ✅
4. **plate_transforms.h** - Add `sample()` method ✅
5. **Tests** - Unit tests for each component ✅
6. **Examples** - Update to validate the design ✅

## Key Design Decisions

1. **No redundant params**: `posterior.sample()` extracts parameter types from posterior itself
2. **Init as bindings**: `(alpha = 0.0, beta = 0.0, sigma = 1.0)` - no implicit ordering
3. **ParameterState is the glue**: Universal currency between all operations
4. **Samples in constrained space**: Users see actual parameter values
5. **Backward compatible**: Existing `logProb(span)` and `gradient(span)` unchanged

## Indexed Parameters (Deferred)

For `plate<Groups>(beta(...))`, sampling with indexed parameters requires runtime-sized HMC:
- Current HMC uses `std::array<T, N>` with compile-time `N`
- Indexed params have runtime-determined size from `spec.size()`
- `sample()` now has a compile-time check that errors if indexed params are present

**Future work**:
1. Create `DynamicHMC` that works with `std::vector`
2. Add `DynamicSamples` that tracks param offsets at runtime
3. Implement `samples[indexed_param]` returning a 2D view

The design for querying indexed samples would be:
```cpp
auto theta_samples = samples[theta];  // IndexedDraws proxy
auto group_3_draws = theta_samples[3];  // vector<double> of draws for group 3
double mean_g3 = mean(group_3_draws);
```

## Verification

```bash
cmake --build build --target state_test
cmake --build build --target samples_test
ctest --test-dir build -R state_test
ctest --test-dir build -R samples_test
./build/examples/stat_rethinking/linear_regression
```
