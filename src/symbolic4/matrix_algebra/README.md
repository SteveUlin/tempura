# Matrix Algebra Extension

## Vision

Extend symbolic4 with general-purpose matrix algebra, unified under the
symbol-lookup pattern. Matrices, vectors, and plate parameters are all
**indexed families of scalars over typed dimensions** — the same abstraction
at different ranks.

```cpp
// Matrix symbols with typed structure
auto Sigma = covMatrix<Dims>();              // PositiveDefinite<Dims, Dims>
auto L = cholesky(Sigma);                    // LowerTriangular — symbolic
auto mu = dimVector<Dims>();                 // Vector<Dims>

// Natural syntax via operator overloads
auto C = A * B;                              // MatMul expression node
auto d = trace(A * transpose(A));            // scalar expression

// Scalar decomposition via operator[]
(A * B)[i, j]                                // SumOver<K>(A[i,k] * B[k,j])
//                                              ReduceOver IS Einstein summation

// Gradient respects shapes
auto g = grad(logProb);
g[mu]                                        // vector gradient
g[Sigma]                                     // matrix gradient (symmetric)
g[sigma]                                     // scalar gradient

// Samples via symbol lookup
samples[Sigma]                               // per-draw matrices
samples[Sigma[0, 1]]                         // per-draw scalar: one covariance component
samples[det(Sigma)]                          // derived quantity
```

## Core Insight: Unification with Plates and ReduceOver

Matrices, vectors, and plate parameters are the same thing at different ranks:

| Object | Free dimension indices | Existing analog |
|--------|----------------------|-----------------|
| Scalar | none | `Symbol<X>` |
| Vector | `v[i]`, `i ∈ Dim` | `plate<Dim>(dist)` |
| Matrix | `M[i,j]`, `i ∈ Rows, j ∈ Cols` | `plate<Rows, Cols>(dist)` |
| 3-tensor | `T[i,j,k]` | `plate<D1, D2, D3>(dist)` |

All reductions are ReduceOver with different index patterns:

| Operation | Contracted dim | Free dims | Result |
|-----------|---------------|-----------|--------|
| Plate log-lik `SumOver<Obs>(lp)` | Obs | none | scalar |
| Dot product `dot(v, w)` | Dims | none | scalar |
| Matrix × vector `A * v` | Cols | Rows | vector |
| Matrix × matrix `A * B` | K (shared) | Rows, Cols | matrix |
| Trace `trace(A)` | Dims (diagonal) | none | scalar |

**Einstein summation is ReduceOver.** The Phase 1 ReduceOver work is the
foundation for all index contraction, including matrix algebra.

## Two Parallel Tracks

```
Track A: matrix3 Numeric Extensions          Track B: Symbolic Matrix Algebra
(independent, start immediately)             (builds on migration phases)

A.1: Cholesky decomposition                  B.1: Multi-indexed symbols + structure tags
A.2: Matrix inverse                          B.2: Matrix expression nodes
A.3: Eigendecomposition (symmetric)          B.3: Scalar decomposition via operator[]
A.4: QR decomposition                              (depends on Phase 1: ReduceOver)
                                             B.4: Matrix-aware evaluation
                                                   (depends on Track A)
                                             B.5: Matrix-aware differentiation
                                                   (depends on Phase 2: differentiate)
                                             B.6: MCMC integration
                                                   (depends on Phase 6: samples[expr])
```

Track A has NO dependencies — it extends the numeric library independently.
Track B phases have dependencies on both Track A and the strategy migration.

## Dependency Graph

```
                Phase 1 (ReduceOver)
                    │
Track A ───────────┐│
(any time)         ││
                   ▼▼
A.1 Cholesky    B.1 Multi-indexed symbols
A.2 Inverse     B.2 Matrix expression nodes
A.3 Eigen          │
A.4 QR             ▼
   │            B.3 Scalar decomposition (needs Phase 1: ReduceOver)
   │               │
   └──────────►B.4 Matrix evaluation (needs A.1+ for numeric dispatch)
                   │
                   ▼     Phase 2 (differentiate)
               B.5 Matrix differentiation ◄────┘
                   │
                   ▼     Phase 6 (samples[expr])
               B.6 MCMC integration ◄──────────┘
```

## What Already Exists

### In matrix3 (numeric)
- Dense, InlineDense, DynamicDense matrix types
- LU decomposition with pivoting, solve, determinant
- Matrix multiply (tiled, cache-optimized)
- Transpose, submatrix, block views (zero-copy)
- Kronecker product
- C++26 `operator[i, j]`, deducing this

### In symbolic4/matrix (symbolic, domain-specific)
- DimVectorSymbol, CholeskyCovSymbol, CholeskyCorrSymbol
- Matrix ops: dot, logDetChol, solveTriangular, quadFormChol, diagPreMult
- MVNormalCholeskyDist, LKJCholeskyDist
- Direct-recursion evaluator (matrix/eval.h)
- Cholesky transforms for HMC (matrix/hmc.h)

### In symbolic4/indexed (reusable infrastructure)
- `IndexedSymbol<Id, Dims...>` — **already variadic** (supports multi-dim)
- `IndexedValuesND<Dims...>` — N-D storage with 1D/2D/3D specializations
- `IndexedBinding<Sym, NDims>` — binding with dimension size tracking
- ReduceOver (after Phase 1) — typed dimension contraction

## Build & Test

```bash
# matrix3 tests
cmake --build build && ctest --test-dir build -L matrix3

# symbolic4 tests
cmake --build build && ctest --test-dir build -R symbolic4

# All
cmake --build build && ctest --test-dir build
```
