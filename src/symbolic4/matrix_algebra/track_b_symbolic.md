# Track B: Symbolic Matrix Algebra

## Goal

Add general-purpose symbolic matrix types and operations to symbolic4,
unified with the existing scalar expression system via ReduceOver and
typed dimension indices. The matrix layer is syntactic sugar over
indexed scalars — `M[i,j]` decomposes to the scalar expression system.

## Prerequisites

- B.1–B.2: Independent (can start immediately)
- B.3: Requires Phase 1 (ReduceOver) — scalar decomposition uses reductions
- B.4: Requires Track A (Cholesky at minimum) — evaluation dispatches to matrix3
- B.5: Requires Phase 2 (strategy-based differentiate)
- B.6: Requires Phase 6 (samples[expr])

## Task B.1: Matrix Symbols with Structure Tags

**Files to create:**
- `src/symbolic4/matrix_algebra/matrix_symbol.h`
- `src/symbolic4/matrix_algebra/structure.h`

### Structure Tags

The type encodes what you know about the matrix. Operations preserve
or transform structure at compile time.

```cpp
namespace tempura::symbolic4::matrix {

// Structure tags — types ARE the representation
struct General {};
struct Symmetric {};           // A = Aᵀ
struct LowerTriangular {};     // A_{ij} = 0 for j > i
struct UpperTriangular {};     // A_{ij} = 0 for j < i
struct Diagonal {};            // A_{ij} = 0 for i ≠ j
struct PositiveDefinite {};    // Symmetric + eigenvalues > 0

// Trait: does this structure imply symmetry?
template <typename S> constexpr bool is_symmetric_v = false;
template <> constexpr bool is_symmetric_v<Symmetric> = true;
template <> constexpr bool is_symmetric_v<PositiveDefinite> = true;
template <> constexpr bool is_symmetric_v<Diagonal> = true;

// Trait: is this a triangular structure?
template <typename S> constexpr bool is_triangular_v = false;
template <> constexpr bool is_triangular_v<LowerTriangular> = true;
template <> constexpr bool is_triangular_v<UpperTriangular> = true;

// Structure preservation rules (compile-time dispatch)
template <typename S> struct TransposeStructure { using type = General; };
template <> struct TransposeStructure<Symmetric> { using type = Symmetric; };
template <> struct TransposeStructure<PositiveDefinite> { using type = PositiveDefinite; };
template <> struct TransposeStructure<Diagonal> { using type = Diagonal; };
template <> struct TransposeStructure<LowerTriangular> { using type = UpperTriangular; };
template <> struct TransposeStructure<UpperTriangular> { using type = LowerTriangular; };

}  // namespace tempura::symbolic4::matrix
```

### MatrixSymbol

```cpp
// MatrixSymbol<Id, RowDim, ColDim, Structure>
// RowDim, ColDim are DimTags (same as plate dimension tags)
template <typename Id, typename RowDim, typename ColDim,
          typename Structure = General>
struct MatrixSymbol : SymbolicTag {
  using id_type = Id;
  using row_dim = RowDim;
  using col_dim = ColDim;
  using structure = Structure;

  // Number of free dimension indices
  static constexpr SizeT rank = 2;

  // Component extraction — returns IndexedLookup
  // M[i, j] where i ∈ RowDim, j ∈ ColDim
  // The actual mechanism: create an IndexedSymbol<Id, RowDim, ColDim>
  // This connects to the existing indexed evaluation system.
};

// VectorSymbol is a rank-1 special case
template <typename Id, typename Dim, typename Structure = General>
using VectorSymbol = MatrixSymbol<Id, Dim, Unit, Structure>;
// where Unit is a singleton dimension (size 1, effectively absent)
```

### Factories

```cpp
// General matrix
template <typename RowDim, typename ColDim, typename Structure = General,
          typename Id = decltype([] {})>
constexpr auto matrixSymbol() {
  return MatrixSymbol<Id, RowDim, ColDim, Structure>{};
}

// Square matrix with structure
template <typename Dim, typename Structure = General,
          typename Id = decltype([] {})>
constexpr auto squareMatrix() {
  return MatrixSymbol<Id, Dim, Dim, Structure>{};
}

// Covariance matrix (positive definite, symmetric)
template <typename Dim, typename Id = decltype([] {})>
constexpr auto covMatrix() {
  return MatrixSymbol<Id, Dim, Dim, PositiveDefinite>{};
}

// Dimension-tagged vector (replaces DimVectorSymbol)
template <typename Dim, typename Id = decltype([] {})>
constexpr auto vectorSymbol() {
  return MatrixSymbol<Id, Dim, Unit, General>{};
}
```

### Relation to Existing Types

MatrixSymbol **subsumes** the existing domain-specific types:

| Current type | Becomes |
|-------------|---------|
| `DimVectorSymbol<Id, Dim>` | `MatrixSymbol<Id, Dim, Unit, General>` |
| `CholeskyCovSymbol<Id, Dim>` | `MatrixSymbol<Id, Dim, Dim, LowerTriangular>` |
| `CholeskyCorrSymbol<Id, Dim>` | `MatrixSymbol<Id, Dim, Dim, LowerTriangular>` + correlation constraint |

The old types should become aliases for backwards compatibility, then be
removed in a cleanup phase.

## Task B.2: Matrix Expression Nodes

**File to create:** `src/symbolic4/matrix_algebra/matrix_ops.h`

Matrix-level expression nodes that encode operations compactly. These
are the "syntactic sugar" — they can be decomposed to scalar expressions
via operator[] (Task B.3).

```cpp
// Expression nodes (all are Expression<Op, Args...>)

// Matrix multiply: C = A * B
struct MatMulOp { /* evaluated via matrix3 multiply */ };

// Transpose: Aᵀ
struct MatTransposeOp { /* swaps row/col dims in type */ };

// Trace: tr(A) — matrix → scalar boundary
struct TraceOp { /* evaluated via diagonal sum */ };

// Log-determinant: log|A| — matrix → scalar boundary
struct LogDetOp { /* evaluated via Cholesky: 2·Σlog(L_ii) */ };

// Symbolic Cholesky: L where A = LLᵀ
struct CholeskyOp { /* matrix → matrix with LowerTriangular structure */ };

// Symbolic inverse: A⁻¹
struct MatInverseOp { /* structure-aware: Diagonal⁻¹ is trivial, etc. */ };
```

### Operator Overloads

```cpp
// Matrix × matrix (detect structure of result)
template <typename A, typename B>
  requires(is_matrix_expr_v<A> && is_matrix_expr_v<B>)
constexpr auto operator*(A a, B b) {
  // Check dimension compatibility: A.col_dim == B.row_dim
  static_assert(std::is_same_v<col_dim_t<A>, row_dim_t<B>>,
                "Matrix dimension mismatch in multiply");
  return Expression<MatMulOp, A, B>{a, b};
}

// Trace: matrix → scalar (crosses the boundary)
template <typename A>
  requires(is_square_matrix_expr_v<A>)
constexpr auto trace(A a) {
  return Expression<TraceOp, A>{a};
}

// Transpose
template <typename A>
  requires(is_matrix_expr_v<A>)
constexpr auto transpose(A a) {
  // For Symmetric/PositiveDefinite/Diagonal: return a itself (no-op)
  if constexpr (is_symmetric_v<structure_t<A>>) {
    return a;
  } else {
    return Expression<MatTransposeOp, A>{a};
  }
}
```

### Scalar boundary operations

`Trace` and `LogDet` return scalar expressions. These are where the
matrix world connects to the scalar expression system — the result can
participate in `grad()`, `evaluate()`, etc. with no special handling.

```cpp
auto f = trace(A * transpose(A)) + log(sigma);
//       ^^^^^^^^^^^^^^^^^^^^^^^^   ^^^^^^^^^^^
//       matrix ops → scalar        scalar ops
//
// f is a scalar expression. grad(f)[A] returns a matrix.
```

## Task B.3: Scalar Decomposition via operator[]

**File to modify:** `src/symbolic4/matrix_algebra/matrix_ops.h` or new `decompose.h`

This is the key bridge. Any matrix expression can produce a scalar
expression by indexing into it. The scalar expression uses the existing
ReduceOver system for contractions.

```cpp
// MatrixSymbol component extraction
template <typename Id, typename RowDim, typename ColDim, typename Structure>
struct MatrixSymbol {
  // operator[] returns a scalar expression
  // The result is an IndexedSymbol<Id, RowDim, ColDim> evaluated at
  // specific dimension indices
  template <typename I, typename J>
  constexpr auto operator[](I, J) const {
    return IndexedLookup<MatrixSymbol, I, J>{};
  }
};
```

### Decomposition Rules

Each matrix expression node defines how its components decompose:

```cpp
// MatMul[i, j] = SumOver<K>(A[i, k] * B[k, j])
template <typename A, typename B, typename I, typename J>
constexpr auto decompose(Expression<MatMulOp, A, B>, I i, J j) {
  using K = col_dim_t<A>;  // = row_dim_t<B>, the contracted dimension
  return sumOver<K>(A{}[i, IndexVar<K>{}] * B{}[IndexVar<K>{}, j]);
}

// Transpose[i, j] = A[j, i]
template <typename A, typename I, typename J>
constexpr auto decompose(Expression<MatTransposeOp, A>, I i, J j) {
  return A{}[j, i];  // just swap indices
}

// Trace = SumOver<Dim>(A[i, i])  (diagonal contraction)
template <typename A>
constexpr auto decompose(Expression<TraceOp, A>) {
  using Dim = row_dim_t<A>;
  return sumOver<Dim>(A{}[IndexVar<Dim>{}, IndexVar<Dim>{}]);
}
```

**This is where ReduceOver (Phase 1) is critical.** Without generalized
reductions, matrix multiply can't be expressed as dimension contraction.

### IndexVar<DimTag>

A new symbol type: a "dimension index variable" that takes values
in 0..size(DimTag)-1 during evaluation. Different from a data symbol
or a parameter symbol — it's the loop variable of a ReduceOver.

```cpp
template <typename DimTag>
struct IndexVar : SymbolicTag {
  using dim_tag = DimTag;
  // Evaluated by looking up the current dimension index in the context
};
```

This may already be implicit in how indexed_eval.h handles SumOver
bodies — the IndexedSymbol's dimension tag is looked up in the
DimIndexMap to get the current loop variable value. IndexVar makes
this explicit and first-class.

## Task B.4: Matrix-Aware Evaluation

**Files to modify/create:**
- `src/symbolic4/matrix_algebra/matrix_eval.h`
- Updates to `src/symbolic4/indexed/indexed_eval.h`

### Binding MatrixSymbol to matrix3 types

```cpp
// Bind a matrix symbol to a numeric matrix
auto result = evaluate(expr,
    A = matrix3::Dense<double, 3, 4>{...},
    mu = std::vector<double>{1.0, 2.0, 3.0});
```

The binding layer converts:

| Symbolic type | Binds to |
|--------------|----------|
| `MatrixSymbol<Id, R, C, General>` | `matrix3::Dense<double, R, C>` or `DynamicDense` |
| `MatrixSymbol<Id, D, D, PositiveDefinite>` | Same, but checked |
| `MatrixSymbol<Id, D, D, LowerTriangular>` | Same, but only lower triangle used |
| `VectorSymbol<Id, D>` | `std::vector<double>` or `matrix3::Dense<double, N, 1>` |

### Fast-path pattern matching

The evaluator can recognize matrix-level patterns and dispatch to
matrix3 instead of scalar-by-scalar ReduceOver loops:

```cpp
if constexpr (is_matmul_expression_v<E>) {
    // Detected MatMul node — dispatch to matrix3::operator*
    auto lhs = evaluateAsMatrix(expr.template arg<0>(), ctx);
    auto rhs = evaluateAsMatrix(expr.template arg<1>(), ctx);
    return lhs * rhs;  // matrix3 tiled multiply
}
```

This is an **optimization**, not correctness — the scalar decomposition
via ReduceOver always gives the right answer, but matrix3's tiled
multiply is O(n^2.37) vs O(n^3) with better cache behavior.

### Integrating with existing matrix/eval.h

The current `matrix/eval.h` already handles DimVectorSymbol and
CholeskyCovSymbol evaluation. Generalize this to handle MatrixSymbol
by routing through the same DimVectorBinding/CholeskyBinding mechanism
but with the generalized types.

## Task B.5: Matrix-Aware Differentiation

**Files to modify:**
- `src/symbolic4/strategy/diff.h` — add matrix differentiation rules

### Key principle

`grad(scalar_expr)[matrix_symbol]` returns a matrix whose (i,j) component
is `∂f/∂M_{ij}`. The type of the result depends on the type of the symbol:

```
grad(f)[scalar_sym]  → scalar
grad(f)[vector_sym]  → vector
grad(f)[matrix_sym]  → matrix
```

### Matrix calculus identities (rewrite rules)

```cpp
// ∂/∂A trace(AB) = Bᵀ
// ∂/∂A trace(AᵀB) = B
// ∂/∂A trace(AᵀA) = 2A
// ∂/∂A log|A| = A⁻ᵀ (for positive definite: A⁻¹ since symmetric)
// ∂/∂L log|LLᵀ| = 2·tril(L⁻ᵀ)  (Cholesky parameterization)
```

These can be expressed as strategy rules:
```cpp
auto matDiff = recursive(
    rrule(trace(A_ * B_),  transpose(B_))   // ∂trace(AB)/∂A = Bᵀ
  | rrule(logDet(A_),      inverse(transpose(A_)))
  | ...
);
```

Or, since matrix derivative of a scalar can be expressed component-wise:
```
[∂f/∂A]_{ij} = ∂f/∂A_{ij}
```

This means we can decompose to scalar differentiation:
```cpp
// ∂f/∂A = matrix where component (i,j) = differentiate(f, A[i,j])
// This uses the scalar decomposition + existing diff infrastructure!
```

Both approaches should be available — matrix-level rules for common
patterns (fast), scalar decomposition as fallback (general).

### Chain rule through Cholesky

The most important specific case for MCMC. When A = LLᵀ and we
differentiate w.r.t. L:

```
∂f/∂L = (∂f/∂A · L + Lᵀ · ∂f/∂Aᵀ) where only lower triangle kept
```

This is well-studied (see Murray 2016 "Differentiation of the Cholesky
decomposition"). The rule should be implemented efficiently, not via
element-wise scalar differentiation.

## Task B.6: MCMC Integration

**Files to modify:**
- `src/symbolic4/mcmc/plate_transforms.h` — matrix parameter support
- `src/symbolic4/mcmc/samples.h` — samples[matrix_symbol] returns matrices
- `src/symbolic4/infer.h` — auto-discover matrix parameters

### Matrix RandomVar

A random variable whose value is a matrix:

```cpp
auto Sigma = wishartInverse(nu, S0);  // Inverse-Wishart prior on covariance
auto L_corr = lkjCholesky(eta);      // LKJ prior on correlation Cholesky

// These are RandomVar<Dist, Id> where the "value" is a matrix
// The transform maps unconstrained params → structured matrix
```

### Automatic parameterization

For a `PositiveDefinite` matrix symbol, the system automatically:
1. Parameterizes via Cholesky factor L (K(K+1)/2 params)
2. Applies exp transform to diagonal elements (positive)
3. Leaves off-diagonal unconstrained
4. Computes Jacobian correction

This is what `matrix/hmc.h` already does manually. The extension
makes it automatic for any PositiveDefinite MatrixSymbol.

### samples[matrix_symbol]

```cpp
auto samples = posterior.sample(config, init, rng);

samples[Sigma]        // returns collection of K×K matrices (one per draw)
samples[Sigma[0,1]]   // returns vector<double> of one component per draw
samples[L]            // returns collection of lower triangular matrices
samples[det(Sigma)]   // returns vector<double> of determinants (derived)
```

This requires `DynamicSamples::operator[]` to recognize matrix symbols
and reconstruct matrices from the flat state vector using the
parameterization's unflatten operation.

## Acceptance Criteria

- [ ] `MatrixSymbol<Id, R, C, Structure>` with all structure tags
- [ ] Structure preservation: `transpose(Symmetric<A>)` returns `Symmetric`
- [ ] `A * B` creates MatMul expression node
- [ ] `trace(A)`, `logDet(A)` produce scalar expressions
- [ ] `(A * B)[i, j]` decomposes to `SumOver<K>(A[i,k] * B[k,j])`
- [ ] Evaluation dispatches to matrix3 for matrix-level patterns
- [ ] `grad(trace(A*Aᵀ))[A]` produces correct matrix gradient
- [ ] `samples[matrix_symbol]` returns per-draw matrices
- [ ] Existing DimVectorSymbol/CholeskyCov/Corr still work (backwards compat)

## Build & Test

```bash
cmake --build build && ctest --test-dir build -R symbolic4
```
