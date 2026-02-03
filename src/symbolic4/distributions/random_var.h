#pragma once

#include "symbolic4/constraints.h"
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
// For distributions with constrained support (Positive, Unit), RandomVar
// automatically handles parameter transforms and Jacobians:
//   - Internal symbol z is unconstrained (what HMC samples)
//   - sym() returns the constrained expression (exp(z), sigmoid(z), etc.)
//   - logProb() includes the Jacobian correction
//
// ============================================================================

// ============================================================================
// RandomVarSymbol - The symbol type for random variables
// ============================================================================
//
// Unlike plain Symbol<Id> (which is Atom<Id, Free>), RandomVarSymbol carries
// its distribution in the type system via Sample<Dist> effect. This enables:
//   - Auto-discovery of random variables during expression traversal
//   - Extraction of distribution for log-probability computation
//   - Type-safe differentiation between free variables and random variables
//
// RandomVarSymbol<Id, Dist> is an alias for Atom<Id, Sample<Dist>>.

template <typename Id, typename Dist>
using RandomVarSymbol = Atom<Id, Sample<Dist>>;

template <typename Dist, typename Id>
struct RandomVar {
  using id_type = Id;
  using dist_type = Dist;
  using support_type = typename Dist::support_type;
  using symbol_type = RandomVarSymbol<Id, Dist>;

  // The unconstrained symbol (what HMC samples / what we differentiate w.r.t.)
  using unconstrained_symbol_type = Symbol<Id>;

  [[no_unique_address]] Dist dist_;

  constexpr explicit RandomVar(Dist dist) : dist_{dist} {}

  // Get the unconstrained symbol (for HMC sampling and differentiation)
  static constexpr auto unconstrainedSym() { return unconstrained_symbol_type{}; }

  // Get the constrained expression - just returns the Sample atom
  // The constraint transform is applied during evaluation (see eval.h)
  // This enables auto-discovery of parent RVs in expression trees
  constexpr auto constrainedExpr() const {
    return symbol_type{Sample<Dist>{dist_}};
  }

  // Static version for use when instance isn't available (e.g., in transforms)
  // Returns the unconstrained symbol (constraint applied at eval time)
  static constexpr auto unconstrainedExpr() {
    return constraints::expr<support_type>(unconstrainedSym());
  }

  // Get the Jacobian expression: log |dx/dz|
  static constexpr auto jacobian() {
    return constraints::logJacobian<support_type>(unconstrainedSym());
  }

  // Get the symbol representing this random variable (with distribution info)
  // Note: For constrained distributions, this carries the transform info
  constexpr auto sym() const { return symbol_type{Sample<Dist>{dist_}}; }

  // For backward compatibility: freeSym() returns the unconstrained symbol
  static constexpr auto freeSym() { return unconstrainedSym(); }

  // Implicit conversion to constrained expression (for use in other distributions)
  constexpr auto operator*() const { return constrainedExpr(); }

  // Get log-probability for this RV: log p(x | params) + log |dx/dz|
  // Uses unconstrainedExpr() which applies the transform symbolically
  // Includes Jacobian correction for constrained distributions
  constexpr auto logProb() const {
    return dist_.logProbFor(unconstrainedExpr()) + jacobian();
  }

  // Get unnormalized log-probability (with Jacobian)
  constexpr auto unnormalizedLogProb() const {
    return dist_.unnormalizedLogProbFor(unconstrainedExpr()) + jacobian();
  }

  // Access the distribution
  constexpr const auto& dist() const { return dist_; }

  // Enable binding syntax: rv = value
  // For constrained distributions, value should be in constrained space,
  // but we bind to the unconstrained symbol
  template <typename T>
    requires std::is_arithmetic_v<T>
  constexpr auto operator=(T value) const {
    // Note: For constrained distributions, we need to transform the value
    // This is handled at evaluation time; here we bind the unconstrained symbol
    return unconstrainedSym() = value;
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
// This enables RandomVar to be used directly in expressions:
//   auto alpha = normal(0, 10);
//   auto beta = normal(0, 5);
//   auto mu = alpha + beta * x;  // Works! No need for *alpha, *beta
//
// Returns the random variable symbol (Atom<Id, Sample<Dist>>), which carries
// distribution info needed for:
//   - Auto-discovery of parent RVs (collectLogProbs, discoverParams)
//   - Correct evaluation with automatic constraint transforms
//
// Evaluation of Sample atoms applies the appropriate transform based on
// the distribution's support type (see eval.h).

template <typename Dist, typename Id>
constexpr auto toSymbolic(const RandomVar<Dist, Id>& rv) {
  return rv.sym();
}

}  // namespace tempura::symbolic4
