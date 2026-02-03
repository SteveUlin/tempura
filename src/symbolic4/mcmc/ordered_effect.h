#pragma once

#include <array>
#include <cstddef>
#include <utility>

// ============================================================================
// ordered_effect.h - Ordered monotonic effects for categorical predictors
// ============================================================================
//
// Encapsulates the "ordered monotonic" predictor pattern from Statistical
// Rethinking Chapter 12. This is used when a categorical variable has
// ordered levels with monotonically increasing effects.
//
// Example: Number of children K in {1, 2, 3, 4}
//   - K=1 is baseline (effect = 0)
//   - K=2, 3, 4 have increasing cumulative effects
//   - The increment delta[i] represents the share of effect from level i+1 to i+2
//   - delta forms a simplex (sums to 1), ensuring monotonicity
//
// Model:
//   effect[k] = bK * cumEffect[k]
//   where cumEffect[k] = sum(delta[0..k-2])
//
// This allows the model to learn that most of the effect may come from
// e.g., the 1->2 transition (diminishing returns).
//
// Usage with SimplexTransform:
//   // For K=4 levels, use K-1=3 simplex elements
//   auto delta_transform = simplexTransform<K-1>();  // K-2 unconstrained -> K-1 simplex
//   auto delta = delta_transform.transform(z);       // Get K-1 element simplex
//   auto cum = OrderedMonotonic<K>::cumEffect(delta);  // Get cumulative effects
//
// ============================================================================

namespace tempura::symbolic4 {

// OrderedMonotonic<K> - Helper for K-level ordered categorical effects
//
// K is the number of levels (e.g., K=4 for children 1-4)
// delta has K-1 elements (a simplex with K-1 increments, sums to 1)
// cumEffect returns K+1 elements where:
//   cumEffect[0] = unused
//   cumEffect[1] = 0 (baseline, first level)
//   cumEffect[k] = sum(delta[0..k-2]) for k in [2, K]
//   cumEffect[K] = 1.0 (maximum effect)
template <std::size_t K>
  requires (K >= 2)
struct OrderedMonotonic {
  // Number of delta parameters (simplex elements)
  static constexpr std::size_t kNumDeltas = K - 1;

  // Compute cumulative effects from simplex delta.
  //
  // Given delta[K-1] where sum(delta) = 1:
  //   result[0] = unused (for 1-indexed convenience)
  //   result[1] = 0 (baseline, first level has no effect)
  //   result[k] = delta[0] + delta[1] + ... + delta[k-2] for k in [2, K]
  //   result[K] = 1.0 (max effect, sum of all deltas)
  //
  // This maps K levels to cumulative effect values in [0, 1].
  static constexpr auto cumEffect(const std::array<double, K - 1>& delta)
      -> std::array<double, K + 1> {
    std::array<double, K + 1> result{};

    // result[0] left as 0 (unused, for 1-based indexing)
    result[1] = 0.0;  // Baseline: first level has no effect

    double cumsum = 0.0;
    for (std::size_t i = 0; i < K - 1; ++i) {
      cumsum += delta[i];
      result[i + 2] = cumsum;  // result[2] = delta[0], result[3] = delta[0]+delta[1], etc.
    }
    // result[K] = cumsum of all K-1 elements = 1.0 (simplex property)
    // The loop already sets result[K] correctly when i = K-2:
    //   result[(K-2) + 2] = result[K] = sum of delta[0..K-2] = 1.0

    return result;
  }

  // For gradient computation: which delta indices contribute to cumEffect[k]?
  //
  // Returns (start, end) where delta[start..end-1] contribute to cumEffect[k].
  // This is useful for backpropagation through the cumulative sum.
  //
  //   k=1: (0, 0) - no deltas contribute (baseline)
  //   k=2: (0, 1) - delta[0] contributes
  //   k=3: (0, 2) - delta[0], delta[1] contribute
  //   k=K: (0, K-1) - all deltas contribute
  static constexpr auto deltaRange(std::size_t k)
      -> std::pair<std::size_t, std::size_t> {
    if (k <= 1) {
      return {0, 0};  // Empty range for baseline
    }
    // cumEffect[k] = sum(delta[0..k-2])
    // So indices 0 to k-2 inclusive, which is range [0, k-1)
    return {0, k - 1};
  }
};

}  // namespace tempura::symbolic4
