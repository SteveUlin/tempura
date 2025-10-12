# Multiplication Term-Structure-Aware Ordering Implementation

## Date: October 12, 2025

## Summary

Enhanced multiplication simplification rules to use **algebraic term structure** (power-based) instead of plain lexicographic ordering. This enables intelligent grouping of like bases (e.g., `x`, `x^2`, `x^3`) for efficient power combining.

## The Problem

**Before:** Using plain `lessThan()` comparison, terms were ordered lexicographically:
```cpp
x^3 · y · x · y^2 · x^2
```
Would order arbitrarily based on expression type structure.

**Issue:** Powers of the same base were scattered, making power combining inefficient:
- `x`, `x^2`, `x^3` not adjacent → hard to combine into `x^6`
- `y`, `y^2` not adjacent → hard to combine into `y^3`

## The Solution

**After:** Using `compareMultiplicationTerms()` from `term_structure.h`, terms are ordered by:
1. **Constants first** (pure numbers like `2`, `5`)
2. **Group by base** (all `x` powers together, all `y` powers together)
3. **Within group, sort by exponent** (`x < x^2 < x^3`)

Example transformation:
```cpp
x^3 · y · x · y^2 · x^2
→ x · x^2 · x^3 · y · y^2  (grouped by base)
→ x^6 · y^3                (after power combining)
```

## Implementation Changes

### 1. Enhanced Documentation Structure

Added comprehensive section header with rule categories overview:
```cpp
// Rule Categories (applied in this order):
//   1. Identity      : 0·x → 0, 1·x → x
//   2. Distribution  : (a+b)·c → a·c + b·c
//   3. Ordering      : y·x → x·y  (when x < y, by base then exponent)
//   4. PowerCombining: x·x^a → x^(a+1)
//   5. Associativity : Strategic reassociation to group like bases
```

Added visual separators for each rule category (like Addition rules).

### 2. Updated Canonical Ordering Rule

**Before:**
```cpp
constexpr auto canonical_order =
    Rewrite{y_ * x_, x_ * y_,
            [](auto ctx) { return lessThan(get(ctx, x_), get(ctx, y_)); }};
```

**After:**
```cpp
constexpr auto canonical_order =
    Rewrite{y_ * x_, x_ * y_, [](auto ctx) {
              return compareMultiplicationTerms(get(ctx, x_), get(ctx, y_)) ==
                     Ordering::Less;
            }};
```

**Key Addition:** Now uses `compareMultiplicationTerms()` which understands power structure:
- Groups terms by base (variable part)
- Sorts by exponent within each group
- Places constants first

### 3. Updated Associativity Rules

All three associativity rules now use term-structure-aware comparison:

**`assoc_left`:**
```cpp
constexpr auto assoc_left = Rewrite{a_ * (b_ * c_), (a_ * b_) * c_, [](auto ctx) {
  return compareMultiplicationTerms(get(ctx, b_), get(ctx, a_)) != Ordering::Less;
}};
```

**`assoc_right`:**
```cpp
constexpr auto assoc_right = Rewrite{(a_ * c_) * b_, a_ * (c_ * b_), [](auto ctx) {
  return compareMultiplicationTerms(get(ctx, b_), get(ctx, c_)) == Ordering::Less;
}};
```

**`assoc_reorder`:**
```cpp
constexpr auto assoc_reorder = Rewrite{a_ * (c_ * b_), a_ * (b_ * c_), [](auto ctx) {
  return compareMultiplicationTerms(get(ctx, b_), get(ctx, c_)) == Ordering::Less;
}};
```

### 4. Maintained `a < b < c` Canonical Ordering

All rules now consistently enforce the `a < b < c` pattern established in the addition rules refactoring.

## How Multiplication Term-Structure Comparison Works

From `term_structure.h`:

### Base and Exponent Extraction

For multiplication terms, we extract:
- **Base**: the variable part (default variable if not a power)
- **Exponent**: the power (default `1` if not present)

Examples:
- `x` → base=`x`, exponent=`1`
- `x^2` → base=`x`, exponent=`2`
- `y^3` → base=`y`, exponent=`3`
- `5` → base=`5`, exponent=`1` (constant treated as base)

### Comparison Strategy

```cpp
constexpr auto compareMultiplicationTerms(A, B) -> Ordering {
  // 1. Constants come first
  if (A_is_const && !B_is_const) return Ordering::Less;
  
  // 2. Compare bases (group like terms)
  auto base_cmp = compare(A_base, B_base);
  if (base_cmp != Equal) return base_cmp;
  
  // 3. Same base: compare exponents (lower exponent first)
  return compare(A_exp, B_exp);
}
```

## Test Results

Created comprehensive test suite: `mult_ordering_test.cpp`

### Test Coverage

✅ **Basic term comparison:**
- Constants before variables
- Grouping by base
- Exponent ordering within groups

✅ **Canonical ordering:**
- `x^2 · x → x · x^2`
- Mixed bases handled correctly

✅ **Associativity with term structure:**
- `x · (x^2 · y)` groups correctly
- Multiple bases maintained

✅ **Power combining:**
- `x · x → x^2` ✓
- `x · x^2 → x^3` ✓
- `x^2 · x^3 → x^5` ✓

✅ **Full simplification examples:**
- `x^3 · x · x^2 → x^6` ✓
- `x^2 · y · x · y^2 → x^3 · y^3` ✓
- `2 · x · 3 · x^2 → 6 · x^3` ✓
- `(x · x^2) · (y + y^2)` distributes correctly ✓
- Complex: `2 · x^2 · y · 3 · x · y^3 · x^2 → 6 · x^5 · y^4` ✓

### Test Execution

```bash
$ ./build/src/symbolic3/mult_ordering_test

Running... Multiplication term comparison: constants come first
Running... Multiplication term comparison: group by base
Running... Multiplication term comparison: different bases sorted separately
Running... Canonical ordering: x^2 · x → x · x^2
Running... Canonical ordering: y · x when bases differ
Running... Associativity groups like bases: x · (x^2 · y)
Running... Associativity with different bases
Running... Power combining: x · x → x^2
Running... Power combining: x · x^2 → x^3
Running... Power combining: x^2 · x^3 → x^5
Running... Full simplify: x^3 · x · x^2 → x^6
Running... Full simplify: x^2 · y · x · y^2 → x^3 · y^3
Running... Full simplify: 2 · x · 3 · x^2 → 6 · x^3
Running... Full simplify with addition: (x · x^2) · (y + y^2)
Running... Complex expression: 2 · x^2 · y · 3 · x · y^3 · x^2

✓ All multiplication term-structure-aware ordering tests passed!
  Terms are now grouped by their algebraic base (powers),
  enabling efficient power combining and simplification.
```

All existing tests also pass:
- ✓ `simplification_basic_test` - 12/12 tests
- ✓ `simplification_pipeline_test` - 18/18 tests
- ✓ `simplification_advanced_test` - 29/29 tests
- ✓ `term_ordering_test` - 12/12 tests (addition)
- ✓ `mult_ordering_test` - 15/15 tests (multiplication) **NEW**

## Benefits

### 1. **Automatic Like-Base Grouping**

Powers with the same base are automatically adjacent:
```
x^3 · y · x · y^2 · x^2  →  x · x^2 · x^3 · y · y^2
```

### 2. **Efficient Power Combining**

Grouped powers enable straightforward combining rules:
```
x · x^2 · x^3  →  x^6
y · y^2        →  y^3
```

### 3. **Consistent with Addition Rules**

Both addition and multiplication now use term-structure-aware ordering:
- **Addition**: Groups by coefficient × base → `x + 2*x + 3*x → 6*x`
- **Multiplication**: Groups by base^exponent → `x · x^2 · x^3 → x^6`

### 4. **Natural Mathematical Ordering**

Results match human intuition:
```
2 · 3 · x · x^2 · y · y^3
```
Constants first, then x powers, then y powers, each internally sorted.

### 5. **Distribution Interacts Correctly**

Power grouping works with distribution:
```
x · x^2 · (y + y^2)
→ x^3 · (y + y^2)      (combine x powers)
→ x^3 · y + x^3 · y^2  (distribute)
```

## Comparison with Addition Rules

| Aspect | Addition Rules | Multiplication Rules |
|--------|---------------|---------------------|
| **Term Structure** | coefficient × base | base^exponent |
| **Grouping Strategy** | Same base (variable) | Same base (power) |
| **Within-Group Sort** | By coefficient (smaller first) | By exponent (smaller first) |
| **Example Grouping** | `x, 2*x, 3*x` → `x, 2*x, 3*x` | `x, x^2, x^3` → `x, x^2, x^3` |
| **Combining Rule** | Factoring: `x + 2*x → 3*x` | Power combining: `x · x^2 → x^3` |
| **Comparison Function** | `compareAdditionTerms()` | `compareMultiplicationTerms()` |

Both use the same underlying framework from `term_structure.h`.

## Implementation Consistency

### Matching Pattern with Addition

The multiplication rules now follow the exact same structure as addition:

1. **Visual separators** for rule categories
2. **Comprehensive documentation** of ordering strategy
3. **Term-structure-aware comparison** in all ordering-dependent rules
4. **Consistent `a < b < c` pattern** in associativity rules
5. **Detailed comments** explaining each rule's purpose

### Code Quality Improvements

- **Readability**: Clear section headers and inline documentation
- **Maintainability**: Consistent patterns across addition and multiplication
- **Testability**: Comprehensive test suite for validation
- **Correctness**: All existing tests continue to pass

## Performance Implications

### Positive

- **Fewer rewrite steps**: Like bases already adjacent after ordering
- **Better power combining**: One pass can combine all powers of same base
- **Predictable behavior**: Deterministic ordering reduces rule applications

### Neutral

- **Slightly more complex comparison**: Extracts base/exponent structure
- **Still constexpr**: All comparisons work at compile-time
- **No runtime overhead**: Pure template metaprogramming

## Files Modified

- `src/symbolic3/simplify.h`:
  - Updated `canonical_order` rule in MultiplicationRuleCategories (line ~310)
  - Updated all 3 associativity rules (lines ~385-403)
  - Enhanced documentation throughout multiplication section
  - Added comprehensive rule category overview
  - Added visual separators matching addition style

## Files Added

- `src/symbolic3/test/mult_ordering_test.cpp`:
  - Comprehensive test suite for multiplication term-structure-aware ordering
  - 15 test cases covering basic to complex scenarios
  - All tests pass ✓

- Updated `src/symbolic3/CMakeLists.txt`:
  - Added `mult_ordering_test` target
  - Test labeled with `symbolic3;term_structure;simplification`

## Conclusion

The multiplication simplification rules now use **algebraic term structure** for ordering, matching both:
1. The sophisticated strategy in `canonical.h`
2. The addition rules we just refactored

This enables:

✓ Automatic grouping of like bases (`x`, `x^2`, `x^3` adjacent)  
✓ Efficient power combining (`x · x^2 · x^3 → x^6`)  
✓ Natural mathematical ordering (constants first, then by base and exponent)  
✓ Consistency across addition and multiplication rules  
✓ All existing tests pass  
✓ New comprehensive test suite validates behavior  

Both addition and multiplication now share a unified approach to term-structure-aware ordering, demonstrating the power of compile-time algebraic analysis for building sophisticated symbolic manipulation systems.

## Next Steps

Potential future enhancements:

1. **Apply same pattern to Power rules**: Group nested powers intelligently
2. **Extend to other operations**: Division (as multiplication by reciprocal)
3. **Cross-operation optimization**: Recognize patterns across + and ·
4. **Documentation improvements**: Visual diagrams of transformation pipelines
5. **Performance benchmarks**: Measure compile-time improvements

The foundation is now in place for systematic, structure-aware symbolic simplification across all algebraic operations.
