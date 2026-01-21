#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <random>

namespace tempura::prob {

// Exponential distribution: p(x|λ) = λ exp(-λx) for x ≥ 0
//
// Models waiting time until the next event in a memoryless process.
// Mean = 1/λ, Variance = 1/λ².
template <std::copyable Lambda>
class Exponential {
 public:
  constexpr explicit Exponential(Lambda lambda) : lambda_{lambda} {}

  template <typename X>
  constexpr auto prob(X x) const {
    using std::exp;
    return lambda_ * exp(-lambda_ * x);
  }

  template <typename X>
  constexpr auto logProb(X x) const {
    using std::log;
    return log(lambda_) - lambda_ * x;
  }

  // Unnormalized: omits log(λ) constant
  template <typename X>
  constexpr auto unnormalizedLogProb(X x) const {
    return -lambda_ * x;
  }

  // Inverse transform sampling: X = -ln(U)/λ
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const {
    constexpr auto range = static_cast<double>(RNG::max() - RNG::min());

    auto bits = rng();
    while (bits == RNG::min()) [[unlikely]] {
      bits = rng();
    }
    double u = static_cast<double>(bits - RNG::min()) / range;

    using std::log;
    return -log(u) / lambda_;
  }

  constexpr auto mean() const { return 1.0 / lambda_; }
  constexpr auto variance() const { return 1.0 / (lambda_ * lambda_); }
  constexpr auto lambda() const { return lambda_; }

  template <typename X>
  constexpr auto cdf(X x) const {
    using std::exp;
    return 1.0 - exp(-lambda_ * x);
  }

  template <typename P>
  constexpr auto invCdf(P p) const {
    using std::log;
    return -log(1.0 - p) / lambda_;
  }

 private:
  Lambda lambda_;
};

// Deduction guide
template <typename Lambda>
Exponential(Lambda) -> Exponential<Lambda>;

}  // namespace tempura::prob
