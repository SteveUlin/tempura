#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <random>

#include "prob/gamma.h"

namespace tempura::prob {

// Beta distribution: p(x|α,β) ∝ x^(α-1) · (1-x)^(β-1) on [0,1]
//
// Models probabilities and proportions. Beta(1,1) = Uniform(0,1).
// α,β > 1 gives bell-shaped; α,β < 1 gives U-shaped.
template <std::copyable Alpha, std::copyable BetaParam>
class Beta {
 public:
  constexpr Beta(Alpha alpha, BetaParam beta) : alpha_{alpha}, beta_{beta} {}

  template <typename X>
  constexpr auto prob(X x) const {
    using std::exp;
    return exp(logProb(x));
  }

  template <typename X>
  constexpr auto logProb(X x) const {
    using std::lgamma;
    using std::log;
    double log_beta_fn = lgamma(alpha_) + lgamma(beta_) - lgamma(alpha_ + beta_);
    return (alpha_ - 1.0) * log(x) + (beta_ - 1.0) * log(1.0 - x) - log_beta_fn;
  }

  // Unnormalized: omits -logB(α,β) constant
  template <typename X>
  constexpr auto unnormalizedLogProb(X x) const {
    using std::log;
    return (alpha_ - 1.0) * log(x) + (beta_ - 1.0) * log(1.0 - x);
  }

  // If X ~ Gamma(α,1) and Y ~ Gamma(β,1), then X/(X+Y) ~ Beta(α,β)
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const {
    double x = Gamma{static_cast<double>(alpha_), 1.0}.sample(rng);
    double y = Gamma{static_cast<double>(beta_), 1.0}.sample(rng);
    return x / (x + y);
  }

  constexpr auto mean() const { return alpha_ / (alpha_ + beta_); }

  constexpr auto variance() const {
    auto sum = alpha_ + beta_;
    return (alpha_ * beta_) / (sum * sum * (sum + 1.0));
  }

  // Mode = (α-1)/(α+β-2), undefined when α ≤ 1 or β ≤ 1
  constexpr auto mode() const {
    return (alpha_ - 1.0) / (alpha_ + beta_ - 2.0);
  }

  constexpr auto alpha() const { return alpha_; }
  constexpr auto beta() const { return beta_; }

 private:
  Alpha alpha_;
  BetaParam beta_;
};

// Deduction guide
template <typename Alpha, typename BetaParam>
Beta(Alpha, BetaParam) -> Beta<Alpha, BetaParam>;

}  // namespace tempura::prob
