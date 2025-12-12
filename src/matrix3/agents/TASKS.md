# Matrix3 Tasks

## Task Status Legend

- `[ ]` Not started
- `[~]` In progress
- `[x]` Completed
- `[!]` Blocked

## Priority Legend

- 🔴 **P0**: Critical - blocks usage or correctness issue
- 🟠 **P1**: High - significant quality improvement
- 🟡 **P2**: Medium - notable enhancement
- 🟢 **P3**: Low - nice to have

---

## Migration Tasks (Completed)

### Phase 1: Core Infrastructure ✓
- [x] Extents (compile-time/dynamic dimensions) - `extents.h`
- [x] Layouts (LayoutLeft, LayoutRight, LayoutPassthrough) - `layouts.h`
- [x] Accessors (RangeAccessor, IdentityAccessor) - `accessors.h`
- [x] GenericMatrix base class - `matrix.h`
- [x] Dense (heap-allocated) - `matrix.h`
- [x] InlineDense (stack-allocated) - `matrix.h`
- [x] Identity - `matrix.h`

### Phase 2: Storage Types ✓
- [x] Complex number wrapper - `complex.h`
- [x] InlineCoordinateList (COO sparse) - `inline_coordinate_list.h`
- [x] Banded - `banded.h`
- [x] Block - `block.h`
- [x] Permutation - `permutation.h`
- [x] Permuted (RowPermuted/ColPermuted) - `permuted.h`

### Phase 3: Algorithms ✓
- [x] Addition - `addition.h`
- [x] Multiplication - `multiplication.h`
- [x] Gauss-Jordan elimination - `gauss_jordan.h`
- [x] LU Decomposition - `lu_decomposition.h`
- [x] Kronecker product - `kronecker_product.h`

### Phase 4: Utilities ✓
- [x] Transpose view - `transpose.h`
- [x] Submatrix view - `submatrix.h`
- [x] to_string - `to_string.h`

---

## Improvement Tasks (From Comprehensive Review)

*Generated from review on 2025-12-11. See `agents/review/FINAL_REPORT.md` for details.*

### 🔴 P0 - Critical

- [ ] **Fix LU Determinant Permutation Sign** - `lu_decomposition.h:82-88`
  - determinant() ignores permutation parity, returns wrong sign for odd swaps
  - Fix: Track parity, return `parity ? -det : det`

- [ ] **Fix Uninitialized Variables in extents.h** - `extents.h:80-102`
  - Variables `ans` in staticExtent()/extent() may be uninitialized
  - Fix: Initialize with default value or restructure

- [ ] **Remove namespace internal** - `gauss_jordan.h`
  - Violates CLAUDE.md guidelines (single namespace `tempura`)
  - Fix: Move to anonymous namespace

### 🟠 P1 - High Priority

- [ ] **Fix API Pattern Fragmentation** - `matrix.h`, `transpose.h`, `submatrix.h`, `to_string.h`
  - Pattern A (extent()) vs Pattern B (rows()/cols()) prevents interoperability
  - Fix: Add extent() to views OR rows()/cols() to GenericMatrix

- [ ] **Make Transpose Store by Reference** - `transpose.h:80`
  - Currently stores by value, violating zero-copy philosophy
  - Fix: Change to `MatrixType& mat_`

- [ ] **Fix Indicies Typo Globally** - `accessors.h`, `matrix.h`, `layouts.h`
  - Consistent misspelling "Indicies" → "Indices"
  - Fix: Global search/replace

- [ ] **Add rows()/cols() to GenericMatrix** - `matrix.h`
  - Forces complex type introspection in views
  - Fix: Add convenience methods forwarding to extent()

- [ ] **Fix Dynamic Extent Operator Incompatibility** - `addition.h`, `multiplication.h`
  - Auto-inference operators fail with dynamic matrices
  - Fix: Add runtime fallback or explicit output overloads

- [ ] **Add Scalar Multiplication** - `multiplication.h`
  - No scalar-matrix operations (scalar * matrix)
  - Fix: Add operator* overloads for scalars

### 🟡 P2 - Medium Priority

- [ ] **Remove Empty base.h File** - `base.h`
  - Empty stub duplicating other files' descriptions

- [ ] **Update toString() to Support Views** - `to_string.h`
  - Cannot format Transpose/Submatrix (requires extent())
  - Fix: Accept rows()/cols() pattern or add extent() to views

- [ ] **Fix safeDivide Error Handling** - `lu_decomposition.h:31-36`
  - Divides by 1e-30, masks singularity; in global scope
  - Fix: Move to anonymous namespace, use named constant

- [ ] **Fix Permutation Parity Algorithm Comments** - `permutation.h`
  - Works correctly but comments use wrong mathematical reasoning

- [ ] **Add Iteration Interface to InlineCoordinateList** - `inline_coordinate_list.h`
  - Lacks iteration over non-zeros, returns by value

- [ ] **Extract Shared MatrixTraits** - `banded.h`, `block.h`
  - Duplicate definitions should be in shared header

- [ ] **Add complex.h Pattern A Compatibility** - `complex.h`
  - Missing ExtentsType/extent() for algorithm compatibility

- [ ] **Improve Test Coverage for const-correctness** - test files
  - Views don't verify const-correctness

### 🟢 P3 - Low Priority

- [ ] **Add layout_stride for mdspan Compatibility** - `layouts.h`
- [ ] **Add Extents Comparison Operators** - `extents.h`
- [ ] **Complete CompressedSparseAccessor** - `accessors.h`
- [ ] **Add Hermitian Transpose** - `transpose.h`
- [ ] **Add Banded Matrix Algorithms** - `banded.h`
- [ ] **Improve to_string Tests** - `to_string_test.cpp`
- [ ] **Add BlockCol for Vertical Concatenation** - `block.h`
- [ ] **Add materialize() to Views** - views
- [ ] **Improve Banded UB Documentation** - `banded.h`

---

## Summary Statistics

| Priority | Total | Done |
|----------|-------|------|
| P0 | 3 | 0 |
| P1 | 6 | 0 |
| P2 | 8 | 0 |
| P3 | 9 | 0 |
| **Total** | **26** | **0** |

## Notes for Implementers

- All code must be `constexpr` where possible
- Follow matrix3's mdspan-inspired design (Extents + Layout + Accessor)
- Tests go in same directory as implementation
- One logical feature per commit
- See `agents/review/TASK_LIST.md` for detailed acceptance criteria
