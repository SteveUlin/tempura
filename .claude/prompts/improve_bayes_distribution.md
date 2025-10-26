# Improve Bayes Distribution

This prompt guides the process of reviewing and improving a probability distribution implementation in `src/bayes/`.

## Workflow

### 1. Code Review

Read the distribution header file (e.g., `src/bayes/beta.h`, `src/bayes/binomial.h`) and identify:

**Critical Issues:**

- Compilation errors (typos, missing includes, syntax errors)
- Logic bugs (integer division, incorrect formulas, off-by-one errors)
- Missing `std::` qualifiers or ADL patterns for custom type support
- Type safety issues

**Quality Issues:**

- Missing `constexpr` where possible
- Inconsistent type usage (mixing `0`, `0.`, `T{0}`)
- Missing standard distribution methods (`mean()`, `variance()`, accessors)
- Lack of validation (e.g., parameter constraints)
- Missing or excessive comments

### 2. Apply Fixes

**Fix critical bugs first:**

- Correct any typos, syntax errors, compilation issues
- Fix algorithmic bugs (division, bounds, formula errors)
- Add missing includes (`<cassert>`, `<cmath>`, `<type_traits>`)

**Type Safety:**

- Add `static_assert(!std::is_integral_v<T>, "...")` if it's a continuous distribution
- Use ADL pattern for math functions: `using std::log; log(x);`
- Use `numeric_infinity(T{})` from `bayes/numeric_traits.h` instead of `std::numeric_limits<T>::infinity()`
- Ensure consistent type construction: prefer `T{0}`, `T{1}`, `T{2}` over raw literals
- Use `std::numbers` constants (e.g., `T{std::numbers::pi}`, `T{std::numbers::sqrt2}`) instead of hardcoded values

**Quality Improvements:**

- Make all methods `constexpr` where possible
- Add parameter validation in constructor (use `assert`)
- Add standard methods if missing: `mean()`, `variance()`, parameter accessors
- Add explicit return types: `-> T`
- Extract complex algorithms into separate private helper functions (e.g., Box-Muller transform)
- Use structured bindings for multi-value returns: `auto [x, y] = helper();`

**Sampling Implementation:**

- **DO NOT use standard library distributions** (e.g., `std::normal_distribution`, `std::binomial_distribution`)
- Implement sampling algorithms from scratch using the RNG directly
- Common approaches:
  - Inverse transform sampling (convert uniform samples to target distribution)
  - Acceptance-rejection methods
  - Direct simulation (e.g., Binomial as n Bernoulli trials)
  - Special transforms (e.g., Box-Muller for Normal)
- This ensures constexpr compatibility and full control over the implementation

**Comments:**

- Short and judicious - explain _why_, not _what_
- **Always place comments above the code**, not inline (exception: very short clarifications)
- Include Unicode math formulas for key algorithms (e.g., `p(x|μ,σ) = ...`)
- Explain non-obvious design decisions (inverse transform, numerical stability, edge cases)
- Highlight common pitfalls (integer division traps, underflow concerns)
- Do NOT repeat what the code obviously does
- Do NOT speak down to the reader

**Documentation Header:**

```cpp
// [Distribution Name] distribution [Mathematical notation]
//
// Intuition:
//   [Brief explanation of what the distribution models and when to use it]
//   [Compare to similar distributions if helpful]
//
```

### 3. Create Comprehensive Test File

Create `src/bayes/[distribution]_test.cpp` following the pattern:

```cpp
#include "bayes/[distribution].h"

#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "[Distribution] [property]"_test = [] {
    // Test body
    expectNear(expected, actual);
    expectEq(expected, actual);
    static_assert(compile_time_check);
  };

  // More tests...
}
```

**Test Coverage:**

- Probability density/mass function (in range, out of range, edge cases)
- Log probability (in range, out of range, -infinity cases)
- CDF (lower bound, upper bound, mid-range)
- Statistical properties (mean, variance, etc.)
- Parameter accessors
- Sampling (range validation, distribution statistics with ~10k samples)
- constexpr construction and evaluation
- Type rejection (verify static_assert works)
- Custom type support (if applicable)

**Register test in CMakeLists.txt:**

```cmake
add_bayes_test([distribution]_test distributions)
```

### 4. Verify

Build and run tests:

```bash
# Build the test
ninja -C build bayes_[distribution]_test

# Run via ctest (auto-approved, preferred)
ctest --test-dir build -R [distribution] --output-on-failure

# Run all bayes tests to ensure no regressions
ctest --test-dir build -L bayes --output-on-failure
```

**Note:** Use `ctest` instead of running binaries directly (e.g., `./build/src/bayes/bayes_[distribution]_test`) as ctest commands are auto-approved and allow faster workflow.

All tests must pass before proceeding.

## Extension Points Reference

**numeric_traits.h already provides:**

- `numeric_infinity(T)` - returns infinity for type T (default uses `std::numeric_limits`)

**Common ADL extension points users may need:**

- `log(T)`, `exp(T)`, `sqrt(T)`, `pow(T, T)`
- `sin(T)`, `cos(T)`, `tan(T)`
- `erf(T)`, `gamma(T)`, `lgamma(T)`

## Style Guidelines

**Modern C++ preferences:**

- Use `std::format` and `std::print` over `iostream`
- Use structured bindings: `auto [x, y] = ...`
- Trailing return types: `auto func() -> T`
- `constexpr` by default

**Naming:**

- `k` prefix for constants: `kDefaultValue`
- Trailing `_` for private members: `mu_`, `sigma_`

**Do NOT:**

- Create new files unless absolutely necessary
- Add emojis unless explicitly requested
- Write documentation files (code comments are sufficient)
- Over-comment obvious operations
- Specialize types in `std::` namespace (use extension points instead)

## Example: Compare Before/After

**Before:**

```cpp
auto prob(T x) const {
  if (x < a_ or x > b_) return 0.;
  return 1. / (b_ - a_);
}

auto logProb(T x) const {
  if (x < a_ or x > b_) return -std::numeric_limits<T>::infinity();
  return -log(b_ - a_);  // Missing std::
}

auto normalProb(T x) const {
  return exp(-(x*x)/2.0) / (sigma * sqrt(2.0 * 3.14159));  // Hardcoded pi
}

template <typename Generator>
auto sample(Generator& g) -> T {
  // Box-Muller inline with caching mixed together
  if (has_value_) return cached_value_;
  T u1 = g() / double(Generator::max());
  T u2 = g() / double(Generator::max());  // Integer division bug!
  T r = sqrt(-2. * log(u1));  // Raw literals
  cached_value_ = r * cos(2. * 3.14159 * u2);  // Inline comment on same line
  return r * sin(2. * 3.14159 * u2);
}
```

**After:**

```cpp
constexpr auto prob(T x) const -> T {
  if (x < a_ or x > b_) {
    return T{0};
  }
  return T{1} / (b_ - a_);
}

// Log-space avoids underflow for very small probabilities
constexpr auto logProb(T x) const -> T {
  using std::log;
  if (x < a_ or x > b_) {
    return -numeric_infinity(T{});
  }
  return -log(b_ - a_);
}

// Probability density function (PDF)
//
// Formula: p(x|μ,σ) = (1 / (σ√(2π))) exp(-x² / 2σ²)
constexpr auto normalProb(T x) const -> T {
  using std::exp;
  using std::sqrt;
  const T two_pi = T{2} * T{std::numbers::pi};
  return exp(-x * x / T{2}) / (sigma * sqrt(two_pi));
}

template <typename Generator>
constexpr auto sample(Generator& g) -> T {
  // Return cached value if available
  if (has_value_) {
    has_value_ = false;
    return std::exchange(cached_value_, T{});
  }

  // Generate a pair of standard normal samples
  auto [z0, z1] = boxMuller(g);

  // Cache second sample
  cached_value_ = z1;
  has_value_ = true;

  return z0;
}

private:
  // Box-Muller transform: converts uniform samples to Gaussian samples
  //
  // Generates two independent N(0,1) samples from two uniform U(0,1) samples:
  //   Z₀ = √(-2 ln U₁) cos(2π U₂)
  //   Z₁ = √(-2 ln U₁) sin(2π U₂)
  //
  // Why it works: In 2D, independent normal variables X,Y have polar coordinates
  // where R² ~ Exponential(1/2) and Θ ~ Uniform(0, 2π). The transform inverts
  // this relationship to generate normals from uniforms.
  template <typename Generator>
  constexpr auto boxMuller(Generator& g) -> std::pair<T, T> {
    // Avoid integer division - cast both terms to T
    constexpr auto range = static_cast<T>(Generator::max() - Generator::min());
    T u1 = static_cast<T>(g()) / range;
    T u2 = static_cast<T>(g()) / range;

    using std::cos;
    using std::log;
    using std::sin;
    using std::sqrt;

    // Radial component
    const T r = sqrt(-T{2} * log(u1));

    // Angular component
    const T theta = T{2} * T{std::numbers::pi} * u2;

    // Convert from polar to Cartesian
    return {r * sin(theta), r * cos(theta)};
  }
```

## Notes

- Focus on correctness over micro-optimization
- Incremental development: add missing functionality, don't work around it
- Test with both standard types (double) and custom types when applicable
- Verify constexpr support with `static_assert` in tests
