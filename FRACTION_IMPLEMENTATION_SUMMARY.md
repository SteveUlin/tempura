# Fraction Feature Implementation Summary

**Date:** October 12, 2025
**Status:** Partial Implementation Complete

## What Was Implemented

### ✅ Core Fraction Infrastructure (COMPLETE)

All the following are fully implemented and tested:

1. **`Fraction<N, D>` Template Type** (`src/symbolic3/core.h`)

   - Compile-time rational number representation
   - Automatic GCD reduction at construction
   - Sign normalization (negative always in numerator)
   - Zero runtime overhead

2. **Fraction Arithmetic** (`src/symbolic3/fraction.h`)

   - Addition, subtraction, multiplication, division
   - Negation operator
   - Mixed arithmetic with `Constant<>` types
   - All operations maintain exact arithmetic

3. **Comparison Operators** (`src/symbolic3/fraction.h`)

   - Equality, inequality
   - Less than, greater than, less-equal, greater-equal
   - Works correctly with reduced form guarantee

4. **Literal Suffix** (`src/symbolic3/fraction.h`)

   - `_frac` suffix for creating fractions: `1_frac / 2_frac → Fraction<1, 2>`
   - Common fraction constants: `half`, `third`, `quarter`, etc.

5. **String Conversion** (`src/symbolic3/to_string.h`)

   - Compile-time: `toString(Fraction<1, 2>{})` → `"1/2"`
   - Runtime: `toStringRuntime(Fraction<1, 2>{})` → `"1/2"`
   - Integer fractions (denominator = 1) display as integers

6. **Evaluation Support** (`src/symbolic3/evaluate.h`)

   - `evaluate(Fraction<N, D>{}, BinderPack{})` → `double`
   - Converts to double for numerical computation

7. **Ordering Integration** (`src/symbolic3/ordering.h`)

   - `compareFractions()` for canonical term ordering
   - Category ordering: Expression < Symbol < Fraction < Constant
   - Cross-multiplication comparison for efficiency

8. **Test Suite** (`src/symbolic3/test/fraction_test.cpp`)

   - 13 test cases covering all fraction operations
   - All tests passing ✅

9. **CMake Integration** (`src/symbolic3/test/CMakeLists.txt`)

   - fraction_test registered and labeled as `symbolic3;exact_math`

10. **Header Integration** (`src/symbolic3/symbolic3.h`)
    - `fraction.h` included in main header

## ❌ What's NOT Yet Implemented

### Automatic Division-to-Fraction Promotion

**Goal:** Make `Constant<5>{} / Constant<2>{}` automatically simplify to `Fraction<5, 2>` during simplification.

**Challenge:** C++ template return type deduction conflicts. The `PromoteDivisionToFraction` strategy needs to return:

- `Expression<DivOp, ...>` when pattern doesn't match
- `Constant<n/d>` when division is exact
- `Fraction<n, d>` when division is non-integer

This violates C++'s requirement for consistent `auto` return types.

**Attempted Solutions:**

1. ❌ Simple `if constexpr` branching - causes "auto deduced as different types" error
2. ❌ Helper function to isolate return type - still has multiple return paths

**Remaining Options:**

1. **Use SFINAE or Concepts to create separate overloads** - each overload has one consistent return type
2. **Use `std::variant` or similar type-erasing wrapper** - adds runtime overhead (not desirable)
3. **Manual application** - users call `promoteDivisionToFraction` explicitly when needed
4. **Rewrite as pattern-matching rule** - might need enhancement to pattern_matching.h

## How to Use Fractions NOW

### Manual Fraction Creation

```cpp
#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

// Method 1: Direct construction
constexpr auto half = Fraction<1, 2>{};
constexpr auto third = Fraction<1, 3>{};

// Method 2: Literal suffix
constexpr auto half2 = 1_frac / 2_frac;

// Method 3: Predefined constants
constexpr auto h = half;  // from fraction.h
constexpr auto t = third;
constexpr auto q = quarter;

// Arithmetic
constexpr auto sum = half + third;  // → Fraction<5, 6>
constexpr auto product = half * two_thirds;  // → Fraction<1, 3>

// Comparison
static_assert(third < half);
static_assert(Fraction<2, 4>{} == half);  // Auto-reduction

// Evaluation
auto numeric = evaluate(half, BinderPack{});  // → 0.5

// String conversion
std::string str = toStringRuntime(half);  // → "1/2"
```

### Symbolic Expressions with Fractions

```cpp
Symbol x;

// Build expression with fraction
auto expr = x * Fraction<1, 2>{};

// Or use predefined constants
auto expr2 = x * half + y * third;

// Simplify (fractions are preserved)
auto result = simplify(expr, default_context());
```

## Next Steps to Complete Automatic Promotion

### Recommendation: Use Rewrite Pattern Matching

Instead of a custom strategy, leverage the existing `Rewrite` infrastructure:

```cpp
// In simplify.h, add to constant folding section:

// Pattern for exact integer division
template <long long n, long long d>
  requires (n % d == 0)
constexpr auto exact_div_rule = Rewrite{
  Constant<n>{} / Constant<d>{},
  Constant<n / d>{}
};

// Pattern for non-integer division
template <long long n, long long d>
  requires (n % d != 0)
constexpr auto frac_div_rule = Rewrite{
  Constant<n>{} / Constant<d>{},
  Fraction<n, d>{}
};
```

**Challenge:** Requires parametric rewrite rules (template-based patterns), which may not be supported by current `Rewrite` implementation.

### Alternative: SFINAE-based Strategy

Create separate `apply` overloads for each case:

```cpp
struct PromoteDivisionToFraction {
  // Overload 1: Exact division
  template <long long n, long long d>
    requires (n % d == 0)
  constexpr auto apply(Expression<DivOp, Constant<n>, Constant<d>>, auto) const {
    return Constant<n / d>{};
  }

  // Overload 2: Non-integer division
  template <long long n, long long d>
    requires (n % d != 0)
  constexpr auto apply(Expression<DivOp, Constant<n>, Constant<d>>, auto) const {
    return Fraction<n, d>{};
  }

  // Overload 3: No match
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context) const {
    return expr;
  }
};
```

This should work because each overload has a single, consistent return type.

## Testing Plan

Once automatic promotion is implemented:

1. Run `src/symbolic3/test/exact_division_test.cpp` (currently disabled due to compilation errors)
2. Verify:
   - `Constant<6>{} / Constant<2>{}` → `Constant<3>`
   - `Constant<5>{} / Constant<2>{}` → `Fraction<5, 2>`
   - `Constant<4>{} / Constant<6>{}` → `Fraction<2, 3>` (auto-reduced)
   - Negative divisions normalize correctly
   - Integration with full simplification pipeline

## Documentation Needs

Once complete, update:

1. **`NEXT_STEPS.md`** - Move P0 items from TODO to DONE
2. **`README.md`** - Add fraction usage examples
3. **API Reference** - Document fraction creation and operations
4. **Examples** - Create `examples/fraction_demo.cpp`

## Files Modified

- ✅ `src/symbolic3/core.h` - Added `Fraction<N, D>`, GCD helpers, `is_fraction` predicate
- ✅ `src/symbolic3/fraction.h` - Full arithmetic, operators, literals
- ✅ `src/symbolic3/to_string.h` - String conversion support
- ✅ `src/symbolic3/evaluate.h` - Evaluation to double
- ✅ `src/symbolic3/ordering.h` - Comparison and ordering
- ✅ `src/symbolic3/symbolic3.h` - Include fraction.h
- ✅ `src/symbolic3/test/fraction_test.cpp` - Comprehensive test suite
- ✅ `src/symbolic3/test/CMakeLists.txt` - Test registration
- ⚠️ `src/symbolic3/simplify.h` - Partial (exact_constant_fold stub)
- ⚠️ `src/symbolic3/test/exact_division_test.cpp` - Created but doesn't compile

## Conclusion

**The fraction infrastructure is complete and fully functional.** Users can create and manipulate fractions manually with full compile-time support. What remains is the "quality of life" feature of automatic division-to-fraction promotion during simplification.

This is a C++ template metaprogramming challenge, not a design issue. The solution requires either:

1. SFINAE/Concepts-based function overloading
2. Enhancement to the pattern matching system
3. Or accepting manual fraction creation as the primary workflow

**Recommendation:** Implement SFINAE-based approach (Option 1 above) as it's most aligned with tempura's template-heavy architecture.
