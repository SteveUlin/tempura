# Matrix Migration Tasks

## Migration Scope

Source directories:
- `src/matrix/` - Original matrix implementation (older, less featured)
- `src/matrix2/` - Improved implementation with more storage types

Target: `src/matrix3/` - New mdspan-inspired implementation

## Task Status Legend

- `[ ]` Not started
- `[~]` In progress
- `[x]` Completed
- `[!]` Blocked

---

## Phase 1: Core Infrastructure

### 1.1 Base Types & Concepts
- [x] Extents (compile-time/dynamic dimensions) - `extents.h`
- [x] Layouts (LayoutLeft, LayoutRight, LayoutPassthrough) - `layouts.h`
- [x] Accessors (RangeAccessor, IdentityAccessor) - `accessors.h`
- [x] GenericMatrix base class - `matrix.h`

### 1.2 Basic Storage Types
- [x] Dense (heap-allocated) - `matrix.h`
- [x] InlineDense (stack-allocated) - `matrix.h`
- [x] Identity - `matrix.h`
- [x] Complex number wrapper - from `matrix2/storage/complex.h`

---

## Phase 2: Storage Types from matrix2

### 2.1 Sparse Storage
- [x] InlineCoordinateList - from `matrix2/storage/inline_coordinate_list.h`
  - COO format for small sparse matrices
  - Must support constexpr operations

### 2.2 Structured Storage
- [x] Banded - from `matrix2/storage/banded.h`
  - Efficient storage for banded matrices
  - Parameterized by upper/lower bandwidth

- [x] Block - from `matrix2/storage/block.h`
  - Block-structured matrices
  - Useful for partitioned algorithms

### 2.3 Permutation Support
- [x] Permutation - from `matrix2/storage/permutation.h`
  - Represents row/column permutations
  - Used by LU decomposition with pivoting

- [x] Permuted - from `matrix2/storage/permuted.h`
  - View applying permutation to another matrix

---

## Phase 3: Algorithms

### 3.1 Basic Operations
- [x] Addition - from `matrix2/addition.h`
  - Element-wise addition
  - Expression template support

- [x] Multiplication - from `matrix2/multiplication.h`
  - Matrix-matrix multiplication
  - Matrix-vector multiplication
  - Expression template support

### 3.2 Decompositions
- [x] Gauss-Jordan elimination - from `matrix2/algorithms/gauss_jordan.h`
  - In-place row reduction
  - Pivoting strategies

- [~] LU Decomposition - from `matrix2/algorithms/lu_decomposition.h`
  - Doolittle/Crout methods
  - Partial pivoting support

---

## Phase 4: Utilities

### 4.1 Views & Slicing
- [ ] Transpose view
- [ ] Submatrix view (slicing)
- [ ] Row/Column iteration

### 4.2 Output
- [ ] to_string - from `matrix2/to_string.h`
  - Pretty printing
  - Configurable formatting

---

## Phase 5: Integration from matrix (if applicable)

### 5.1 Additional Features
- [ ] Kronecker product - from `matrix/kronecker_product.h`
- [ ] MatRef (reference wrapper) - from `matrix/matrix.h`

---

## Task Dependencies

```
Phase 1 (Core) ──► Phase 2 (Storage) ──► Phase 3 (Algorithms)
                          │
                          ▼
                   Phase 4 (Utilities)
                          │
                          ▼
                   Phase 5 (Integration)
```

## Prioritization Criteria

1. **Foundation First**: Core types before derived types
2. **Test Coverage**: Each feature must have tests
3. **Incremental Value**: Prefer features that unblock others
4. **Complexity Gradient**: Simple storage types before algorithms

## Notes for Implementers

- All code must be `constexpr` where possible
- Follow matrix3's mdspan-inspired design (Extents + Layout + Accessor)
- Use existing GenericMatrix as base class
- Tests go in same directory as implementation
- One logical feature per commit
