# Sophisticated Term Sorting Implementation

## Summary

Implemented algebraically-aware term sorting for symbolic3 canonical forms. This addresses the first major challenge in Recommendation #2: sorting terms like `x` and `3*x` so they are adjacent and can be combined.

## What Was Implemented

### 1. Term Structure Analysis (`src/symbolic3/term_structure.h`)

New utilities to extract algebraic structure from expressions:

**For Addition Terms:**

- `GetCoefficient<Term>`: Extracts the multiplicative coefficient

  - `x` → coefficient = 1
  - `3*x` → coefficient = 3
  - `5` → coefficient = 5

- `GetBase<Term>`: Extracts the non-coefficient part
  - `x` → base = x
  - `3*x` → base = x
  - `5` → base = 1 (pure constant)

**For Multiplication Terms:**

- `GetPowerBase<Term>`: Extracts the base of a power

  - `x` → base = x
  - `x^2` → base = x

- `GetPowerExponent<Term>`: Extracts the exponent
  - `x` → exponent = 1
  - `x^2` → exponent = 2

### 2. Algebraic Comparators

**`compareAdditionTerms(A, B)`**: Sorts addition terms by algebraic structure

- Pure constants (base=1) come first
- Then group by base (so `x` and `3*x` are adjacent)
- Within same base, sort by coefficient

Example ordering: `[5, 3, x, 3*x, y, 2*y]`

- Constants: `[5, 3]`
- x terms: `[x, 3*x]`
- y terms: `[y, 2*y]`

**`compareMultiplicationTerms(A, B)`**: Sorts multiplication terms

- Constants come first
- Then group by base (so `x` and `x^2` are adjacent)
- Within same base, sort by exponent

Example ordering: `[2, 3, x, x^2, x^3, y]`

- Constants: `[2, 3]`
- x powers: `[x, x^2, x^3]`
- y terms: `[y]`

### 3. Integration with Canonical Forms

Updated `src/symbolic3/canonical.h`:

```cpp
template <typename List>
struct SortForAddition {
  using type = ::tempura::SortTypes_t<List, AdditionTermComparator>;
};

template <typename List>
struct SortForMultiplication {
  using type = ::tempura::SortTypes_t<List, MultiplicationTermComparator>;
};
```

These replace the previous simple constant/non-constant partitioning with sophisticated algebraic sorting.

### 4. Comprehensive Tests

**`test/term_structure_test.cpp`**: Unit tests for structure extraction and comparison

- Coefficient extraction from various term forms
- Power structure extraction
- Comparison predicates

**`test/sophisticated_sorting_demo.cpp`**: Integration demo showing:

- Like terms becoming adjacent (`x` and `3*x`)
- Powers grouping (`x`, `x^2`, `x^3`)
- Constants grouping for easy combination
- Mixed term scenarios

## Key Benefits

1. **Like Terms Are Adjacent**: `x + 3*x + 2 + 4*x` sorts to `2 + x + 3*x + 4*x`

   - Reduction rule can now scan linearly: see two adjacent x terms → combine

2. **Powers Are Grouped**: `x^3 * x * x^2` sorts to `x * x^2 * x^3`

   - Reduction rule: see adjacent powers of same base → add exponents

3. **Constants First**: Easy to find and combine all constant terms

4. **Simpler Reduction Rules**: Instead of complex pattern matching across entire expression, rules can now be simple sequential scans

## How It Works

### Example: Addition Sorting

Input: `[y, 2, x, 3*x, 5]`

Analysis phase:

- `y` → coeff=1, base=y
- `2` → coeff=2, base=1 (constant)
- `x` → coeff=1, base=x
- `3*x` → coeff=3, base=x
- `5` → coeff=5, base=1 (constant)

Sorting:

1. Constants (base=1) come first: `[2, 5, ...]`
2. Group by base: `[..., x, 3*x, ..., y]`
3. Within groups, sort by coefficient

Result: `[2, 5, x, 3*x, y]`

### Example: Multiplication Sorting

Input: `[y, 2, x^2, 3, x]`

Analysis:

- `y` → base=y, exp=1
- `2` → constant
- `x^2` → base=x, exp=2
- `3` → constant
- `x` → base=x, exp=1

Sorting:

1. Constants first: `[2, 3, ...]`
2. Group by base: `[..., x, x^2, ..., y]`
3. Within groups, sort by exponent

Result: `[2, 3, x, x^2, y]`

## Implementation Details

### Design Decisions

1. **Type-based extraction**: Uses template metaprogramming to extract structure at compile-time

   - Zero runtime cost
   - Fully constexpr compatible
   - Works with tempura's value-based symbolic types

2. **Separate structure extractors from comparators**:

   - Extractors: `GetCoefficient`, `GetBase`, etc.
   - Comparators: `compareAdditionTerms`, `compareMultiplicationTerms`
   - This separation makes logic testable and composable

3. **Operation-specific sorting**: Addition and multiplication have different sorting strategies
   - Addition: group by base for term collection
   - Multiplication: group by base/exponent for power collection

### Edge Cases Handled

1. **Pure constants**: Treated as having base=1
2. **Pure symbols**: Treated as having implicit coefficient=1 or exponent=1
3. **Nested multiplication**: `2*x*y` correctly extracts coefficient=2
4. **Powers without explicit notation**: `x` treated as `x^1`

### Limitations (By Design)

1. **Binary tree structure**: Works with current Expression<Op, A, B> structure

   - Flattening to Expression<Op, A, B, C, ...> handled separately
   - Coefficient extraction from `2*x*y` depends on operator associativity

2. **Integer coefficients/exponents**: Currently assumes simple numeric constants

   - Symbolic coefficients/exponents not yet supported
   - Fractions work but not thoroughly tested

3. **No reduction**: This implementation ONLY sorts, doesn't combine
   - `[x, 3*x]` becomes sorted but stays as two terms
   - Reduction rules (combining to `4*x`) are next step

## Testing

All tests pass:

```bash
$ ninja -C build term_structure_test && ./build/src/symbolic3/term_structure_test
# All 11 tests pass ✓

$ ninja -C build sophisticated_sorting_demo && ./build/src/symbolic3/sophisticated_sorting_demo
# All 6 demonstration tests pass ✓
```

Key test scenarios:

- Coefficient extraction from various term forms
- Power structure extraction
- Like term comparison (x vs 3\*x)
- Constants-first ordering
- Mixed term sorting
- Power grouping

## Next Steps

This completes Step 1 of the three-phase plan:

- [x] **Phase 1: Sophisticated Sorting** (this implementation)

  - Term structure extractors
  - Algebraic comparators
  - Integration with canonical forms

- [ ] **Phase 2: Reduction Rules**

  - Combine like terms: `[x, 3*x] → [4*x]`
  - Combine constants: `[2, 3] → [5]`
  - Combine powers: `[x, x^2] → [x^3]`
  - Implement as rewrite strategies

- [ ] **Phase 3: Integration & Testing**
  - Integrate with full simplification pipeline
  - Test with real symbolic expressions
  - Benchmark compile-time performance
  - Document usage patterns

## Usage Example

```cpp
#include "symbolic3/canonical.h"
#include "symbolic3/term_structure.h"

Symbol x, y;

// Create list of terms
using Terms = TypeList<
    decltype(3_c * x),
    Constant<5>,
    decltype(x),
    decltype(y)
>;

// Sort for addition (like terms adjacent)
using Sorted = tempura::symbolic3::detail::SortForAddition_t<Terms>;
// Result: TypeList<Constant<5>, x, 3*x, y>

// After sorting, reduction rule can scan linearly:
// - See x followed by 3*x → combine to 4*x
// - Result: [5, 4*x, y]
```

## Files Added/Modified

**New Files:**

- `src/symbolic3/term_structure.h` - Core implementation
- `src/symbolic3/test/term_structure_test.cpp` - Unit tests
- `src/symbolic3/test/sophisticated_sorting_demo.cpp` - Integration demo
- `TERM_SORTING_DESIGN.md` - Design document

**Modified Files:**

- `src/symbolic3/canonical.h` - Updated sorting strategies
- `src/symbolic3/CMakeLists.txt` - Added new tests

## References

- Original design: `TERM_SORTING_DESIGN.md`
- Recommendation #2: `RECOMMENDATION_2_IMPLEMENTATION.md`
- Computer algebra algorithms: Geddes et al., "Algorithms for Computer Algebra"
- Existing infrastructure: `symbolic3/ordering.h`, `symbolic3/canonical.h`
