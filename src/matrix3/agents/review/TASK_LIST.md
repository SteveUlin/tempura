# Matrix3 Improvement Task List

Generated from comprehensive review on 2025-12-11.

## Priority Legend
- 🔴 **P0**: Critical - blocks usage or correctness issue
- 🟠 **P1**: High - significant quality improvement
- 🟡 **P2**: Medium - notable enhancement
- 🟢 **P3**: Low - nice to have

## Task Categories
- 🐛 Bug: Incorrect behavior
- ✨ Enhancement: New functionality
- ♻️ Refactor: Code improvement
- 📝 Documentation: Comments, docs, examples
- 🧪 Testing: Test improvements

---

## 🔴 P0 - Critical

### [TASK-001] Fix LU Determinant Permutation Sign
- **Category**: 🐛 Bug
- **File(s)**: lu_decomposition.h:82-88
- **Source**: Mathematics Reviewer (Batch 6)
- **Description**: The determinant() method returns det(U) but ignores permutation parity. For PA=LU, det(A) = sgn(P) · det(U). Currently returns wrong sign when odd swaps occurred.
- **Acceptance Criteria**:
  - [ ] Track permutation parity during decomposition
  - [ ] Return `parity ? -det : det` in determinant()
  - [ ] Add test case with matrix requiring odd number of swaps
  - [ ] Verify negative determinant returns correctly

### [TASK-002] Fix Uninitialized Variables in extents.h
- **Category**: 🐛 Bug
- **File(s)**: extents.h:80-87, 90-102
- **Source**: Mathematics Reviewer (Batch 1)
- **Description**: Variables `ans` in staticExtent() and extent() may be uninitialized if assertions disabled or unexpected code path taken.
- **Acceptance Criteria**:
  - [ ] Initialize `ans` variables with default value
  - [ ] Or restructure to guarantee assignment before use
  - [ ] Test with NDEBUG defined
  - [ ] Verify no warnings with -Wuninitialized

### [TASK-003] Remove namespace internal
- **Category**: ♻️ Refactor
- **File(s)**: gauss_jordan.h
- **Source**: Architecture Reviewer (Batch 6)
- **Description**: Uses `namespace internal` which violates CLAUDE.md guidelines (single namespace `tempura` required).
- **Acceptance Criteria**:
  - [ ] Move helpers to anonymous namespace or prefix with underscore
  - [ ] Ensure no public API changes
  - [ ] Update any internal references
  - [ ] Verify compilation succeeds

---

## 🟠 P1 - High Priority

### [TASK-004] Fix API Pattern Fragmentation
- **Category**: ♻️ Refactor
- **File(s)**: matrix.h, transpose.h, submatrix.h, to_string.h
- **Source**: Architecture Reviewer (Batches 2, 4, 7)
- **Description**: Pattern A (extent()) vs Pattern B (rows()/cols()) fragmentation prevents interoperability. Views cannot be used with toString() or generic algorithms.
- **Acceptance Criteria**:
  - [ ] Add extent() method to Transpose (return dynamic Extents)
  - [ ] Add extent() method to Submatrix (return dynamic Extents)
  - [ ] OR add rows()/cols() to GenericMatrix
  - [ ] Update toString() to accept both patterns
  - [ ] Verify all views work with toString()

### [TASK-005] Make Transpose Store by Reference
- **Category**: ♻️ Refactor
- **File(s)**: transpose.h:80
- **Source**: Architecture Reviewer (Batch 7)
- **Description**: Transpose stores `MatrixType mat_` by value, violating zero-copy philosophy. Should match Submatrix's reference semantics.
- **Acceptance Criteria**:
  - [ ] Change line 80 to `MatrixType& mat_`
  - [ ] Update constructors to accept lvalue references only
  - [ ] Update tests to verify modification propagation
  - [ ] Verify double transpose doesn't create copies

### [TASK-006] Fix Indicies Typo Globally
- **Category**: 📝 Documentation
- **File(s)**: accessors.h, matrix.h, layouts.h, others
- **Source**: Code Quality Reviewer (multiple batches)
- **Description**: Consistent misspelling of "Indices" as "Indicies" throughout codebase.
- **Acceptance Criteria**:
  - [ ] Search/replace `Indicies` → `Indices` in all files
  - [ ] Verify no compilation errors
  - [ ] Verify tests pass

### [TASK-007] Add rows()/cols() to GenericMatrix
- **Category**: ✨ Enhancement
- **File(s)**: matrix.h
- **Source**: Architecture Reviewer (Batch 2)
- **Description**: GenericMatrix only provides extent() but views need rows()/cols(). Forces complex type introspection.
- **Acceptance Criteria**:
  - [ ] Add `constexpr auto rows() const` method
  - [ ] Add `constexpr auto cols() const` method
  - [ ] Forward to extent().extent(0) and extent().extent(1)
  - [ ] Verify works with Dense, InlineDense, Identity

### [TASK-008] Fix Dynamic Extent Operator Incompatibility
- **Category**: 🐛 Bug
- **File(s)**: addition.h, multiplication.h
- **Source**: Mathematics Reviewer (Batch 5)
- **Description**: Auto-inference operators use staticExtent() which returns kDynamic for dynamic matrices, causing compilation failures.
- **Acceptance Criteria**:
  - [ ] Add runtime fallback for dynamic extents
  - [ ] Or add explicit output type overloads
  - [ ] Or document limitation clearly
  - [ ] Test with Dense (dynamic) matrices

### [TASK-009] Add Scalar Multiplication
- **Category**: ✨ Enhancement
- **File(s)**: multiplication.h
- **Source**: Feature Completeness Reviewer (Batch 5)
- **Description**: No support for scalar-matrix operations (scalar * matrix, matrix * scalar).
- **Acceptance Criteria**:
  - [ ] Add `operator*(scalar, matrix)` overload
  - [ ] Add `operator*(matrix, scalar)` overload
  - [ ] Add `operator*=(matrix, scalar)` for in-place
  - [ ] Add tests for various scalar types (int, double)

---

## 🟡 P2 - Medium Priority

### [TASK-010] Remove Empty base.h File
- **Category**: ♻️ Refactor
- **File(s)**: base.h
- **Source**: Architecture Reviewer (Batch 1)
- **Description**: Empty stub file with no implementation. All described features exist in layouts.h, accessors.h, matrix.h.
- **Acceptance Criteria**:
  - [ ] Remove base.h from codebase
  - [ ] Remove any includes of base.h
  - [ ] Update CMakeLists.txt if necessary
  - [ ] Verify compilation succeeds

### [TASK-011] Update toString() to Support Views
- **Category**: ✨ Enhancement
- **File(s)**: to_string.h
- **Source**: Architecture Reviewer (Batch 7)
- **Description**: toString() requires extent() method, cannot format Transpose/Submatrix views.
- **Acceptance Criteria**:
  - [ ] Add overload accepting rows()/cols() pattern
  - [ ] Or relax requires clause
  - [ ] Add tests for Transpose and Submatrix formatting
  - [ ] Update matrix3_demo.cpp to remove manual materialization

### [TASK-012] Fix safeDivide Error Handling
- **Category**: 🐛 Bug
- **File(s)**: lu_decomposition.h:31-36
- **Source**: Mathematics Reviewer (Batch 6)
- **Description**: safeDivide divides by 1e-30 instead of proper error handling. Masks singularity.
- **Acceptance Criteria**:
  - [ ] Move to anonymous namespace (currently global scope)
  - [ ] Replace magic number 1e-30 with named constant
  - [ ] Consider throwing or returning std::optional
  - [ ] Add isSingular() query method

### [TASK-013] Fix Permutation Parity Algorithm Comments
- **Category**: 📝 Documentation
- **File(s)**: permutation.h
- **Source**: Mathematics Reviewer (Batch 4)
- **Description**: Parity calculation in validate() uses incorrect mathematical reasoning. Works by accident (k+1 mod 2 = k-1 mod 2) but comments are misleading.
- **Acceptance Criteria**:
  - [ ] Update comments to match actual algorithm
  - [ ] Or rewrite to use correct k-1 cycles approach
  - [ ] Add explanation of parity invariant

### [TASK-014] Add Iteration Interface to InlineCoordinateList
- **Category**: ✨ Enhancement
- **File(s)**: inline_coordinate_list.h
- **Source**: Architecture Reviewer (Batch 3)
- **Description**: Sparse matrix lacks iteration over non-zeros. Returns by value (can't use `[i,j]=val`).
- **Acceptance Criteria**:
  - [ ] Add begin()/end() returning iterator over (row, col, value) tuples
  - [ ] Consider returning reference from operator[] for assignment
  - [ ] Add compression/deduplication method
  - [ ] Add tests for iteration

### [TASK-015] Extract Shared MatrixTraits
- **Category**: ♻️ Refactor
- **File(s)**: banded.h, block.h
- **Source**: Architecture Reviewer (Batch 3)
- **Description**: Both files define identical `MatrixTraits` helper. Should be extracted to shared header.
- **Acceptance Criteria**:
  - [ ] Create matrix_traits.h with shared definition
  - [ ] Update banded.h to use shared header
  - [ ] Update block.h to use shared header
  - [ ] Verify compilation succeeds

### [TASK-016] Add complex.h Pattern A Compatibility
- **Category**: ✨ Enhancement
- **File(s)**: complex.h
- **Source**: Architecture Reviewer (Batch 4)
- **Description**: Complex matrix lacks ExtentsType/extent() for algorithm compatibility.
- **Acceptance Criteria**:
  - [ ] Add `using ExtentsType = Extents<std::size_t, 2, 2>`
  - [ ] Add `constexpr auto extent() const` method
  - [ ] Verify works with addition/multiplication algorithms
  - [ ] Verify works with toString()

### [TASK-017] Improve Test Coverage for const-correctness
- **Category**: 🧪 Testing
- **File(s)**: transpose_test.cpp, submatrix_test.cpp, permuted_test.cpp
- **Source**: Test Quality Reviewer (multiple batches)
- **Description**: Tests don't verify const-correctness of views.
- **Acceptance Criteria**:
  - [ ] Add tests with const matrices passed to views
  - [ ] Verify const views return const references
  - [ ] Add tests for data() accessor
  - [ ] Verify compile failure on mutating const view

---

## 🟢 P3 - Low Priority

### [TASK-018] Add layout_stride for mdspan Compatibility
- **Category**: ✨ Enhancement
- **File(s)**: layouts.h
- **Source**: Feature Completeness Reviewer (Batch 1)
- **Description**: Missing layout_stride, required_span_size(), stride(r) vs std::mdspan.
- **Acceptance Criteria**:
  - [ ] Add LayoutStride class with arbitrary strides
  - [ ] Add required_span_size() method to layouts
  - [ ] Add stride(r) accessor method
  - [ ] Add tests

### [TASK-019] Add Extents Comparison Operators
- **Category**: ✨ Enhancement
- **File(s)**: extents.h
- **Source**: Feature Completeness Reviewer (Batch 1)
- **Description**: Missing operator==, hash support, size() method.
- **Acceptance Criteria**:
  - [ ] Add `operator==` and `operator!=`
  - [ ] Add `std::hash` specialization
  - [ ] Add `size()` for total element count
  - [ ] Add tests

### [TASK-020] Complete CompressedSparseAccessor
- **Category**: ✨ Enhancement
- **File(s)**: accessors.h
- **Source**: Code Quality Reviewer (Batch 2)
- **Description**: Commented-out CompressedSparseAccessor should be completed or removed.
- **Acceptance Criteria**:
  - [ ] Implement CSR/CSC accessor
  - [ ] Or remove commented-out code
  - [ ] Add tests if implemented

### [TASK-021] Add Hermitian Transpose
- **Category**: ✨ Enhancement
- **File(s)**: transpose.h
- **Source**: Feature Completeness Reviewer (Batch 7)
- **Description**: No conjugate transpose for complex matrices.
- **Acceptance Criteria**:
  - [ ] Add HermitianTranspose class
  - [ ] Or add template parameter to Transpose for conjugation
  - [ ] Add tests with complex matrices

### [TASK-022] Add Banded Matrix Algorithms
- **Category**: ✨ Enhancement
- **File(s)**: banded.h
- **Source**: Feature Completeness Reviewer (Batch 3)
- **Description**: No banded-specific algorithms (tridiagonal solver, banded multiply).
- **Acceptance Criteria**:
  - [ ] Add solveBandedTridiagonal()
  - [ ] Add multiplyBanded() optimization
  - [ ] Add conversion to dense
  - [ ] Add tests

### [TASK-023] Improve to_string Tests
- **Category**: 🧪 Testing
- **File(s)**: to_string_test.cpp
- **Source**: Test Quality Reviewer (Batch 7)
- **Description**: Tests use `find() != npos` instead of exact matching. Doesn't verify structure.
- **Acceptance Criteria**:
  - [ ] Add at least one exact output verification
  - [ ] Add test for 2-row matrix (edge case)
  - [ ] Add test for complex numbers
  - [ ] Verify actual column alignment

### [TASK-024] Add BlockCol for Vertical Concatenation
- **Category**: ✨ Enhancement
- **File(s)**: block.h
- **Source**: Feature Completeness Reviewer (Batch 3)
- **Description**: Only horizontal BlockRow exists. Missing vertical concatenation.
- **Acceptance Criteria**:
  - [ ] Add BlockCol class for vertical concatenation
  - [ ] Add BlockMatrix for 2D block structure
  - [ ] Add tests

### [TASK-025] Add materialize() to Views
- **Category**: ✨ Enhancement
- **File(s)**: transpose.h, submatrix.h, permuted.h, kronecker_product.h
- **Source**: Feature Completeness Reviewer (multiple batches)
- **Description**: No convenient way to convert views to concrete matrices.
- **Acceptance Criteria**:
  - [ ] Add materialize() method returning InlineDense or Dense
  - [ ] Template on output type for flexibility
  - [ ] Add tests

### [TASK-026] Improve Banded UB Documentation
- **Category**: 📝 Documentation
- **File(s)**: banded.h
- **Source**: Code Quality Reviewer (Batch 3)
- **Description**: DANGER comment incomplete for out-of-band write UB. kZero could be corrupted.
- **Acceptance Criteria**:
  - [ ] Expand DANGER documentation
  - [ ] Explain why (writes to kZero corrupt reads)
  - [ ] Suggest how to avoid
  - [ ] Consider runtime protection option

---

## Summary Statistics

| Priority | Count |
|----------|-------|
| P0 | 3 |
| P1 | 6 |
| P2 | 8 |
| P3 | 9 |
| **Total** | **26** |

| Category | Count |
|----------|-------|
| 🐛 Bug | 5 |
| ✨ Enhancement | 13 |
| ♻️ Refactor | 5 |
| 📝 Documentation | 3 |
| 🧪 Testing | 2 |

## Dependency Graph

```
TASK-001 (LU determinant) ← standalone
TASK-002 (extents UB) ← standalone
TASK-003 (namespace internal) ← standalone

TASK-004 (API fragmentation) ← depends on TASK-007 (add rows/cols to GenericMatrix)
TASK-005 (Transpose ref) ← standalone
TASK-006 (Indicies typo) ← standalone
TASK-007 (GenericMatrix rows/cols) ← standalone
TASK-008 (dynamic extent ops) ← standalone
TASK-009 (scalar multiply) ← standalone

TASK-011 (toString views) ← depends on TASK-004 (API fragmentation)
TASK-016 (complex Pattern A) ← depends on TASK-004 (API fragmentation)
```
