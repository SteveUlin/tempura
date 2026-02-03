#pragma once

#include "symbolic4/core.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/mcmc/transforms.h"

// ============================================================================
// support.h - Distribution support inference for automatic transforms
// ============================================================================
//
// Distributions have natural supports (domains). This header:
//   1. Defines support type tags
//   2. Maps distributions to their support
//   3. Provides autoTransform() to infer the appropriate MCMC transform
//
// Usage:
//   auto sigma = halfNormal(2.0);
//   auto posterior = makeTransformedPosterior(
//       lp,
//       autoTransform(sigma)  // Automatically uses positive() transform
//   );
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Support type tags
// ============================================================================

// x ∈ ℝ (unconstrained)
struct RealSupport {};

// x ∈ (0, ∞)
struct PositiveSupport {};

// x ∈ (0, 1)
struct UnitIntervalSupport {};

// x ∈ simplex (sum to 1, each > 0)
struct SimplexSupport {};

// x ∈ {0, 1}
struct BinarySupport {};

// x ∈ ℤ⁺ (positive integers)
struct PositiveIntegerSupport {};

// ============================================================================
// Distribution → Support mapping
// ============================================================================

// Primary template: unknown support
template <typename Dist>
struct DistributionSupport {
  using type = void;  // Unknown
};

// Normal: ℝ
template <Symbolic Mu, Symbolic Sigma>
struct DistributionSupport<NormalDist<Mu, Sigma>> {
  using type = RealSupport;
};

// Half-Normal: (0, ∞)
template <Symbolic Sigma>
struct DistributionSupport<HalfNormalDist<Sigma>> {
  using type = PositiveSupport;
};

// Beta: (0, 1)
template <Symbolic Alpha, Symbolic Beta>
struct DistributionSupport<BetaDist<Alpha, Beta>> {
  using type = UnitIntervalSupport;
};

// Gamma: (0, ∞)
template <Symbolic Alpha, Symbolic Beta>
struct DistributionSupport<GammaDist<Alpha, Beta>> {
  using type = PositiveSupport;
};

// Exponential: (0, ∞)
template <Symbolic Lambda>
struct DistributionSupport<ExponentialDist<Lambda>> {
  using type = PositiveSupport;
};

// Uniform(a, b): effectively unconstrained for MCMC purposes
// (Transform to (0,1) then rescale)
template <Symbolic A, Symbolic B>
struct DistributionSupport<UniformDist<A, B>> {
  using type = RealSupport;  // Treat as unconstrained; bounded handled separately
};

// Cauchy: ℝ
template <Symbolic X0, Symbolic Gamma>
struct DistributionSupport<CauchyDist<X0, Gamma>> {
  using type = RealSupport;
};

// Bernoulli: {0, 1}
template <Symbolic P>
struct DistributionSupport<BernoulliDist<P>> {
  using type = BinarySupport;
};

// Student's t: ℝ
template <Symbolic Nu, Symbolic Mu, Symbolic Sigma>
struct DistributionSupport<StudentTDist<Nu, Mu, Sigma>> {
  using type = RealSupport;
};

// Dirichlet: simplex
template <Symbolic Alpha>
struct DistributionSupport<DirichletDist<Alpha>> {
  using type = SimplexSupport;
};

// Categorical: discrete (integer indices)
template <Symbolic LogP>
struct DistributionSupport<CategoricalDist<LogP>> {
  using type = PositiveIntegerSupport;
};

// Alias for convenience
template <typename Dist>
using distribution_support_t = typename DistributionSupport<Dist>::type;

// ============================================================================
// Support → Transform mapping
// ============================================================================

namespace detail {

template <typename Support, typename Param>
struct SupportToTransform;

// Real → Unconstrained (identity)
template <typename Param>
struct SupportToTransform<RealSupport, Param> {
  static constexpr auto apply(Param p) { return unconstrained(p); }
};

// Positive → exp/log transform
template <typename Param>
struct SupportToTransform<PositiveSupport, Param> {
  static constexpr auto apply(Param p) { return positive(p); }
};

// UnitInterval → sigmoid/logit transform
template <typename Param>
struct SupportToTransform<UnitIntervalSupport, Param> {
  static constexpr auto apply(Param p) { return unitInterval(p); }
};

// Simplex → stick-breaking transform
// Note: SimplexTransform<K> maps K-1 unconstrained params to K-simplex.
// autoTransform falls back to unconstrained for simplex parameters because
// TransformedPosterior doesn't yet support dimension-changing transforms.
//
// For Dirichlet and other multivariate priors, use SimplexTransform<K> directly:
//
//   auto delta_transform = simplexTransform<3>();  // For 3-simplex
//   auto z = ...; // K-1 unconstrained params
//   auto delta = delta_transform.transform(z);
//   double log_jac = delta_transform.logJacobian(z);
//   auto grad_z = delta_transform.chainRuleGrad(z, grad_delta);
//
template <typename Param>
struct SupportToTransform<SimplexSupport, Param> {
  static constexpr auto apply(Param p) {
    // TODO: Full integration requires VectorTransformedPosterior
    // For now, use SimplexTransform<K> directly in your sampler
    return unconstrained(p);
  }
};

// Binary/Integer → unconstrained (discrete params typically not transformed)
template <typename Param>
struct SupportToTransform<BinarySupport, Param> {
  static constexpr auto apply(Param p) { return unconstrained(p); }
};

template <typename Param>
struct SupportToTransform<PositiveIntegerSupport, Param> {
  static constexpr auto apply(Param p) { return unconstrained(p); }
};

// Unknown support → unconstrained (safe default)
template <typename Param>
struct SupportToTransform<void, Param> {
  static constexpr auto apply(Param p) { return unconstrained(p); }
};

}  // namespace detail

// ============================================================================
// autoTransform - Infer transform from RandomVar's distribution
// ============================================================================

// For StochasticNode/RandomVar: extract distribution and infer transform
template <typename RV>
  requires IsRandomVar<RV>
constexpr auto autoTransform(const RV& rv) {
  using Dist = typename RV::dist_type;
  using Support = distribution_support_t<Dist>;
  return detail::SupportToTransform<Support, RV>::apply(rv);
}

// For wrapped transforms: pass through
template <typename T>
  requires is_transform_v<T>
constexpr auto autoTransform(T t) {
  return t;
}

// For raw symbols: default to unconstrained
template <typename T>
  requires(Symbolic<T> && !IsRandomVar<T> && !is_transform_v<T>)
constexpr auto autoTransform(T t) {
  return unconstrained(t);
}

// ============================================================================
// makeAutoTransformedPosterior - Convenience with automatic transforms
// ============================================================================

template <Symbolic LogProbExpr, typename... Params>
constexpr auto makeAutoTransformedPosterior(LogProbExpr lp, Params... params) {
  return makeTransformedPosterior(lp, autoTransform(params)...);
}

}  // namespace tempura::symbolic4
