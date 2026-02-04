#pragma once

#include "symbolic4/core.h"
#include "symbolic4/distributions/wrappers.h"

// ============================================================================
// random_var.h - Simple random variables for probabilistic models
// ============================================================================
//
// RandomVar represents a random variable with:
//   - A distribution (NormalDist, BetaDist, etc.)
//   - A unique identity type (for bindings and differentiation)
//
// Unlike the old StochasticNode, RandomVar does NOT track parent dependencies.
// Dependencies are encoded implicitly in the symbolic expressions. This makes
// the type simpler and avoids redundant graph machinery.
//
// Usage:
//   auto mu = normal(lit(0), lit(10));      // mu ~ Normal(0, 10)
//   auto sigma = halfNormal(lit(5));         // sigma ~ HalfNormal(5)
//   auto y = normal(mu, sigma);              // y ~ Normal(mu, sigma)
//
//   // Joint log probability (user lists all RVs)
//   auto joint = logProb(mu, sigma, y);
//
//   // Evaluate
//   double lp = evaluate(joint, mu=1.0, sigma=2.0, y=0.5);
//
//   // Sample (explicit ordering)
//   auto trace = sampleAll(rng, mu, sigma, y);
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// RandomVar - A random variable in a probabilistic model
// ============================================================================
//
// RandomVar uses plain Free symbols (Atom<Id, Free>) in expression trees.
// Constraint transforms (exp for positive, sigmoid for unit) are handled
// externally by the posterior's TransformPack — NOT embedded in the expression.
//
// Key invariant: operator= binds CONSTRAINED values directly.
//   sigma = 2.0  →  bind Symbol<Id> = 2.0
//   The posterior handles z↔x conversion internally.
//
// ============================================================================

template <typename Dist, typename Id>
struct RandomVar {
  using id_type = Id;
  using dist_type = Dist;
  using support_type = typename Dist::support_type;
  using symbol_type = Symbol<Id>;

  [[no_unique_address]] Dist dist_;

  constexpr explicit RandomVar(Dist dist) : dist_{dist} {}

  // The symbol for this random variable — a plain Free atom.
  // Expression trees use this directly; no constraint transform embedded.
  static constexpr auto sym() { return symbol_type{}; }

  // Alias: freeSym() == sym() (both return Symbol<Id>)
  static constexpr auto freeSym() { return symbol_type{}; }

  // Implicit conversion to symbol (for use in other distributions' parameter expressions)
  constexpr auto operator*() const { return sym(); }

  // Get log-probability in constrained space: log p(x | params)
  // No Jacobian — MCMC adds Jacobian corrections separately via TransformPack.
  constexpr auto logProb() const {
    return dist_.logProbFor(sym());
  }

  // Get unnormalized log-probability in constrained space (no Jacobian)
  constexpr auto unnormalizedLogProb() const {
    return dist_.unnormalizedLogProbFor(sym());
  }

  // Access the distribution
  constexpr const auto& dist() const { return dist_; }

  // Bind constrained value directly: sigma = 2.0 → Symbol<Id> = 2.0
  template <typename T>
    requires std::is_arithmetic_v<T>
  constexpr auto operator=(T value) const {
    return sym() = static_cast<double>(value);
  }
};

// ============================================================================
// Concept for RandomVar
// ============================================================================

template <typename T>
concept IsRandomVar = requires(const T& rv) {
  typename T::id_type;
  typename T::symbol_type;
  typename T::dist_type;
  { rv.sym() } -> Symbolic;
  { rv.logProb() } -> Symbolic;
  { rv.dist() };
};

// ============================================================================
// Factory functions - create RandomVar with unique Id per call site
// ============================================================================
// Each factory uses decltype([] {}) to generate a unique Id per call site.

template <typename Id = decltype([] {}), typename Mu, typename Sigma>
constexpr auto normal(Mu mu, Sigma sigma) {
  auto dist = NormalDist{toSymbolic(mu), toSymbolic(sigma)};
  return RandomVar<decltype(dist), Id>{dist};
}

template <typename Id = decltype([] {}), typename Sigma>
constexpr auto halfNormal(Sigma sigma) {
  auto dist = HalfNormalDist{toSymbolic(sigma)};
  return RandomVar<decltype(dist), Id>{dist};
}

template <typename Id = decltype([] {}), typename Alpha, typename B>
constexpr auto beta(Alpha alpha, B b) {
  auto dist = BetaDist{toSymbolic(alpha), toSymbolic(b)};
  return RandomVar<decltype(dist), Id>{dist};
}

template <typename Id = decltype([] {}), typename Alpha, typename B>
constexpr auto gamma(Alpha alpha, B b) {
  auto dist = GammaDist{toSymbolic(alpha), toSymbolic(b)};
  return RandomVar<decltype(dist), Id>{dist};
}

template <typename Id = decltype([] {}), typename Lambda>
constexpr auto exponential(Lambda lambda) {
  auto dist = ExponentialDist{toSymbolic(lambda)};
  return RandomVar<decltype(dist), Id>{dist};
}

template <typename Id = decltype([] {}), typename A, typename B>
constexpr auto uniform(A a, B b) {
  auto dist = UniformDist{toSymbolic(a), toSymbolic(b)};
  return RandomVar<decltype(dist), Id>{dist};
}

template <typename Id = decltype([] {}), typename X0, typename G>
constexpr auto cauchy(X0 x0, G g) {
  auto dist = CauchyDist{toSymbolic(x0), toSymbolic(g)};
  return RandomVar<decltype(dist), Id>{dist};
}

template <typename Id = decltype([] {}), typename P>
constexpr auto bernoulli(P p) {
  auto dist = BernoulliDist{toSymbolic(p)};
  return RandomVar<decltype(dist), Id>{dist};
}

template <typename Id = decltype([] {}), typename Nu, typename Mu, typename Sigma>
constexpr auto studentT(Nu nu, Mu mu, Sigma sigma) {
  auto dist = StudentTDist{toSymbolic(nu), toSymbolic(mu), toSymbolic(sigma)};
  return RandomVar<decltype(dist), Id>{dist};
}

template <typename Id = decltype([] {}), typename N, typename P>
constexpr auto binomial(N n, P p) {
  auto dist = BinomialDist{toSymbolic(n), toSymbolic(p)};
  return RandomVar<decltype(dist), Id>{dist};
}

template <typename Id = decltype([] {}), typename Lambda>
constexpr auto poisson(Lambda lambda) {
  auto dist = PoissonDist{toSymbolic(lambda)};
  return RandomVar<decltype(dist), Id>{dist};
}

// ============================================================================
// toSymbolic overload for RandomVar
// ============================================================================
//
// Enables RandomVar to be used directly in expressions:
//   auto alpha = normal(0, 10);
//   auto beta = normal(0, 5);
//   auto mu = alpha + beta * x;  // Works! No need for *alpha, *beta
//
// Returns Atom<Id, Sample<Dist>> — a discoverable atom that carries distribution
// info. This enables collectLogProbs() to auto-discover parent distributions by
// traversing expression trees. The evaluator resolves these atoms by ID lookup
// against Free symbol bindings (no constraint transform at eval time — the
// posterior's TransformPack handles that externally).

template <typename Dist, typename Id>
constexpr auto toSymbolic(const RandomVar<Dist, Id>& rv) {
  return Atom<Id, Sample<Dist>>{Sample<Dist>{rv.dist()}};
}

}  // namespace tempura::symbolic4
