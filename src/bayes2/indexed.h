#pragma once

#include <span>
#include <tuple>
#include <vector>

#include "bayes2/core.h"
#include "bayes2/distributions.h"
#include "symbolic3/indexed.h"
#include "symbolic3/indexed_evaluate.h"
#include "symbolic3/reduction.h"

// Plate notation for bayes2 - replicate a RandomVar over a dimension.
//
// A plate represents a family of IID random variables:
//   auto theta = plate<Countries>(beta(alpha, beta_param));
//
// The key insight: a plate is ONE RandomVar whose symbol varies over a dimension,
// not N different RandomVars. This keeps compile-time types tractable.
//
// The joint log-probability is: Σᵢ log p(theta[i] | alpha, beta)
// Using symbolic reduction: sumOver<Countries>(log p(theta | alpha, beta))
//
// Example:
//   struct Countries {};
//
//   auto alpha = halfNormal(5.0);
//   auto beta_param = halfNormal(5.0);
//   auto theta = plate<Countries>(beta(alpha, beta_param));
//
//   // Log-prob for one instance (symbolic)
//   auto instance_lp = theta.instanceLogProb();
//
//   // Total log-prob = Σ_countries log p(θ[i] | α, β)
//   auto total_lp = theta.nodeLogProb();  // Returns SumOver<Countries, ...>

namespace tempura::bayes2 {

using namespace tempura::symbolic3;

// =============================================================================
// IndexedRandomVar - RandomVar whose symbol varies over a dimension
// =============================================================================
//
// Template parameters:
//   Id      - Unique type identity (stateless lambda from call site)
//   Dist    - Distribution type with logProbFor() method
//   DimTag  - Dimension tag (e.g., Countries, Observations)
//   Parents - Parent GraphNodes this variable depends on

template <typename Id, typename Dist, typename DimTag, typename... Parents>
struct IndexedRandomVar {
  using id_type = Id;
  using dist_type = Dist;
  using dim_tag = DimTag;
  using dim_type = Dim<DimTag>;
  using symbol_type = IndexedSymbol<Id, Dim<DimTag>>;

  [[no_unique_address]] Dist dist_;
  [[no_unique_address]] std::tuple<Parents...> parents_;

  // Unique indexed symbol for this variable
  static constexpr auto sym() { return symbol_type{}; }

  // Log-prob for ONE instance (symbolic template)
  // This is log p(theta[i] | parents) as a symbolic expression
  constexpr auto instanceLogProb() const { return dist_.logProbFor(sym()); }

  // Total log-prob = sum over dimension
  // Returns SumOver<DimTag, log p(theta | parents)>
  constexpr auto nodeLogProb() const {
    return sumOver<DimTag>(instanceLogProb());
  }

  constexpr const auto& parents() const { return parents_; }

  // Binding syntax: theta = indexed(values)
  auto operator=(IndexedValues iv) const {
    return IndexedBinding<symbol_type>{iv.values};
  }
};

// Type traits for IndexedRandomVar
template <typename T>
constexpr bool is_indexed_random_var = false;

template <typename Id, typename Dist, typename DimTag, typename... Parents>
constexpr bool
    is_indexed_random_var<IndexedRandomVar<Id, Dist, DimTag, Parents...>> =
        true;

// =============================================================================
// toSymbolic overload for IndexedRandomVar
// =============================================================================
//
// IndexedRandomVar doesn't satisfy GraphNode (different operator= signature),
// so we need an explicit toSymbolic overload.

template <typename Id, typename Dist, typename DimTag, typename... Parents>
constexpr auto toSymbolic(const IndexedRandomVar<Id, Dist, DimTag, Parents...>& rv) {
  return rv.sym();
}

// =============================================================================
// plate<Dim>() - Replicate a RandomVar over a dimension
// =============================================================================
//
// Usage:
//   auto theta = plate<Countries>(beta(alpha, beta_param));
//
// This wraps any RandomVar into an IndexedRandomVar that varies over the
// specified dimension. The plate notation is standard in probabilistic
// programming (BUGS, Stan, Pyro, NumPyro).
//
// Benefits over the old beta<Countries>() syntax:
//   - Explicit: "plate" clearly indicates replication
//   - Composable: works with ANY distribution without duplication
//   - Extensible: easy to add multi-dimensional plates later

// plate<DimTag>(rv) - wrap a RandomVar into an IndexedRandomVar
template <typename DimTag, typename Id = decltype([] {}), typename RV>
  requires is_random_var<RV>
constexpr auto plate(const RV& rv) {
  using Dist = typename RV::dist_type;
  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return IndexedRandomVar<Id, Dist, DimTag, Ps...>{rv.dist_,
                                                          std::tuple{ps...}};
      },
      rv.parents_);
}

// =============================================================================
// Diff overload for IndexedRandomVar
// =============================================================================
//
// Differentiate w.r.t. an IndexedRandomVar's symbol

template <Symbolic Expr, typename Id, typename Dist, typename DimTag,
          typename... Parents>
constexpr auto diff(Expr expr,
                    const IndexedRandomVar<Id, Dist, DimTag, Parents...>& var) {
  return symbolic3::diff(expr, var.sym());
}

}  // namespace tempura::bayes2
