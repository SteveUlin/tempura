# Term Collecting and Canonical Ordering Implementation

## Overview

Successfully implemented term collecting and canonical ordering in symbolic3, following patterns from symbolic2 but integrated with symbolic3's combinator-based architecture.

## Features Implemented

### 1. Canonical Ordering

- **Addition**: `y + x → x + y` (when x < y)
- **Multiplication**: `y * x → x * y` (when x < y)
- **Prevents infinite rewrite loops** using predicated ordering rules
- **Ensures deterministic simplification** - same expression always produces same canonical form

### 2. Term Collecting (Like Terms)

- **Simple like terms**: `x + x → 2·x` ✅
- **With coefficients**: `x·2 + x → x·3` ✅
- **Both sides**: `x·2 + x·3 → x·5` ✅
- **Reversed order**: `x + x·2 → x·3` ✅

### 3. Power Combining

- **Base with power**: `x · x^2 → x^3` ✅
- **Powers with powers**: `x^2 · x^3 → x^5` ✅
- **Nested in trees**: `(x^2 · x^3) · (x · x^4) → x^10` ✅

### 4. Associativity with Conditional Reordering

- **Right-associative**: `(a + b) + c → a + (b + c)`
- **With reordering**: `(a + c) + b → (a + b) + c` (when b < c)

## Test Results

### Basic Tests

| Expression         | Simplified | Evaluation | Result |
| ------------------ | ---------- | ---------- | ------ |
| `x + x`            | `x * 2`    | x=5 → 10   | ✅     |
| `x*2 + x`          | `x * 3`    | x=10 → 30  | ✅     |
| `y + x` vs `x + y` | Same type  | Canonical  | ✅     |

### Complex Tests (Many Terms)

#### Test 4: Linear Chain with Alternating Bases

```
x + y + x + z + y + x
= 3x + 2y + z
```

**Evaluation**: x=10, y=5, z=3 → **43** ✅

#### Test 5: Coefficients with Alternating Bases

```
2x + 3y + 4x + 5y + 6x
= 12x + 8y
```

**Evaluation**: x=10, y=100 → **920** ✅

### Non-Linear Tree Structures

#### Test 6: Nested Parentheses (Balanced Tree)

```
((x + y) + (z + x)) + ((y + z) + x)
= 3x + 2y + 2z
```

**Evaluation**: x=10, y=5, z=2 → **44** ✅

#### Test 7: Deep Nested with Mixed Operations

```
(x*2 + y*3) + ((x*4 + y) + (x + y*2))
= 7x + 6y
```

**Evaluation**: x=10, y=100 → **670** ✅

#### Test 8: Multiplication Tree (Power Combining)

```
(x^2 * x^3) * (x * x^4)
= x^10
```

**Evaluation**: x=2 → **1024** ✅

#### Test 9: Mixed Addition and Multiplication Tree

```
(x + x*2) + (y*3 + y) + (x*4 + y*5)
= 7x + 9y
```

**Evaluation**: x=10, y=100 → **970** ✅

#### Test 10: Unbalanced Tree (Right-Heavy)

```
x + (x + (x + (x + (x + x))))
= 6x
```

**Evaluation**: x=7 → **42** ✅

## Implementation Details

### Files Modified

1. **`src/symbolic3/simplify.h`**

   - Added canonical ordering rules with predicates
   - Enhanced term collecting/factoring rules
   - Added `#include "symbolic3/ordering.h"`

2. **`src/symbolic3/ordering.h`**

   - Fixed forward declaration ordering
   - Moved `compareExprs` declaration before `compare` function

3. **`src/symbolic3/CMakeLists.txt`**

   - Added `term_collecting_test`
   - Added `term_collecting_debug` (diagnostic tool)

4. **`src/symbolic3/test/term_collecting_debug.cpp`** (NEW)
   - Comprehensive test suite with 10 test cases
   - Tests simple cases, complex chains, and non-linear trees

### Key Design Patterns

1. **Predicated Rewrite Rules**

   ```cpp
   constexpr auto canonical_order = Rewrite{
       y_ + x_,
       x_ + y_,
       [](auto ctx) {
           return lessThan(get(ctx, x_), get(ctx, y_));
       }
   };
   ```

2. **Combinator Composition**

   ```cpp
   constexpr auto AdditionRules =
       AdditionRuleCategories::Identity |
       AdditionRuleCategories::LikeTerms |
       AdditionRuleCategories::Ordering |
       AdditionRuleCategories::Factoring |
       AdditionRuleCategories::Associativity;
   ```

3. **Category Ordering** (Critical!)
   - Distribution BEFORE Associativity (prevents re-factoring loops)
   - Ordering BEFORE Factoring (establishes canonical form first)

## Verification

All 10 test cases pass:

- ✅ Simple like terms collection
- ✅ Factoring with coefficients
- ✅ Canonical ordering consistency
- ✅ Linear chains with 6+ alternating terms
- ✅ Nested tree structures (balanced and unbalanced)
- ✅ Mixed operations across trees
- ✅ Power combining across multiplication trees
- ✅ No infinite rewrite loops

## Future Enhancements

Potential improvements:

1. Constant folding in coefficients (e.g., `(1+2)` → `3`)
2. Distribution awareness to detect when to factor vs distribute
3. Performance optimization for very deep trees
4. More sophisticated power simplification (e.g., `x^(a+b)` → `x^a · x^b`)

## References

- Based on symbolic2 implementation patterns
- Follows term rewriting theory principles
- Integrates with symbolic3's combinator-based strategy system
- Uses symbolic3's ordering system for canonical forms
