#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "symbolic4/core.h"
#include "symbolic4/matrix/eval.h"

// ============================================================================
// transforms.h - Parameter transform types for MCMC
// ============================================================================
//
// Defines transform types for constrained parameters:
//   - Unconstrained (identity)
//   - Positive: exp/log
//   - UnitInterval: sigmoid/logit
//   - LowerBounded, UpperBounded, Interval
//   - CholeskyTransform (K×K matrix → K(K+1)/2 unconstrained)
//   - SimplexTransform<K> (K-simplex → K-1 unconstrained via stick-breaking)
//
// Each transform provides: transform(), inverse(), logJacobian(),
// and chainRuleGrad() for gradient computation.
//
// These types are used by PlateTransformedPosterior (plate_transforms.h)
// via TransformPack to automatically handle constrained parameters in MCMC.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Transform types
// ============================================================================

// No transform - parameter is unconstrained
template <typename Param>
struct Unconstrained {
  Param param;

  static constexpr auto transform(double z) -> double { return z; }
  static constexpr auto inverse(double x) -> double { return x; }
  static constexpr auto logJacobian([[maybe_unused]] double z) -> double { return 0.0; }

  // Symbolic: log|dx/dz| = 0
  template <Symbolic Z>
  static constexpr auto symbolicLogJacobian([[maybe_unused]] Z) { return lit(0.0); }

  // Chain rule: grad_z = grad_x * dx/dz + d(logJ)/dz = grad_x * 1 + 0 = grad_x
  static constexpr auto chainRuleGrad(double grad_x, [[maybe_unused]] double z) -> double {
    return grad_x;
  }
};

// Positive parameter: x = exp(z), z = log(x)
// Jacobian: dx/dz = exp(z) = x, so log|J| = z
template <typename Param>
struct Positive {
  Param param;

  static auto transform(double z) -> double { return std::exp(z); }
  static auto inverse(double x) -> double { return std::log(x); }
  static auto logJacobian(double z) -> double { return z; }

  template <Symbolic Z>
  static constexpr auto symbolicLogJacobian(Z z) { return z; }

  // Chain rule: grad_z = grad_x * dx/dz + d(logJ)/dz = grad_x * exp(z) + 1
  static auto chainRuleGrad(double grad_x, double z) -> double {
    double x = std::exp(z);
    return grad_x * x + 1.0;
  }
};

// (0,1) bounded: x = sigmoid(z) = 1/(1+exp(-z)), z = logit(x)
// Jacobian: dx/dz = x(1-x), so log|J| = log(x) + log(1-x)
template <typename Param>
struct UnitInterval {
  Param param;

  static auto transform(double z) -> double {
    return 1.0 / (1.0 + std::exp(-z));
  }
  static auto inverse(double x) -> double {
    return std::log(x / (1.0 - x));
  }
  static auto logJacobian(double z) -> double {
    double x = transform(z);
    return std::log(x) + std::log(1.0 - x);
  }

  template <Symbolic Z>
  static constexpr auto symbolicLogJacobian(Z z) {
    auto x = lit(1.0) / (lit(1.0) + exp(-z));
    return log(x) + log(lit(1.0) - x);
  }

  // Chain rule: grad_z = grad_x * dx/dz + d(logJ)/dz
  // dx/dz = x(1-x), d(logJ)/dz = (1-x) - x = 1 - 2x
  static auto chainRuleGrad(double grad_x, double z) -> double {
    double x = transform(z);
    double dx_dz = x * (1.0 - x);
    double dlogJ_dz = 1.0 - 2.0 * x;
    return grad_x * dx_dz + dlogJ_dz;
  }
};

// Lower-bounded: x = a + exp(z), z = log(x - a)
template <typename Param>
struct LowerBounded {
  Param param;
  double lower;

  auto transform(double z) const -> double { return lower + std::exp(z); }
  auto inverse(double x) const -> double { return std::log(x - lower); }
  auto logJacobian(double z) const -> double { return z; }

  // Same as Positive: dx/dz = exp(z), d(logJ)/dz = 1
  auto chainRuleGrad(double grad_x, double z) const -> double {
    double dx_dz = std::exp(z);
    return grad_x * dx_dz + 1.0;
  }
};

// Upper-bounded: x = b - exp(z), z = log(b - x)
template <typename Param>
struct UpperBounded {
  Param param;
  double upper;

  auto transform(double z) const -> double { return upper - std::exp(z); }
  auto inverse(double x) const -> double { return std::log(upper - x); }
  auto logJacobian(double z) const -> double { return z; }

  // dx/dz = -exp(z), d(logJ)/dz = 1
  auto chainRuleGrad(double grad_x, double z) const -> double {
    double dx_dz = -std::exp(z);
    return grad_x * dx_dz + 1.0;
  }
};

// Interval (a,b): x = a + (b-a)*sigmoid(z)
// Jacobian: dx/dz = (b-a)*sigmoid(z)*(1-sigmoid(z))
template <typename Param>
struct Interval {
  Param param;
  double lower;
  double upper;

  auto transform(double z) const -> double {
    double s = 1.0 / (1.0 + std::exp(-z));
    return lower + (upper - lower) * s;
  }
  auto inverse(double x) const -> double {
    double s = (x - lower) / (upper - lower);
    return std::log(s / (1.0 - s));
  }
  auto logJacobian(double z) const -> double {
    double s = 1.0 / (1.0 + std::exp(-z));
    return std::log(upper - lower) + std::log(s) + std::log(1.0 - s);
  }
};

// ============================================================================
// CholeskyTransform - Maps K×K Cholesky factor to K(K+1)/2 unconstrained params
// ============================================================================
//
// Packing order: column-major lower triangle
//   [L_00, L_10, L_20, ..., L_11, L_21, ..., L_22, ...]
//
// Transform:
//   Diagonal: L_ii = exp(z_i)  (ensures positivity)
//   Off-diagonal: L_ij = z_k   (unconstrained)
//
// Log-Jacobian: Σ z_diag[i] (sum of z values for diagonal elements)
//   This comes from the change of variables: L_ii = exp(z_i), so dL_ii/dz_i = L_ii
//   Thus log|det(Jacobian)| = Σ log(L_ii) = Σ z_i for diagonal indices

struct CholeskyTransform {
  std::size_t dim;  // K, the matrix dimension

  explicit CholeskyTransform(std::size_t k) : dim{k} {}

  // Number of unconstrained parameters: K(K+1)/2
  static constexpr auto stateSize(std::size_t k) -> std::size_t {
    return k * (k + 1) / 2;
  }

  auto stateSize() const -> std::size_t { return stateSize(dim); }

  // Get index in packed vector for element (i, j) where i >= j (lower triangle)
  // Column-major packing: column j starts at index j*K - j*(j-1)/2
  // Element (i, j) is at that offset plus (i - j)
  static constexpr auto packedIndex(std::size_t i, std::size_t j, std::size_t k) -> std::size_t {
    assert(i >= j && "Must be lower triangle: i >= j");
    assert(i < k && j < k && "Indices must be within matrix bounds");
    // Column j contains elements (j,j), (j+1,j), ..., (k-1,j)
    // Column j starts at: sum_{c=0}^{j-1} (k-c) = j*k - j*(j-1)/2
    std::size_t col_start = j * k - j * (j - 1) / 2;
    return col_start + (i - j);
  }

  auto packedIndex(std::size_t i, std::size_t j) const -> std::size_t {
    return packedIndex(i, j, dim);
  }

  // Check if packed index corresponds to a diagonal element
  static constexpr auto isDiagonalIndex(std::size_t packed_idx, std::size_t k) -> bool {
    // Diagonal elements are at positions: 0, k, k+(k-1), k+(k-1)+(k-2), ...
    // i.e., cumulative sums: 0, k, 2k-1, 3k-3, ...
    // General formula: j*k - j*(j-1)/2 for j = 0, 1, ..., k-1
    for (std::size_t j = 0; j < k; ++j) {
      if (packedIndex(j, j, k) == packed_idx) return true;
    }
    return false;
  }

  auto isDiagonalIndex(std::size_t packed_idx) const -> bool {
    return isDiagonalIndex(packed_idx, dim);
  }

  // Get column index from packed index
  static constexpr auto columnFromPacked(std::size_t packed_idx, std::size_t k) -> std::size_t {
    // Find which column this index belongs to by finding largest j where col_start <= packed_idx
    std::size_t col = 0;
    std::size_t col_start = 0;
    while (col + 1 < k) {
      std::size_t next_col_start = col_start + (k - col);
      if (next_col_start > packed_idx) break;
      col_start = next_col_start;
      ++col;
    }
    return col;
  }

  // Get (row, col) from packed index
  static constexpr auto unpackIndex(std::size_t packed_idx, std::size_t k)
      -> std::pair<std::size_t, std::size_t> {
    std::size_t col = columnFromPacked(packed_idx, k);
    std::size_t col_start = col * k - col * (col - 1) / 2;
    std::size_t row = col + (packed_idx - col_start);
    return {row, col};
  }

  auto unpackIndex(std::size_t packed_idx) const -> std::pair<std::size_t, std::size_t> {
    return unpackIndex(packed_idx, dim);
  }

  // Transform: unconstrained z -> Cholesky factor L
  auto transform(std::span<const double> z) const -> DynamicMatrix {
    assert(z.size() == stateSize() && "z must have K(K+1)/2 elements");
    DynamicMatrix l(dim, dim);

    for (std::size_t packed_idx = 0; packed_idx < z.size(); ++packed_idx) {
      auto [i, j] = unpackIndex(packed_idx);
      if (i == j) {
        // Diagonal: L_ii = exp(z)
        l[i, j] = std::exp(z[packed_idx]);
      } else {
        // Off-diagonal: L_ij = z (unconstrained)
        l[i, j] = z[packed_idx];
      }
    }
    return l;
  }

  // Inverse: Cholesky factor L -> unconstrained z
  auto inverse(const DynamicMatrix& l) const -> std::vector<double> {
    assert(l.rows() == dim && l.cols() == dim && "L must be K×K");
    std::vector<double> z(stateSize());

    for (std::size_t packed_idx = 0; packed_idx < z.size(); ++packed_idx) {
      auto [i, j] = unpackIndex(packed_idx);
      if (i == j) {
        // Diagonal: z = log(L_ii)
        z[packed_idx] = std::log(l[i, j]);
      } else {
        // Off-diagonal: z = L_ij
        z[packed_idx] = l[i, j];
      }
    }
    return z;
  }

  // Log-Jacobian: Σ z_diag[i]
  // The Jacobian of the transform is diagonal (block diagonal actually), with:
  //   ∂L_ii/∂z_i = exp(z_i) for diagonal elements
  //   ∂L_ij/∂z_k = 1 for off-diagonal elements
  // So log|det(J)| = Σ log(exp(z_i)) = Σ z_i for diagonal indices
  auto logJacobian(std::span<const double> z) const -> double {
    assert(z.size() == stateSize() && "z must have K(K+1)/2 elements");
    double result = 0.0;

    // Sum z values at diagonal positions
    for (std::size_t j = 0; j < dim; ++j) {
      std::size_t diag_idx = packedIndex(j, j);
      result += z[diag_idx];
    }
    return result;
  }
};

// ============================================================================
// Factory functions
// ============================================================================

template <typename Param>
constexpr auto unconstrained(Param p) { return Unconstrained<Param>{p}; }

template <typename Param>
constexpr auto positive(Param p) { return Positive<Param>{p}; }

template <typename Param>
constexpr auto unitInterval(Param p) { return UnitInterval<Param>{p}; }

template <typename Param>
constexpr auto lowerBounded(Param p, double lower) {
  return LowerBounded<Param>{p, lower};
}

template <typename Param>
constexpr auto upperBounded(Param p, double upper) {
  return UpperBounded<Param>{p, upper};
}

template <typename Param>
constexpr auto interval(Param p, double lower, double upper) {
  return Interval<Param>{p, lower, upper};
}

// Factory for CholeskyTransform
inline auto choleskyTransform(std::size_t dim) -> CholeskyTransform {
  return CholeskyTransform{dim};
}

// ============================================================================
// SimplexTransform - Maps K-1 unconstrained to K-simplex via stick-breaking
// ============================================================================
//
// Stick-breaking construction:
//   s[i] = sigmoid(z[i])
//   delta[0] = s[0]
//   delta[i] = s[i] * (1 - sum(delta[0:i-1]))  for i < K-1
//   delta[K-1] = 1 - sum(delta[0:K-2])
//
// Log-Jacobian: sum_{i=0}^{K-2} [log(s[i]) + log(1-s[i]) + log(remaining[i])]
//   where remaining[i] = 1 - sum(delta[0:i-1])
//
// This maps R^{K-1} -> simplex{K} with proper volume correction.

template <std::size_t K>
  requires (K >= 2)
struct SimplexTransform {
  static constexpr std::size_t kDim = K;
  static constexpr std::size_t kUnconstrainedDim = K - 1;

  // Numerically stable sigmoid: avoids overflow for large |z|
  static auto stableSigmoid(double z) -> double {
    if (z >= 0.0) {
      double ez = std::exp(-z);
      return 1.0 / (1.0 + ez);
    } else {
      double ez = std::exp(z);
      return ez / (1.0 + ez);
    }
  }

  // Forward: z[K-1] -> delta[K] where sum(delta) = 1, all delta > 0
  auto transform(std::span<const double, K - 1> z) const -> std::array<double, K> {
    std::array<double, K> delta{};
    double remaining = 1.0;

    for (std::size_t i = 0; i < K - 1; ++i) {
      double s = stableSigmoid(z[i]);
      delta[i] = s * remaining;
      remaining -= delta[i];
    }
    delta[K - 1] = remaining;

    return delta;
  }

  // Overload for fixed-size array input
  auto transform(const std::array<double, K - 1>& z) const -> std::array<double, K> {
    return transform(std::span<const double, K - 1>{z});
  }

  // Inverse: delta[K] -> z[K-1]
  // Given delta[i] = s[i] * remaining[i], solve for s[i] then z[i] = logit(s[i])
  auto inverse(std::span<const double, K> delta) const -> std::array<double, K - 1> {
    std::array<double, K - 1> z{};
    double remaining = 1.0;

    for (std::size_t i = 0; i < K - 1; ++i) {
      // s[i] = delta[i] / remaining
      double s = delta[i] / remaining;
      // Clamp for numerical stability
      s = std::clamp(s, 1e-15, 1.0 - 1e-15);
      // logit(s) = log(s / (1-s))
      z[i] = std::log(s / (1.0 - s));
      remaining -= delta[i];
    }

    return z;
  }

  // Overload for fixed-size array input
  auto inverse(const std::array<double, K>& delta) const -> std::array<double, K - 1> {
    return inverse(std::span<const double, K>{delta});
  }

  // Log-Jacobian: log|det(d delta / d z)|
  // = sum_{i=0}^{K-2} [log(s[i]) + log(1-s[i]) + log(remaining[i])]
  auto logJacobian(std::span<const double, K - 1> z) const -> double {
    double log_jac = 0.0;
    double remaining = 1.0;

    for (std::size_t i = 0; i < K - 1; ++i) {
      double s = stableSigmoid(z[i]);
      // log(s) + log(1-s) for the sigmoid derivative
      // Numerically stable: log(s(1-s)) = -log(1 + exp(-z)) - log(1 + exp(z))
      //                                 = -z - 2*log(1 + exp(-|z|)) for z >= 0
      //                                 = z - 2*log(1 + exp(|z|)) for z < 0
      // But simpler: log(s) + log(1-s)
      log_jac += std::log(s) + std::log(1.0 - s) + std::log(remaining);
      remaining -= s * remaining;
    }

    return log_jac;
  }

  // Overload for fixed-size array input
  auto logJacobian(const std::array<double, K - 1>& z) const -> double {
    return logJacobian(std::span<const double, K - 1>{z});
  }

  // Gradient of log-Jacobian w.r.t. z
  //
  // log J = sum_{i=0}^{K-2} [log(s[i]) + log(1-s[i]) + log(r[i])]
  // where r[0] = 1, r[j] = prod_{k=0}^{j-1}(1-s[k]) for j > 0
  //
  // d(log J)/d(z[i]):
  //   1. From log(s[i]): d/dz[log(s)] = (1/s)*s*(1-s) = 1-s
  //   2. From log(1-s[i]): d/dz[log(1-s)] = -s*(1-s)/(1-s) = -s
  //   3. From log(r[i]): r[i] depends on z[0]..z[i-1], not z[i], so 0
  //   4. From log(r[j]) for j > i:
  //      r[j] = prod_{k=0}^{j-1}(1-s[k])
  //      d(r[j])/d(z[i]) = r[j] / (1-s[i]) * (-s[i]*(1-s[i])) = -s[i] * r[j]
  //      d(log r[j])/d(z[i]) = -s[i]
  //
  // Sum: (1-s) + (-s) + 0 + (K-2-i)*(-s) = 1 - 2s - (K-2-i)*s = 1 - s*(K-i)
  //
  auto logJacobianGrad(std::span<const double, K - 1> z) const -> std::array<double, K - 1> {
    std::array<double, K - 1> grad{};

    for (std::size_t i = 0; i < K - 1; ++i) {
      double s = stableSigmoid(z[i]);
      // d(log J)/d(z[i]) = 1 - s * (K - i)
      grad[i] = 1.0 - s * static_cast<double>(K - i);
    }

    return grad;
  }

  // Overload for fixed-size array input
  auto logJacobianGrad(const std::array<double, K - 1>& z) const -> std::array<double, K - 1> {
    return logJacobianGrad(std::span<const double, K - 1>{z});
  }

  // Chain rule: given grad_delta (gradient of objective w.r.t. delta),
  // compute grad_z including Jacobian term
  // grad_z[i] = sum_j (grad_delta[j] * d(delta[j])/d(z[i])) + d(logJacobian)/d(z[i])
  //
  // Derivatives:
  //   delta[i] = s[i] * r[i], where r[i] = prod_{k=0}^{i-1}(1-s[k])
  //   d(delta[i])/d(z[i]) = s[i]*(1-s[i]) * r[i]  (r[i] doesn't depend on z[i])
  //   d(delta[j])/d(z[i]) = -s[i] * delta[j]  for j > i
  //     (since d(r[j])/d(z[i]) = -s[i] * r[j], and delta[j] = s[j]*r[j] or r[j])
  //
  auto chainRuleGrad(std::span<const double, K - 1> z,
                     std::span<const double, K> grad_delta) const -> std::array<double, K - 1> {
    std::array<double, K - 1> grad_z{};
    std::array<double, K - 1> s_vals{};
    std::array<double, K> delta_vals{};

    // Forward pass: compute s[i], delta[i], and remaining
    double remaining = 1.0;
    for (std::size_t i = 0; i < K - 1; ++i) {
      s_vals[i] = stableSigmoid(z[i]);
      delta_vals[i] = s_vals[i] * remaining;
      remaining -= delta_vals[i];
    }
    delta_vals[K - 1] = remaining;

    // Compute suffix sum: sum_{j>i} grad_delta[j] * delta[j]
    std::array<double, K> suffix_sums{};
    suffix_sums[K - 1] = grad_delta[K - 1] * delta_vals[K - 1];
    for (std::size_t i = K - 1; i-- > 0;) {
      suffix_sums[i] = suffix_sums[i + 1] + grad_delta[i] * delta_vals[i];
    }

    // Compute remaining values for the direct term
    std::array<double, K - 1> remaining_at{};
    remaining = 1.0;
    for (std::size_t i = 0; i < K - 1; ++i) {
      remaining_at[i] = remaining;
      remaining -= delta_vals[i];
    }

    // Compute gradients
    for (std::size_t i = 0; i < K - 1; ++i) {
      double s = s_vals[i];
      double ds_dz = s * (1.0 - s);

      // Direct term: grad_delta[i] * d(delta[i])/d(z[i])
      double direct = grad_delta[i] * ds_dz * remaining_at[i];

      // Indirect term: sum_{j>i} grad_delta[j] * d(delta[j])/d(z[i])
      //              = sum_{j>i} grad_delta[j] * (-s[i] * delta[j])
      //              = -s[i] * suffix_sums[i+1]
      double indirect = -s * suffix_sums[i + 1];

      // Jacobian gradient: 1 - s * (K - i)
      double jac_grad = 1.0 - s * static_cast<double>(K - i);

      grad_z[i] = direct + indirect + jac_grad;
    }

    return grad_z;
  }

  // Overloads for fixed-size array inputs
  auto chainRuleGrad(const std::array<double, K - 1>& z,
                     const std::array<double, K>& grad_delta) const -> std::array<double, K - 1> {
    return chainRuleGrad(std::span<const double, K - 1>{z},
                         std::span<const double, K>{grad_delta});
  }
};

// Factory for SimplexTransform
template <std::size_t K>
  requires (K >= 2)
constexpr auto simplexTransform() -> SimplexTransform<K> {
  return SimplexTransform<K>{};
}

// ============================================================================
// Type traits
// ============================================================================

template <typename T>
struct IsTransform : std::false_type {};

template <typename P>
struct IsTransform<Unconstrained<P>> : std::true_type {};

template <typename P>
struct IsTransform<Positive<P>> : std::true_type {};

template <typename P>
struct IsTransform<UnitInterval<P>> : std::true_type {};

template <typename P>
struct IsTransform<LowerBounded<P>> : std::true_type {};

template <typename P>
struct IsTransform<UpperBounded<P>> : std::true_type {};

template <typename P>
struct IsTransform<Interval<P>> : std::true_type {};

template <typename T>
constexpr bool is_transform_v = IsTransform<T>::value;

// Type traits for specific transform types
template <typename T>
struct IsUnconstrained : std::false_type {};
template <typename P>
struct IsUnconstrained<Unconstrained<P>> : std::true_type {};
template <typename T>
constexpr bool is_unconstrained_v = IsUnconstrained<T>::value;

template <typename T>
struct IsPositive : std::false_type {};
template <typename P>
struct IsPositive<Positive<P>> : std::true_type {};
template <typename T>
constexpr bool is_positive_v = IsPositive<T>::value;

template <typename T>
struct IsUnitInterval : std::false_type {};
template <typename P>
struct IsUnitInterval<UnitInterval<P>> : std::true_type {};
template <typename T>
constexpr bool is_unit_interval_v = IsUnitInterval<T>::value;

template <typename T>
struct IsLowerBounded : std::false_type {};
template <typename P>
struct IsLowerBounded<LowerBounded<P>> : std::true_type {};
template <typename T>
constexpr bool is_lower_bounded_v = IsLowerBounded<T>::value;

template <typename T>
struct IsUpperBounded : std::false_type {};
template <typename P>
struct IsUpperBounded<UpperBounded<P>> : std::true_type {};
template <typename T>
constexpr bool is_upper_bounded_v = IsUpperBounded<T>::value;

template <typename T>
struct IsInterval : std::false_type {};
template <typename P>
struct IsInterval<Interval<P>> : std::true_type {};
template <typename T>
constexpr bool is_interval_v = IsInterval<T>::value;

template <typename T>
struct IsCholeskyTransform : std::false_type {};
template <>
struct IsCholeskyTransform<CholeskyTransform> : std::true_type {};
template <typename T>
constexpr bool is_cholesky_transform_v = IsCholeskyTransform<T>::value;

template <typename T>
struct IsSimplexTransform : std::false_type {};
template <std::size_t K>
struct IsSimplexTransform<SimplexTransform<K>> : std::true_type {};
template <typename T>
constexpr bool is_simplex_transform_v = IsSimplexTransform<T>::value;

}  // namespace tempura::symbolic4
