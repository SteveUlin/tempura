# Track A: matrix3 Numeric Extensions

## Goal

Add the matrix decompositions and operations needed to serve as the
evaluation backend for symbolic matrix algebra. These are general-purpose
numeric additions — useful independent of symbolic4.

## Prerequisites

None. This track is fully independent and can start immediately.

## Current State of matrix3

**Has:**
- Dense, InlineDense, DynamicDense matrix types
- LU decomposition (pivoted, with solve and determinant)
- Matrix multiply (tiled/blocked for cache locality)
- Addition, subtraction, scalar multiply
- Transpose, submatrix, block, permutation (zero-copy views)
- Kronecker product
- C++26 `operator[i, j]`, column-major layout

**Missing (needed for matrix algebra):**
- Cholesky decomposition (critical for Bayesian inference)
- Matrix inverse (explicit, not just via LU::solve)
- Eigendecomposition (symmetric)
- QR decomposition
- SVD (lower priority)

## Task A.1: Cholesky Decomposition

**File to create:** `src/matrix3/cholesky.h`

The most critical missing piece. Every MVN log-probability, every
covariance matrix parameterization, and every Riemannian HMC method
needs Cholesky factorization.

```cpp
namespace tempura::matrix3 {

template <typename Matrix>
class Cholesky {
 public:
  // Factor A = LLᵀ where A is symmetric positive definite
  constexpr explicit Cholesky(Matrix a);

  // The lower triangular factor L
  constexpr auto factor() const -> const auto&;

  // Solve Ax = b via L(Lᵀx) = b (forward + backward substitution)
  template <typename RHS>
  constexpr auto solve(RHS b) const;

  // log|A| = 2·Σ log(L_ii) — avoids computing full determinant
  constexpr auto logDeterminant() const -> double;

  // A⁻¹ via solving with identity columns
  constexpr auto inverse() const;

  // Check if factorization succeeded (positive definite check)
  constexpr auto isPositiveDefinite() const -> bool;
};

// Factory
template <typename Matrix>
constexpr auto cholesky(Matrix a) { return Cholesky<Matrix>{a}; }

}  // namespace tempura::matrix3
```

**Algorithm:** Standard Cholesky–Banachiewicz (row-by-row):
```
L[i,j] = (A[i,j] - Σ_{k<j} L[i,k]*L[j,k]) / L[j,j]   for j < i
L[i,i] = sqrt(A[i,i] - Σ_{k<i} L[i,k]²)
```

Should be constexpr. For small matrices (≤ 4×4), consider unrolled
specializations.

**Tests:** `src/matrix3/cholesky_test.cpp`
- Factor known positive definite matrix, verify L * Lᵀ = A
- Solve system, verify solution
- logDeterminant matches LU determinant
- Non-positive-definite detection
- 1×1, 2×2, 3×3 special cases
- constexpr test with InlineDense

## Task A.2: Matrix Inverse

**File to create:** `src/matrix3/inverse.h`

Wrapper around existing LU (general) or new Cholesky (positive definite).

```cpp
namespace tempura::matrix3 {

// General inverse via LU
template <typename Matrix>
constexpr auto inverse(Matrix a);

// Symmetric positive definite inverse via Cholesky (more efficient)
template <typename Matrix>
constexpr auto inverseSPD(Matrix a);

// Triangular inverse (forward/backward substitution with identity)
template <typename Matrix>
constexpr auto inverseTriangular(Matrix L);

}  // namespace tempura::matrix3
```

Small-matrix specializations (2×2, 3×3) with closed-form formulas
for performance. The 2×2 case is just:
```
A⁻¹ = (1/det) * [[d, -b], [-c, a]]
```

**Tests:** Verify A * A⁻¹ = I for various sizes and types.

## Task A.3: Eigendecomposition (Symmetric)

**File to create:** `src/matrix3/eigen.h`

For symmetric matrices: A = QΛQᵀ where Q is orthogonal and Λ is diagonal.

```cpp
namespace tempura::matrix3 {

template <typename Matrix>
class SymmetricEigen {
 public:
  constexpr explicit SymmetricEigen(Matrix a);

  // Eigenvalues (sorted ascending)
  constexpr auto eigenvalues() const -> const auto&;

  // Eigenvectors as columns of Q
  constexpr auto eigenvectors() const -> const auto&;

  // Reconstruct: QΛQᵀ (verification)
  constexpr auto reconstruct() const;
};

}  // namespace tempura::matrix3
```

**Algorithm:** Jacobi iteration for small matrices (constexpr-friendly).
For larger matrices, consider QR algorithm (Task A.4 first).

This is needed for:
- Fisher information metric (Riemannian HMC)
- Spectral analysis of covariance matrices
- Positive-definiteness verification (all eigenvalues > 0)

## Task A.4: QR Decomposition

**File to create:** `src/matrix3/qr.h`

A = QR where Q is orthogonal and R is upper triangular.

```cpp
namespace tempura::matrix3 {

template <typename Matrix>
class QR {
 public:
  constexpr explicit QR(Matrix a);

  constexpr auto Q() const -> const auto&;
  constexpr auto R() const -> const auto&;

  // Least-squares solve via R⁻¹Qᵀb
  template <typename RHS>
  constexpr auto solve(RHS b) const;
};

}  // namespace tempura::matrix3
```

**Algorithm:** Householder reflections (numerically stable).

Needed for:
- Numerically stable linear regression
- Eigendecomposition via QR algorithm
- Orthogonalization

## Priority Order

1. **A.1 Cholesky** — critical path, blocks B.4 evaluation
2. **A.2 Inverse** — useful utility, quick to build on LU + Cholesky
3. **A.4 QR** — foundation for eigendecomposition
4. **A.3 Eigendecomposition** — needed for advanced methods

A.1 and A.2 are the must-haves. A.3 and A.4 are for Level 2 (Riemannian).

## Acceptance Criteria

- [ ] `cholesky(A)` produces correct L for positive definite A
- [ ] `Cholesky::solve(b)` matches LU::solve(b) for SPD matrices
- [ ] `Cholesky::logDeterminant()` matches 2*sum(log(diag(L)))
- [ ] `inverse(A) * A` ≈ Identity (within numeric tolerance)
- [ ] All decompositions work as constexpr with InlineDense
- [ ] All decompositions work with DynamicDense at runtime
- [ ] Tests added to CMakeLists.txt with `LABELS "matrix3"`

## Build & Test

```bash
cmake --build build && ctest --test-dir build -L matrix3
```
