#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <random>

namespace tempura::prob {

// Binomial distribution: P(X=k) = C(n,k) p^k (1-p)^(n-k)
//
// Models the number of successes in n independent Bernoulli(p) trials.
template <std::copyable N, std::copyable P>
class Binomial {
 public:
  constexpr Binomial(N n, P p) : n_{n}, p_{p} {}

  template <typename K>
  constexpr auto prob(K k) const {
    using std::exp;
    return exp(logProb(k));
  }

  template <typename K>
  constexpr auto logProb(K k) const {
    using std::lgamma;
    using std::log;

    auto n = static_cast<double>(n_);
    auto kd = static_cast<double>(k);
    auto p = static_cast<double>(p_);

    // log C(n,k) = logΓ(n+1) - logΓ(k+1) - logΓ(n-k+1)
    double log_binom_coef =
        lgamma(n + 1.0) - lgamma(kd + 1.0) - lgamma(n - kd + 1.0);

    double log_p_term = 0.0;
    double log_1mp_term = 0.0;

    if (kd > 0.0) {
      log_p_term = kd * log(p);
    }

    if (kd < n) {
      log_1mp_term = (n - kd) * log(1.0 - p);
    }

    return log_binom_coef + log_p_term + log_1mp_term;
  }

  // Unnormalized: omits log C(n,k) constant
  template <typename K>
  constexpr auto unnormalizedLogProb(K k) const {
    using std::log;

    auto n = static_cast<double>(n_);
    auto kd = static_cast<double>(k);
    auto p = static_cast<double>(p_);

    double result = 0.0;
    if (kd > 0.0) {
      result += kd * log(p);
    }
    if (kd < n) {
      result += (n - kd) * log(1.0 - p);
    }
    return result;
  }

  // Direct simulation: O(n) but exact
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const -> int64_t {
    constexpr double scale =
        1.0 / static_cast<double>(RNG::max() - RNG::min());

    auto n = static_cast<int64_t>(n_);
    auto p = static_cast<double>(p_);

    int64_t count = 0;
    for (int64_t i = 0; i < n; ++i) {
      double u = static_cast<double>(rng() - RNG::min()) * scale;
      if (u < p) {
        ++count;
      }
    }
    return count;
  }

  constexpr auto mean() const { return static_cast<double>(n_) * p_; }

  constexpr auto variance() const {
    return static_cast<double>(n_) * p_ * (1.0 - p_);
  }

  constexpr auto n() const { return n_; }
  constexpr auto p() const { return p_; }

 private:
  N n_;
  P p_;
};

// Deduction guide
template <typename N, typename P>
Binomial(N, P) -> Binomial<N, P>;

}  // namespace tempura::prob
