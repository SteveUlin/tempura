#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <numeric>
#include <random>
#include <span>
#include <vector>

namespace tempura::prob {

// Categorical distribution: P(X=k) = p_k for k in {0, 1, ..., K-1}
//
// A discrete distribution over K categories with probabilities p.
// This is the generalization of Bernoulli to K > 2 categories.
//
// Note: probs should sum to 1, but this is not enforced for efficiency.
// Use unnormalized weights and let the class normalize if needed.

template <std::size_t K>
class Categorical {
 public:
  static_assert(K >= 2, "Categorical requires at least 2 categories");

  constexpr Categorical(std::array<double, K> probs) : probs_{probs} {
    normalizeIfNeeded();
  }

  template <typename... Ts>
    requires(sizeof...(Ts) == K && (std::convertible_to<Ts, double> && ...))
  constexpr Categorical(Ts... ps) : probs_{static_cast<double>(ps)...} {
    normalizeIfNeeded();
  }

  // P(X=k)
  auto prob(std::size_t k) const -> double {
    assert(k < K);
    return probs_[k];
  }

  // log P(X=k)
  auto logProb(std::size_t k) const -> double {
    assert(k < K);
    return std::log(probs_[k]);
  }

  // Sample a category
  template <std::uniform_random_bit_generator RNG>
  auto sample(RNG& rng) const -> std::size_t {
    constexpr double scale = 1.0 / static_cast<double>(RNG::max() - RNG::min());
    double u = static_cast<double>(rng() - RNG::min()) * scale;

    double cumsum = 0.0;
    for (std::size_t k = 0; k < K - 1; ++k) {
      cumsum += probs_[k];
      if (u < cumsum) return k;
    }
    return K - 1;
  }

  // Expected value (useful when categories represent ordered values 0,1,...,K-1)
  auto mean() const -> double {
    double m = 0.0;
    for (std::size_t k = 0; k < K; ++k) {
      m += static_cast<double>(k) * probs_[k];
    }
    return m;
  }

  // Mode: most likely category
  auto mode() const -> std::size_t {
    std::size_t best = 0;
    for (std::size_t k = 1; k < K; ++k) {
      if (probs_[k] > probs_[best]) best = k;
    }
    return best;
  }

  auto probs() const -> const std::array<double, K>& { return probs_; }
  static constexpr auto numCategories() -> std::size_t { return K; }

 private:
  std::array<double, K> probs_;

  void normalizeIfNeeded() {
    double sum = std::accumulate(probs_.begin(), probs_.end(), 0.0);
    if (std::abs(sum - 1.0) > 1e-10) {
      for (auto& p : probs_) p /= sum;
    }
  }
};

// Deduction guide
template <typename... Ts>
Categorical(Ts...) -> Categorical<sizeof...(Ts)>;

// Dynamic-size Categorical
class CategoricalDynamic {
 public:
  explicit CategoricalDynamic(std::vector<double> probs) : probs_{std::move(probs)} {
    assert(probs_.size() >= 2);
    normalizeIfNeeded();
  }

  auto prob(std::size_t k) const -> double {
    assert(k < probs_.size());
    return probs_[k];
  }

  auto logProb(std::size_t k) const -> double {
    assert(k < probs_.size());
    return std::log(probs_[k]);
  }

  template <std::uniform_random_bit_generator RNG>
  auto sample(RNG& rng) const -> std::size_t {
    constexpr double scale = 1.0 / static_cast<double>(RNG::max() - RNG::min());
    double u = static_cast<double>(rng() - RNG::min()) * scale;

    double cumsum = 0.0;
    for (std::size_t k = 0; k < probs_.size() - 1; ++k) {
      cumsum += probs_[k];
      if (u < cumsum) return k;
    }
    return probs_.size() - 1;
  }

  auto mean() const -> double {
    double m = 0.0;
    for (std::size_t k = 0; k < probs_.size(); ++k) {
      m += static_cast<double>(k) * probs_[k];
    }
    return m;
  }

  auto mode() const -> std::size_t {
    std::size_t best = 0;
    for (std::size_t k = 1; k < probs_.size(); ++k) {
      if (probs_[k] > probs_[best]) best = k;
    }
    return best;
  }

  auto probs() const -> const std::vector<double>& { return probs_; }
  auto numCategories() const -> std::size_t { return probs_.size(); }

 private:
  std::vector<double> probs_;

  void normalizeIfNeeded() {
    double sum = std::accumulate(probs_.begin(), probs_.end(), 0.0);
    if (std::abs(sum - 1.0) > 1e-10) {
      for (auto& p : probs_) p /= sum;
    }
  }
};

}  // namespace tempura::prob
