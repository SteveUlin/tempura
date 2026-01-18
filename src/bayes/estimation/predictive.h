#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <random>
#include <utility>
#include <vector>

namespace tempura::bayes {

// =============================================================================
// Concepts for Predictive Sampling
// =============================================================================

template <typename S, typename Gen>
concept PriorSamplerConcept = requires(S sampler, Gen& g) {
  { sampler(g) };
};

template <typename S, typename Param, typename Gen>
concept LikelihoodSamplerConcept = requires(S sampler, const Param& p, Gen& g) {
  { sampler(p, g) };
};

// =============================================================================
// PriorPredictive - Generate data from the prior
// =============================================================================
//
// Prior predictive distribution: p(y) = integral p(y|theta) p(theta) d(theta)
//
// This answers: "What data would my model generate before seeing any observations?"
// Useful for prior predictive checking - ensuring priors encode reasonable beliefs.
//
// Usage:
//   auto prior_pred = makePriorPredictive(
//       [](auto& g) { return Normal<>(0, 10).sample(g); },  // Sample theta from prior
//       [](double theta, auto& g) { return Normal<>(theta, 2).sample(g); }  // Sample y|theta
//   );
//   auto y = prior_pred.sample(gen);
//   auto ys = prior_pred.sampleN(gen, 1000);

template <typename PriorSampler, typename LikelihoodSampler>
class PriorPredictive {
 public:
  constexpr PriorPredictive(PriorSampler prior, LikelihoodSampler likelihood)
      : prior_{std::move(prior)}, likelihood_{std::move(likelihood)} {}

  // Sample one data point from the prior predictive
  template <std::uniform_random_bit_generator Gen>
  auto sample(Gen& g) {
    auto theta = prior_(g);
    return likelihood_(theta, g);
  }

  // Sample N data points from the prior predictive
  template <std::uniform_random_bit_generator Gen>
  auto sampleN(Gen& g, std::size_t n) {
    using DataType = decltype(sample(g));
    std::vector<DataType> samples;
    samples.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      samples.push_back(sample(g));
    }
    return samples;
  }

 private:
  PriorSampler prior_;
  LikelihoodSampler likelihood_;
};

// Factory function for PriorPredictive
template <typename PriorSampler, typename LikelihoodSampler>
auto makePriorPredictive(PriorSampler prior, LikelihoodSampler lik) {
  return PriorPredictive<PriorSampler, LikelihoodSampler>{
      std::move(prior), std::move(lik)};
}

// =============================================================================
// PosteriorPredictive - Generate data from posterior samples
// =============================================================================
//
// Posterior predictive distribution: p(y*|y) = integral p(y*|theta) p(theta|y) d(theta)
//
// This answers: "What new data would my model generate after seeing observations?"
// Approximated by sampling theta from posterior MCMC samples, then sampling y*|theta.
//
// Useful for posterior predictive checking - comparing model predictions to actual data.
//
// Usage:
//   std::vector<double> posterior_samples = runMCMC(...);
//   auto post_pred = makePosteriorPredictive(
//       std::move(posterior_samples),
//       [](double theta, auto& g) { return Normal<>(theta, 2).sample(g); }
//   );
//   auto y = post_pred.sample(gen);
//   auto ys = post_pred.sampleN(gen, 1000);

template <typename ParamType, typename LikelihoodSampler>
class PosteriorPredictive {
 public:
  PosteriorPredictive(std::vector<ParamType> posterior_samples,
                      LikelihoodSampler likelihood)
      : samples_{std::move(posterior_samples)},
        likelihood_{std::move(likelihood)} {
    assert(!samples_.empty() && "posterior samples cannot be empty");
  }

  // Sample one data point from the posterior predictive
  // Picks a random posterior sample, then samples from the likelihood
  template <std::uniform_random_bit_generator Gen>
  auto sample(Gen& g) {
    std::uniform_int_distribution<std::size_t> idx_dist{0, samples_.size() - 1};
    const auto& theta = samples_[idx_dist(g)];
    return likelihood_(theta, g);
  }

  // Sample N data points from the posterior predictive
  template <std::uniform_random_bit_generator Gen>
  auto sampleN(Gen& g, std::size_t n) {
    using DataType = decltype(sample(g));
    std::vector<DataType> result;
    result.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      result.push_back(sample(g));
    }
    return result;
  }

  // Access the underlying posterior samples
  auto posteriorSamples() const -> const std::vector<ParamType>& {
    return samples_;
  }

  auto numSamples() const -> std::size_t { return samples_.size(); }

 private:
  std::vector<ParamType> samples_;
  LikelihoodSampler likelihood_;
};

// Factory function for PosteriorPredictive
template <typename ParamType, typename LikelihoodSampler>
auto makePosteriorPredictive(std::vector<ParamType> samples,
                             LikelihoodSampler lik) {
  return PosteriorPredictive<ParamType, LikelihoodSampler>{
      std::move(samples), std::move(lik)};
}

// =============================================================================
// PredictiveCheck - Statistics comparing observed data to predictive samples
// =============================================================================
//
// Computes summary statistics for Bayesian model checking:
// - Observed value vs predictive mean and standard deviation
// - Bayesian p-value: P(y_rep >= y_obs) or P(|y_rep| >= |y_obs|)
//
// A p-value near 0 or 1 indicates the observed data is extreme relative to
// what the model predicts - a sign of model misspecification.

template <typename T>
struct PredictiveCheck {
  T observed;
  T predictive_mean;
  T predictive_std;
  double p_value;  // Fraction of predictive samples >= observed
};

// Compute predictive check statistics
template <typename T>
auto computePredictiveCheck(T observed, const std::vector<T>& predictive_samples)
    -> PredictiveCheck<T> {
  assert(!predictive_samples.empty() && "predictive samples cannot be empty");

  const auto n = static_cast<double>(predictive_samples.size());

  // Compute mean
  T sum = T{0};
  for (const auto& x : predictive_samples) {
    sum += x;
  }
  const T mean = sum / static_cast<T>(n);

  // Compute standard deviation
  T sum_sq = T{0};
  for (const auto& x : predictive_samples) {
    const T diff = x - mean;
    sum_sq += diff * diff;
  }
  const T std_dev = std::sqrt(sum_sq / static_cast<T>(n - 1));

  // Compute p-value: fraction of samples >= observed
  std::size_t count_ge = 0;
  for (const auto& x : predictive_samples) {
    if (x >= observed) ++count_ge;
  }
  const double p_value = static_cast<double>(count_ge) / n;

  return PredictiveCheck<T>{
      .observed = observed,
      .predictive_mean = mean,
      .predictive_std = std_dev,
      .p_value = p_value,
  };
}

// Compute two-sided p-value (for testing if |y_rep| >= |y_obs|)
template <typename T>
auto computePredictiveCheckTwoSided(T observed,
                                    const std::vector<T>& predictive_samples)
    -> PredictiveCheck<T> {
  assert(!predictive_samples.empty() && "predictive samples cannot be empty");

  const auto n = static_cast<double>(predictive_samples.size());

  // Compute mean
  T sum = T{0};
  for (const auto& x : predictive_samples) {
    sum += x;
  }
  const T mean = sum / static_cast<T>(n);

  // Compute standard deviation
  T sum_sq = T{0};
  for (const auto& x : predictive_samples) {
    const T diff = x - mean;
    sum_sq += diff * diff;
  }
  const T std_dev = std::sqrt(sum_sq / static_cast<T>(n - 1));

  // Two-sided p-value: fraction of samples with |x - mean| >= |observed - mean|
  const T obs_deviation = std::abs(observed - mean);
  std::size_t count_extreme = 0;
  for (const auto& x : predictive_samples) {
    if (std::abs(x - mean) >= obs_deviation) ++count_extreme;
  }
  const double p_value = static_cast<double>(count_extreme) / n;

  return PredictiveCheck<T>{
      .observed = observed,
      .predictive_mean = mean,
      .predictive_std = std_dev,
      .p_value = p_value,
  };
}

// =============================================================================
// Summary Statistics for Predictive Samples
// =============================================================================

// Compute percentiles from samples (for credible intervals)
template <typename T>
auto computePercentile(std::vector<T> samples, double p) -> T {
  assert(!samples.empty() && "samples cannot be empty");
  assert(p >= 0.0 && p <= 1.0 && "percentile must be in [0, 1]");

  std::sort(samples.begin(), samples.end());

  const double idx = p * static_cast<double>(samples.size() - 1);
  const auto lo = static_cast<std::size_t>(std::floor(idx));
  const auto hi = static_cast<std::size_t>(std::ceil(idx));

  if (lo == hi) return samples[lo];

  const double frac = idx - static_cast<double>(lo);
  return samples[lo] * static_cast<T>(1.0 - frac) +
         samples[hi] * static_cast<T>(frac);
}

// Compute mean and standard deviation of samples
template <typename T>
auto computeMeanStd(const std::vector<T>& samples) -> std::pair<T, T> {
  assert(!samples.empty() && "samples cannot be empty");

  const auto n = static_cast<double>(samples.size());

  T sum = T{0};
  for (const auto& x : samples) {
    sum += x;
  }
  const T mean = sum / static_cast<T>(n);

  T sum_sq = T{0};
  for (const auto& x : samples) {
    const T diff = x - mean;
    sum_sq += diff * diff;
  }
  const T std_dev = std::sqrt(sum_sq / static_cast<T>(n - 1));

  return {mean, std_dev};
}

}  // namespace tempura::bayes
