#pragma once

#include <cmath>

// ============================================================================
// likelihoods.h - Numerically stable Bernoulli log-likelihood helpers
// ============================================================================
//
// Provides stable implementations for logistic regression:
//   - logSigmoid(eta): log(sigmoid(eta))
//   - log1mSigmoid(eta): log(1 - sigmoid(eta))
//   - sigmoid(eta): 1 / (1 + exp(-eta))
//   - bernoulliLogLik(eta, y): y * log(p) + (1-y) * log(1-p) where p = sigmoid(eta)
//   - bernoulliGrad(eta, y): d(bernoulliLogLik)/d(eta) = y - sigmoid(eta)
//
// All functions use branch-based formulas to avoid overflow at extreme eta.
// ============================================================================

namespace tempura::symbolic4 {

// Numerically stable log(sigmoid(eta))
// For eta >= 0: log(1/(1+exp(-eta))) = -log(1+exp(-eta))
// For eta < 0:  log(exp(eta)/(1+exp(eta))) = eta - log(1+exp(eta))
inline constexpr auto logSigmoid(double eta) -> double {
  if (eta >= 0.0) {
    return -std::log1p(std::exp(-eta));
  } else {
    return eta - std::log1p(std::exp(eta));
  }
}

// Numerically stable log(1 - sigmoid(eta))
// For eta >= 0: log(exp(-eta)/(1+exp(-eta))) = -eta - log(1+exp(-eta))
// For eta < 0:  log(1/(1+exp(eta))) = -log(1+exp(eta))
inline constexpr auto log1mSigmoid(double eta) -> double {
  if (eta >= 0.0) {
    return -eta - std::log1p(std::exp(-eta));
  } else {
    return -std::log1p(std::exp(eta));
  }
}

// Numerically stable sigmoid: 1 / (1 + exp(-eta))
// For eta >= 0: 1 / (1 + exp(-eta))
// For eta < 0:  exp(eta) / (1 + exp(eta))
inline constexpr auto sigmoid(double eta) -> double {
  if (eta >= 0.0) {
    return 1.0 / (1.0 + std::exp(-eta));
  } else {
    double ez = std::exp(eta);
    return ez / (1.0 + ez);
  }
}

// Bernoulli log-likelihood: y * log(p) + (1-y) * log(1-p) where p = sigmoid(eta)
// Using logSigmoid and log1mSigmoid for numerical stability.
inline constexpr auto bernoulliLogLik(double eta, double y) -> double {
  return y * logSigmoid(eta) + (1.0 - y) * log1mSigmoid(eta);
}

// Gradient of Bernoulli log-likelihood w.r.t. eta: y - sigmoid(eta)
// Derivation:
//   d/d(eta) [y * logSigmoid(eta) + (1-y) * log1mSigmoid(eta)]
//   = y * sigmoid(eta) * (1 - sigmoid(eta)) / sigmoid(eta)
//     - (1-y) * sigmoid(eta) * (1 - sigmoid(eta)) / (1 - sigmoid(eta))
//   = y * (1 - sigmoid(eta)) - (1-y) * sigmoid(eta)
//   = y - y*sigmoid(eta) - sigmoid(eta) + y*sigmoid(eta)
//   = y - sigmoid(eta)
inline constexpr auto bernoulliGrad(double eta, double y) -> double {
  return y - sigmoid(eta);
}

}  // namespace tempura::symbolic4
