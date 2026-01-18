#pragma once

#include "bayes/graph/core.h"
#include "prob/log_prob.h"

// Distribution factories for expression-graph DSL.
//
// Each factory (Normal, HalfNormal, etc.) returns a RandomVar that:
//   1. Stores the distribution with symbolic parameters
//   2. Records parent RandomVars as dependency edges
//   3. Has a unique type identity for DAG deduplication
//
// Usage:
//   auto mu = Normal(0, 10);       // Root prior
//   auto sigma = HalfNormal(5);    // Root prior
//   auto y = Normal(mu, sigma);    // Depends on mu and sigma

namespace tempura::bayes::graph {

using namespace tempura::symbolic3;
using namespace tempura::prob;

// =============================================================================
// Distribution Wrappers - Store symbolic parameters, provide logProbFor()
// =============================================================================

template <Symbolic Mu, Symbolic Sigma>
struct NormalDist {
  [[no_unique_address]] Mu mu_;
  [[no_unique_address]] Sigma sigma_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logNormal(x, mu_, sigma_);
  }
};

template <Symbolic Sigma>
struct HalfNormalDist {
  [[no_unique_address]] Sigma sigma_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logHalfNormal(x, sigma_);
  }
};

template <Symbolic Lambda>
struct ExponentialDist {
  [[no_unique_address]] Lambda lambda_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logExponential(x, lambda_);
  }
};

template <Symbolic Alpha, Symbolic Bet>
struct BetaDist {
  [[no_unique_address]] Alpha alpha_;
  [[no_unique_address]] Bet beta_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logBeta(x, alpha_, beta_);
  }
};

template <Symbolic Alpha, Symbolic Bet>
struct GammaDist {
  [[no_unique_address]] Alpha alpha_;
  [[no_unique_address]] Bet beta_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logGamma(x, alpha_, beta_);
  }
};

template <Symbolic A, Symbolic B>
struct UniformDist {
  [[no_unique_address]] A a_;
  [[no_unique_address]] B b_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logUniform(x, a_, b_);
  }
};

template <Symbolic X0, Symbolic Gam>
struct CauchyDist {
  [[no_unique_address]] X0 x0_;
  [[no_unique_address]] Gam gamma_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logCauchy(x, x0_, gamma_);
  }
};

template <Symbolic P>
struct BernoulliDist {
  [[no_unique_address]] P p_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logBernoulli(x, p_);
  }
};

// =============================================================================
// Factory Functions - Create RandomVars with unique identities
// =============================================================================
//
// Each factory uses `decltype([] {})` as a default template argument.
// This creates a unique lambda type at each call site, giving each
// RandomVar a distinct Symbol type:
//
//   auto x = Normal(0.0, 1.0);  // Unique type
//   auto y = Normal(0.0, 1.0);  // Different unique type!
//

template <typename Id = decltype([] {}), typename Mu, typename Sigma>
constexpr auto Normal(const Mu& mu, const Sigma& sigma) {
  auto mu_sym = toSymbolic(mu);
  auto sigma_sym = toSymbolic(sigma);

  using Dist = NormalDist<decltype(mu_sym), decltype(sigma_sym)>;
  auto parents = collectParents(mu, sigma);

  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{Dist{mu_sym, sigma_sym},
                                          std::tuple{ps...}};
      },
      parents);
}

template <typename Id = decltype([] {}), typename Sigma>
constexpr auto HalfNormal(const Sigma& sigma) {
  auto sigma_sym = toSymbolic(sigma);

  using Dist = HalfNormalDist<decltype(sigma_sym)>;
  auto parents = collectParents(sigma);

  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{Dist{sigma_sym}, std::tuple{ps...}};
      },
      parents);
}

template <typename Id = decltype([] {}), typename Lambda>
constexpr auto Exponential(const Lambda& lambda) {
  auto lambda_sym = toSymbolic(lambda);

  using Dist = ExponentialDist<decltype(lambda_sym)>;
  auto parents = collectParents(lambda);

  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{Dist{lambda_sym}, std::tuple{ps...}};
      },
      parents);
}

template <typename Id = decltype([] {}), typename Alpha, typename Bet>
constexpr auto Beta(const Alpha& alpha, const Bet& beta) {
  auto alpha_sym = toSymbolic(alpha);
  auto beta_sym = toSymbolic(beta);

  using Dist = BetaDist<decltype(alpha_sym), decltype(beta_sym)>;
  auto parents = collectParents(alpha, beta);

  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{Dist{alpha_sym, beta_sym},
                                          std::tuple{ps...}};
      },
      parents);
}

template <typename Id = decltype([] {}), typename Alpha, typename Bet>
constexpr auto Gamma(const Alpha& alpha, const Bet& beta) {
  auto alpha_sym = toSymbolic(alpha);
  auto beta_sym = toSymbolic(beta);

  using Dist = GammaDist<decltype(alpha_sym), decltype(beta_sym)>;
  auto parents = collectParents(alpha, beta);

  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{Dist{alpha_sym, beta_sym},
                                          std::tuple{ps...}};
      },
      parents);
}

template <typename Id = decltype([] {}), typename A, typename B>
constexpr auto Uniform(const A& a, const B& b) {
  auto a_sym = toSymbolic(a);
  auto b_sym = toSymbolic(b);

  using Dist = UniformDist<decltype(a_sym), decltype(b_sym)>;
  auto parents = collectParents(a, b);

  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{Dist{a_sym, b_sym},
                                          std::tuple{ps...}};
      },
      parents);
}

template <typename Id = decltype([] {}), typename X0, typename Gam>
constexpr auto Cauchy(const X0& x0, const Gam& gamma) {
  auto x0_sym = toSymbolic(x0);
  auto gamma_sym = toSymbolic(gamma);

  using Dist = CauchyDist<decltype(x0_sym), decltype(gamma_sym)>;
  auto parents = collectParents(x0, gamma);

  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{Dist{x0_sym, gamma_sym},
                                          std::tuple{ps...}};
      },
      parents);
}

template <typename Id = decltype([] {}), typename P>
constexpr auto Bernoulli(const P& p) {
  auto p_sym = toSymbolic(p);

  using Dist = BernoulliDist<decltype(p_sym)>;
  auto parents = collectParents(p);

  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{Dist{p_sym}, std::tuple{ps...}};
      },
      parents);
}

}  // namespace tempura::bayes::graph
