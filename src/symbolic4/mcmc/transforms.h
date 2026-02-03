#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include "symbolic4/core.h"
#include "symbolic4/matrix/eval.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/interpreter/diff.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/interpreter/simplify.h"

// ============================================================================
// transforms.h - Parameter transforms for MCMC with automatic Jacobian
// ============================================================================
//
// Provides automatic parameter transforms for constrained parameters:
//   - Positive parameters (sigma, alpha) -> log transform
//   - (0,1) bounded parameters (probabilities) -> logit transform
//   - Lower-bounded -> shifted log transform
//   - Upper-bounded -> reflected shifted log transform
//   - Interval (a,b) -> scaled logit transform
//
// Usage:
//   auto sigma = halfNormal(2.0);
//   auto posterior = makeTransformedPosterior(
//       logProb(mu, sigma),
//       unconstrained(mu),           // No transform
//       positive(sigma)              // Log transform: sigma = exp(z)
//   );
//
//   // MCMC samples z (unconstrained), get sigma back:
//   double z_sample = ...;
//   double sigma_val = posterior.untransform<1>(z_sample);  // exp(z)
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
};

// Lower-bounded: x = a + exp(z), z = log(x - a)
template <typename Param>
struct LowerBounded {
  Param param;
  double lower;

  auto transform(double z) const -> double { return lower + std::exp(z); }
  auto inverse(double x) const -> double { return std::log(x - lower); }
  auto logJacobian(double z) const -> double { return z; }
};

// Upper-bounded: x = b - exp(z), z = log(b - x)
template <typename Param>
struct UpperBounded {
  Param param;
  double upper;

  auto transform(double z) const -> double { return upper - std::exp(z); }
  auto inverse(double x) const -> double { return std::log(upper - x); }
  auto logJacobian(double z) const -> double { return z; }
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

// ============================================================================
// TransformedPosterior - Posterior with automatic parameter transforms
// ============================================================================
//
// Operates in the unconstrained space but evaluates model in constrained space.
// Automatically adds Jacobian correction to log-probability.

template <Symbolic LogProbExpr, typename ObsBindings, typename TransformsTuple,
          typename ParamSymbolsTuple, typename GradExprsTuple>
class TransformedPosterior {
 public:
  static constexpr std::size_t NumParams = std::tuple_size_v<ParamSymbolsTuple>;
  using GradArray = std::array<double, NumParams>;
  using StateArray = std::array<double, NumParams>;

  constexpr TransformedPosterior(LogProbExpr lp, ObsBindings obs,
                                  TransformsTuple transforms,
                                  ParamSymbolsTuple params,
                                  GradExprsTuple grads)
      : log_prob_expr_{lp},
        observations_{obs},
        transforms_{transforms},
        param_symbols_{params},
        grad_exprs_{grads} {}

  // Evaluate log-probability at unconstrained values (includes Jacobian)
  auto logProb(const StateArray& z) const -> double {
    return logProbImpl(z, std::make_index_sequence<NumParams>{});
  }

  // Evaluate gradient at unconstrained values
  auto gradient(const StateArray& z) const -> GradArray {
    return gradientImpl(z, std::make_index_sequence<NumParams>{});
  }

  // Transform from unconstrained z to constrained x
  auto transform(const StateArray& z) const -> StateArray {
    return transformImpl(z, std::make_index_sequence<NumParams>{});
  }

  // Transform single parameter by index
  template <std::size_t I>
  auto transformOne(double z) const -> double {
    return std::get<I>(transforms_).transform(z);
  }

  // Inverse: constrained x to unconstrained z
  auto inverse(const StateArray& x) const -> StateArray {
    return inverseImpl(x, std::make_index_sequence<NumParams>{});
  }

 private:
  LogProbExpr log_prob_expr_;
  ObsBindings observations_;
  TransformsTuple transforms_;
  ParamSymbolsTuple param_symbols_;
  GradExprsTuple grad_exprs_;

  template <std::size_t... Is>
  auto logProbImpl(const StateArray& z, std::index_sequence<Is...>) const -> double {
    // Transform z to x
    StateArray x = transform(z);

    // Evaluate model log-prob at x
    auto bindings = mergeBindings(
        BinderPack{(std::get<Is>(param_symbols_) = x[Is])...},
        observations_);
    double lp = evaluate(log_prob_expr_, bindings);

    // Add Jacobian correction: log|dx/dz|
    double log_jacobian = (std::get<Is>(transforms_).logJacobian(z[Is]) + ...);

    return lp + log_jacobian;
  }

  template <std::size_t... Is>
  auto gradientImpl(const StateArray& z, std::index_sequence<Is...>) const -> GradArray {
    // Transform z to x
    StateArray x = transform(z);

    // Compute gradient of model log-prob w.r.t. x
    auto bindings = mergeBindings(
        BinderPack{(std::get<Is>(param_symbols_) = x[Is])...},
        observations_);
    GradArray grad_x{evaluate(std::get<Is>(grad_exprs_), bindings)...};

    // Chain rule: d/dz = (d/dx) * (dx/dz)
    // For log transform: dx/dz = exp(z) = x, so d/dz[f] = d/dx[f] * x
    // Plus Jacobian gradient: d/dz[log|J|]
    GradArray grad_z;
    ((grad_z[Is] = chainRuleGrad<Is>(z[Is], x[Is], grad_x[Is])), ...);

    return grad_z;
  }

  template <std::size_t I>
  auto chainRuleGrad(double z, double x, double grad_x) const -> double {
    using Transform = std::tuple_element_t<I, TransformsTuple>;

    if constexpr (is_unconstrained_v<Transform>) {
      // No transform: d/dz = d/dx, Jacobian grad = 0
      return grad_x;
    } else if constexpr (is_positive_v<Transform>) {
      // x = exp(z): dx/dz = x, d/dz[log|J|] = d/dz[z] = 1
      return grad_x * x + 1.0;
    } else if constexpr (is_unit_interval_v<Transform>) {
      // x = sigmoid(z): dx/dz = x(1-x)
      // log|J| = log(x) + log(1-x)
      // d/dz[log|J|] = (1/x)(dx/dz) - (1/(1-x))(dx/dz) = (1-x) - x = 1 - 2x
      double dxdz = x * (1.0 - x);
      double jacobian_grad = 1.0 - 2.0 * x;
      return grad_x * dxdz + jacobian_grad;
    } else if constexpr (is_lower_bounded_v<Transform>) {
      // x = a + exp(z): dx/dz = exp(z) = x - a
      // log|J| = z, d/dz[log|J|] = 1
      const auto& t = std::get<I>(transforms_);
      double dxdz = x - t.lower;
      return grad_x * dxdz + 1.0;
    } else if constexpr (is_upper_bounded_v<Transform>) {
      // x = b - exp(z): dx/dz = -exp(z) = x - b
      // log|J| = z, d/dz[log|J|] = 1
      const auto& t = std::get<I>(transforms_);
      double dxdz = x - t.upper;
      return grad_x * dxdz + 1.0;
    } else if constexpr (is_interval_v<Transform>) {
      // x = a + (b-a)*sigmoid(z), let s = sigmoid(z)
      // dx/dz = (b-a)*s*(1-s)
      // log|J| = log(b-a) + log(s) + log(1-s)
      // d/dz[log|J|] = 1 - 2s
      const auto& t = std::get<I>(transforms_);
      double s = (x - t.lower) / (t.upper - t.lower);
      double dxdz = (t.upper - t.lower) * s * (1.0 - s);
      double jacobian_grad = 1.0 - 2.0 * s;
      return grad_x * dxdz + jacobian_grad;
    } else {
      // Fallback for unknown transforms: use finite differences
      const auto& t = std::get<I>(transforms_);
      constexpr double eps = 1e-8;
      double dxdz = (t.transform(z + eps) - t.transform(z - eps)) / (2.0 * eps);
      double jac_grad = (t.logJacobian(z + eps) - t.logJacobian(z - eps)) / (2.0 * eps);
      return grad_x * dxdz + jac_grad;
    }
  }

  template <std::size_t... Is>
  auto transformImpl(const StateArray& z, std::index_sequence<Is...>) const -> StateArray {
    return StateArray{std::get<Is>(transforms_).transform(z[Is])...};
  }

  template <std::size_t... Is>
  auto inverseImpl(const StateArray& x, std::index_sequence<Is...>) const -> StateArray {
    return StateArray{std::get<Is>(transforms_).inverse(x[Is])...};
  }

  // MergedBinderPack: observations override params when symbol exists in both
  template <typename ParamPack, typename ObsPack>
  struct MergedBinderPack {
    ParamPack params;
    ObsPack obs;

    constexpr MergedBinderPack(ParamPack p, ObsPack o) : params{p}, obs{o} {}

    // Lookup: try observations first, fall back to params
    template <typename Sym>
    constexpr auto operator[](Sym s) const {
      if constexpr (requires { obs[s]; }) {
        return obs[s];
      } else {
        return params[s];
      }
    }
  };

  template <typename... PB, typename... Obs>
  static auto mergeBindings(BinderPack<PB...> pb, BinderPack<Obs...> obs) {
    return MergedBinderPack<BinderPack<PB...>, BinderPack<Obs...>>{pb, obs};
  }
};

// ============================================================================
// TransformedPosteriorBuilder
// ============================================================================

template <Symbolic LogProbExpr, typename TransformsTuple, typename ParamSymbolsTuple,
          typename GradExprsTuple>
class TransformedPosteriorBuilder {
 public:
  static constexpr std::size_t NumParams = std::tuple_size_v<ParamSymbolsTuple>;

  constexpr TransformedPosteriorBuilder(LogProbExpr lp, TransformsTuple transforms,
                                         ParamSymbolsTuple params, GradExprsTuple grads)
      : log_prob_expr_{lp}, transforms_{transforms},
        param_symbols_{params}, grad_exprs_{grads} {}

  template <typename... Binders>
  auto observe(Binders... binders) const {
    auto obs = BinderPack{binders...};
    return TransformedPosterior<LogProbExpr, BinderPack<Binders...>, TransformsTuple,
                                 ParamSymbolsTuple, GradExprsTuple>{
        log_prob_expr_, obs, transforms_, param_symbols_, grad_exprs_};
  }

  auto build() const {
    return TransformedPosterior<LogProbExpr, BinderPack<>, TransformsTuple,
                                 ParamSymbolsTuple, GradExprsTuple>{
        log_prob_expr_, BinderPack<>{}, transforms_, param_symbols_, grad_exprs_};
  }

 private:
  LogProbExpr log_prob_expr_;
  TransformsTuple transforms_;
  ParamSymbolsTuple param_symbols_;
  GradExprsTuple grad_exprs_;
};

// ============================================================================
// Factory function
// ============================================================================

namespace detail {

template <typename T>
constexpr auto extractParam(T t) {
  if constexpr (is_transform_v<T>) {
    return t.param;
  } else {
    return t;
  }
}

template <typename T>
constexpr auto extractSym(T t) {
  auto param = extractParam(t);
  if constexpr (IsRandomVar<decltype(param)>) {
    // Use freeSym() for binding compatibility (returns Atom<Id, Free>)
    return param.freeSym();
  } else {
    return param;
  }
}

template <typename T>
constexpr auto wrapTransform(T t) {
  if constexpr (is_transform_v<T>) {
    return t;
  } else {
    // Default: unconstrained
    return unconstrained(t);
  }
}

}  // namespace detail

// Create transformed posterior from log-prob and transformed parameters
template <Symbolic LogProbExpr, typename... TransformedParams>
constexpr auto makeTransformedPosterior(LogProbExpr lp, TransformedParams... params) {
  auto transforms = std::make_tuple(detail::wrapTransform(params)...);
  auto param_symbols = std::make_tuple(detail::extractSym(params)...);

  auto grad_exprs = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    return std::make_tuple(simplify(diff(lp, std::get<Is>(param_symbols)))...);
  }(std::make_index_sequence<sizeof...(TransformedParams)>{});

  return TransformedPosteriorBuilder<LogProbExpr, decltype(transforms),
                                      decltype(param_symbols), decltype(grad_exprs)>{
      lp, transforms, param_symbols, grad_exprs};
}

}  // namespace tempura::symbolic4
