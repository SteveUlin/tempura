#pragma once

#include "symbolic4/constants.h"
#include "symbolic4/distributions/dist_base.h"
#include "symbolic4/distributions/log_prob.h"

// ============================================================================
// wrappers.h - Distribution wrappers with symbolic parameters
// ============================================================================
//
// Each wrapper stores symbolic parameters via a CRTP base (DistBase) and
// provides logProbFor(x) returning a symbolic expression.
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

// Convert symbolic-compatible types to symbolic expressions
// NOTE: The else branch only accepts arithmetic types to prevent accidental
// matches with iterators and other non-symbolic types (clang's stricter
// concept checking requires this constraint).
template <typename T>
constexpr auto toSymbolic(T x)
  requires Symbolic<T> || HasSymbol<T> || std::is_arithmetic_v<T>
{
  if constexpr (Symbolic<T>) {
    return x;
  } else if constexpr (HasSymbol<T>) {
    // RandomVar or similar - use its symbol (now non-static, carries distribution)
    return x.sym();
  } else {
    static_assert(!std::is_arithmetic_v<T>,
        "Use _c suffix for constants: normal(0_c, 1_c). "
        "For runtime data, use a Symbol and bind at eval time.");
  }
}

// ============================================================================
// Normal distribution
// ============================================================================

template <Symbolic Mu, Symbolic Sigma>
struct NormalDist : DistBase<NormalDist<Mu, Sigma>, support::Real, Mu, Sigma> {
  using Base = DistBase<NormalDist, support::Real, Mu, Sigma>;
  using Base::Base;

  constexpr auto mu() const { return Base::template param<0>(); }
  constexpr auto sigma() const { return Base::template param<1>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto mu, auto sigma) {
    return logNormal(x, mu, sigma);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto mu, auto sigma) {
    return unnormalizedLogNormal(x, mu, sigma);
  }
};

template <typename Mu, typename Sigma>
NormalDist(Mu, Sigma) -> NormalDist<decltype(toSymbolic(std::declval<Mu>())),
                                    decltype(toSymbolic(std::declval<Sigma>()))>;

template <typename Mu, typename Sigma>
constexpr auto Normal(Mu mu, Sigma sigma) {
  return NormalDist{toSymbolic(mu), toSymbolic(sigma)};
}

// ============================================================================
// Half-Normal distribution
// ============================================================================

template <Symbolic Sigma>
struct HalfNormalDist : DistBase<HalfNormalDist<Sigma>, support::PositiveReal, Sigma> {
  using Base = DistBase<HalfNormalDist, support::PositiveReal, Sigma>;
  using Base::Base;

  constexpr auto sigma() const { return Base::template param<0>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto sigma) {
    return logHalfNormal(x, sigma);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto sigma) {
    return unnormalizedLogHalfNormal(x, sigma);
  }
};

template <typename Sigma>
HalfNormalDist(Sigma) -> HalfNormalDist<decltype(toSymbolic(std::declval<Sigma>()))>;

template <typename Sigma>
constexpr auto HalfNormal(Sigma sigma) {
  return HalfNormalDist{toSymbolic(sigma)};
}

// ============================================================================
// Beta distribution
// ============================================================================

template <Symbolic Alpha, Symbolic Beta>
struct BetaDist : DistBase<BetaDist<Alpha, Beta>, support::Unit, Alpha, Beta> {
  using Base = DistBase<BetaDist, support::Unit, Alpha, Beta>;
  using Base::Base;

  constexpr auto alpha() const { return Base::template param<0>(); }
  constexpr auto beta() const { return Base::template param<1>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto alpha, auto beta) {
    return logBeta(x, alpha, beta);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto alpha, auto beta) {
    return unnormalizedLogBeta(x, alpha, beta);
  }
};

template <typename A, typename B>
BetaDist(A, B) -> BetaDist<decltype(toSymbolic(std::declval<A>())),
                           decltype(toSymbolic(std::declval<B>()))>;

template <typename A, typename B>
constexpr auto Beta(A alpha, B beta) {
  return BetaDist{toSymbolic(alpha), toSymbolic(beta)};
}

// ============================================================================
// Gamma distribution
// ============================================================================

template <Symbolic Alpha, Symbolic Beta>
struct GammaDist : DistBase<GammaDist<Alpha, Beta>, support::PositiveReal, Alpha, Beta> {
  using Base = DistBase<GammaDist, support::PositiveReal, Alpha, Beta>;
  using Base::Base;

  constexpr auto alpha() const { return Base::template param<0>(); }
  constexpr auto beta() const { return Base::template param<1>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto alpha, auto beta) {
    return logGamma(x, alpha, beta);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto alpha, auto beta) {
    return unnormalizedLogGamma(x, alpha, beta);
  }
};

template <typename Alpha, typename Beta>
GammaDist(Alpha, Beta) -> GammaDist<decltype(toSymbolic(std::declval<Alpha>())),
                                    decltype(toSymbolic(std::declval<Beta>()))>;

template <typename Alpha, typename Beta>
constexpr auto Gamma(Alpha alpha, Beta beta) {
  return GammaDist{toSymbolic(alpha), toSymbolic(beta)};
}

// ============================================================================
// Exponential distribution
// ============================================================================

template <Symbolic Lambda>
struct ExponentialDist : DistBase<ExponentialDist<Lambda>, support::PositiveReal, Lambda> {
  using Base = DistBase<ExponentialDist, support::PositiveReal, Lambda>;
  using Base::Base;

  constexpr auto lambda() const { return Base::template param<0>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto lambda) {
    return logExponential(x, lambda);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto lambda) {
    return unnormalizedLogExponential(x, lambda);
  }
};

template <typename Lambda>
ExponentialDist(Lambda) -> ExponentialDist<decltype(toSymbolic(std::declval<Lambda>()))>;

template <typename Lambda>
constexpr auto Exponential(Lambda lambda) {
  return ExponentialDist{toSymbolic(lambda)};
}

// ============================================================================
// Uniform distribution
// ============================================================================

template <Symbolic A, Symbolic B>
struct UniformDist : DistBase<UniformDist<A, B>, support::Real, A, B> {
  using Base = DistBase<UniformDist, support::Real, A, B>;
  using Base::Base;

  constexpr auto a() const { return Base::template param<0>(); }
  constexpr auto b() const { return Base::template param<1>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto a, auto b) {
    return logUniform(x, a, b);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl([[maybe_unused]] X x,
                                                [[maybe_unused]] auto a,
                                                [[maybe_unused]] auto b) {
    return 0_c;
  }
};

template <typename A, typename B>
UniformDist(A, B) -> UniformDist<decltype(toSymbolic(std::declval<A>())),
                                 decltype(toSymbolic(std::declval<B>()))>;

template <typename A, typename B>
constexpr auto Uniform(A a, B b) {
  return UniformDist{toSymbolic(a), toSymbolic(b)};
}

// ============================================================================
// Cauchy distribution
// ============================================================================

template <Symbolic X0, Symbolic Gamma>
struct CauchyDist : DistBase<CauchyDist<X0, Gamma>, support::Real, X0, Gamma> {
  using Base = DistBase<CauchyDist, support::Real, X0, Gamma>;
  using Base::Base;

  constexpr auto x0() const { return Base::template param<0>(); }
  constexpr auto gamma() const { return Base::template param<1>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto x0, auto gamma) {
    return logCauchy(x, x0, gamma);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto x0, auto gamma) {
    return unnormalizedLogCauchy(x, x0, gamma);
  }
};

template <typename X0, typename Gamma>
CauchyDist(X0, Gamma) -> CauchyDist<decltype(toSymbolic(std::declval<X0>())),
                                    decltype(toSymbolic(std::declval<Gamma>()))>;

template <typename X0, typename Gamma>
constexpr auto Cauchy(X0 x0, Gamma gamma) {
  return CauchyDist{toSymbolic(x0), toSymbolic(gamma)};
}

// ============================================================================
// Bernoulli distribution
// ============================================================================

template <Symbolic P>
struct BernoulliDist : DistBase<BernoulliDist<P>, support::Unit, P> {
  using Base = DistBase<BernoulliDist, support::Unit, P>;
  using Base::Base;

  constexpr auto p() const { return Base::template param<0>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto p) {
    return logBernoulli(x, p);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto p) {
    return logBernoulli(x, p);  // No normalizing constant
  }
};

template <typename P>
BernoulliDist(P) -> BernoulliDist<decltype(toSymbolic(std::declval<P>()))>;

template <typename P>
constexpr auto Bernoulli(P p) {
  return BernoulliDist{toSymbolic(p)};
}

// ============================================================================
// Binomial distribution
// ============================================================================

template <Symbolic N, Symbolic P>
struct BinomialDist : DistBase<BinomialDist<N, P>, support::Real, N, P> {
  using Base = DistBase<BinomialDist, support::Real, N, P>;
  using Base::Base;

  constexpr auto n() const { return Base::template param<0>(); }
  constexpr auto p() const { return Base::template param<1>(); }

  template <Symbolic K>
  static constexpr auto logProbImpl(K k, auto n, auto p) {
    return logBinomial(k, n, p);
  }
  template <Symbolic K>
  static constexpr auto unnormalizedLogProbImpl(K k, auto n, auto p) {
    return unnormalizedLogBinomial(k, n, p);
  }
};

template <typename N, typename P>
BinomialDist(N, P) -> BinomialDist<decltype(toSymbolic(std::declval<N>())),
                                   decltype(toSymbolic(std::declval<P>()))>;

template <typename N, typename P>
constexpr auto Binomial(N n, P p) {
  return BinomialDist{toSymbolic(n), toSymbolic(p)};
}

// ============================================================================
// Poisson distribution
// ============================================================================

template <Symbolic Lambda>
struct PoissonDist : DistBase<PoissonDist<Lambda>, support::Real, Lambda> {
  using Base = DistBase<PoissonDist, support::Real, Lambda>;
  using Base::Base;

  constexpr auto lambda() const { return Base::template param<0>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto lambda) {
    return logPoisson(x, lambda);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto lambda) {
    return unnormalizedLogPoisson(x, lambda);
  }
};

template <typename Lambda>
PoissonDist(Lambda) -> PoissonDist<decltype(toSymbolic(std::declval<Lambda>()))>;

template <typename Lambda>
constexpr auto Poisson(Lambda lambda) {
  return PoissonDist{toSymbolic(lambda)};
}

// ============================================================================
// Student's t distribution
// ============================================================================

template <Symbolic Nu, Symbolic Mu, Symbolic Sigma>
struct StudentTDist : DistBase<StudentTDist<Nu, Mu, Sigma>, support::Real, Nu, Mu, Sigma> {
  using Base = DistBase<StudentTDist, support::Real, Nu, Mu, Sigma>;
  using Base::Base;

  constexpr auto nu() const { return Base::template param<0>(); }
  constexpr auto mu() const { return Base::template param<1>(); }
  constexpr auto sigma() const { return Base::template param<2>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto nu, auto mu, auto sigma) {
    return logStudentT(x, nu, mu, sigma);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto nu, auto mu, auto sigma) {
    return unnormalizedLogStudentT(x, nu, mu, sigma);
  }
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
// Dirichlet distribution
// ============================================================================

template <Symbolic Alpha>
struct DirichletDist : DistBase<DirichletDist<Alpha>, support::Unit, Alpha> {
  using Base = DistBase<DirichletDist, support::Unit, Alpha>;
  using Base::Base;

  constexpr auto alpha() const { return Base::template param<0>(); }

  // Only unnormalized (lgamma not symbolic)
  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto alpha) {
    return unnormalizedLogDirichlet(x, alpha);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto alpha) {
    return unnormalizedLogDirichlet(x, alpha);
  }
};

template <typename Alpha>
DirichletDist(Alpha) -> DirichletDist<decltype(toSymbolic(std::declval<Alpha>()))>;

template <typename Alpha>
constexpr auto Dirichlet(Alpha alpha) {
  return DirichletDist{toSymbolic(alpha)};
}

// ============================================================================
// Categorical distribution
// ============================================================================

template <Symbolic LogP>
struct CategoricalDist : DistBase<CategoricalDist<LogP>, support::Real, LogP> {
  using Base = DistBase<CategoricalDist, support::Real, LogP>;
  using Base::Base;

  constexpr auto logP() const { return Base::template param<0>(); }

  template <Symbolic X>
  static constexpr auto logProbImpl(X x, auto log_p) {
    return logCategorical(x, log_p);
  }
  template <Symbolic X>
  static constexpr auto unnormalizedLogProbImpl(X x, auto log_p) {
    return logCategorical(x, log_p);
  }
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
