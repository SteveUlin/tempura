# Term-Structure-Aware Ordering Implementation

## Date: October 12, 2025

## Summary

Enhanced addition simplification rules to use **algebraic term structure** instead of plain lexicographic ordering. This enables intelligent grouping of like terms (e.g., `3*x`, `x`, `2*x`) for efficient term collection and factoring.

## The Problem

**Before:** Using plain `lessThan()` comparison, terms were ordered lexicographically:
```cpp
3*x + y + 2*y + x
```
Would order as: `2*y < 3*x < x < y` (arbitrary type-based ordering)

**Issue:** Like terms (terms with the same base) were scattered, making factoring inefficient:
- `3*x` and `x` not adjacent → hard to factor
- `y` and `2*y` not adjacent → hard to factor

## The Solution

**After:** Using `compareAdditionTerms()` from `term_structure.h`, terms are ordered by:
1. **Constants first** (pure numbers like `5`, `7`)
2. **Group by base** (all `x` terms together, all `y` terms together)
3. **Within group, sort by coefficient** (`x < 2*x < 3*x`)

Example transformation:
```cpp
3*x + y + 2*y + x
→ x + 3*x + y + 2*y  (grouped by base)
→ 4*x + 3*y          (after factoring)
```

## Implementation Changes

### 1. Added `term_structure.h` Include

```cpp
#include "symbolic3/term_structure.h"
```

### 2. Updated Canonical Ordering Rule

**Before:**
```cpp
constexpr auto canonical_order =
    Rewrite{y_ + x_, x_ + y_, [](auto ctx) {
              return lessThan(get(ctx, x_), get(ctx, y_));
            }};
```

**After:**
```cpp
constexpr auto canonical_order =
    Rewrite{y_ + x_, x_ + y_, [](auto ctx) {
              return compareAdditionTerms(get(ctx, x_), get(ctx, y_)) ==
                     Ordering::Less;
            }};
```

**Key Addition:** Now uses `compareAdditionTerms()` which understands algebraic structure:
- Groups terms by base (variable part)
- Sorts by coefficient within each group
- Places constants first

### 3. Updated Associativity Rules

All three associativity rules now use term-structure-aware comparison:

**`assoc_left`:**
```cpp
constexpr auto assoc_left = Rewrite{a_ + (b_ + c_), (a_ + b_) + c_, [](auto ctx) {
  return compareAdditionTerms(get(ctx, b_), get(ctx, a_)) != Ordering::Less;
}};
```

**`assoc_right`:**
```cpp
constexpr auto assoc_right = Rewrite{(a_ + c_) + b_, a_ + (c_ + b_), [](auto ctx) {
  return compareAdditionTerms(get(ctx, b_), get(ctx, c_)) == Ordering::Less;
}};
```

**`assoc_reorder`:**
```cpp
constexpr auto assoc_reorder = Rewrite{a_ + (c_ + b_), a_ + (b_ + c_), [](auto ctx) {
  return compareAdditionTerms(get(ctx, b_), get(ctx, c_)) == Ordering::Less;
}};
```

## How Term-Structure Comparison Works

From `term_structure.h`:

### Coefficient and Base Extraction

For addition terms, we extract:
- **Coefficient**: the numeric factor (default `1` if not present)
- **Base**: the variable part

Examples:
- `x` → coefficient=`1`, base=`x`
- `3*x` → coefficient=`3`, base=`x`
- `2*y` → coefficient=`2`, base=`y`
- `5` → coefficient=`5`, base=`1` (pure constant)

### Comparison Strategy

```cpp
constexpr auto compareAdditionTerms(A, B) -> Ordering {
  // 1. Pure constants (base=1) come first
  if (A_is_pure_const && !B_is_pure_const) return Ordering::Less;
  
  // 2. Compare bases (group like terms)
  auto base_cmp = compare(A_base, B_base);
  if (base_cmp != Equal) return base_cmp;
  
  // 3. Same base: compare coefficients (smaller first)
  return compare(A_coeff, B_coeff);
}
```

## Test Results

Created comprehensive test suite: `term_ordering_test.cpp`

### Test Coverage

✅ **Basic term comparison:**
- Constants before variables
- Grouping by base
- Coefficient ordering within groups

✅ **Canonical ordering:**
- `2*x + x → x + 2*x`
- Mixed bases handled correctly

✅ **Associativity with term structure:**
- `x + (2*x + y)` groups correctly
- Multiple bases maintained

✅ **Full simplification examples:**
- `3*x + y + 2*x + y → 5*x + 2*y` ✓
- `x + 3*x + 2*x → 6*x` ✓
- `2*y + x + 3*x + y → 4*x + 3*y` ✓
- `5 + 2*x + 3 + x → 8 + 3*x` ✓
- Complex: `3*x + 2*y + x + 5 + 4*x + y + 2 → 7 + 8*x + 3*y` ✓

### Test Execution

```bash
$ ./build/src/symbolic3/term_ordering_test

Running... Term comparison: constants come first
Running... Term comparison: group by base
Running... Term comparison: different bases sorted separately
Running... Canonical ordering: 2*x + x → x + 2*x
Running... Canonical ordering: y + x when bases differ
Running... Associativity groups like terms: x + (2*x + y)
Running... Associativity with different bases
Running... Full simplify: 3*x + y + 2*x + y → 5*x + 2*y
Running... Full simplify: x + 3*x + 2*x → 6*x
Running... Full simplify: 2*y + x + 3*x + y → 4*x + 3*y
Running... Full simplify with constants: 5 + 2*x + 3 + x → 8 + 3*x
Running... Complex expression: 3*x + 2*y + x + 5 + 4*x + y + 2

✓ All term-structure-aware ordering tests passed!
  Terms are now grouped by their algebraic base,
  enabling efficient term collection and factoring.
```

All existing tests also pass:
- ✓ `simplification_basic_test` - 12/12 tests
- ✓ `simplification_pipeline_test` - 18/18 tests
- ✓ `simplification_advanced_test` - (transcendental functions)

## Benefits

### 1. **Automatic Like-Term Grouping**

Terms with the same algebraic base are automatically adjacent:
```
3*x + y + x + 2*y  →  x + 3*x + y + 2*y
```

### 2. **Efficient Factoring**

Grouped terms enable straightforward factoring rules:
```
x + 3*x  →  4*x
y + 2*y  →  3*y
```

### 3. **Consistent with Canonical Forms**

Aligns with the flattening strategy in `canonical.h`:
- Both use `AdditionTermComparator`
- Both group by base, sort by coefficient
- Unified approach across the codebase

### 4. **Natural Mathematical Ordering**

Results match human intuition:
```
5 + x + 2*x + y + 3*y
```
Constants, then x terms, then y terms, each internally sorted.

## Comparison with canonical.h

The `canonical.h` file implements a similar strategy for **type-level** canonical forms:
- **Flattens**: `(a+b)+c` → `Expression<Add, a, b, c>`
- **Sorts**: Uses `AdditionTermComparator` at type level

Our `simplify.h` rules now implement the **value-level** equivalent:
- **Reorders**: Binary expressions based on term structure
- **Groups**: Like terms using same `compareAdditionTerms` logic

**Complementary approaches:**
- `canonical.h`: Type system (compile-time structure)
- `simplify.h`: Rewrite rules (runtime/compile-time transformations)

Both use the same underlying term structure analysis from `term_structure.h`.

## Performance Implications

### Positive

- **Fewer rewrite steps**: Like terms already adjacent after ordering
- **Better factoring**: One pass can factor all terms with same base
- **Predictable behavior**: Deterministic ordering reduces rule applications

### Neutral

- **Slightly more complex comparison**: Extracts coefficient/base structure
- **Still constexpr**: All comparisons work at compile-time
- **No runtime overhead**: Pure template metaprogramming

## Future Work

### Potential Enhancements

1. **Multiplication term ordering**: Apply similar strategy
   - Group by base: `x`, `x^2`, `x^3` adjacent
   - Sort by exponent within groups

2. **Nested expression handling**: Extend to nested structures
   - `(2*x + y) + (x + 3*y)` → flatten and group

3. **Coefficient collection**: Smarter constant folding
   - Recognize `2*x + 3*x` pattern more aggressively
   - Combine with evaluation strategy

4. **Documentation**: Add visual examples
   - Step-by-step transformation diagrams
   - Comparison trees showing decision process

## Files Modified

- `src/symbolic3/simplify.h`:
  - Added `term_structure.h` include
  - Updated `canonical_order` rule (line ~208)
  - Updated all 3 associativity rules (lines ~245-265)
  - Enhanced documentation throughout

## Files Added

- `src/symbolic3/test/term_ordering_test.cpp`:
  - Comprehensive test suite for term-structure-aware ordering
  - 12 test cases covering basic to complex scenarios
  - Evaluation-based verification

- Updated `src/symbolic3/CMakeLists.txt`:
  - Added `term_ordering_test` target
  - Test labeled with `symbolic3;term_structure;simplification`

## Conclusion

The addition simplification rules now use **algebraic term structure** for ordering, matching the sophisticated strategy in `canonical.h`. This enables:

✓ Automatic grouping of like terms (`x`, `2*x`, `3*x` adjacent)  
✓ Efficient term collection and factoring  
✓ Natural mathematical ordering (constants first, then by base)  
✓ Consistency with canonical form infrastructure  
✓ All existing tests pass  
✓ New comprehensive test suite validates behavior  

The implementation demonstrates the power of compile-time term structure analysis for building intelligent symbolic manipulation systems.
