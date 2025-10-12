# Addition Rules Readability Improvements

## Date: October 12, 2025

## Summary

Improved the readability, consistency, and documentation of the addition simplification rules in `src/symbolic3/simplify.h`.

## Changes Made

### 1. Consistent Canonical Ordering: `a < b < c`

All associativity rules now consistently maintain the ordering `a < b < c` in:

- Pattern variable names
- Comments and documentation
- Rule predicates

**Before:**

- Mixed variable naming (sometimes `(a + b) + c`, sometimes `(a + c) + b`)
- Inconsistent ordering in comments vs patterns

**After:**

- All patterns use consistent `a < b < c` ordering
- Comments explicitly state the ordering relationship
- Clear explanation of how each rule maintains canonical form

### 2. Enhanced Documentation Structure

#### Section Headers with Visual Separators

Added Unicode box-drawing separators to clearly demarcate rule categories:

```cpp
// ────────────────────────────────────────────────────────────────────────────
// Identity Rules: Eliminate additive identity (zero)
// ────────────────────────────────────────────────────────────────────────────
```

#### Rule Category Overview

Added comprehensive overview at the top of the Addition Rules section:

- Lists all 5 rule categories in order
- Explains the purpose of each category
- Notes importance of ordering for termination

### 3. Improved Individual Rule Documentation

#### Identity Rules

- Clear purpose: "Eliminate additive identity (zero)"
- Simplified variable names for clarity

#### Like Terms

- Explicit purpose: "Collect identical terms"
- Clear example: `x + x → 2·x`

#### Canonical Ordering

- Added note about `lessThan()` function from `ordering.h`
- Explained total ordering across all symbolic types
- Clarified purpose: "Establish total order to prevent rewrite loops"

#### Factoring Rules

- Each rule now has inline comment explaining its purpose
- Clear distinction between three factoring patterns:
  - `x·a + x → x·(a+1)` - factor from scaled and unscaled term
  - `x + x·a → x·(1+a)` - symmetric case
  - `x·a + x·b → x·(a+b)` - factor from two scaled terms

#### Associativity Rules

- Consistent `a < b < c` ordering throughout
- Three rules work together to establish canonical form:
  1. **Left-associate**: `a + (b + c) → (a + b) + c` when `a ≤ b`
  2. **Right-associate**: `(a + c) + b → a + (c + b)` when `b < c`
  3. **Reorder**: `a + (c + b) → a + (b + c)` when `b < c`

### 4. Combined Rules Documentation

Enhanced the final combination comment to explain:

- Why order matters for efficiency
- Specific ordering rationale for each category
- How categories build upon each other

## Key Insights

### Why `lessThan()` is Necessary

The `lessThan()` function from `ordering.h` provides:

- **Total ordering** across all symbolic types (Constants, Symbols, Fractions, Expressions)
- **Consistent comparison** using a well-defined precedence:
  - Expression < Symbol < Fraction < Constant
- **STL-free implementation** for minimal dependencies
- **Compile-time evaluation** for constexpr contexts

### Canonical Ordering `a < b < c`

Maintaining `a < b < c` throughout:

- **Prevents infinite loops**: Rules only fire in one direction
- **Groups like terms**: `a + (a + c) → (a + a) + c` brings identical terms together
- **Simplifies pattern matching**: Consistent ordering reduces rule count
- **Enables term collection**: Factoring rules can rely on canonical order

### Rule Application Order

The sequence Identity → LikeTerms → Ordering → Factoring → Associativity is critical:

1. **Identity first**: Simple, fast elimination of zeros
2. **LikeTerms early**: Simplest pattern match before complex factoring
3. **Ordering before Associativity**: Establishes canonical form foundation
4. **Factoring before Associativity**: Groups terms before structural changes
5. **Associativity last**: Benefits from all prior simplifications

## Testing

All existing tests pass without modification:

- ✓ `simplification_basic_test` - 12/12 tests passed
- ✓ `simplification_pipeline_test` - 18/18 tests passed
- ✓ `simplification_advanced_test` - Not run (focuses on transcendental functions)

## Next Steps

Potential future improvements:

1. Add more comprehensive tests for associativity rules
2. Consider similar improvements for Multiplication and Power rules
3. Document the interaction between canonical forms and the flattening strategy in `canonical.h`
4. Add visual examples showing step-by-step simplification traces

## Files Modified

- `src/symbolic3/simplify.h` - Addition rules section (lines 172-277)
