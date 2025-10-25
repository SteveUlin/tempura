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

**Quality Improvements:**
- Make all methods `constexpr` where possible
- Add parameter validation in constructor (use `assert`)
- Add standard methods if missing: `mean()`, `variance()`, parameter accessors
- Add explicit return types: `-> T`

**Comments:**
- Short and judicious - explain *why*, not *what*
- Explain non-obvious design decisions (inverse transform, numerical stability, edge cases)
- Highlight common pitfalls (integer division traps, underflow concerns)
- Do NOT repeat what the code obviously does
- Do NOT speak down to the reader

**Documentation Header:**
```cpp
// [Distribution Name] distribution [Mathematical notation]
//
// Requirements for custom types T:
//   - Arithmetic operators: +, -, *, / (both binary and unary +/-)
//   - Comparison: <, >, ==, != (as needed)
//   - Convertible from integer literals (0, 1, 2, ...)
//
// For [special methods] support, provide these extension points (found via ADL):
//   namespace mylib {
//     constexpr auto log(MyType x) -> MyType { ... }
//     constexpr auto numeric_infinity(MyType) -> MyType { ... }
//     // ... other functions as needed
//   }
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
ninja -C build bayes_[distribution]_test
./build/src/bayes/bayes_[distribution]_test
ctest --test-dir build -L bayes --output-on-failure
```

All tests must pass before proceeding.

### 5. Check-in Before Commit

**Present a summary:**
- Critical bugs fixed (list them)
- Quality improvements (list them)
- Test coverage (number of tests, what they cover)
- All tests passing ✓

**Ask:** "Ready to commit these changes?"

**If approved, commit:**
```bash
git add src/bayes/[distribution].h src/bayes/[distribution]_test.cpp src/bayes/CMakeLists.txt
git commit -m "Fix [distribution] bugs, improve type safety and add comprehensive tests"
```

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
```

## Notes

- Focus on correctness over micro-optimization
- Incremental development: add missing functionality, don't work around it
- Test with both standard types (double) and custom types when applicable
- Verify constexpr support with `static_assert` in tests
