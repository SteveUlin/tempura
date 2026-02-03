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

#include "prob/categorical.h"

namespace tempura::prob {

// Multinomial distribution: P(X=x) = n! / prod_k(x_k!) * prod_k(p_k^x_k)
//
// Models counts when drawing n items from K categories with probabilities p.
// This is the multivariate generalization of Binomial.
//
// If X ~ Multinomial(n, p), then:
//   - X is a K-dimensional count vector
//   - sum_k X_k = n
//   - Each X_k ~ Binomial(n, p_k) marginally (but not independently)

template <std::size_t K>
class Multinomial {
 public:
  static_assert(K >= 2, "Multinomial requires at least 2 categories");

  constexpr Multinomial(int64_t n, std::array<double, K> probs)
      : n_{n}, probs_{probs} {
    assert(n >= 0);
    normalizeIfNeeded();
  }

  // P(X=x) - returns probability (not log)
  auto prob(std::span<const int64_t, K> x) const -> double {
    return std::exp(logProb(x));
  }

  auto prob(const std::array<int64_t, K>& x) const -> double {
    return std::exp(logProb(x));
  }

  // log P(X=x) = log(n!) - sum_k log(x_k!) + sum_k x_k * log(p_k)
  auto logProb(std::span<const int64_t, K> x) const -> double {
    // Check constraint: sum x_k = n
    int64_t sum_x = std::accumulate(x.begin(), x.end(), int64_t{0});
    if (sum_x != n_) {
      return -std::numeric_limits<double>::infinity();
    }

    double log_p = std::lgamma(static_cast<double>(n_) + 1.0);
    for (std::size_t k = 0; k < K; ++k) {
      log_p -= std::lgamma(static_cast<double>(x[k]) + 1.0);
      if (x[k] > 0) {
        log_p += static_cast<double>(x[k]) * std::log(probs_[k]);
      } else if (probs_[k] == 0.0) {
        // 0^0 = 1, so contributes 0 to log-prob
      }
    }
    return log_p;
  }

  auto logProb(const std::array<int64_t, K>& x) const -> double {
    return logProb(std::span<const int64_t, K>(x));
  }

  // Unnormalized: sum_k x_k * log(p_k)
  auto unnormalizedLogProb(std::span<const int64_t, K> x) const -> double {
    double log_p = 0.0;
    for (std::size_t k = 0; k < K; ++k) {
      if (x[k] > 0) {
        log_p += static_cast<double>(x[k]) * std::log(probs_[k]);
      }
    }
    return log_p;
  }

  // Sample by drawing n times from categorical
  template <std::uniform_random_bit_generator RNG>
  auto sample(RNG& rng) const -> std::array<int64_t, K> {
    Categorical<K> cat{probs_};
    std::array<int64_t, K> counts{};
    for (int64_t i = 0; i < n_; ++i) {
      ++counts[cat.sample(rng)];
    }
    return counts;
  }

  // E[X_k] = n * p_k
  auto mean() const -> std::array<double, K> {
    std::array<double, K> m;
    for (std::size_t k = 0; k < K; ++k) {
      m[k] = static_cast<double>(n_) * probs_[k];
    }
    return m;
  }

  // Var[X_k] = n * p_k * (1 - p_k)
  auto variance() const -> std::array<double, K> {
    std::array<double, K> v;
    for (std::size_t k = 0; k < K; ++k) {
      v[k] = static_cast<double>(n_) * probs_[k] * (1.0 - probs_[k]);
    }
    return v;
  }

  auto n() const -> int64_t { return n_; }
  auto probs() const -> const std::array<double, K>& { return probs_; }
  static constexpr auto numCategories() -> std::size_t { return K; }

 private:
  int64_t n_;
  std::array<double, K> probs_;

  void normalizeIfNeeded() {
    double sum = std::accumulate(probs_.begin(), probs_.end(), 0.0);
    if (std::abs(sum - 1.0) > 1e-10) {
      for (auto& p : probs_) p /= sum;
    }
  }
};

// Dynamic-size Multinomial
class MultinomialDynamic {
 public:
  MultinomialDynamic(int64_t n, std::vector<double> probs)
      : n_{n}, probs_{std::move(probs)} {
    assert(n >= 0);
    assert(probs_.size() >= 2);
    normalizeIfNeeded();
  }

  auto prob(std::span<const int64_t> x) const -> double {
    return std::exp(logProb(x));
  }

  auto logProb(std::span<const int64_t> x) const -> double {
    assert(x.size() == probs_.size());
    int64_t sum_x = std::accumulate(x.begin(), x.end(), int64_t{0});
    if (sum_x != n_) {
      return -std::numeric_limits<double>::infinity();
    }

    double log_p = std::lgamma(static_cast<double>(n_) + 1.0);
    for (std::size_t k = 0; k < probs_.size(); ++k) {
      log_p -= std::lgamma(static_cast<double>(x[k]) + 1.0);
      if (x[k] > 0) {
        log_p += static_cast<double>(x[k]) * std::log(probs_[k]);
      }
    }
    return log_p;
  }

  auto unnormalizedLogProb(std::span<const int64_t> x) const -> double {
    double log_p = 0.0;
    for (std::size_t k = 0; k < probs_.size(); ++k) {
      if (x[k] > 0) {
        log_p += static_cast<double>(x[k]) * std::log(probs_[k]);
      }
    }
    return log_p;
  }

  template <std::uniform_random_bit_generator RNG>
  auto sample(RNG& rng) const -> std::vector<int64_t> {
    CategoricalDynamic cat{probs_};
    std::vector<int64_t> counts(probs_.size(), 0);
    for (int64_t i = 0; i < n_; ++i) {
      ++counts[cat.sample(rng)];
    }
    return counts;
  }

  auto mean() const -> std::vector<double> {
    std::vector<double> m(probs_.size());
    for (std::size_t k = 0; k < probs_.size(); ++k) {
      m[k] = static_cast<double>(n_) * probs_[k];
    }
    return m;
  }

  auto n() const -> int64_t { return n_; }
  auto probs() const -> const std::vector<double>& { return probs_; }
  auto numCategories() const -> std::size_t { return probs_.size(); }

 private:
  int64_t n_;
  std::vector<double> probs_;

  void normalizeIfNeeded() {
    double sum = std::accumulate(probs_.begin(), probs_.end(), 0.0);
    if (std::abs(sum - 1.0) > 1e-10) {
      for (auto& p : probs_) p /= sum;
    }
  }
};

}  // namespace tempura::prob
