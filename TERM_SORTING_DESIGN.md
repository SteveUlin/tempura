# Sophisticated Term Sorting for Symbolic3

## Problem Statement

To enable term collection in expressions like `x + 3*x`, we need to sort terms so that like terms are adjacent. This is more subtle than simple type ordering because:

1. **`x` and `3*x` should be adjacent** - they have the same "base" (x)
2. **`x*y` and `y*x` should be the same** - commutativity
3. **`x^2` and `x` should be adjacent** - same base, different powers

Current `lessThan` ordering is too simplistic - it just compares type structure, not algebraic meaning.

## Algebraic Structure

### Addition Terms

A sum term has the form: `coefficient * base`

Examples:

- `x` → coefficient=1, base=x
- `3*x` → coefficient=3, base=x
- `2*x*y` → coefficient=2, base=x\*y
- `5` → coefficient=5, base=1

**Sorting strategy for addition:**

1. Group by base (using term_base comparison)
2. Within same base, sort by coefficient
3. Constants (base=1) come first

### Multiplication Terms

A product term has the form: `base^exponent`

Examples:

- `x` → base=x, exponent=1
- `x^2` → base=x, exponent=2
- `x*y` → flattens to multiple bases
- `2` → base=2, exponent=1 (constant)

**Sorting strategy for multiplication:**

1. Constants first
2. Group by base
3. Within same base, sort by exponent
4. Multiple bases: sort lexicographically

## Implementation Strategy

### Step 1: Extract Algebraic Structure

```cpp
// For addition terms: extract coefficient and base
// x → (1, x)
// 3*x → (3, x)
// 2*x*y → (2, x*y)
template<Symbolic Term>
struct AdditionTermStructure {
  using coefficient = ...; // Constant type or 1
  using base = ...;        // Non-coefficient part
};

// For multiplication terms: extract base and exponent
// x → (x, 1)
// x^2 → (x, 2)
// 2*x → (2, 1) and (x, 1) - multiple factors
template<Symbolic Term>
struct MultiplicationTermStructure {
  // Returns list of (base, exponent) pairs
};
```

### Step 2: Comparison Functions

```cpp
// Compare addition terms by their structure
template<Symbolic A, Symbolic B>
constexpr auto compareAdditionTerms(A, B) -> Ordering {
  using A_struct = AdditionTermStructure<A>;
  using B_struct = AdditionTermStructure<B>;

  // First compare bases
  auto base_cmp = compare(A_struct::base{}, B_struct::base{});
  if (base_cmp != Ordering::Equal) return base_cmp;

  // Same base: compare coefficients
  return compare(A_struct::coefficient{}, B_struct::coefficient{});
}

// Compare multiplication terms by their structure
template<Symbolic A, Symbolic B>
constexpr auto compareMultiplicationTerms(A, B) -> Ordering {
  // Compare as lists of (base, exponent) pairs
  // Constants come first, then alphabetically by base
};
```

### Step 3: Specialized Sorting

Update `canonical.h` to use algebraic-aware sorting:

```cpp
template <typename List>
struct SortForAddition {
  using type = SortTypes_t<List, AdditionTermComparator>;
};

template <typename List>
struct SortForMultiplication {
  using type = SortTypes_t<List, MultiplicationTermComparator>;
};
```

## Examples

### Addition Sorting

Input: `[x, 3*x, y, 2*y, 5]`

Analysis:

- `5` → coeff=5, base=1
- `x` → coeff=1, base=x
- `3*x` → coeff=3, base=x
- `y` → coeff=1, base=y
- `2*y` → coeff=2, base=y

Sorted by base, then coefficient:
`[5, x, 3*x, y, 2*y]`

After term collection:
`5 + 4*x + 3*y`

### Multiplication Sorting

Input: `[x, 2, y, x^2, 3]`

Analysis:

- `2` → constant
- `3` → constant
- `x` → base=x, exp=1
- `x^2` → base=x, exp=2
- `y` → base=y, exp=1

Sorted:
`[2, 3, x, x^2, y]`

After term collection:
`6 * x^3 * y`

## Edge Cases

1. **Mixed constants and variables**: `2 + x + 3` → `5 + x`
2. **Implicit coefficients**: `x + x` (both have implicit coefficient 1)
3. **Nested expressions**: `(x+y) + 2*(x+y)` → `3*(x+y)`
4. **Non-commutative ops**: Don't sort Pow, Sub, Div arguments
5. **Complex bases**: `x*y` as a base in `2*x*y + 3*x*y`

## Implementation Plan

1. **Phase 1**: Implement term structure extractors

   - `getCoefficient(term)` → returns constant or 1
   - `getBase(term)` → returns non-coefficient part
   - Handle edge cases (pure constants, pure symbols)

2. **Phase 2**: Implement specialized comparators

   - `AdditionTermComparator` using structure
   - `MultiplicationTermComparator` using structure
   - Add tests for each case

3. **Phase 3**: Integrate with canonical form

   - Replace `SortForAddition` implementation
   - Replace `SortForMultiplication` implementation
   - Verify with term collection tests

4. **Phase 4**: Add reduction rules (separate from sorting)
   - Collect like terms: `[x, 3*x]` → `4*x`
   - Combine constants: `[2, 3]` → `5`
   - Will be implemented as rewrite strategies

## Non-Goals (For Now)

- Polynomial term ordering (graded lexicographic, etc.) - overkill for now
- Symbolic exponents - assume integer exponents
- Non-integer coefficients - assume integer or simple fractions
- Multi-variable polynomial canonical forms - just basic grouping

## Testing Strategy

Critical test cases:

```cpp
// Adjacent like terms
static_assert(sortedAdd([x, 3*x]) == [x, 3*x] || [3*x, x]);

// Mixed types
static_assert(sortedAdd([y, 2, x]) == [2, x, y]);

// Commutativity preservation
static_assert(sortedMul([y, x]) == sortedMul([x, y]));

// Power grouping
static_assert(sortedMul([x^2, x, x^3]) == [x, x^2, x^3]);
```

## References

- Computer Algebra textbooks (Geddes, Czapor, Labahn - "Algorithms for Computer Algebra")
- Mathematica term ordering
- Current symbolic3/ordering.h for baseline comparison
