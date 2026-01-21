#pragma once

#include "bayes2/core.h"
#include "prob/log_prob.h"

// Distribution factories for bayes2.
//
// Each factory returns a RandomVar that:
//   1. Stores the distribution with symbolic parameters
//   2. Records parent GraphNodes as dependency edges
//   3. Has unique type identity for DAG deduplication
//
// Usage:
//   auto mu = normal(0, 10);       // Root prior
//   auto sigma = halfNormal(5);    // Root prior
//   auto y = normal(mu, sigma);    // Depends on mu and sigma

namespace tempura::bayes2 {

using namespace tempura::symbolic3;
using namespace tempura::prob;

// =============================================================================
// Distribution Wrappers
// =============================================================================
//
// These structs store symbolic parameters and provide logProbFor() to generate
// the log-probability expression at compile time. Parameters can be:
//   - Literal<double>: when created from scalar (e.g., normal(0, 10))
//   - Symbol<Id>: when created from another RandomVar (e.g., normal(mu, sigma))
//
// The logProbFor() method is called during DAG traversal to build the joint
// log-probability as a symbolic expression.

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

template <Symbolic Alpha, Symbolic Beta>
struct BetaDist {
  [[no_unique_address]] Alpha alpha_;
  [[no_unique_address]] Beta beta_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logBeta(x, alpha_, beta_);
  }
};

template <Symbolic Shape, Symbolic Rate>
struct GammaDist {
  [[no_unique_address]] Shape shape_;
  [[no_unique_address]] Rate rate_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logGamma(x, shape_, rate_);
  }
};

template <Symbolic Lo, Symbolic Hi>
struct UniformDist {
  [[no_unique_address]] Lo lo_;
  [[no_unique_address]] Hi hi_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logUniform(x, lo_, hi_);
  }
};

template <Symbolic Location, Symbolic Scale>
struct CauchyDist {
  [[no_unique_address]] Location location_;
  [[no_unique_address]] Scale scale_;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logCauchy(x, location_, scale_);
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

template <Symbolic N, Symbolic P>
struct BinomialDist {
  [[no_unique_address]] N n_;
  [[no_unique_address]] P p_;

  template <Symbolic K>
  constexpr auto logProbFor(K k) const {
    return logBinomial(k, n_, p_);
  }
};

// =============================================================================
// Factory Functions - Create RandomVars with unique identities
// =============================================================================
//
// Each factory uses `decltype([] {})` as default template argument.
// This creates a unique lambda type at each call site, giving each
// RandomVar a distinct type identity for DAG deduplication:
//
//   auto x = normal(0.0, 1.0);  // Type A
//   auto y = normal(0.0, 1.0);  // Type B (different from A!)

// Helper: packages distribution + parents into RandomVar.
// std::apply unpacks parent tuple into template parameters for exact types.
template <typename Id, typename Dist, typename... Args>
constexpr auto makeRandomVar(Dist dist, const Args&... args) {
  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{dist, std::tuple{ps...}};
      },
      collectParents(args...));
}

template <typename Id = decltype([] {}), typename Mu, typename Sigma>
constexpr auto normal(const Mu& mu, const Sigma& sigma) {
  return makeRandomVar<Id>(NormalDist{toSymbolic(mu), toSymbolic(sigma)}, mu,
                           sigma);
}

template <typename Id = decltype([] {}), typename Sigma>
constexpr auto halfNormal(const Sigma& sigma) {
  return makeRandomVar<Id>(HalfNormalDist{toSymbolic(sigma)}, sigma);
}

template <typename Id = decltype([] {}), typename Lambda>
constexpr auto exponential(const Lambda& lambda) {
  return makeRandomVar<Id>(ExponentialDist{toSymbolic(lambda)}, lambda);
}

template <typename Id = decltype([] {}), typename Alpha, typename Beta>
constexpr auto beta(const Alpha& a, const Beta& b) {
  return makeRandomVar<Id>(BetaDist{toSymbolic(a), toSymbolic(b)}, a, b);
}

template <typename Id = decltype([] {}), typename Shape, typename Rate>
constexpr auto gamma(const Shape& shape, const Rate& rate) {
  return makeRandomVar<Id>(GammaDist{toSymbolic(shape), toSymbolic(rate)}, shape,
                           rate);
}

template <typename Id = decltype([] {}), typename Lo, typename Hi>
constexpr auto uniform(const Lo& lo, const Hi& hi) {
  return makeRandomVar<Id>(UniformDist{toSymbolic(lo), toSymbolic(hi)}, lo, hi);
}

template <typename Id = decltype([] {}), typename Location, typename Scale>
constexpr auto cauchy(const Location& location, const Scale& scale) {
  return makeRandomVar<Id>(CauchyDist{toSymbolic(location), toSymbolic(scale)},
                           location, scale);
}

template <typename Id = decltype([] {}), typename P>
constexpr auto bernoulli(const P& p) {
  return makeRandomVar<Id>(BernoulliDist{toSymbolic(p)}, p);
}

template <typename Id = decltype([] {}), typename N, typename P>
constexpr auto binomial(const N& n, const P& p) {
  return makeRandomVar<Id>(BinomialDist{toSymbolic(n), toSymbolic(p)}, n, p);
}

}  // namespace tempura::bayes2
