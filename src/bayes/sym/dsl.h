#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <utility>

#include "prob/log_prob.h"
#include "symbolic3/core.h"
#include "symbolic3/derivative.h"
#include "symbolic3/evaluate.h"
#include "symbolic3/operators.h"

// Model DSL for Bayesian inference using symbolic expressions.
//
// Usage:
//
//   constexpr Symbol mu;
//   constexpr Symbol sigma;
//   constexpr Symbol y;
//
//   auto joint = Joint{
//       mu | Normal(0_c, 10_c),
//       sigma | HalfNormal(5_c),
//       y | Normal(mu, sigma)
//   };
//
//   // Condition on observations, specify inference targets
//   auto posterior = joint
//       .observe(y = 2.5)
//       .infer(mu, sigma);  // param order for logProb/gradient
//
//   double lp = posterior.logProb(1.0, 2.0);  // mu=1.0, sigma=2.0
//   auto grad = posterior.gradient(1.0, 2.0); // [d/dmu, d/dsigma]

namespace tempura::bayes::sym {

using namespace tempura::symbolic3;
using namespace tempura::prob;

// =============================================================================
// Var - Just use Symbol directly (each declaration is unique)
// =============================================================================
// Usage: constexpr Symbol mu;  (not Var, since alias doesn't work)

// =============================================================================
// wrapSymbolic - Wrap arithmetic types as Literal, pass through Symbolic types
// =============================================================================
// Enables Normal(0, 10) syntax instead of Normal(0_c, 10_c)

template <typename T>
concept SymbolicOrArithmetic = Symbolic<T> || std::is_arithmetic_v<T>;

template <typename T>
constexpr auto wrapSymbolic(T x) {
  if constexpr (Symbolic<T>) {
    return x;
  } else {
    return Literal{x};
  }
}

// =============================================================================
// Distributions - Lightweight builders that generate log-probability expressions
// =============================================================================

template <Symbolic Mu, Symbolic Sigma>
struct NormalDist {
  [[no_unique_address]] Mu mu;
  [[no_unique_address]] Sigma sigma;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logNormal(x, mu, sigma);
  }
};

template <Symbolic Sigma>
struct HalfNormalDist {
  [[no_unique_address]] Sigma sigma;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logHalfNormal(x, sigma);
  }
};

template <Symbolic Lambda>
struct ExponentialDist {
  [[no_unique_address]] Lambda lambda;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logExponential(x, lambda);
  }
};

template <Symbolic Alpha, Symbolic Bet>
struct BetaDist {
  [[no_unique_address]] Alpha alpha;
  [[no_unique_address]] Bet beta;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logBeta(x, alpha, beta);
  }
};

template <Symbolic A, Symbolic B>
struct UniformDist {
  [[no_unique_address]] A a;
  [[no_unique_address]] B b;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logUniform(x, a, b);
  }
};

template <Symbolic X0, Symbolic Gamma>
struct CauchyDist {
  [[no_unique_address]] X0 x0;
  [[no_unique_address]] Gamma gamma;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logCauchy(x, x0, gamma);
  }
};

template <Symbolic Alpha, Symbolic Bet>
struct GammaDist {
  [[no_unique_address]] Alpha alpha;
  [[no_unique_address]] Bet beta;

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return logGamma(x, alpha, beta);
  }
};

// Factory functions - accept both Symbolic and arithmetic types
template <SymbolicOrArithmetic Mu, SymbolicOrArithmetic Sigma>
constexpr auto Normal(Mu mu, Sigma sigma) {
  auto mu_s = wrapSymbolic(mu);
  auto sigma_s = wrapSymbolic(sigma);
  return NormalDist<decltype(mu_s), decltype(sigma_s)>{mu_s, sigma_s};
}

template <SymbolicOrArithmetic Sigma>
constexpr auto HalfNormal(Sigma sigma) {
  auto sigma_s = wrapSymbolic(sigma);
  return HalfNormalDist<decltype(sigma_s)>{sigma_s};
}

template <SymbolicOrArithmetic Lambda>
constexpr auto Exponential(Lambda lambda) {
  auto lambda_s = wrapSymbolic(lambda);
  return ExponentialDist<decltype(lambda_s)>{lambda_s};
}

template <SymbolicOrArithmetic Alpha, SymbolicOrArithmetic Bet>
constexpr auto Beta(Alpha alpha, Bet beta) {
  auto alpha_s = wrapSymbolic(alpha);
  auto beta_s = wrapSymbolic(beta);
  return BetaDist<decltype(alpha_s), decltype(beta_s)>{alpha_s, beta_s};
}

template <SymbolicOrArithmetic A, SymbolicOrArithmetic B>
constexpr auto Uniform(A a, B b) {
  auto a_s = wrapSymbolic(a);
  auto b_s = wrapSymbolic(b);
  return UniformDist<decltype(a_s), decltype(b_s)>{a_s, b_s};
}

template <SymbolicOrArithmetic X0, SymbolicOrArithmetic Gamma>
constexpr auto Cauchy(X0 x0, Gamma gamma) {
  auto x0_s = wrapSymbolic(x0);
  auto gamma_s = wrapSymbolic(gamma);
  return CauchyDist<decltype(x0_s), decltype(gamma_s)>{x0_s, gamma_s};
}

template <SymbolicOrArithmetic Alpha, SymbolicOrArithmetic Bet>
constexpr auto Gamma(Alpha alpha, Bet beta) {
  auto alpha_s = wrapSymbolic(alpha);
  auto beta_s = wrapSymbolic(beta);
  return GammaDist<decltype(alpha_s), decltype(beta_s)>{alpha_s, beta_s};
}

// Forward declarations
template <typename JointT, typename... Binders>
struct ConditionedJoint;

template <typename JointT, typename Observations, Symbolic... Params>
class Posterior;

// =============================================================================
// Statement - Variable | Distribution creates a statement
// =============================================================================

template <Symbolic Var, typename Dist>
struct Statement {
  using variable = Var;
  using distribution = Dist;

  [[no_unique_address]] Dist dist;

  constexpr auto logProb() const {
    return dist.logProbFor(Var{});
  }
};

// Operator | to create statements: var | Distribution(...)
template <Symbolic Var, typename Dist>
constexpr auto operator|(Var, Dist dist) {
  return Statement<Var, Dist>{dist};
}

// =============================================================================
// Joint - A joint distribution over variables
// =============================================================================

template <typename... Statements>
struct Joint {
  std::tuple<Statements...> statements;

  constexpr Joint(Statements... stmts) : statements{stmts...} {}

  // Total log-probability: sum of all statement log-probs
  constexpr auto logProb() const {
    return std::apply(
        [](auto&... stmt) { return (stmt.logProb() + ...); }, statements);
  }

  // Observe: condition on values, returns a ConditionedJoint
  template <typename... Binders>
  constexpr auto observe(Binders... binders) const {
    return ConditionedJoint<Joint, Binders...>{*this, binders...};
  }
};

// Deduction guide
template <typename... Statements>
Joint(Statements...) -> Joint<Statements...>;

// =============================================================================
// ConditionedJoint - Joint with some variables observed
// =============================================================================

template <typename JointT, typename... Binders>
struct ConditionedJoint {
  JointT joint;
  std::tuple<Binders...> observations;

  constexpr ConditionedJoint(JointT j, Binders... obs)
      : joint{j}, observations{obs...} {}

  // Infer: specify which variables to infer and their order
  // Usage: .infer(mu, sigma) - pass the actual symbol values
  template <Symbolic... Params>
  constexpr auto infer(Params...) const {
    return Posterior<JointT, std::tuple<Binders...>, Params...>{
        joint, observations};
  }
};

// =============================================================================
// Posterior - Ready for inference with specified param order
// =============================================================================

template <typename JointT, typename Observations, Symbolic... Params>
class Posterior {
 public:
  static constexpr std::size_t NumParams = sizeof...(Params);
  using GradArray = std::array<double, NumParams>;
  using ParamTuple = std::tuple<Params...>;

  constexpr Posterior(JointT joint, Observations obs)
      : joint_{joint}, observations_{obs} {}

  // Evaluate log-probability: logProb(p0, p1, ...)
  template <typename... ParamValues>
  auto logProb(ParamValues... params) const -> double {
    static_assert(sizeof...(ParamValues) == NumParams,
                  "Number of arguments must match number of infer parameters");
    auto bindings = makeAllBindings(params...);
    return evaluate(joint_.logProb(), bindings);
  }

  // Evaluate gradient: gradient(p0, p1, ...)
  template <typename... ParamValues>
  auto gradient(ParamValues... params) const -> GradArray {
    static_assert(sizeof...(ParamValues) == NumParams,
                  "Number of arguments must match number of infer parameters");
    auto bindings = makeAllBindings(params...);

    GradArray grad;
    computeGradients(bindings, grad, std::make_index_sequence<NumParams>{});
    return grad;
  }

 private:
  JointT joint_;
  Observations observations_;

  static constexpr ParamTuple param_symbols{};

  // Create bindings for all variables (observations + params)
  template <typename... ParamValues>
  auto makeAllBindings(ParamValues... params) const {
    // Create param bindings
    auto param_bindings = makeParamBindings(
        std::make_index_sequence<NumParams>{}, params...);
    // Merge with observation bindings
    return std::apply(
        [&](auto... obs) {
          return mergeBindings(param_bindings, obs...);
        },
        observations_);
  }

  // Create param bindings from variadic args
  template <std::size_t... Is, typename... ParamValues>
  auto makeParamBindings(std::index_sequence<Is...>,
                         ParamValues... params) const {
    std::array<double, sizeof...(ParamValues)> arr{
        static_cast<double>(params)...};
    return BinderPack{(std::get<Is>(param_symbols) = arr[Is])...};
  }

  // Merge param bindings with observation binders
  template <typename ParamBindings, typename... Obs>
  static auto mergeBindings(ParamBindings pb, Obs... obs) {
    return mergePacks(pb, BinderPack{obs...});
  }

  // Merge two BinderPacks
  template <typename... B1, typename... B2>
  static auto mergePacks(BinderPack<B1...> bp1, BinderPack<B2...> bp2) {
    return BinderPack{static_cast<B1&>(bp1)..., static_cast<B2&>(bp2)...};
  }

  // Compute all gradients
  template <typename Bindings, std::size_t... Is>
  void computeGradients(const Bindings& bindings, GradArray& grad,
                        std::index_sequence<Is...>) const {
    ((grad[Is] = evaluate(diff(joint_.logProb(), std::get<Is>(param_symbols)),
                          bindings)),
     ...);
  }
};

}  // namespace tempura::bayes::sym
