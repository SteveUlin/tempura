# Symbolic3 Recommendation #2 Implementation Summary

## Goal

Implement canonical forms for associative/commutative operators (Add, Mul) to:

- Flatten nested operations: `(a+b)+c` → `Expression<Add, a, b, c>`
- Sort arguments automatically: `b+a` → `a+b` (using type ordering)
- Reduce rewrite rules needed for commutativity and associativity

## Implementation Status

### ✅ Completed

1. **Variadic Function Objects** (`src/symbolic3/operators.h`)

   - `AddOp` and `MulOp` now support variadic evaluation
   - Binary interface preserved for backward compatibility
   - Fold-left evaluation: `(((a + b) + c) + d)`

2. **Canonical Form Infrastructure** (`src/symbolic3/canonical.h`)

   - Type-based flattening utilities for nested operations
   - Operation-specific sorting strategies:
     - `SortForAddition`: Groups constants together for term collection
     - `SortForMultiplication`: Groups constants for coefficient combining
   - `UsesCanonicalForm` trait system for operation classification

3. **Strategy Integration**

   - `Canonicalize` strategy for applying canonical forms
   - Named `to_canonical` to avoid glibc namespace conflict
   - Integrates with existing strategy combinator system

4. **Test Suite** (`src/symbolic3/test/canonical_test.cpp`)
   - Basic flattening tests
   - Commutativity verification
   - Non-associative operation preservation

### 🚧 In Progress / Known Issues

1. **TypeList vs std::tuple Mismatch**

   - `get_args_t<Expression>` returns `std::tuple<Args...>`
   - Canonical form utilities use `TypeList<Args...>`
   - Need adapter layer to convert between representations
   - Current implementation incomplete - compilation errors remain

2. **Flattening Logic**

   - Core algorithm correct but needs tuple/TypeList adaptation
   - `FlattenArgs` recursively extracts nested operation arguments
   - `Concat` operation fails when mixing tuple/TypeList types

3. **Sorting Implementation**
   - Insertion sort implemented for compile-time type sorting
   - Uses existing `lessThan` predicate from `ordering.h`
   - Partition-based sorting for operation-specific grouping
   - Needs testing with actual expressions

## Architecture

### Type Flow

```
Expression<Add, Expression<Add, a, b>, c>
  ↓ get_args_t → std::tuple<Expression<Add, a, b>, c>
  ↓ MakeCanonicalFromTuple
  ↓ MakeCanonical<Add, Expression<Add, a, b>, c>
  ↓ FlattenArgs → TypeList<a, b, c>
  ↓ SortForAddition → TypeList<a, b, c> (sorted)
  ↓ ToExpression
  → Expression<Add, a, b, c>
```

### Key Design Decisions

1. **Strategy-based Application**

   - Canonical forms applied via `to_canonical` strategy
   - NOT applied at construction time (preserves flexibility)
   - Users can choose when to canonicalize

2. **Operation-Specific Sorting**

   - Addition: Constants first (enables term collection)
   - Multiplication: Constants first (enables coefficient combining)
   - Default: Generic type ordering

3. **Trait-Based Opt-In**
   - Only `AddOp` and `MulOp` use canonical forms initially
   - Easy to extend to other associative operations
   - Non-associative ops (Sub, Div, Pow) unchanged

## Next Steps

1. **Fix TypeList/Tuple Adapter**

   - Implement clean conversion utilities
   - Either: Convert tuple → TypeList at boundary
   - Or: Rewrite canonical utilities to work with tuples directly

2. **Complete Test Suite**

   - Fix compilation errors
   - Add tests for sorting correctness
   - Verify commutativity: `a+b` and `b+a` have same type
   - Verify associativity: `(a+b)+c` and `a+(b+c)` have same type

3. **Integration with Simplification**

   - Add `to_canonical` to simplification pipeline
   - Test interaction with existing rewrite rules
   - Measure reduction in rule complexity

4. **Performance Validation**

   - Compile-time benchmarks (template instantiation depth)
   - Verify no runtime overhead
   - Test with deeply nested expressions

5. **Documentation**
   - Add usage examples to `examples/`
   - Document when to use canonical forms
   - Explain trade-offs vs. rewrite rules

## Benefits Expected

Once complete, this will:

- **Eliminate ~50% of associativity/commutativity rules** from simplify.h
- **Automatic term collection**: `2x + 3x` → `(2+3)x` naturally
- **Type-level equality**: `a+b` ≡ `b+a` without pattern matching
- **Easier debugging**: Canonical forms are predictable

## Code Locations

- `src/symbolic3/canonical.h` - Core implementation
- `src/symbolic3/operators.h` - Variadic function objects
- `src/symbolic3/test/canonical_test.cpp` - Test suite
- `src/symbolic3/simplify.h` - Will integrate canonical forms

## References

- SYMBOLIC3_RECOMMENDATIONS.md - Original recommendation
- src/symbolic3/ordering.h - Type ordering predicates
- src/meta/type_list.h - Compile-time list operations
