#pragma once

#include "symbolic4/constants.h"
#include "symbolic4/distributions/log_prob.h"

// ============================================================================
// wrappers.h - Distribution wrappers with symbolic parameters
// ============================================================================
//
// Each wrapper stores symbolic parameters and provides logProbFor(x) method
// returning a symbolic expression. This enables compositional model building.
//
// Each distribution declares its support via a support_type alias:
//   - Unbounded: (-∞, ∞) - no transform needed
//   - Positive: (0, ∞) - log transform, x = exp(z)
//   - UnitInterval: (0, 1) - logit transform, x = sigmoid(z)
//
// RandomVar uses this to automatically handle parameter transforms and
// include the appropriate Jacobian in logProb().
//
// Usage:
//   Symbol<struct Mu> mu;
//   Symbol<struct Sigma> sigma;
//   auto dist = NormalDist{mu, sigma};
//   auto lp = dist.logProbFor(x);  // Symbolic log-probability
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Support types - declare the domain of a distribution
// ============================================================================

namespace support {
struct Real {};        // (-∞, ∞) - Normal, Cauchy, StudentT
struct PositiveReal {};  // (0, ∞) - Exponential, HalfNormal, Gamma
struct Unit {};        // (0, 1) - Beta, Uniform(0,1)
}  // namespace support

// Trait to check support type
template <typename T>
constexpr bool is_positive_support_v = std::is_same_v<T, support::PositiveReal>;

template <typename T>
constexpr bool is_unit_support_v = std::is_same_v<T, support::Unit>;

template <typename T>
constexpr bool is_real_support_v = std::is_same_v<T, support::Real>;

// ============================================================================
// Conversion helper
// ============================================================================

// Check if T has a sym() method (static or non-static) returning Symbolic
template <typename T>
concept HasSymbol = requires(const T& t) {
  { t.sym() } -> Symbolic;
};

// Convert anything to a symbolic type
template <typename T>
constexpr auto toSymbolic(T x) {
  if constexpr (Symbolic<T>) {
    return x;
  } else if constexpr (HasSymbol<T>) {
    // RandomVar or similar - use its symbol (now non-static, carries distribution)
    return x.sym();
  } else {
    return lit(static_cast<double>(x));
  }
}

// ============================================================================
// Normal distribution wrapper
// ============================================================================

template <Symbolic Mu, Symbolic Sigma>
struct NormalDist {
  using support_type = support::Real;

  [[no_unique_address]] Mu mu_;
  [[no_unique_address]] Sigma sigma_;

  constexpr NormalDist() = default;
  constexpr NormalDist(Mu mu, Sigma sigma) : mu_{mu}, sigma_{sigma} {}

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logNormal(x, mu_, sigma_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return unnormalizedLogNormal(x, mu_, sigma_);
  }

  constexpr auto mu() const { return mu_; }
  constexpr auto sigma() const { return sigma_; }
};

template <typename Mu, typename Sigma>
NormalDist(Mu, Sigma) -> NormalDist<decltype(toSymbolic(std::declval<Mu>())),
                                    decltype(toSymbolic(std::declval<Sigma>()))>;

// Factory function
template <typename Mu, typename Sigma>
constexpr auto Normal(Mu mu, Sigma sigma) {
  return NormalDist{toSymbolic(mu), toSymbolic(sigma)};
}

// ============================================================================
// Half-Normal distribution wrapper
// ============================================================================

template <Symbolic Sigma>
struct HalfNormalDist {
  using support_type = support::PositiveReal;

  [[no_unique_address]] Sigma sigma_;

  constexpr HalfNormalDist() = default;
  constexpr explicit HalfNormalDist(Sigma sigma) : sigma_{sigma} {}

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logHalfNormal(x, sigma_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return unnormalizedLogHalfNormal(x, sigma_);
  }

  constexpr auto sigma() const { return sigma_; }
};

template <typename Sigma>
HalfNormalDist(Sigma) -> HalfNormalDist<decltype(toSymbolic(std::declval<Sigma>()))>;

template <typename Sigma>
constexpr auto HalfNormal(Sigma sigma) {
  return HalfNormalDist{toSymbolic(sigma)};
}

// ============================================================================
// Beta distribution wrapper
// ============================================================================

template <Symbolic Alpha, Symbolic Beta>
struct BetaDist {
  using support_type = support::Unit;

  [[no_unique_address]] Alpha alpha_;
  [[no_unique_address]] Beta beta_;

  constexpr BetaDist() = default;
  constexpr BetaDist(Alpha alpha, Beta beta) : alpha_{alpha}, beta_{beta} {}

  // Full normalized log-probability (uses lgamma for normalization constant)
  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logBeta(x, alpha_, beta_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return unnormalizedLogBeta(x, alpha_, beta_);
  }

  constexpr auto alpha() const { return alpha_; }
  constexpr auto beta() const { return beta_; }
};

template <typename A, typename B>
BetaDist(A, B) -> BetaDist<decltype(toSymbolic(std::declval<A>())),
                           decltype(toSymbolic(std::declval<B>()))>;

template <typename A, typename B>
constexpr auto Beta(A alpha, B beta) {
  return BetaDist{toSymbolic(alpha), toSymbolic(beta)};
}

// ============================================================================
// Gamma distribution wrapper
// ============================================================================

template <Symbolic Alpha, Symbolic Beta>
struct GammaDist {
  using support_type = support::PositiveReal;

  [[no_unique_address]] Alpha alpha_;
  [[no_unique_address]] Beta beta_;

  constexpr GammaDist() = default;
  constexpr GammaDist(Alpha alpha, Beta beta) : alpha_{alpha}, beta_{beta} {}

  // Full normalized log-probability (uses lgamma for normalization constant)
  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logGamma(x, alpha_, beta_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return unnormalizedLogGamma(x, alpha_, beta_);
  }

  constexpr auto alpha() const { return alpha_; }
  constexpr auto beta() const { return beta_; }
};

template <typename Alpha, typename Beta>
GammaDist(Alpha, Beta) -> GammaDist<decltype(toSymbolic(std::declval<Alpha>())),
                                    decltype(toSymbolic(std::declval<Beta>()))>;

template <typename Alpha, typename Beta>
constexpr auto Gamma(Alpha alpha, Beta beta) {
  return GammaDist{toSymbolic(alpha), toSymbolic(beta)};
}

// ============================================================================
// Exponential distribution wrapper
// ============================================================================

template <Symbolic Lambda>
struct ExponentialDist {
  using support_type = support::PositiveReal;

  [[no_unique_address]] Lambda lambda_;

  constexpr ExponentialDist() = default;
  constexpr explicit ExponentialDist(Lambda lambda) : lambda_{lambda} {}

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logExponential(x, lambda_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return unnormalizedLogExponential(x, lambda_);
  }

  constexpr auto lambda() const { return lambda_; }
};

template <typename Lambda>
ExponentialDist(Lambda) -> ExponentialDist<decltype(toSymbolic(std::declval<Lambda>()))>;

template <typename Lambda>
constexpr auto Exponential(Lambda lambda) {
  return ExponentialDist{toSymbolic(lambda)};
}

// ============================================================================
// Uniform distribution wrapper
// ============================================================================

template <Symbolic A, Symbolic B>
struct UniformDist {
  using support_type = support::Real;  // General interval; specialize for Unit if (0,1)

  [[no_unique_address]] A a_;
  [[no_unique_address]] B b_;

  constexpr UniformDist() = default;
  constexpr UniformDist(A a, B b) : a_{a}, b_{b} {}

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logUniform(x, a_, b_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor([[maybe_unused]] X x) const {
    return 0_c;
  }

  constexpr auto a() const { return a_; }
  constexpr auto b() const { return b_; }
};

template <typename A, typename B>
UniformDist(A, B) -> UniformDist<decltype(toSymbolic(std::declval<A>())),
                                 decltype(toSymbolic(std::declval<B>()))>;

template <typename A, typename B>
constexpr auto Uniform(A a, B b) {
  return UniformDist{toSymbolic(a), toSymbolic(b)};
}

// ============================================================================
// Cauchy distribution wrapper
// ============================================================================

template <Symbolic X0, Symbolic Gamma>
struct CauchyDist {
  using support_type = support::Real;

  [[no_unique_address]] X0 x0_;
  [[no_unique_address]] Gamma gamma_;

  constexpr CauchyDist() = default;
  constexpr CauchyDist(X0 x0, Gamma gamma) : x0_{x0}, gamma_{gamma} {}

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logCauchy(x, x0_, gamma_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return unnormalizedLogCauchy(x, x0_, gamma_);
  }

  constexpr auto x0() const { return x0_; }
  constexpr auto gamma() const { return gamma_; }
};

template <typename X0, typename Gamma>
CauchyDist(X0, Gamma) -> CauchyDist<decltype(toSymbolic(std::declval<X0>())),
                                    decltype(toSymbolic(std::declval<Gamma>()))>;

template <typename X0, typename Gamma>
constexpr auto Cauchy(X0 x0, Gamma gamma) {
  return CauchyDist{toSymbolic(x0), toSymbolic(gamma)};
}

// ============================================================================
// Bernoulli distribution wrapper
// ============================================================================

template <Symbolic P>
struct BernoulliDist {
  using support_type = support::Unit;  // {0, 1} ⊂ (0, 1)

  [[no_unique_address]] P p_;

  constexpr BernoulliDist() = default;
  constexpr explicit BernoulliDist(P p) : p_{p} {}

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logBernoulli(x, p_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return logBernoulli(x, p_);  // No normalizing constant
  }

  constexpr auto p() const { return p_; }
};

template <typename P>
BernoulliDist(P) -> BernoulliDist<decltype(toSymbolic(std::declval<P>()))>;

template <typename P>
constexpr auto Bernoulli(P p) {
  return BernoulliDist{toSymbolic(p)};
}

// ============================================================================
// Binomial distribution wrapper
// ============================================================================
// Models k successes in n trials with probability p
// k ~ Binomial(n, p)

template <Symbolic N, Symbolic P>
struct BinomialDist {
  using support_type = support::Real;  // Discrete: {0, 1, ..., n}

  [[no_unique_address]] N n_;
  [[no_unique_address]] P p_;

  constexpr BinomialDist() = default;
  constexpr BinomialDist(N n, P p) : n_{n}, p_{p} {}

  template <Symbolic K>
  constexpr auto logProbFor(K k) const {
    return logBinomial(k, n_, p_);
  }

  template <Symbolic K>
  constexpr auto unnormalizedLogProbFor(K k) const {
    return unnormalizedLogBinomial(k, n_, p_);
  }

  constexpr auto n() const { return n_; }
  constexpr auto p() const { return p_; }
};

template <typename N, typename P>
BinomialDist(N, P) -> BinomialDist<decltype(toSymbolic(std::declval<N>())),
                                   decltype(toSymbolic(std::declval<P>()))>;

template <typename N, typename P>
constexpr auto Binomial(N n, P p) {
  return BinomialDist{toSymbolic(n), toSymbolic(p)};
}

// ============================================================================
// Poisson distribution wrapper
// ============================================================================

template <Symbolic Lambda>
struct PoissonDist {
  using support_type = support::Real;  // Discrete: {0, 1, 2, ...}

  [[no_unique_address]] Lambda lambda_;

  constexpr PoissonDist() = default;
  constexpr explicit PoissonDist(Lambda lambda) : lambda_{lambda} {}

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logPoisson(x, lambda_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return unnormalizedLogPoisson(x, lambda_);
  }

  constexpr auto lambda() const { return lambda_; }
};

template <typename Lambda>
PoissonDist(Lambda) -> PoissonDist<decltype(toSymbolic(std::declval<Lambda>()))>;

template <typename Lambda>
constexpr auto Poisson(Lambda lambda) {
  return PoissonDist{toSymbolic(lambda)};
}

// ============================================================================
// Student's t distribution wrapper
// ============================================================================

template <Symbolic Nu, Symbolic Mu, Symbolic Sigma>
struct StudentTDist {
  using support_type = support::Real;

  [[no_unique_address]] Nu nu_;
  [[no_unique_address]] Mu mu_;
  [[no_unique_address]] Sigma sigma_;

  constexpr StudentTDist() = default;
  constexpr StudentTDist(Nu nu, Mu mu, Sigma sigma)
      : nu_{nu}, mu_{mu}, sigma_{sigma} {}

  // Full normalized log-probability (uses lgamma for normalization constant)
  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logStudentT(x, nu_, mu_, sigma_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return unnormalizedLogStudentT(x, nu_, mu_, sigma_);
  }

  constexpr auto nu() const { return nu_; }
  constexpr auto mu() const { return mu_; }
  constexpr auto sigma() const { return sigma_; }
};

template <typename Nu, typename Mu, typename Sigma>
StudentTDist(Nu, Mu, Sigma)
    -> StudentTDist<decltype(toSymbolic(std::declval<Nu>())),
                    decltype(toSymbolic(std::declval<Mu>())),
                    decltype(toSymbolic(std::declval<Sigma>()))>;

template <typename Nu, typename Mu, typename Sigma>
constexpr auto StudentT(Nu nu, Mu mu, Sigma sigma) {
  return StudentTDist{toSymbolic(nu), toSymbolic(mu), toSymbolic(sigma)};
}

// ============================================================================
// Dirichlet distribution wrapper
// ============================================================================
// For use with plates/indexed variables where each component has symbolic alpha

template <Symbolic Alpha>
struct DirichletDist {
  using support_type = support::Unit;  // Simplex: sum = 1, each component in (0, 1)

  [[no_unique_address]] Alpha alpha_;

  constexpr DirichletDist() = default;
  constexpr explicit DirichletDist(Alpha alpha) : alpha_{alpha} {}

  // Only unnormalized (lgamma not symbolic)
  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return unnormalizedLogDirichlet(x, alpha_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return unnormalizedLogDirichlet(x, alpha_);
  }

  constexpr auto alpha() const { return alpha_; }
};

template <typename Alpha>
DirichletDist(Alpha) -> DirichletDist<decltype(toSymbolic(std::declval<Alpha>()))>;

template <typename Alpha>
constexpr auto Dirichlet(Alpha alpha) {
  return DirichletDist{toSymbolic(alpha)};
}

// ============================================================================
// Categorical distribution wrapper
// ============================================================================
// Represents discrete distribution over K categories
// Uses log-probability parameterization for numerical stability

template <Symbolic LogP>
struct CategoricalDist {
  using support_type = support::Real;  // Discrete: {0, 1, ..., K-1}

  [[no_unique_address]] LogP log_p_;

  constexpr CategoricalDist() = default;
  constexpr explicit CategoricalDist(LogP log_p) : log_p_{log_p} {}

  // log p(x=k) where x is one-hot encoded (or probability of category)
  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logCategorical(x, log_p_);
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return logCategorical(x, log_p_);
  }

  constexpr auto logP() const { return log_p_; }
};

template <typename LogP>
CategoricalDist(LogP) -> CategoricalDist<decltype(toSymbolic(std::declval<LogP>()))>;

// Factory with log-probability parameterization
template <typename LogP>
constexpr auto CategoricalLogP(LogP log_p) {
  return CategoricalDist{toSymbolic(log_p)};
}

// Factory with probability parameterization (converts to log internally)
template <Symbolic P>
constexpr auto Categorical(P p) {
  return CategoricalDist{log(p)};
}

}  // namespace tempura::symbolic4
