# indexed/ - Plate Notation for Vectorized Parameters

## Purpose

This directory implements plate notation for hierarchical models with repeated structure (i.i.d. observations, group-level parameters).

## Key Files

| File | Purpose |
|------|---------|
| `dim.h` | Dimension tags and IndexedSymbol |
| `indexed.h` | Umbrella header, plate factories |
| `sum_over.h` | Symbolic summation (Σᵢ f(i)) |
| `sum_over_diff.h` | Differentiation through sums |
| `indexed_eval.h` | Evaluation with index bindings |
| `data.h` | IndexedValues wrappers for observed data |

## Core Concepts

### Dimension Tags

Empty structs that name dimensions:
```cpp
struct Countries {};
struct Years {};
struct Observations {};
```

Dimensions get concrete sizes at model construction, not compile time.

### IndexedSymbol

A symbol that varies over one or more dimensions:
```cpp
template <typename Id, typename... Dims>
struct IndexedSymbol : SymbolicTag {
  using dims_list = TypeList<Dims...>;
  static constexpr SizeT ndims = sizeof...(Dims);
};
```

Examples:
- `IndexedSymbol<ThetaId, Countries>` → θ[c]
- `IndexedSymbol<YId, Countries, Years>` → y[c,y]

### Plate Notation

Create indexed random variables:
```cpp
auto alpha = plate<Countries>(normal(mu, sigma));  // α[c] ~ Normal(μ, σ)
auto y = plate<Years>(plate<Countries>(normal(alpha, lit(1))));  // y[c,y]
```

The resulting `IndexedRandomVar` has:
- `sym()` returning an `IndexedSymbol`
- `logProb()` that sums over all indices

## SumOver - Symbolic Summation

```cpp
template <typename DimTag, Symbolic E>
struct SumOver : SymbolicTag {
  E body_;
  SizeT size_;  // Runtime dimension size
};
```

Represents Σᵢ₌₀^(n-1) f(i) symbolically. Evaluation loops over indices:
```cpp
double result = 0;
for (SizeT i = 0; i < size; ++i) {
  result += evaluate(body, index<DimTag> = i, bindings...);
}
```

## Data Binding

Observed data is bound via `indexed()`:
```cpp
std::vector<double> data = {...};
auto posterior = model(theta, y).posterior()
    .withDimension<Countries>(38)
    .observe(y = indexed(data))
    .build();
```

For multi-dimensional data:
```cpp
// 10 countries × 20 years
auto data_2d = IndexedValuesND<Countries, Years>(raw_data, {10, 20});
```

## Evaluation with Indices

`indexed_eval.h` extends evaluation to handle index lookups:
```cpp
// During evaluation of SumOver<Countries, E>
// Inside the loop: evaluate(body, index<Countries> = i, ...)
```

The context carries current index values for each active dimension.

## Differentiation through Sums

`sum_over_diff.h` handles:
```cpp
d/dθ[Σᵢ f(θ, xᵢ)] = Σᵢ d/dθ[f(θ, xᵢ)]
```

Differentiation distributes into the sum, preserving the SumOver structure.

## Integration with Model

Plate support in `model.h`:
1. `Model::HasPlates` detects if any RV is indexed
2. `PosteriorBuilderFromModel<_, true>` handles dimension setup
3. `.withDimension<Tag>(size)` sets runtime dimension sizes
4. State vector layout: `[scalar_params, indexed_params[0..n-1]]`

## Example: Hierarchical Model

```cpp
struct Groups {};

// Hyperparameters
auto mu_0 = normal(0, 10);
auto sigma_0 = halfNormal(5);

// Group-level parameters
auto theta = plate<Groups>(normal(mu_0, sigma_0));  // θ[g]

// Observations (implicit sum over observations)
auto y = normal(theta, lit(1));

auto m = model(mu_0, sigma_0, theta, y);
auto posterior = m.posterior()
    .withDimension<Groups>(50)
    .observe(y = indexed(data))
    .build();
```
