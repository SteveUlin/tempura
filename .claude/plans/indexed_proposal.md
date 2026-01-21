# Proposal: Indexed Symbols for bayes2

## Design Goals

1. **Fit naturally with bayes2's architecture** - no ctx object, dependencies in types
2. **Dimension info flows through types** - compile-time tracking
3. **Minimal changes to symbolic3** - extend, don't rewrite
4. **Beautiful syntax** - should feel natural to use

## Core Insight

The key insight: **a plate is not N different RandomVars, it's ONE RandomVar whose symbol varies over a dimension**.

```cpp
// Current: one scalar
auto theta = beta(alpha, beta_param);  // theta is a scalar

// Proposed: one symbol that varies over Countries
auto theta = beta<Countries>(alpha, beta_param);  // theta[i] for each country
```

The RandomVar is still a single compile-time entity. Its symbol just carries dimension information.

---

## Part 1: Dimension Tags (in symbolic3)

```cpp
// symbolic3/indexed.h

namespace tempura::symbolic3 {

// A dimension is a named axis - just a tag type
// The actual size is provided at evaluation time
template <typename Tag>
struct Dim {
  using tag = Tag;
};

// Example dimension declarations (user-defined)
// struct Countries {};
// struct Observations {};
// struct TimeSteps {};

// Check if a type is a dimension
template <typename T>
constexpr bool is_dim = false;

template <typename Tag>
constexpr bool is_dim<Dim<Tag>> = true;

}  // namespace tempura::symbolic3
```

---

## Part 2: Indexed Symbols (in symbolic3)

```cpp
// symbolic3/indexed.h (continued)

namespace tempura::symbolic3 {

// An indexed symbol represents a family of values varying over dimensions
// IndexedSymbol<BaseId, Dim<Countries>> means "one value per country"
// IndexedSymbol<BaseId, Dim<Countries>, Dim<Time>> means "one per (country, time)"
template <typename BaseId, typename... Dims>
struct IndexedSymbol : SymbolicTag {
  using base_id = BaseId;
  using dimensions = TypeList<Dims...>;

  static constexpr std::size_t rank = sizeof...(Dims);
  static constexpr bool is_scalar = (rank == 0);

  // For binding syntax
  constexpr auto operator=(auto value) const;
};

// Type trait to get dimensions of any symbolic type
template <typename T>
struct get_dimensions {
  using type = TypeList<>;  // scalars have no dimensions
};

template <typename BaseId, typename... Dims>
struct get_dimensions<IndexedSymbol<BaseId, Dims...>> {
  using type = TypeList<Dims...>;
};

template <typename T>
using dimensions_of = typename get_dimensions<T>::type;

// Check if a symbol is indexed over a specific dimension
template <typename Sym, typename Dim>
constexpr bool is_indexed_over = Contains<Dim, dimensions_of<Sym>>::value;

}  // namespace tempura::symbolic3
```

---

## Part 3: Reduction Operators (in symbolic3)

```cpp
// symbolic3/reduction.h

namespace tempura::symbolic3 {

// Sum over a dimension: Σᵢ expr
// The dimension is eliminated from the result
template <typename DimTag, Symbolic Expr>
struct SumOver : SymbolicTag {
  using dim = Dim<DimTag>;
  using inner_type = Expr;

  // Result dimensions = Expr dimensions minus the summed dimension
  using result_dimensions = Remove<dim, dimensions_of<Expr>>;

  [[no_unique_address]] Expr expr_;

  constexpr SumOver(Expr e) : expr_{e} {}
};

// Factory function
template <typename DimTag, Symbolic Expr>
constexpr auto sumOver(Expr expr) {
  return SumOver<DimTag, Expr>{expr};
}

// Product over a dimension: Πᵢ expr
template <typename DimTag, Symbolic Expr>
struct ProductOver : SymbolicTag {
  using dim = Dim<DimTag>;
  [[no_unique_address]] Expr expr_;
};

template <typename DimTag, Symbolic Expr>
constexpr auto productOver(Expr expr) {
  return ProductOver<DimTag, Expr>{expr};
}

}  // namespace tempura::symbolic3
```

---

## Part 4: Differentiation Rules for Reductions

```cpp
// symbolic3/derivative.h (additions)

// Key rule: d/dx [Σᵢ f(x, yᵢ)] = Σᵢ [d/dx f(x, yᵢ)]
// This holds when x is NOT indexed over the summed dimension

template <typename DimTag, Symbolic Expr, Symbolic Var>
constexpr auto diff(SumOver<DimTag, Expr> sum, Var var) {
  // Check: is var indexed over this dimension?
  if constexpr (is_indexed_over<Var, Dim<DimTag>>) {
    // var is indexed - this produces an indexed result (Jacobian-like)
    // For now, static_assert to disallow this case
    static_assert(!is_indexed_over<Var, Dim<DimTag>>,
        "Differentiating sum w.r.t. indexed variable not yet supported");
  } else {
    // var is scalar - differentiation commutes with sum
    return sumOver<DimTag>(diff(sum.expr_, var));
  }
}
```

---

## Part 5: Integration with bayes2

Now the key part - how this integrates with `RandomVar`:

```cpp
// bayes2/indexed.h

namespace tempura::bayes2 {

// An IndexedRandomVar is a RandomVar whose symbol is indexed
// It represents: for each i in Dim, sample theta[i] ~ Dist
template <typename Id, typename Dist, typename DimTag, typename... Parents>
struct IndexedRandomVar {
  using id_type = Id;
  using dist_type = Dist;
  using dim_type = Dim<DimTag>;
  using symbol_type = IndexedSymbol<Id, Dim<DimTag>>;

  [[no_unique_address]] Dist dist_;
  [[no_unique_address]] std::tuple<Parents...> parents_;

  static constexpr auto sym() { return symbol_type{}; }

  // Log-prob for ONE instance (symbolic template)
  constexpr auto instanceLogProb() const {
    return dist_.logProbFor(sym());
  }

  // Total log-prob = sum over dimension
  constexpr auto nodeLogProb() const {
    return sumOver<DimTag>(instanceLogProb());
  }

  constexpr const auto& parents() const { return parents_; }

  // Binding takes a span of values
  constexpr auto operator=(std::span<const double> values) const {
    return IndexedBinding<symbol_type>{values};
  }
};

// Factory: beta<Countries>(alpha, beta_param) creates an IndexedRandomVar
template <typename DimTag, typename Id = decltype([] {}), typename Alpha, typename Beta>
constexpr auto beta(const Alpha& a, const Beta& b) {
  auto dist = BetaDist{toSymbolic(a), toSymbolic(b)};
  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return IndexedRandomVar<Id, decltype(dist), DimTag, Ps...>{
            dist, std::tuple{ps...}};
      },
      collectParents(a, b));
}

// Non-indexed version (existing) when no dimension specified
template <typename Id = decltype([] {}), typename Alpha, typename Beta>
constexpr auto beta(const Alpha& a, const Beta& b) {
  // ... existing implementation
}

}  // namespace tempura::bayes2
```

---

## Part 6: Evaluation with Indexed Bindings

```cpp
// bayes2/indexed.h (continued)

// Indexed binding - holds array data for a dimension
template <typename Sym>
struct IndexedBinding {
  using symbol_type = Sym;
  using dim_type = typename Sym::dimensions::head;  // first dimension

  std::span<const double> values;

  double at(std::size_t i) const { return values[i]; }
  std::size_t size() const { return values.size(); }
};

// Evaluate SumOver by looping
template <typename DimTag, Symbolic Expr, typename... Binders>
auto evaluate(SumOver<DimTag, Expr> sum, const BinderPack<Binders...>& binders) {
  // Find the indexed binding for this dimension
  constexpr std::size_t n = dimensionSize<DimTag>(binders);

  double total = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    auto instance_binders = extractInstance<DimTag>(binders, i);
    total += evaluate(sum.expr_, instance_binders);
  }
  return total;
}
```

---

## Part 7: Usage Example

```cpp
// User code

// Declare dimensions
struct Countries {};
struct Observations {};

// Hyperparameters (scalar, shared across all countries)
auto alpha = halfNormal(5.0);
auto beta_param = halfNormal(5.0);

// Per-country rate (indexed over Countries)
auto theta = beta<Countries>(alpha, beta_param);

// Observations (indexed over both Countries and Observations)
// auto y = binomial<Countries, Observations>(n, theta);

// Joint log-prob is automatically:
// log p(α) + log p(β) + Σ_countries log p(θ[c] | α, β)
auto joint_lp = jointLogProb(theta);

// Gradient w.r.t. alpha (scalar) - works because diff commutes with sum
auto d_alpha = diff(joint_lp, alpha);

// Evaluate with data
std::vector<double> theta_values = {0.3, 0.4, 0.5, 0.6};
double lp = evaluate(joint_lp, BinderPack{
    alpha = 2.0,
    beta_param = 3.0,
    theta = std::span{theta_values}  // indexed binding
});
```

---

## Part 8: Dimension Algebra

Dimensions propagate through expressions:

```cpp
// Scalar + Scalar → Scalar
auto a = normal(0, 1);          // dims: {}
auto b = normal(0, 1);          // dims: {}
auto c = a.sym() + b.sym();     // dims: {}

// Indexed + Scalar → Indexed (broadcasting)
auto theta = beta<Countries>(alpha, beta_param);  // dims: {Countries}
auto prior_lp = theta.instanceLogProb();          // dims: {Countries}
// prior_lp references theta.sym() which is indexed

// Indexed + Indexed (same dim) → Indexed
auto y = normal<Countries>(theta, sigma);  // dims: {Countries}
auto obs_lp = y.instanceLogProb();         // dims: {Countries}

// Sum eliminates dimension
auto total = sumOver<Countries>(prior_lp);  // dims: {}
```

The dimension algebra is tracked at compile-time via `dimensions_of<Expr>`.

---

## Part 9: What Changes in symbolic3

Minimal changes needed:

1. **New file `symbolic3/indexed.h`**:
   - `Dim<Tag>` dimension markers
   - `IndexedSymbol<BaseId, Dims...>`
   - `dimensions_of<T>` trait

2. **New file `symbolic3/reduction.h`**:
   - `SumOver<DimTag, Expr>`
   - `ProductOver<DimTag, Expr>`

3. **Additions to `symbolic3/derivative.h`**:
   - `diff(SumOver<...>, var)` rule

4. **Additions to `symbolic3/evaluate.h`**:
   - `evaluate(SumOver<...>, binders)`
   - `evaluate(IndexedSymbol<...>, binders)` with index extraction

---

## Part 10: What This Enables

With indexed symbols, bayes2 can express:

1. **Simple plates**: `theta[i] ~ Beta(α, β)` for i in Countries

2. **Nested plates**: `theta[i,j] ~ Normal(mu[i], sigma)` for schools in districts

3. **Ragged plates**: Different sizes per group (via runtime dimension sizes)

4. **Automatic gradients**: `diff` commutes with `sumOver` for scalar params

5. **Symbolic model structure**: The plate structure is visible in the type

---

## Summary

| Component | Location | Purpose |
|-----------|----------|---------|
| `Dim<Tag>` | symbolic3/indexed.h | Dimension marker |
| `IndexedSymbol<Id, Dims...>` | symbolic3/indexed.h | Symbol varying over dimensions |
| `SumOver<Dim, Expr>` | symbolic3/reduction.h | Symbolic sum reduction |
| `diff(SumOver, var)` | symbolic3/derivative.h | Differentiation rule |
| `IndexedRandomVar` | bayes2/indexed.h | RandomVar with indexed symbol |
| `IndexedBinding` | bayes2/indexed.h | Bind symbol to array |
| `evaluate(SumOver, ...)` | bayes2/indexed.h | Loop and sum |

The beauty: **plates become first-class symbolic structure**, not runtime loops hidden from the type system.
