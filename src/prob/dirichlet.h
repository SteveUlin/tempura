#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <numeric>
#include <random>
#include <span>
#include <vector>

#include "prob/gamma.h"

namespace tempura::prob {

// Dirichlet distribution: p(x|alpha) = (1/B(alpha)) * prod_k x_k^(alpha_k - 1)
//
// The K-dimensional Dirichlet is the multivariate generalization of Beta.
// If X ~ Dirichlet(alpha), then X is a K-dimensional probability vector:
//   sum_k x_k = 1 and x_k >= 0 for all k.
//
// Special case: Dirichlet([1,1,...,1]) = Uniform on the simplex.
// Special case: K=2 Dirichlet([a,b]) is equivalent to Beta(a,b) for x[0].

template <std::size_t K>
class Dirichlet {
 public:
  static_assert(K >= 2, "Dirichlet requires at least 2 dimensions");

  constexpr Dirichlet(std::array<double, K> alpha) : alpha_{alpha} {
    for (auto a : alpha_) {
      assert(a > 0.0 && "Dirichlet concentration parameters must be positive");
    }
  }

  // Convenience constructor from initializer list
  template <typename... Ts>
    requires(sizeof...(Ts) == K && (std::convertible_to<Ts, double> && ...))
  constexpr Dirichlet(Ts... alphas) : alpha_{static_cast<double>(alphas)...} {}

  // Log-probability: sum_k (alpha_k - 1) * log(x_k) - logB(alpha)
  auto logProb(std::span<const double, K> x) const -> double {
    double log_p = -logBeta();
    for (std::size_t k = 0; k < K; ++k) {
      if (x[k] > 0.0) {
        log_p += (alpha_[k] - 1.0) * std::log(x[k]);
      } else if (alpha_[k] > 1.0) {
        return -std::numeric_limits<double>::infinity();
      }
    }
    return log_p;
  }

  auto logProb(const std::array<double, K>& x) const -> double {
    return logProb(std::span<const double, K>(x));
  }

  // Unnormalized: sum_k (alpha_k - 1) * log(x_k)
  auto unnormalizedLogProb(std::span<const double, K> x) const -> double {
    double log_p = 0.0;
    for (std::size_t k = 0; k < K; ++k) {
      if (x[k] > 0.0) {
        log_p += (alpha_[k] - 1.0) * std::log(x[k]);
      } else if (alpha_[k] > 1.0) {
        return -std::numeric_limits<double>::infinity();
      }
    }
    return log_p;
  }

  // Sample using gamma trick: if Y_k ~ Gamma(alpha_k, 1), then X_k = Y_k / sum(Y)
  template <std::uniform_random_bit_generator RNG>
  auto sample(RNG& rng) const -> std::array<double, K> {
    std::array<double, K> y;
    for (std::size_t k = 0; k < K; ++k) {
      y[k] = Gamma{alpha_[k], 1.0}.sample(rng);
    }
    double sum = std::accumulate(y.begin(), y.end(), 0.0);
    for (std::size_t k = 0; k < K; ++k) {
      y[k] /= sum;
    }
    return y;
  }

  // Mean: E[X_k] = alpha_k / sum(alpha)
  auto mean() const -> std::array<double, K> {
    double sum_alpha = std::accumulate(alpha_.begin(), alpha_.end(), 0.0);
    std::array<double, K> m;
    for (std::size_t k = 0; k < K; ++k) {
      m[k] = alpha_[k] / sum_alpha;
    }
    return m;
  }

  // Concentration (sum of alphas)
  auto concentration() const -> double {
    return std::accumulate(alpha_.begin(), alpha_.end(), 0.0);
  }

  auto alpha() const -> const std::array<double, K>& { return alpha_; }
  auto alpha(std::size_t k) const -> double { return alpha_[k]; }

  static constexpr auto dim() -> std::size_t { return K; }

 private:
  std::array<double, K> alpha_;

  // Log of multivariate beta function: logB(alpha) = sum_k lgamma(alpha_k) - lgamma(sum_k alpha_k)
  auto logBeta() const -> double {
    double sum_lgamma = 0.0;
    double sum_alpha = 0.0;
    for (std::size_t k = 0; k < K; ++k) {
      sum_lgamma += std::lgamma(alpha_[k]);
      sum_alpha += alpha_[k];
    }
    return sum_lgamma - std::lgamma(sum_alpha);
  }
};

// Deduction guide
template <typename... Ts>
Dirichlet(Ts...) -> Dirichlet<sizeof...(Ts)>;

// Dynamic-size Dirichlet for runtime-determined K
class DirichletDynamic {
 public:
  explicit DirichletDynamic(std::vector<double> alpha) : alpha_{std::move(alpha)} {
    assert(alpha_.size() >= 2 && "Dirichlet requires at least 2 dimensions");
    for (auto a : alpha_) {
      assert(a > 0.0 && "Dirichlet concentration parameters must be positive");
    }
  }

  auto logProb(std::span<const double> x) const -> double {
    assert(x.size() == alpha_.size());
    double log_p = -logBeta();
    for (std::size_t k = 0; k < alpha_.size(); ++k) {
      if (x[k] > 0.0) {
        log_p += (alpha_[k] - 1.0) * std::log(x[k]);
      } else if (alpha_[k] > 1.0) {
        return -std::numeric_limits<double>::infinity();
      }
    }
    return log_p;
  }

  auto unnormalizedLogProb(std::span<const double> x) const -> double {
    assert(x.size() == alpha_.size());
    double log_p = 0.0;
    for (std::size_t k = 0; k < alpha_.size(); ++k) {
      if (x[k] > 0.0) {
        log_p += (alpha_[k] - 1.0) * std::log(x[k]);
      } else if (alpha_[k] > 1.0) {
        return -std::numeric_limits<double>::infinity();
      }
    }
    return log_p;
  }

  template <std::uniform_random_bit_generator RNG>
  auto sample(RNG& rng) const -> std::vector<double> {
    std::vector<double> y(alpha_.size());
    for (std::size_t k = 0; k < alpha_.size(); ++k) {
      y[k] = Gamma{alpha_[k], 1.0}.sample(rng);
    }
    double sum = std::accumulate(y.begin(), y.end(), 0.0);
    for (auto& yk : y) {
      yk /= sum;
    }
    return y;
  }

  auto mean() const -> std::vector<double> {
    double sum_alpha = std::accumulate(alpha_.begin(), alpha_.end(), 0.0);
    std::vector<double> m(alpha_.size());
    for (std::size_t k = 0; k < alpha_.size(); ++k) {
      m[k] = alpha_[k] / sum_alpha;
    }
    return m;
  }

  auto concentration() const -> double {
    return std::accumulate(alpha_.begin(), alpha_.end(), 0.0);
  }

  auto alpha() const -> const std::vector<double>& { return alpha_; }
  auto alpha(std::size_t k) const -> double { return alpha_[k]; }
  auto dim() const -> std::size_t { return alpha_.size(); }

 private:
  std::vector<double> alpha_;

  auto logBeta() const -> double {
    double sum_lgamma = 0.0;
    double sum_alpha = 0.0;
    for (auto a : alpha_) {
      sum_lgamma += std::lgamma(a);
      sum_alpha += a;
    }
    return sum_lgamma - std::lgamma(sum_alpha);
  }
};

}  // namespace tempura::prob
