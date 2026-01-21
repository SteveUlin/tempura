#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <random>

namespace tempura::prob {

// Bernoulli distribution: P(X=1) = p, P(X=0) = 1-p
//
// Models a single coin flip. The building block for binary random events.
template <std::copyable P>
class Bernoulli {
 public:
  constexpr explicit Bernoulli(P p) : p_{p} {}

  template <typename X>
  constexpr auto prob(X x) const {
    // x should be 0 or 1 (or boolean)
    return x ? p_ : (1.0 - p_);
  }

  template <typename X>
  constexpr auto logProb(X x) const {
    using std::log;
    return x ? log(p_) : log(1.0 - p_);
  }

  // Unnormalized: same as logProb for Bernoulli (no normalizing constant)
  template <typename X>
  constexpr auto unnormalizedLogProb(X x) const {
    return logProb(x);
  }

  // Inverse transform sampling
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const -> bool {
    using R = typename RNG::result_type;
    constexpr R range = RNG::max() - RNG::min();
    R threshold = static_cast<R>(static_cast<double>(p_) * static_cast<double>(range));
    return (rng() - RNG::min()) < threshold;
  }

  constexpr auto mean() const { return p_; }
  constexpr auto variance() const { return p_ * (1.0 - p_); }
  constexpr auto p() const { return p_; }

 private:
  P p_;
};

// Deduction guide
template <typename P>
Bernoulli(P) -> Bernoulli<P>;

}  // namespace tempura::prob
