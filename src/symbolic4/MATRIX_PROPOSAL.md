# Matrix Support for symbolic4

## Summary

This proposal adds matrix-valued random variables to symbolic4, enabling multivariate normal distributions, correlation matrices with LKJ priors, and covariance matrices. The design follows Stan's Cholesky-first approach while integrating with our existing dimension/plate system.

## Motivation

Many statistical models require:
- **Multivariate normal likelihoods**: `y ~ MVN(μ, Σ)` for correlated observations
- **Correlation priors**: LKJ prior for correlation matrices in hierarchical models
- **Covariance decomposition**: `Σ = diag(τ) × Ω × diag(τ)` separating scale from correlation

Current symbolic4 can express scalar and indexed-scalar models, but not matrix-valued parameters.

## Design Principles

1. **Cholesky-first**: Always parameterize by Cholesky factors, never raw matrices
2. **Dimension tags**: Matrix sizes come from dimension tags (like plates)
3. **Type-safe dimensions**: Row/column dimensions are distinct types
4. **Symbolic differentiation**: Full matrix calculus through the expression system
5. **Stan-compatible**: Similar API patterns for users familiar with Stan

---

## 1. Core Types

### 1.1 Dimension-Tagged Vectors

```cpp
// A vector whose length is determined by a dimension tag
template <typename Id, typename DimTag>
struct VectorSymbol : SymbolicTag {
  using id_type = Id;
  using dim_tag = DimTag;

  // At evaluation time, dimension size is known
  // VectorSymbol<X, Countries> has length = size of Countries dimension
};

// Convenience: create vector symbol
template <typename DimTag, typename Id = decltype([]{})*>
constexpr auto vectorSym() -> VectorSymbol<Id, DimTag>;

// Example:
// struct Countries;  // dimension tag
// auto mu = vectorSym<Countries>();  // μ ∈ ℝ^N where N = |Countries|
```

### 1.2 Cholesky Factor Types

```cpp
// Cholesky factor of a correlation matrix (K×K lower triangular, unit diagonal)
// Constrained: L[i,i] > 0, L*L' has unit diagonal
template <typename Id, typename DimTag>
struct CholeskyCorrSymbol : SymbolicTag {
  using id_type = Id;
  using dim_tag = DimTag;
  static constexpr bool is_correlation = true;

  // Size: K×K where K = size of DimTag
  // Actual parameters: K*(K-1)/2 (off-diagonal elements of L)
};

// Cholesky factor of a covariance matrix (K×K lower triangular)
// Constrained: L[i,i] > 0
template <typename Id, typename DimTag>
struct CholeskyCovSymbol : SymbolicTag {
  using id_type = Id;
  using dim_tag = DimTag;
  static constexpr bool is_correlation = false;

  // Size: K×K where K = size of DimTag
  // Actual parameters: K*(K+1)/2 (lower triangle of L)
};
```

### 1.3 Matrix Operations (Expression Operators)

```cpp
// Matrix multiplication: C = A × B
struct MatMulOp {
  template <typename A, typename B>
  auto operator()(const A& a, const B& b) const;
};

// Transpose: B = Aᵀ
struct TransposeOp {
  template <typename A>
  auto operator()(const A& a) const;
};

// Form covariance from Cholesky: Σ = L × Lᵀ
struct CholToCovarOp {
  template <typename L>
  auto operator()(const L& l) const;
};

// Log determinant: log|det(A)|
struct LogDetOp {
  template <typename A>
  auto operator()(const A& a) const;
};

// Quadratic form: xᵀ A⁻¹ x (used in MVN log-prob)
struct QuadFormOp {
  template <typename X, typename L>
  auto operator()(const X& x, const L& l) const;  // x, Cholesky factor
};

// Scale Cholesky by diagonal: diag(τ) × L
struct DiagPreMultiplyOp {
  template <typename Tau, typename L>
  auto operator()(const Tau& tau, const L& l) const;
};
```

---

## 2. Distributions

### 2.1 LKJ Correlation Prior

The LKJ distribution is a prior over correlation matrices parameterized by shape η:
- η = 1: uniform over correlation matrices
- η > 1: shrinks toward identity (less correlation)
- η < 1: favors more correlation

```cpp
// LKJ prior on Cholesky factor of correlation matrix
template <typename Eta>
struct LKJCholeskyDist {
  Eta eta;  // shape parameter (symbolic expression)

  // Log-prob for Cholesky factor L where L*L' ~ LKJ(η)
  template <typename L>
  constexpr auto logProb(L l_chol) const {
    // log p(L | η) = Σᵢ (K - i + 2η - 2) * log(L[i,i])
    // where K is the dimension
    // This is the Cholesky parameterization from Stan
    return lkjCholeskyLogProb(l_chol, eta);
  }
};

// Helper to create LKJ distribution
template <typename Eta>
constexpr auto lkjCholesky(Eta eta) {
  return LKJCholeskyDist<Eta>{eta};
}

// Usage:
// auto L_Omega = cholCorr<Predictors>();  // Cholesky of K×K correlation
// L_Omega ~ lkjCholesky(lit(2.0));        // LKJ(2) prior
```

### 2.2 Multivariate Normal

```cpp
// Multivariate normal with Cholesky factor of covariance
template <typename Mean, typename CholCov>
struct MVNormalCholeskyDist {
  Mean mu;         // mean vector (K-dimensional)
  CholCov l_sigma; // Cholesky factor of covariance (K×K)

  // Log-prob: log p(y | μ, L) where Σ = L × Lᵀ
  template <typename Y>
  constexpr auto logProb(Y y) const {
    // -K/2 log(2π) - Σᵢ log(L[i,i]) - 0.5 * ||L⁻¹(y-μ)||²
    auto k = dimensionSize(mu);  // symbolic or literal
    auto diff = y - mu;
    auto z = solveTriangular(l_sigma, diff);  // L⁻¹(y-μ)

    return -lit(0.5) * k * log(2_c * pi())
           - sumDiagonalLog(l_sigma)
           - lit(0.5) * dot(z, z);
  }
};

// Helper
template <typename M, typename L>
constexpr auto mvNormalCholesky(M mean, L l_cov) {
  return MVNormalCholeskyDist<M, L>{mean, l_cov};
}
```

### 2.3 Decomposed Covariance (Scale + Correlation)

Stan's recommended pattern separates scale and correlation:

```cpp
// Covariance = diag(τ) × Ω × diag(τ) where Ω is correlation
// Cholesky: L_Σ = diag(τ) × L_Ω

template <typename Tau, typename LCorr>
struct ScaledCholeskyCov {
  Tau tau;        // scale vector (K positive values)
  LCorr l_omega;  // Cholesky of correlation (K×K)

  // Returns Cholesky of full covariance
  constexpr auto cholCov() const {
    return diagPreMultiply(tau, l_omega);
  }
};

// Usage pattern (matching Stan):
// auto tau = plate<Predictors>(halfNormal(lit(2.5)));  // K scales
// auto L_Omega = cholCorr<Predictors>();               // K×K correlation Cholesky
// L_Omega ~ lkjCholesky(lit(1.0));
// auto L_Sigma = diagPreMultiply(tau, L_Omega);        // Full covariance Cholesky
// y ~ mvNormalCholesky(mu, L_Sigma);
```

---

## 3. Plate Interaction

### 3.1 Per-Observation MVN Likelihood

The most common pattern: N observations from same MVN:

```cpp
// Dimension tags
struct Observations;  // N observations
struct Features;      // K features per observation

// Model
auto mu = vectorSym<Features>();                    // K-dimensional mean
mu ~ plate<Features>(normal(lit(0.0), lit(10.0))); // Prior on each component

auto L_Sigma = cholCov<Features>();                 // K×K covariance Cholesky
// ... priors on L_Sigma ...

// Likelihood: yᵢ ~ MVN(μ, Σ) for i = 1..N
auto y = plate<Observations>(mvNormalCholesky(mu, L_Sigma));

// Data binding:
// y_data is N×K matrix, mu is K-vector, L_Sigma is K×K
```

The plate over `Observations` means:
- `y[i]` is a K-dimensional vector for observation i
- Log-likelihood sums over i: `Σᵢ log MVN(y[i] | μ, Σ)`

### 3.2 Group-Level Covariances

Hierarchical models with per-group effects:

```cpp
struct Groups;     // J groups
struct Predictors; // K predictors

// Varying slopes: each group has K coefficients
// β[j] ~ MVN(γ, Σ) for j = 1..J

auto gamma = vectorSym<Predictors>();  // Population mean
auto L_Sigma = cholCov<Predictors>();  // Population covariance Cholesky

// Group coefficients as plate
auto beta = plate<Groups>(mvNormalCholesky(gamma, L_Sigma));

// Now beta[j] is a K-vector for group j
```

### 3.3 Dimension-Sized Matrices

What if matrix size equals a plate dimension?

```cpp
struct Items;  // N items

// N×N covariance matrix (e.g., GP kernel)
auto L_K = cholCov<Items>();  // N×N Cholesky

// This is supported but expensive: O(N³) operations
// Use sparingly, or with specialized GP approximations
```

---

## 4. Differentiation

Matrix calculus rules for symbolic differentiation.

### 4.1 Scalar Derivatives Through Matrix Ops

For HMC, we need ∂/∂θ log p(θ) where θ may include matrix parameters (as flattened vectors).

**Key insight**: We don't differentiate "with respect to a matrix". We differentiate with respect to the free parameters (the lower triangle of L, or the unconstrained parameterization).

```cpp
// For CholeskyCorrSymbol<Id, DimTag>:
// - Free parameters: K*(K-1)/2 values (off-diagonal of L)
// - Diagonal is determined by unit-diagonal constraint
// - diff(expr, L[i,j]) for i > j

// For CholeskyCovSymbol<Id, DimTag>:
// - Free parameters: K*(K+1)/2 values (lower triangle including diagonal)
// - diff(expr, L[i,j]) for i >= j
```

### 4.2 Differentiation Rules

```cpp
// Log-determinant of Cholesky: log|det(L)| = Σᵢ log(L[i,i])
// d/dL[i,i] log|det(L)| = 1/L[i,i]
// d/dL[i,j] log|det(L)| = 0 for i ≠ j

template <typename P>
struct Diff::Rule<LogDetCholeskyOp, P> {
  static constexpr auto apply(P p) {
    // p.first = L (the Cholesky factor)
    // p.second = dL (derivative of L w.r.t. some scalar)
    // Result: d/dθ log|det(L)| = Σᵢ (1/L[i,i]) * dL[i,i]/dθ
    return sumDiagonalInvMul(p.first, diagonalOf(p.second));
  }
};

// Quadratic form: q = xᵀ Σ⁻¹ x = ||L⁻¹x||²
// d/dx q = 2 Σ⁻¹ x = 2 L⁻ᵀ L⁻¹ x
// d/dL q = -2 L⁻ᵀ (L⁻¹ x)(L⁻¹ x)ᵀ L⁻ᵀ  (more complex)

template <typename X, typename L>
struct Diff::Rule<QuadFormOp, X, L> {
  static constexpr auto apply(X x_pair, L l_pair) {
    auto [x, dx] = x_pair;
    auto [l, dl] = l_pair;

    auto z = solveTriangular(l, x);  // L⁻¹ x

    // d/dx term: 2 * L⁻ᵀ z * dx
    auto grad_x = lit(2.0) * solveTriangularTranspose(l, z);
    auto term_x = dot(grad_x, dx);

    // d/dL term: -2 * L⁻ᵀ outer(z,z) L⁻ᵀ ⊙ dL  (Hadamard product, lower tri)
    // This is more involved...
    auto term_l = quadFormCholeskyGrad(l, z, dl);

    return term_x + term_l;
  }
};
```

### 4.3 Handling Matrix Parameters in HMC

HMC works with flat parameter vectors. Matrix parameters are flattened:

```cpp
// For K×K Cholesky correlation:
// - K*(K-1)/2 free parameters (strict lower triangle)
// - Stored in column-major order: L[1,0], L[2,0], L[2,1], L[3,0], ...

// For K×K Cholesky covariance:
// - K*(K+1)/2 free parameters (lower triangle including diagonal)
// - Diagonal stored as log(L[i,i]) for unconstrained optimization

template <typename DimTag>
struct CholeskyParameterization {
  // Pack Cholesky factor into flat parameter vector
  static auto flatten(const Matrix<K,K>& L) -> Vector<NumParams>;

  // Unpack flat parameters into Cholesky factor
  static auto unflatten(const Vector<NumParams>& theta) -> Matrix<K,K>;

  // Jacobian adjustment for change of variables
  static auto logJacobian(const Vector<NumParams>& theta) -> double;
};
```

---

## 5. Evaluation

### 5.1 Bindings for Matrix Symbols

```cpp
// Binding a vector symbol
auto mu = vectorSym<Countries>();
Vector<kNumCountries> mu_values = {...};
// mu = mu_values binds the entire vector

// Binding a Cholesky symbol
auto L = cholCov<Predictors>();
Matrix<K, K> L_values = {...};  // Lower triangular
// L = L_values binds the matrix

// The evaluator handles matrix operations:
// - matmul(A, B) → runtime matrix multiply
// - logDet(L) → sum of log diagonal
// - quadForm(x, L) → solve and dot
```

### 5.2 Integration with IndexedEval

For plates containing MVN, evaluation loops over the plate dimension:

```cpp
// y = plate<Observations>(mvNormalCholesky(mu, L_Sigma))
// log p(y|μ,Σ) = Σᵢ log MVN(y[i] | μ, L_Σ)

// IndexedEval handles this:
// - Outer loop over Observations dimension
// - For each i: compute MVN log-prob for y[i]
// - Sum the results
```

---

## 6. Complete Example

Hierarchical model with varying slopes (matching Stan's recommended pattern):

```cpp
// Dimensions
struct Groups;      // J groups
struct Predictors;  // K predictors (including intercept)
struct Observations;// N total observations

// === Hyperpriors ===

// Population-level means for each predictor
auto gamma = plate<Predictors>(normal(lit(0.0), lit(5.0)));

// Scales for each predictor (half-normal)
auto tau = plate<Predictors>(halfNormal(lit(2.5)));

// Correlation matrix Cholesky factor
auto L_Omega = cholCorr<Predictors>();
L_Omega ~ lkjCholesky(lit(2.0));  // LKJ(2) prior

// === Group-level parameters ===

// Non-centered parameterization for better sampling
// z[j] ~ MVN(0, I) then β[j] = γ + diag(τ) × L_Ω × z[j]
auto z = plate<Groups>(plate<Predictors>(normal(lit(0.0), lit(1.0))));

// Transform to get actual coefficients
auto beta = [&](auto j) {
  return gamma + diagPreMultiply(tau, L_Omega) * z[j];
};

// === Likelihood ===

auto sigma = halfNormal(lit(1.0));  // Observation noise

// For each observation n in group group[n]:
// y[n] ~ Normal(x[n] · β[group[n]], σ)
auto y = plate<Observations>([&](auto n) {
  auto j = group[n];  // group membership
  return normal(dot(x[n], beta(j)), sigma);
});

// === Build model ===
auto m = model(gamma, tau, L_Omega, z, sigma, y);

auto posterior = m.posterior()
    .withDimension<Groups>(J)
    .withDimension<Predictors>(K)
    .withDimension<Observations>(N)
    .bindData(x = design_matrix)
    .bindData(group = group_indices)
    .observe(y = y_values)
    .build();
```

---

## 7. Implementation Phases

### Phase 1: Core Types (foundation)
- [ ] `VectorSymbol<Id, DimTag>`
- [ ] `CholeskyCorrSymbol<Id, DimTag>`
- [ ] `CholeskyCovSymbol<Id, DimTag>`
- [ ] Type traits: `is_vector_symbol_v`, `is_cholesky_symbol_v`
- [ ] Binding support for vector/matrix values

### Phase 2: Basic Operations
- [ ] `LogDetCholeskyOp` - log determinant of Cholesky
- [ ] `SolveTriangularOp` - L⁻¹ x
- [ ] `DiagPreMultiplyOp` - diag(τ) × L
- [ ] `DotOp` - vector dot product (already exists?)
- [ ] Evaluation for these ops

### Phase 3: Distributions
- [ ] `LKJCholeskyDist` - LKJ prior on correlation Cholesky
- [ ] `MVNormalCholeskyDist` - multivariate normal
- [ ] Log-prob expressions for both
- [ ] Tests comparing to Stan/manual calculations

### Phase 4: Differentiation
- [ ] `diff` rules for `LogDetCholeskyOp`
- [ ] `diff` rules for `SolveTriangularOp`
- [ ] `diff` rules for `DiagPreMultiplyOp`
- [ ] `diff` rules for LKJ log-prob
- [ ] `diff` rules for MVN log-prob
- [ ] Gradient tests (finite differences)

### Phase 5: Integration
- [ ] Plate interaction (MVN in plates)
- [ ] Non-centered parameterization helpers
- [ ] Flattening/unflattening for HMC
- [ ] End-to-end hierarchical model test

---

## 8. Open Questions

1. **Sparse matrices**: For GPs with N×N kernel, should we support sparse Cholesky?

2. **Block structure**: Hierarchical models often have block-diagonal covariances. Specialize?

3. **Compile-time dimensions**: Should we support compile-time known dimensions (Eigen-style) in addition to runtime dimensions?

4. **Matrix literals**: How to bind matrix data elegantly?
   ```cpp
   L = matrix_literal{{1,0,0}, {0.5,0.866,0}, {0.3,0.4,0.866}}
   // vs
   L = cholesky_from_data(flat_values, K)
   ```

5. **Error messages**: Matrix dimension mismatches need clear compile-time errors.

---

## References

- [Stan User's Guide: Multivariate Priors](https://mc-stan.org/docs/2_18/stan-users-guide/multivariate-hierarchical-priors-section.html)
- [Stan Functions: Cholesky LKJ](https://mc-stan.org/docs/2_21/functions-reference/cholesky-lkj-correlation-distribution.html)
- [Lewandowski, Kurowicka, Joe (2009). "Generating random correlation matrices"](https://www.sciencedirect.com/science/article/pii/S0047259X09000876)
- Matrix Cookbook (Petersen & Pedersen) for differentiation formulas
