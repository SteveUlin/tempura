#pragma once

#include <tuple>
#include <vector>

#include "prob/bernoulli.h"
#include "prob/beta.h"
#include "prob/cauchy.h"
#include "prob/exponential.h"
#include "prob/gamma.h"
#include "prob/half_normal.h"
#include "prob/normal.h"
#include "prob/uniform.h"
#include "symbolic4/core.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/interpreter/eval.h"

// ============================================================================
// sample.h - Sampling from probabilistic models
// ============================================================================
//
// Simple, explicit sampling without DAG traversal:
//   - sample(rv, rng)           - Sample RV with no dependencies
//   - sample(rv, rng, bindings) - Sample RV given parent values
//   - sampleAll(rng, rv1, rv2, ...) - Sample all RVs in dependency order
//   - samplePlate(rv, n, rng, bindings) - Sample n iid copies
//
// The user controls sampling order by listing RVs in dependency order.
// Missing dependencies cause compile errors (type-safe!).
//
// Example:
//   auto mu = normal(lit(0), lit(10));
//   auto sigma = halfNormal(lit(5));
//   auto y = normal(mu, sigma);
//
//   std::mt19937 rng{42};
//   auto trace = sampleAll(rng, mu, sigma, y);
//   double mu_val = trace[mu];
//   double y_val = trace[y];
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Trace - Stores sampled values, usable as bindings
// ============================================================================

// Specialization for empty Trace
template <typename... Binders>
struct Trace;

template <>
struct Trace<> {
  constexpr Trace() = default;

  // Dummy lookup for evaluate() calls with no bindings needed
  template <typename Sym>
  constexpr double operator[](Sym) const {
    return 0.0;
  }
};

// Primary template for non-empty Trace
template <typename... Binders>
struct Trace : BinderPack<Binders...> {
  using BinderPack<Binders...>::BinderPack;
  using BinderPack<Binders...>::operator[];

  // Access by RandomVar (uses RV's free symbol for binding lookup)
  template <IsRandomVar RV>
  constexpr auto operator[](const RV& /*rv*/) const {
    return BinderPack<Binders...>::operator[](RV::freeSym());
  }
};

template <typename... Bs>
Trace(Bs...) -> Trace<Bs...>;

// Add a single binding to a trace
template <typename... Bs, typename Sym, typename Val>
constexpr auto addToTrace(const Trace<Bs...>& trace,
                          TypeValueBinder<Sym, Val> binder) {
  return Trace<Bs..., TypeValueBinder<Sym, Val>>{static_cast<const Bs&>(trace)...,
                                                  binder};
}

// ============================================================================
// sampleDist - Sample from distribution wrappers given evaluated parameters
// ============================================================================

template <Symbolic Mu, Symbolic Sigma, typename Bindings, typename RNG>
auto sampleDist(const NormalDist<Mu, Sigma>& dist, const Bindings& b, RNG& rng) {
  double mu_val = evaluate(dist.mu(), b);
  double sigma_val = evaluate(dist.sigma(), b);
  return prob::Normal{mu_val, sigma_val}.sample(rng);
}

template <Symbolic Sigma, typename Bindings, typename RNG>
auto sampleDist(const HalfNormalDist<Sigma>& dist, const Bindings& b, RNG& rng) {
  double sigma_val = evaluate(dist.sigma(), b);
  return prob::HalfNormal{sigma_val}.sample(rng);
}

template <Symbolic Alpha, Symbolic Beta, typename Bindings, typename RNG>
auto sampleDist(const BetaDist<Alpha, Beta>& dist, const Bindings& b, RNG& rng) {
  double alpha_val = evaluate(dist.alpha(), b);
  double beta_val = evaluate(dist.beta(), b);
  return prob::Beta{alpha_val, beta_val}.sample(rng);
}

template <Symbolic Alpha, Symbolic Beta, typename Bindings, typename RNG>
auto sampleDist(const GammaDist<Alpha, Beta>& dist, const Bindings& b, RNG& rng) {
  double alpha_val = evaluate(dist.alpha(), b);
  double beta_val = evaluate(dist.beta(), b);
  return prob::Gamma{alpha_val, beta_val}.sample(rng);
}

template <Symbolic Lambda, typename Bindings, typename RNG>
auto sampleDist(const ExponentialDist<Lambda>& dist, const Bindings& b,
                RNG& rng) {
  double lambda_val = evaluate(dist.lambda(), b);
  return prob::Exponential{lambda_val}.sample(rng);
}

template <Symbolic A, Symbolic B, typename Bindings, typename RNG>
auto sampleDist(const UniformDist<A, B>& dist, const Bindings& b, RNG& rng) {
  double a_val = evaluate(dist.a(), b);
  double b_val = evaluate(dist.b(), b);
  return prob::Uniform{a_val, b_val}.sample(rng);
}

template <Symbolic X0, Symbolic Gamma, typename Bindings, typename RNG>
auto sampleDist(const CauchyDist<X0, Gamma>& dist, const Bindings& b, RNG& rng) {
  double x0_val = evaluate(dist.x0(), b);
  double gamma_val = evaluate(dist.gamma(), b);
  return prob::Cauchy{x0_val, gamma_val}.sample(rng);
}

template <Symbolic P, typename Bindings, typename RNG>
auto sampleDist(const BernoulliDist<P>& dist, const Bindings& b, RNG& rng) {
  double p_val = evaluate(dist.p(), b);
  return prob::Bernoulli{p_val}.sample(rng);
}

// StudentT: sample standard t, then transform to location-scale
template <Symbolic Nu, Symbolic Mu, Symbolic Sigma, typename Bindings,
          typename RNG>
auto sampleDist(const StudentTDist<Nu, Mu, Sigma>& dist, const Bindings& b,
                RNG& rng) {
  double nu_val = evaluate(dist.nu(), b);
  double mu_val = evaluate(dist.mu(), b);
  double sigma_val = evaluate(dist.sigma(), b);
  double z = prob::Normal{0.0, 1.0}.sample(rng);
  double v = prob::Gamma{nu_val / 2.0, nu_val / 2.0}.sample(rng);
  double t = z / std::sqrt(v);
  return mu_val + sigma_val * t;
}

// ============================================================================
// sample - Sample a single RandomVar
// ============================================================================

// Sample with no dependencies (distribution has only literal parameters)
template <IsRandomVar RV, typename RNG>
auto sample(const RV& rv, RNG& rng) {
  return sampleDist(rv.dist(), Trace<>{}, rng);
}

// Sample with explicit bindings (for RVs that depend on other RVs)
template <IsRandomVar RV, typename RNG, typename... Binders>
auto sample(const RV& rv, RNG& rng, Binders... binders) {
  auto bindings = BinderPack{binders...};
  return sampleDist(rv.dist(), bindings, rng);
}

// Sample with a Trace as bindings
template <IsRandomVar RV, typename RNG, typename... TraceBinders>
auto sample(const RV& rv, RNG& rng, const Trace<TraceBinders...>& trace) {
  return sampleDist(rv.dist(), trace, rng);
}

// ============================================================================
// sampleAll - Sample multiple RVs in dependency order
// ============================================================================

namespace detail {

// Base case: no more RVs
template <typename RNG>
auto sampleAllImpl(Trace<> trace, RNG& /*rng*/) {
  return trace;
}

// Recursive case: sample head, add to trace, continue
template <typename... TraceBinders, typename RNG, IsRandomVar RV,
          IsRandomVar... Rest>
auto sampleAllImpl(Trace<TraceBinders...> trace, RNG& rng, const RV& rv,
                   const Rest&... rest) {
  // Sample this RV using current trace as bindings
  auto value = sampleDist(rv.dist(), trace, rng);

  // Add to trace using freeSym() for binding (bindings use Atom<Id, Free>)
  auto new_trace = addToTrace(trace, rv.freeSym() = value);

  // Recurse
  if constexpr (sizeof...(rest) == 0) {
    return new_trace;
  } else {
    return sampleAllImpl(new_trace, rng, rest...);
  }
}

}  // namespace detail

template <typename RNG, IsRandomVar... RVs>
auto sampleAll(RNG& rng, const RVs&... rvs) {
  return detail::sampleAllImpl(Trace<>{}, rng, rvs...);
}

// ============================================================================
// samplePlate - Sample n iid copies from a distribution
// ============================================================================

template <IsRandomVar RV, typename RNG>
auto samplePlate(const RV& rv, SizeT n, RNG& rng) {
  std::vector<double> samples;
  samples.reserve(n);
  for (SizeT i = 0; i < n; ++i) {
    samples.push_back(sample(rv, rng));
  }
  return samples;
}

template <IsRandomVar RV, typename RNG, typename... Binders>
auto samplePlate(const RV& rv, SizeT n, RNG& rng, Binders... binders) {
  auto bindings = BinderPack{binders...};
  std::vector<double> samples;
  samples.reserve(n);
  for (SizeT i = 0; i < n; ++i) {
    samples.push_back(sampleDist(rv.dist(), bindings, rng));
  }
  return samples;
}

template <IsRandomVar RV, typename RNG, typename... TraceBinders>
auto samplePlate(const RV& rv, SizeT n, RNG& rng,
                 const Trace<TraceBinders...>& trace) {
  std::vector<double> samples;
  samples.reserve(n);
  for (SizeT i = 0; i < n; ++i) {
    samples.push_back(sampleDist(rv.dist(), trace, rng));
  }
  return samples;
}

}  // namespace tempura::symbolic4
