# Generic Matrix Algebra Proposal for symbolic4

## Executive Summary

This proposal extends symbolic4's matrix support from its current probabilistic-programming focus (MVN, LKJ, Cholesky) to a more complete symbolic linear algebra system. The goal is to enable a broader class of models while maintaining the type-safe, dimension-tagged architecture.

## Current State (Completed from MATRIX_PROPOSAL.md)

| Component | Status | Notes |
|-----------|--------|-------|
| Dimension-tagged vectors | ✓ | `DimVectorSymbol<Id, DimTag>` |
| Cholesky factor types | ✓ | `CholeskyCovSymbol`, `CholeskyCorrSymbol` |
| Basic ops | ✓ | `dot`, `logDetChol`, `solveTriangular`, `quadFormChol`, `diagPreMult` |
| MVN distribution | ✓ | `mvnCholesky()` with log-prob |
| LKJ prior | ✓ | `lkjCholesky()` |
| Matrix evaluation | ✓ | `DynamicMatrix`, bindings, `evaluateMatrix()` |
| Basic differentiation | ✓ | Rules for existing ops |
| HMC integration | ✓ | `CholeskyTransform`, `MatrixPosterior` |

---

## Phase 1: Core Matrix Operations

### 1.1 General Matrix Type

Currently we only have Cholesky-specific matrices. Add a general matrix symbol:

```cpp
// General K×M matrix (rectangular supported)
template <typename Id, typename RowDimTag, typename ColDimTag>
struct MatrixSymbol : SymbolicTag {
  using id_type = Id;
  using row_dim_tag = RowDimTag;
  using col_dim_tag = ColDimTag;
};

// Square matrix (same row/col dimension)
template <typename Id, typename DimTag>
using SquareMatrixSymbol = MatrixSymbol<Id, DimTag, DimTag>;

// Factory functions
template <typename RowDim, typename ColDim>
constexpr auto matrixSym();

template <typename Dim>
constexpr auto squareMatrixSym();
```

### 1.2 Matrix Multiplication

```cpp
// C = A × B where A is (M×K), B is (K×N), C is (M×N)
struct MatMulOp {
  template <Matrix A, Matrix B>
  constexpr auto operator()(const A& a, const B& b) const;
};

template <Symbolic A, Symbolic B>
constexpr auto matmul(A a, B b);

// Dimension checking at compile-time via concepts
template <typename A, typename B>
concept MatMulCompatible =
  is_matrix_v<A> && is_matrix_v<B> &&
  std::is_same_v<col_dim_tag_t<A>, row_dim_tag_t<B>>;
```

### 1.3 Transpose

```cpp
// B = Aᵀ where A is (M×N), B is (N×M)
struct TransposeOp {
  template <Matrix A>
  constexpr auto operator()(const A& a) const;
};

template <Symbolic A>
constexpr auto transpose(A a);

// Symbolic type preserves dimension swap
// transpose(MatrixSymbol<Id, RowDim, ColDim>) → inferred as (ColDim × RowDim)
```

### 1.4 Matrix-Vector Multiplication

```cpp
// y = A × x where A is (M×N), x is (N), y is (M)
struct MatVecMulOp {
  template <Matrix A, Vector X>
  constexpr auto operator()(const A& a, const X& x) const;
};

template <Symbolic A, Symbolic X>
constexpr auto matvec(A a, X x);

// And the transpose version: y = Aᵀ × x
template <Symbolic A, Symbolic X>
constexpr auto matvecT(A a, X x);  // Aᵀx
```

### 1.5 Outer Product

```cpp
// A = x × yᵀ where x is (M), y is (N), A is (M×N)
struct OuterProductOp {
  template <Vector X, Vector Y>
  constexpr auto operator()(const X& x, const Y& y) const;
};

template <Symbolic X, Symbolic Y>
constexpr auto outer(X x, Y y);
```

### 1.6 Element-wise Operations

```cpp
// Hadamard (element-wise) product
struct HadamardOp {
  template <Matrix A, Matrix B>
  constexpr auto operator()(const A& a, const B& b) const;
};

template <Symbolic A, Symbolic B>
constexpr auto hadamard(A a, B b);

// Matrix + Matrix, Matrix - Matrix (already have for vectors)
// Matrix * scalar, Matrix / scalar
```

**Priority: HIGH** - These are fundamental building blocks.

---

## Phase 2: Matrix Decompositions

### 2.1 Cholesky Decomposition (Runtime)

Currently we have Cholesky *symbols* but no decomposition operation:

```cpp
// L = chol(A) where A is symmetric positive definite
struct CholeskyDecompOp {
  template <Matrix A>
  constexpr auto operator()(const A& a) const;
};

template <Symbolic A>
constexpr auto chol(A a);

// Evaluation: use standard O(n³) algorithm or delegate to BLAS
```

### 2.2 LU Decomposition

```cpp
// (L, U, P) = lu(A) - useful for solving linear systems
struct LUDecompOp {
  // Returns tuple-like symbolic expression
};
```

### 2.3 QR Decomposition

```cpp
// (Q, R) = qr(A) - useful for least squares
struct QRDecompOp {
  template <Matrix A>
  constexpr auto operator()(const A& a) const;
};

// A = Q × R where Q is orthogonal, R is upper triangular
template <Symbolic A>
constexpr auto qr(A a);
```

### 2.4 Eigendecomposition (Symmetric)

```cpp
// For symmetric matrices: A = V × D × Vᵀ
struct EigenSymOp {
  template <SymmetricMatrix A>
  constexpr auto operator()(const A& a) const;
};

// Returns (eigenvalues, eigenvectors)
template <Symbolic A>
constexpr auto eigenSym(A a);
```

**Priority: MEDIUM** - Cholesky is high priority, others medium.

---

## Phase 3: Linear System Solvers

### 3.1 Triangular Solves (Complete Set)

Currently only have forward substitution. Add:

```cpp
// Forward substitution: L x = b (already have: solveTriangular)
// Back substitution: U x = b
struct SolveUpperTriangularOp {
  template <Matrix U, Vector B>
  constexpr auto operator()(const U& u, const B& b) const;
};

// Solve Lᵀ x = b (transpose solve)
struct SolveTriangularTransposeOp {
  template <Matrix L, Vector B>
  constexpr auto operator()(const L& l, const B& b) const;
};

template <Symbolic L, Symbolic B>
constexpr auto solveLT(L l, B b);  // Lᵀx = b
```

### 3.2 General Linear Solve

```cpp
// Solve A x = b for general A (via LU internally)
struct SolveOp {
  template <Matrix A, Vector B>
  constexpr auto operator()(const A& a, const B& b) const;
};

template <Symbolic A, Symbolic B>
constexpr auto solve(A a, B b);

// Matrix right-hand side: solve A X = B
template <Symbolic A, Symbolic B>
constexpr auto solveMatrix(A a, B b);
```

### 3.3 Least Squares

```cpp
// Solve min_x ||Ax - b||² (via QR)
struct LeastSquaresOp {
  template <Matrix A, Vector B>
  constexpr auto operator()(const A& a, const B& b) const;
};

template <Symbolic A, Symbolic B>
constexpr auto lstsq(A a, B b);
```

**Priority: HIGH** - `solveLT` especially needed for gradients.

---

## Phase 4: Matrix Functions

### 4.1 Determinant and Log-Determinant

```cpp
// det(A) - via LU
struct DetOp {
  template <Matrix A>
  constexpr auto operator()(const A& a) const;
};

// logdet(A) = log|det(A)| - more numerically stable
// Already have logDetChol; add general version
struct LogDetOp {
  template <Matrix A>
  constexpr auto operator()(const A& a) const;
};
```

### 4.2 Trace

```cpp
// trace(A) = Σ A[i,i]
struct TraceOp {
  template <Matrix A>
  constexpr auto operator()(const A& a) const;
};

template <Symbolic A>
constexpr auto trace(A a);
```

### 4.3 Norms

```cpp
// Frobenius norm: ||A||_F = sqrt(Σ A[i,j]²)
struct FrobeniusNormOp {};

// Spectral norm: ||A||_2 = largest singular value
struct SpectralNormOp {};

// Vector norms (extend existing)
struct L1NormOp {};   // ||x||_1
struct L2NormOp {};   // ||x||_2 (Euclidean)
struct LInfNormOp {}; // ||x||_∞
```

### 4.4 Matrix Inverse

```cpp
// A⁻¹ - generally discouraged, but sometimes needed
struct InverseOp {
  template <Matrix A>
  constexpr auto operator()(const A& a) const;
};

template <Symbolic A>
constexpr auto inverse(A a);

// Prefer solve(A, b) over inverse(A) * b for efficiency
```

### 4.5 Matrix Exponential / Logarithm

```cpp
// exp(A) - useful for matrix differential equations
struct MatrixExpOp {};

// log(A) - for symmetric positive definite matrices
struct MatrixLogOp {};
```

**Priority: HIGH** for trace/norms, MEDIUM for inverse, LOW for exp/log.

---

## Phase 5: Differentiation Rules

### 5.1 Matrix Calculus Fundamentals

Each operation needs symbolic differentiation rules:

```cpp
// d/dA (B × A × C) = Bᵀ × dA/dθ × Cᵀ  (for scalar θ)
// d/dA trace(A × B) = Bᵀ
// d/dA log|det(A)| = A⁻ᵀ
// d/dA (Aᵀ) = (dA)ᵀ
```

### 5.2 Priority Derivatives

| Operation | Derivative Rule | Priority |
|-----------|----------------|----------|
| `matmul(A, B)` | Product rule: `dA×B + A×dB` | HIGH |
| `transpose(A)` | `transpose(dA)` | HIGH |
| `matvec(A, x)` | `dA×x + A×dx` | HIGH |
| `trace(A)` | `trace(dA)` | HIGH |
| `logdet(A)` | `trace(A⁻¹ dA)` | HIGH |
| `inverse(A)` | `-A⁻¹ dA A⁻¹` | MEDIUM |
| `chol(A)` | Complex (see literature) | MEDIUM |
| `solve(A, b)` | Via implicit differentiation | MEDIUM |
| `eigenSym(A)` | Perturbation theory | LOW |

### 5.3 Implement Incomplete Derivatives

From diff.h, these placeholders need evaluation:

```cpp
// Already declared, need eval rules in eval.h:
struct LogDetCholDerivOp {};      // ✓ Done (Phase 0)
struct QuadFormCholDerivLOp {};   // ✓ Done (Phase 0)

// Need symbolic construction + eval:
struct MatMulDerivOp {};
struct TransposeDerivOp {};
struct TraceDerivOp {};
struct SolveLTDerivOp {};
```

---

## Phase 6: Testing Strategy

### 6.1 Unit Tests Per Operation

For each operation, test:

```cpp
// 1. Type traits
static_assert(is_matrix_v<MatrixSymbol<X, Rows, Cols>>);

// 2. Expression construction
auto A = matrixSym<Rows, Cols>();
auto B = matrixSym<Cols, Cols>();
auto C = matmul(A, B);
static_assert(is_expression_v<decltype(C)>);
static_assert(row_dim_tag_t<decltype(C)> == Rows);

// 3. Evaluation correctness
DynamicMatrix a_val(2, 3);
DynamicMatrix b_val(3, 3);
// ... fill with test values
auto result = evaluateMatrix(C, bindings...);
expectNear(result[0,0], expected_00, 1e-10);

// 4. Gradient verification (finite differences)
auto lp = trace(matmul(A, transpose(A)));  // tr(AAᵀ)
auto grad = gradient(lp, A_params);
auto grad_fd = finiteGradient(lp, A_params);
for (size_t i = 0; i < n; ++i) {
  expectNear(grad[i], grad_fd[i], 1e-5);
}
```

### 6.2 Property-Based Tests

```cpp
// Matrix algebra identities
"(AB)C = A(BC)"_test = []{...};  // Associativity
"(AB)ᵀ = BᵀAᵀ"_test = []{...};   // Transpose of product
"tr(AB) = tr(BA)"_test = []{...}; // Trace cyclic
"|det(AB)| = |det(A)||det(B)|"_test = []{...};

// Numerical stability
"solve vs inverse consistency"_test = []{
  // solve(A, b) ≈ inverse(A) * b
  // But solve should be more accurate for ill-conditioned A
};
```

### 6.3 Integration Tests

```cpp
// Full probabilistic models
"Gaussian Process regression"_test = []{
  // y ~ MVN(0, K) where K = kernel(X, X)
  // Requires: matmul, chol, solve, logdet
};

"Factor analysis"_test = []{
  // y ~ MVN(0, ΛΛᵀ + Ψ)
  // Requires: matmul, transpose, outer, diag
};

"Matrix-variate normal"_test = []{
  // X ~ MN(M, U, V) - matrix normal distribution
  // Requires: Kronecker product, vec operator
};
```

### 6.4 Benchmark Tests

```cpp
// Performance baselines
"matmul K=10 benchmark"_bench = []{...};
"chol K=50 benchmark"_bench = []{...};
"gradient MVN K=20 benchmark"_bench = []{...};
```

---

## Phase 7: Implementation Order

### Sprint 1: Foundation (1-2 weeks)
- [ ] `MatrixSymbol<Id, RowDim, ColDim>` type
- [ ] `matmul(A, B)` operation + eval + tests
- [ ] `transpose(A)` operation + eval + tests
- [ ] `matvec(A, x)` operation + eval + tests
- [ ] Update CMakeLists.txt with new test targets

### Sprint 2: Solvers (1 week)
- [ ] `solveLT(L, b)` - back substitution via transpose
- [ ] `solveUT(U, b)` - upper triangular solve
- [ ] Tests comparing solve chains to direct methods

### Sprint 3: Norms & Functions (1 week)
- [ ] `trace(A)` + derivative
- [ ] `frobeniusNorm(A)`
- [ ] `l2Norm(x)` (if not already)
- [ ] Tests

### Sprint 4: Derivatives (2 weeks)
- [ ] `diff` rule for `matmul`
- [ ] `diff` rule for `transpose`
- [ ] `diff` rule for `matvec`
- [ ] `diff` rule for `trace`
- [ ] Finite difference verification for all

### Sprint 5: Decompositions (1-2 weeks)
- [ ] `chol(A)` runtime decomposition (vs symbol)
- [ ] `qr(A)` decomposition
- [ ] `eigenSym(A)` for symmetric matrices
- [ ] Tests against known decompositions

### Sprint 6: Advanced (ongoing)
- [ ] `inverse(A)` with derivative
- [ ] `logdet(A)` for general matrices
- [ ] `lstsq(A, b)` least squares
- [ ] Block matrix operations
- [ ] Sparse matrix support

---

## API Design Decisions

### Decision 1: Row-Major vs Column-Major

**Recommendation**: Row-major (current choice)
- Matches C++ memory layout conventions
- Consistent with existing `DynamicMatrix`
- Trade-off: BLAS typically expects column-major

### Decision 2: Expression Templates vs Eager Evaluation

**Recommendation**: Expression templates (current approach)
- Zero-cost abstractions
- Enables symbolic differentiation
- Trade-off: Complex type signatures

### Decision 3: Runtime vs Compile-Time Dimensions

**Recommendation**: Runtime dimensions with compile-time tags
- Flexibility for data-driven sizes
- Type safety for dimension matching
- Trade-off: No compile-time size optimization

### Decision 4: BLAS Integration

**Recommendation**: Optional BLAS backend
- Default: Pure C++ implementation (portable)
- Optional: BLAS/LAPACK for large matrices
- Interface via policy template

```cpp
template <typename Backend = PureCppBackend>
struct MatrixOps {
  static auto matmul(const DynamicMatrix& a, const DynamicMatrix& b);
};

// With BLAS:
using FastMatrixOps = MatrixOps<BlasBackend>;
```

---

## Open Questions

1. **Complex numbers**: Should we support complex matrices for quantum/signal processing?

2. **GPU support**: Future-proof the interface for CUDA/ROCm backends?

3. **Tensor generalization**: Is rank-N tensor support on the roadmap?

4. **Interop**: Should we provide conversions to/from Eigen, Armadillo, etc.?

5. **Memory management**: Pool allocators for temporary matrices in evaluation?

---

## Success Criteria

1. **Correctness**: All operations match NumPy/Eigen to 1e-10 relative error
2. **Gradients**: All derivatives verified against finite differences
3. **Coverage**: Can express GP, factor analysis, VAR models end-to-end
4. **Performance**: Within 2x of Eigen for matrices up to 100×100
5. **Ergonomics**: Models readable, dimension errors caught at compile time

---

## References

- Petersen & Pedersen, "The Matrix Cookbook" (2012) - differentiation formulas
- Giles, "An Extended Collection of Matrix Derivative Results" (2008)
- Stan Math Library - reference implementation
- JAX/Autograd - automatic differentiation patterns
