# matrix/ - Multivariate Distributions and Matrix Operations

## Purpose

This directory extends symbolic4 to matrix-valued parameters for multivariate distributions (MVN, LKJ correlation).

## Key Files

| File | Purpose |
|------|---------|
| `types.h` | DimVectorSymbol, CholeskyCovSymbol, CholeskyCorrSymbol |
| `mvn.h` | Multivariate Normal distribution |
| `lkj.h` | LKJ correlation distribution |
| `ops.h` | Matrix operations (multiply, transpose) |
| `eval.h` | Matrix expression evaluation |
| `diff.h` | Differentiation for matrix expressions |
| `hmc.h` | HMC integration for matrix parameters |
| `plate_integration.h` | Combining matrices with plates |
| `matrix.h` | Umbrella header |

## Dimension-Tagged Types

Unlike fixed-dimension types, these use dimension tags:

### DimVectorSymbol
```cpp
template <typename Id, typename DimTag>
struct DimVectorSymbol;

// μ ∈ ℝ^N where N = |Countries|
auto mu = dimVector<Countries>();
```

### CholeskyCovSymbol
```cpp
template <typename Id, typename DimTag>
struct CholeskyCovSymbol;

// L is K×K lower triangular, Σ = LLᵀ
auto L = choleskyCov<Countries>();  // K(K+1)/2 parameters
```

### CholeskyCorrSymbol
```cpp
template <typename Id, typename DimTag>
struct CholeskyCorrSymbol;

// L for correlation matrix (unit row norms)
auto L = choleskyCorr<Countries>();  // K(K-1)/2 parameters
```

## Multivariate Normal

```cpp
template <Symbolic Mean, Symbolic CholeskyL>
struct MultivariateNormalDist {
  Mean mu_;
  CholeskyL l_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const;
  // Returns: -0.5 * ||L⁻¹(x-μ)||² - Σ log(L_ii) - (K/2)log(2π)
};
```

Log-prob uses the Cholesky factor directly to avoid matrix inversion:
- Solve L z = (x - μ) via forward substitution
- Compute -0.5 * zᵀz
- Subtract log determinant: Σ log(L_ii)

## LKJ Correlation Distribution

Prior over correlation matrices:
```cpp
template <Symbolic Eta>
struct LKJDist {
  Eta eta_;  // Concentration parameter (η > 0)

  // η = 1: uniform over correlation matrices
  // η > 1: shrinks toward identity
  // η < 1: favors extreme correlations
};
```

Log-prob computed from Cholesky factor without forming full matrix.

## Matrix Operations

Symbolic operations in `ops.h`:
```cpp
// Matrix-vector multiply
auto y = matVecMul(L, x);  // y = Lx

// Triangular solve (forward substitution)
auto z = triSolve(L, y);  // z where Lz = y

// Cholesky decomposition (symbolic marker)
auto L = cholesky(Sigma);
```

## Evaluation

`eval.h` extends evaluation to matrix types:
- `DynamicMatrix` for runtime-sized matrices
- `std::vector<double>` for vectors
- Bindings carry matrix/vector values

## Differentiation

`diff.h` handles matrix derivatives:
- Gradients flow through matrix operations
- Cholesky derivatives use efficient formulas
- No explicit matrix inversion in backward pass

## HMC Integration

`hmc.h` provides `MatrixHMCAdapter`:
- Packs matrix parameters into flat state vector
- Unpacks for evaluation
- Handles Cholesky transform (positive diagonal)

State layout:
```
[scalar_params, vector_elements, cholesky_lower_triangle]
```

## Example: Multivariate Hierarchical Model

```cpp
struct Dims {};  // K dimensions

// Correlation structure
auto L_corr = lkjCorr<Dims>(lit(2.0));  // η = 2

// Scales
auto sigma = plate<Dims>(halfNormal(lit(1.0)));

// Full covariance: Σ = diag(σ) * Ω * diag(σ) where Ω = L_corr * L_corrᵀ
auto mu = dimVector<Dims>();
auto y = mvNormal(mu, scaledCorrelation(sigma, L_corr));

auto posterior = model(mu, sigma, L_corr, y).posterior()
    .withDimension<Dims>(5)
    .observe(y = data_matrix)
    .build();
```

## Current Status

Working:
- Basic matrix type system
- MVN log-probability
- LKJ distribution
- Forward evaluation

In progress:
- Full HMC integration
- Efficient matrix differentiation
- Plate × matrix combinations
