# Symbolic3: Sophisticated Term Sorting for Canonical Forms

## Problem

To enable term collection in expressions like `x + 3*x → 4*x`, we need terms with the same algebraic "base" to be adjacent after sorting. Simple type-based ordering doesn't achieve this because `x` and `3*x` have different types.

## Solution

Implemented **algebraically-aware sorting** that extracts the mathematical structure of terms before comparison:

### For Addition: `coefficient × base`

- `x` → coefficient=1, base=x
- `3*x` → coefficient=3, base=x
- Sorting strategy: group by base, then sort by coefficient
- Result: `x` and `3*x` become adjacent

### For Multiplication: `base^exponent`

- `x` → base=x, exponent=1
- `x^2` → base=x, exponent=2
- Sorting strategy: group by base, then sort by exponent
- Result: `x`, `x^2`, `x^3` become adjacent

## Implementation

### Core Components

**1. Term Structure Extractors** (`src/symbolic3/term_structure.h`)

```cpp
template<Symbolic Term>
using GetCoefficient<Term>;  // For addition terms

template<Symbolic Term>
using GetBase<Term>;         // Non-coefficient part

template<Symbolic Term>
using GetPowerBase<Term>;    // For multiplication terms

template<Symbolic Term>
using GetPowerExponent<Term>;
```

**2. Algebraic Comparators**

```cpp
constexpr auto compareAdditionTerms(A, B) -> Ordering;
constexpr auto compareMultiplicationTerms(A, B) -> Ordering;

struct AdditionTermComparator { ... };
struct MultiplicationTermComparator { ... };
```

**3. Integration with Canonical Forms** (`src/symbolic3/canonical.h`)

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

## Examples

### Addition Sorting

```
Input:  [y, 2, x, 3*x, 5]

Analysis:
  y    → coeff=1, base=y
  2    → coeff=2, base=1 (constant)
  x    → coeff=1, base=x
  3*x  → coeff=3, base=x
  5    → coeff=5, base=1 (constant)

Output: [2, 5, x, 3*x, y]
        ^^^^^ ^^^^^^  ^
        const x terms y

Now reduction rules can scan linearly:
  - [2, 5] → 7
  - [x, 3*x] → 4*x
  - [y] stays as is
Final: 7 + 4*x + y
```

### Multiplication Sorting

```
Input:  [y, 2, x^2, 3, x]

Analysis:
  y   → base=y, exp=1
  2   → constant
  x^2 → base=x, exp=2
  3   → constant
  x   → base=x, exp=1

Output: [2, 3, x, x^2, y]
        ^^^^^ ^^^^^^^^ ^
        const x powers y

Reduction:
  - [2, 3] → 6
  - [x, x^2] → x^(1+2) = x^3
  - [y] stays
Final: 6 * x^3 * y
```

## Testing

**Unit Tests** (`test/term_structure_test.cpp`):

- Coefficient extraction: x, 3*x, 5, 2*x\*y
- Power extraction: x, x^2
- Comparison predicates: like terms, constants-first, different bases

**Integration Demo** (`test/sophisticated_sorting_demo.cpp`):

- Like terms adjacency
- Power grouping
- Mixed term scenarios
- Real-world examples

All 17 symbolic3 tests pass ✓

## Impact

### Before (Simple Ordering)

```
x + 3*x + 2
→ might sort to [x, 3*x, 2] or [2, x, 3*x]
→ requires complex pattern matching to find like terms
```

### After (Algebraic Ordering)

```
x + 3*x + 2
→ always sorts to [2, x, 3*x]
→ like terms are adjacent
→ simple linear scan can combine them
```

## Next Steps

This completes **Phase 1** of Recommendation #2:

- [x] **Phase 1: Sophisticated Sorting** ← This implementation

  - ✅ Term structure extraction
  - ✅ Algebraic comparators
  - ✅ Integration with canonical forms
  - ✅ Comprehensive tests

- [ ] **Phase 2: Reduction Rules**

  - Combine like terms: `[x, 3*x] → [4*x]`
  - Combine constants: `[2, 3] → [5]`
  - Combine powers: `[x, x^2] → [x^3]`

- [ ] **Phase 3: Full Integration**
  - Integrate with simplification pipeline
  - Performance testing
  - Documentation

## Files

**New:**

- `src/symbolic3/term_structure.h`
- `src/symbolic3/test/term_structure_test.cpp`
- `src/symbolic3/test/sophisticated_sorting_demo.cpp`
- `TERM_SORTING_DESIGN.md`
- `SOPHISTICATED_SORTING_IMPLEMENTATION.md`

**Modified:**

- `src/symbolic3/canonical.h`
- `src/symbolic3/CMakeLists.txt`

## Design Principles

1. **Compile-time only**: Zero runtime cost
2. **Type-based**: Works with tempura's value-based symbolic types
3. **Composable**: Extractors separate from comparators
4. **Operation-specific**: Addition and multiplication have different strategies
5. **Test-driven**: Every component has unit tests

## References

- Design doc: `TERM_SORTING_DESIGN.md`
- Implementation: `SOPHISTICATED_SORTING_IMPLEMENTATION.md`
- Original recommendation: `RECOMMENDATION_2_IMPLEMENTATION.md`
- Computer algebra: Geddes et al., "Algorithms for Computer Algebra"
