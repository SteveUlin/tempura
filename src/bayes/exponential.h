#pragma once

#include <cassert>
#include <cmath>
#include <random>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Exponential distribution: p(x|λ) = λ exp(-λx) for x ≥ 0
//
// Models waiting time until the next event in a memoryless process.
template <typename T = double>
class Exponential {
  static_assert(!std::is_integral_v<T>, "Exponential requires a floating-point type");

 public:
  constexpr Exponential(T λ = T{1}) : λ_{λ} {
    assert(λ > T{0} && "requires λ > 0");
  }

  // Inverse transform sampling: X = -ln(U)/λ, reject U=0 to avoid log(0)
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& gen) -> T {
    constexpr auto range = static_cast<T>(Generator::max() - Generator::min());
    auto bits = gen();
    while (bits == Generator::min()) [[unlikely]] {
      bits = gen();
    }
    const T u = static_cast<T>(bits - Generator::min()) / range;
    using std::log;
    return -log(u) / λ_;
  }

  constexpr auto prob(T x) const -> T {
    if (x < T{0}) {
      return T{0};
    }
    using std::exp;
    return λ_ * exp(-λ_ * x);
  }

  constexpr auto logProb(T x) const -> T {
    if (x < T{0}) return -numeric_infinity(T{});
    using std::log;
    return log(λ_) - λ_ * x;
  }

  constexpr auto cdf(T x) const -> T {
    if (x < T{0}) {
      return T{0};
    }
    using std::exp;
    return T{1} - exp(-λ_ * x);
  }

  constexpr auto invCdf(T p) const -> T {
    assert(p >= T{0} && p <= T{1} && "requires 0 ≤ p ≤ 1");
    using std::log;
    return -log(T{1} - p) / λ_;
  }

  constexpr auto mean() const -> T { return T{1} / λ_; }
  constexpr auto variance() const -> T { return T{1} / (λ_ * λ_); }
  constexpr auto lambda() const -> T { return λ_; }

 private:
  T λ_;
};

}  // namespace tempura::bayes
