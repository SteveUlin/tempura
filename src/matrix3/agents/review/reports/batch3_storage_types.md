# Batch 3 Review: Storage Types (inline_coordinate_list.h, banded.h, block.h)

## inline_coordinate_list.h

### Mathematics Review
**Overall Assessment**: ✓ Sound

- COO (Coordinate List) format correctly implemented with three parallel arrays
- Reverse linear search O(n) correct for "last insert wins" semantics
- Zero initialization via `ValueType{}` correct

**Concerns**:
- Duplicates never removed (memory grows with repeated updates)
- Search time degrades with duplicates

**Summary**: Critical: 0, Major: 0, Minor: 1 (duplicate accumulation)

### Code Quality Review
**Overall Grade**: B+

**Style**: Compliant (PascalCase types, camelCase methods, snake_case_ members)

**Issues**:
- Loop variable `i` uses `int64_t` but compared with unsigned `size_`
- Missing comments explaining why reverse search is used

**Positive**: Excellent constexpr support, clean separation of accessor and matrix class

### Architecture Review
**Overall Fit**: ⚠️ Needs Work

**Issues**:
1. Not using Extents/Layout pattern (uses raw Rows/Cols parameters)
2. Accessor takes tuple, inconsistent with other accessors
3. Returns by value (can't use `matrix[i,j] = value`)
4. Missing iteration interface for algorithms
5. Index type inconsistency (int64_t vs template Indices)

**Positive**: Has `rows()`/`cols()` methods (consistent with views)

### Feature Completeness
**Completeness Level**: Partial

**Present**: COO storage, insert, lookup, triplet construction, constexpr

**Missing Critical**:
1. No compression/deduplication method
2. No iteration over non-zeros
3. No sparse arithmetic

**Missing Nice-to-Have**: Format conversion (COO→CSR/CSC), clear(), reserve()

### Test Quality
**Grade**: B+

**Covered**: Empty construction, insert/lookup, zeros, last-insert-wins, capacity, triplets, constexpr

**Missing**: Out-of-bounds, negative indices, insert with value=0, complex numbers

---

## banded.h

### Mathematics Review
**Overall Assessment**: ✓ Sound

- Band calculation `band = col - row + CenterBand` is correct
- Storage mapping verified correct via documentation example
- Default `CenterBand = kCols / 2` reasonable for symmetric banded matrices

**Summary**: Critical: 0, Major: 0, Minor: 0

### Code Quality Review
**Overall Grade**: B

**Critical Issue**: DANGER documentation incomplete for UB
```cpp
// DANGER: Writing to out-of-band elements is undefined behavior.
```
Doesn't explain *why* (writes to kZero corrupt future reads) or *how* to avoid.

**Issue**: `kZero` should be const or mutable with reset

**Positive**: Good wrapper pattern, proper use of deducing this

### Architecture Review
**Overall Fit**: ✓ Good

- Wrapper pattern fits matrix3 view architecture
- MatrixTraits design is verbose but works
- Has `rows()`/`cols()` for consistency

**Issues**:
1. MatrixTraits only works with InlineDense (not extensible)
2. MatrixTraits lambda-in-array pattern overly complex

**Recommendation**: Extract MatrixTraits to shared `matrix_traits.h`

### Feature Completeness
**Completeness Level**: Partial

**Present**: Band access, deduction guide, factory function, constexpr

**Missing Essential**:
1. No banded algorithms (solveBandedTridiagonal, multiplyBanded)
2. No conversion to dense
3. No band range queries (lowerBandwidth, upperBandwidth)

### Test Quality
**Grade**: A-

**Comprehensive**: Construction, in-band access, out-of-band reads, writes, band calculation, different centers, constexpr, deduction guide, factory, tridiagonal, types

**Missing**: Out-of-band write UB documentation, move semantics, const correctness

---

## block.h

### Mathematics Review
**Overall Assessment**: ✓ Sound

- Horizontal concatenation correct: columns summed, rows shared
- Column offset calculation correct via short-circuit fold
- Boundary access verified in tests

**Summary**: Critical: 0, Major: 0, Minor: 0

### Code Quality Review
**Overall Grade**: B+

**Style**: Correct naming, proper constexpr

**Issue**: Complex short-circuit fold in operator[] is hard to understand:
```cpp
((j < offset + MatrixTraits<...>::kCols
      ? (result = &mats[i, j - offset], true)
      : (offset += MatrixTraits<...>::kCols, false)) or ...);
```
Should be refactored with helper function and comments.

**Type Inconsistency**: Uses `int64_t` while banded.h uses `std::size_t`

### Architecture Review
**Overall Fit**: ✓ Good

- Variadic template pattern fits well
- Stores tuple of blocks
- Has `rows()`/`cols()` methods

**Issue**: MatrixTraits duplicated from banded.h → should be extracted

### Feature Completeness
**Completeness Level**: Mostly Complete (for BlockRow)

**Present**: Horizontal concatenation, variadic blocks, deduction guide, const/mutable access

**Missing**:
- BlockCol (vertical concatenation)
- BlockMatrix (2D block structure)

### Test Quality
**Grade**: A

**Comprehensive**: 2/3/5 blocks, deduction guide, const, mutable, boundaries, single block, types, constexpr, large composition

**Minor gaps**: Nested BlockRow, integration with views

---

## Batch 3 Summary

| File | Math | Code | Arch | Tests | Feature |
|------|------|------|------|-------|---------|
| inline_coordinate_list.h | ✓ Sound | B+ | ⚠️ Needs Work | B+ | Partial |
| banded.h | ✓ Sound | B | ✓ Good | A- | Partial |
| block.h | ✓ Sound | B+ | ✓ Good | A | Mostly Complete |

### Key Cross-Cutting Findings

1. **MatrixTraits Duplication**: Both banded.h and block.h define identical `MatrixTraits` - should be extracted to shared header

2. **Type Inconsistency**: banded.h uses `std::size_t` for dimensions, block.h uses `int64_t`

3. **All Have rows()/cols()**: These files correctly provide `rows()`/`cols()` methods, unlike GenericMatrix in matrix.h

4. **Sparse Matrix Limitations**: InlineCoordinateList returns by value (can't use `[i,j]=val` syntax), needs iteration interface

5. **Banded UB Risk**: Writing to out-of-band elements corrupts kZero - needs better documentation or runtime protection

### Priority Recommendations

**High**:
1. Extract MatrixTraits to `matrix_traits.h`
2. Add iteration interface to InlineCoordinateList
3. Expand DANGER documentation in banded.h

**Medium**:
1. Add deduplication to InlineCoordinateList
2. Add banded algorithms
3. Standardize on int64_t for dimensions

**Low**:
1. Add BlockCol for vertical concatenation
2. Add conversion to dense for Banded
