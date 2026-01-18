#pragma once

#include <cmath>
#include <concepts>

// Constraint transforms for Bayesian inference.
//
// When MCMC samplers operate in unconstrained space but the model
// requires constrained parameters (positive, bounded, etc.), we need:
//   1. transform: unconstrained -> constrained (for evaluating log-prob)
//   2. inverse: constrained -> unconstrained (for initialization)
//   3. logJacobian: log|d(transform)/dx| (for change-of-variables correction)
//
// The log-probability in unconstrained space is:
//   log p_unconstrained(x) = log p_constrained(transform(x)) + logJacobian(x)

namespace tempura::bayes::sym {

// Concept for constraint transforms
template <typename T>
concept Constraint = requires(double x) {
  { T::transform(x) } -> std::same_as<double>;
  { T::inverse(x) } -> std::same_as<double>;
  { T::logJacobian(x) } -> std::same_as<double>;
};

// =============================================================================
// Unconstrained - Identity transform for (-∞, +∞)
// =============================================================================

struct Unconstrained {
  static constexpr double transform(double x) { return x; }
  static constexpr double inverse(double y) { return y; }
  static constexpr double logJacobian(double /*x*/) { return 0.0; }
};

static_assert(Constraint<Unconstrained>);

// =============================================================================
// Positive - Exponential transform for (0, +∞)
// =============================================================================
// transform(x) = exp(x)
// inverse(y) = log(y)
// Jacobian = d(exp(x))/dx = exp(x), so logJacobian = x

struct Positive {
  static constexpr double transform(double x) { return std::exp(x); }
  static constexpr double inverse(double y) { return std::log(y); }
  static constexpr double logJacobian(double x) { return x; }
};

static_assert(Constraint<Positive>);

// =============================================================================
// UnitInterval - Logistic (sigmoid) transform for (0, 1)
// =============================================================================
// transform(x) = sigmoid(x) = 1 / (1 + exp(-x))
// inverse(y) = logit(y) = log(y / (1 - y))
// Jacobian = sigmoid(x) * (1 - sigmoid(x))
// logJacobian = log(sigmoid(x)) + log(1 - sigmoid(x))
//             = -log(1 + exp(-x)) + (-x - log(1 + exp(-x)))
//             = x - 2*log(1 + exp(x))  [numerically stable form]

struct UnitInterval {
  static constexpr double transform(double x) {
    // Numerically stable sigmoid
    if (x >= 0) {
      return 1.0 / (1.0 + std::exp(-x));
    }
    double ex = std::exp(x);
    return ex / (1.0 + ex);
  }

  static constexpr double inverse(double y) {
    // logit(y) = log(y / (1-y))
    return std::log(y / (1.0 - y));
  }

  static constexpr double logJacobian(double x) {
    // log(sigmoid(x) * (1 - sigmoid(x)))
    // = log(exp(x) / (1 + exp(x))^2)
    // = x - 2*log(1 + exp(x))
    // Numerically stable: use log1p for large negative x
    if (x >= 0) {
      return -x - 2.0 * std::log1p(std::exp(-x));
    }
    return x - 2.0 * std::log1p(std::exp(x));
  }
};

static_assert(Constraint<UnitInterval>);

// =============================================================================
// Bounded - Scaled logistic transform for (Lower, Upper)
// =============================================================================
// transform(x) = Lower + (Upper - Lower) * sigmoid(x)
// inverse(y) = logit((y - Lower) / (Upper - Lower))
// Jacobian = (Upper - Lower) * sigmoid(x) * (1 - sigmoid(x))
// logJacobian = log(Upper - Lower) + logJacobian_UnitInterval(x)

template <auto Lower, auto Upper>
  requires(Lower < Upper)
struct Bounded {
  static constexpr double kLower = static_cast<double>(Lower);
  static constexpr double kUpper = static_cast<double>(Upper);
  static constexpr double kRange = kUpper - kLower;

  static constexpr double transform(double x) {
    return kLower + kRange * UnitInterval::transform(x);
  }

  static constexpr double inverse(double y) {
    return UnitInterval::inverse((y - kLower) / kRange);
  }

  static constexpr double logJacobian(double x) {
    return std::log(kRange) + UnitInterval::logJacobian(x);
  }
};

static_assert(Constraint<Bounded<0, 10>>);
static_assert(Constraint<Bounded<-1, 1>>);

// =============================================================================
// LowerBounded - Shifted exponential transform for (Lower, +∞)
// =============================================================================
// transform(x) = Lower + exp(x)
// inverse(y) = log(y - Lower)
// Jacobian = exp(x), so logJacobian = x

template <auto Lower>
struct LowerBounded {
  static constexpr double kLower = static_cast<double>(Lower);

  static constexpr double transform(double x) { return kLower + std::exp(x); }

  static constexpr double inverse(double y) { return std::log(y - kLower); }

  static constexpr double logJacobian(double x) { return x; }
};

static_assert(Constraint<LowerBounded<0>>);
static_assert(Constraint<LowerBounded<-5>>);

// =============================================================================
// UpperBounded - Reflected exponential transform for (-∞, Upper)
// =============================================================================
// transform(x) = Upper - exp(-x)
// inverse(y) = -log(Upper - y)
// Jacobian = exp(-x), so logJacobian = -x

template <auto Upper>
struct UpperBounded {
  static constexpr double kUpper = static_cast<double>(Upper);

  static constexpr double transform(double x) { return kUpper - std::exp(-x); }

  static constexpr double inverse(double y) { return -std::log(kUpper - y); }

  static constexpr double logJacobian(double x) { return -x; }
};

static_assert(Constraint<UpperBounded<0>>);
static_assert(Constraint<UpperBounded<10>>);

}  // namespace tempura::bayes::sym
