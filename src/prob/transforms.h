#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>

namespace tempura::prob {

// ============================================================================
// transforms.h - Parameter transforms with automatic Jacobian correction
// ============================================================================
//
// Simplifies MCMC by automatically handling constrained parameters.
// Each transform provides:
//   - forward(z) -> x: unconstrained to constrained
//   - inverse(x) -> z: constrained to unconstrained
//   - logJacobian(z): log|dx/dz| for density correction
//
// Usage:
//   Positive<double> sigma_t;
//   double z = 0.5;                    // unconstrained
//   double sigma = sigma_t.forward(z); // exp(0.5) ≈ 1.65
//   double lp = dist.logProb(sigma) + sigma_t.logJacobian(z);
//
// ============================================================================

// ============================================================================
// Transform base - CRTP for common operations
// ============================================================================

template <typename Derived, typename T = double>
struct TransformBase {
  // Check if value is in valid range (after transform)
  auto isValid(T z) const -> bool {
    T x = static_cast<const Derived*>(this)->forward(z);
    return std::isfinite(x);
  }

  // Combined: log_prob + log_jacobian (what you usually want)
  template <typename Dist>
  auto adjustedLogProb(const Dist& dist, T z) const -> T {
    const auto* self = static_cast<const Derived*>(this);
    T x = self->forward(z);
    if (!std::isfinite(x)) return -std::numeric_limits<T>::infinity();
    return dist.logProb(x) + self->logJacobian(z);
  }
};

// ============================================================================
// Identity - No transform (unconstrained parameters)
// ============================================================================

template <typename T = double>
struct Identity : TransformBase<Identity<T>, T> {
  static constexpr auto forward(T z) -> T { return z; }
  static constexpr auto inverse(T x) -> T { return x; }
  static constexpr auto logJacobian([[maybe_unused]] T z) -> T { return T{0}; }
};

// ============================================================================
// Positive - For x > 0 (sigma, variance, rate parameters)
// ============================================================================
// x = exp(z), z = log(x)
// dx/dz = exp(z) = x, so log|J| = z

template <typename T = double>
struct Positive : TransformBase<Positive<T>, T> {
  static auto forward(T z) -> T { return std::exp(z); }
  static auto inverse(T x) -> T { return std::log(x); }
  static auto logJacobian(T z) -> T { return z; }
};

// ============================================================================
// UnitInterval - For x in (0, 1) (probabilities)
// ============================================================================
// x = sigmoid(z) = 1/(1+exp(-z)), z = logit(x)
// dx/dz = x(1-x), so log|J| = log(x) + log(1-x)

template <typename T = double>
struct UnitInterval : TransformBase<UnitInterval<T>, T> {
  static auto forward(T z) -> T {
    // Numerically stable sigmoid
    if (z >= 0) {
      return T{1} / (T{1} + std::exp(-z));
    }
    T ez = std::exp(z);
    return ez / (T{1} + ez);
  }

  static auto inverse(T x) -> T {
    return std::log(x / (T{1} - x));
  }

  static auto logJacobian(T z) -> T {
    T x = forward(z);
    return std::log(x) + std::log(T{1} - x);
  }
};

// ============================================================================
// LowerBounded - For x > a
// ============================================================================
// x = a + exp(z)

template <typename T = double>
struct LowerBounded : TransformBase<LowerBounded<T>, T> {
  T lower;

  constexpr explicit LowerBounded(T a) : lower{a} {}

  auto forward(T z) const -> T { return lower + std::exp(z); }
  auto inverse(T x) const -> T { return std::log(x - lower); }
  auto logJacobian(T z) const -> T { return z; }
};

// ============================================================================
// Interval - For x in (a, b)
// ============================================================================
// x = a + (b-a) * sigmoid(z)

template <typename T = double>
struct Interval : TransformBase<Interval<T>, T> {
  T lower;
  T upper;

  constexpr Interval(T a, T b) : lower{a}, upper{b} {}

  auto forward(T z) const -> T {
    T s = UnitInterval<T>::forward(z);
    return lower + (upper - lower) * s;
  }

  auto inverse(T x) const -> T {
    T s = (x - lower) / (upper - lower);
    return UnitInterval<T>::inverse(s);
  }

  auto logJacobian(T z) const -> T {
    T s = UnitInterval<T>::forward(z);
    return std::log(upper - lower) + std::log(s) + std::log(T{1} - s);
  }
};

// ============================================================================
// Simplex element - For x_k in (0, 1) that's part of a simplex
// ============================================================================
// Uses stick-breaking: x_k = sigmoid(z_k) * remaining

template <typename T = double>
struct SimplexElement : TransformBase<SimplexElement<T>, T> {
  T remaining;  // How much probability mass is left

  constexpr explicit SimplexElement(T r = T{1}) : remaining{r} {}

  auto forward(T z) const -> T {
    return remaining * UnitInterval<T>::forward(z);
  }

  auto inverse(T x) const -> T {
    return UnitInterval<T>::inverse(x / remaining);
  }

  auto logJacobian(T z) const -> T {
    return std::log(remaining) + UnitInterval<T>::logJacobian(z);
  }
};

// ============================================================================
// TransformedParam - Convenience wrapper combining value and transform
// ============================================================================

template <typename Transform>
struct TransformedParam {
  using value_type = double;
  Transform transform;
  double unconstrained;

  constexpr TransformedParam(Transform t, double z)
      : transform{t}, unconstrained{z} {}

  auto value() const -> double { return transform.forward(unconstrained); }
  auto logJacobian() const -> double { return transform.logJacobian(unconstrained); }

  template <typename Dist>
  auto logProb(const Dist& dist) const -> double {
    return transform.adjustedLogProb(dist, unconstrained);
  }
};

// Factory functions
template <typename T = double>
auto positive(T z) { return TransformedParam{Positive<T>{}, z}; }

template <typename T = double>
auto unitInterval(T z) { return TransformedParam{UnitInterval<T>{}, z}; }

template <typename T = double>
auto interval(T z, T lower, T upper) {
  return TransformedParam{Interval<T>{lower, upper}, z};
}

// ============================================================================
// Convenience: Apply transform to array of parameters
// ============================================================================

template <typename Transform, std::size_t N>
auto transformAll(const Transform& t, const std::array<double, N>& z)
    -> std::array<double, N> {
  std::array<double, N> x;
  for (std::size_t i = 0; i < N; ++i) {
    x[i] = t.forward(z[i]);
  }
  return x;
}

template <typename Transform, std::size_t N>
auto totalLogJacobian(const Transform& t, const std::array<double, N>& z) -> double {
  double lj = 0.0;
  for (std::size_t i = 0; i < N; ++i) {
    lj += t.logJacobian(z[i]);
  }
  return lj;
}

}  // namespace tempura::prob
