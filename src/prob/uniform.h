#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <random>

namespace tempura::prob {

// Uniform distribution: p(x|a,b) = 1/(b-a) for a ≤ x ≤ b
//
// Every value in the interval is equally likely. Maximum entropy for bounded support.
template <std::copyable Lo, std::copyable Hi>
class Uniform {
 public:
  constexpr Uniform(Lo lo, Hi hi) : lo_{lo}, hi_{hi} {}

  template <typename X>
  constexpr auto prob(X x) const {
    // Uniform density doesn't depend on x (within bounds)
    return 1.0 / (hi_ - lo_);
  }

  template <typename X>
  constexpr auto logProb(X x) const {
    using std::log;
    return -log(hi_ - lo_);
  }

  // Unnormalized: constant (0 in log space)
  template <typename X>
  constexpr auto unnormalizedLogProb(X /*x*/) const {
    return 0.0;
  }

  // Inverse transform sampling
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const {
    constexpr auto range = static_cast<double>(RNG::max() - RNG::min());
    double u = static_cast<double>(rng() - RNG::min()) / range;
    return lo_ + (hi_ - lo_) * u;
  }

  constexpr auto mean() const { return (lo_ + hi_) / 2.0; }

  constexpr auto variance() const {
    auto width = hi_ - lo_;
    return width * width / 12.0;
  }

  constexpr auto lo() const { return lo_; }
  constexpr auto hi() const { return hi_; }

  template <typename X>
  constexpr auto cdf(X x) const {
    return (x - lo_) / (hi_ - lo_);
  }

 private:
  Lo lo_;
  Hi hi_;
};

// Deduction guide
template <typename Lo, typename Hi>
Uniform(Lo, Hi) -> Uniform<Lo, Hi>;

}  // namespace tempura::prob
