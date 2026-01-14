#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

namespace tempura {

// ============================================================================
// T-Digest
// ============================================================================
//
// T-Digest is a data structure for accurate estimation of quantiles from a
// stream of data, with particularly good accuracy at the tails (p01, p99).
// Introduced by Ted Dunning in 2013, it's widely used in distributed systems
// where you need approximate percentiles without storing all values.
//
// CORE IDEA
// ---------
// Instead of storing every value, T-Digest clusters nearby values into
// "centroids" - each centroid stores a mean and a count (total weight).
// The key insight is HOW centroids are allowed to grow:
//
//   - Near the median (q ≈ 0.5): centroids can absorb many points
//   - Near the tails (q ≈ 0, q ≈ 1): centroids must stay small
//
// This gives high resolution where it matters most for extreme percentiles.
//
// SCALE FUNCTION
// --------------
// The "scale function" k(q) maps quantile q ∈ [0,1] to a scaled space where
// centroids of equal size in k-space have varying sizes in q-space. We use
// the arcsin scale function (k₂ from the paper):
//
//   k(q) = (δ/π) · arcsin(2q - 1)
//
// where δ (delta) is the compression parameter. The derivative k'(q) is:
//
//   k'(q) = (2δ/π) / √(4q(1-q))
//
// This derivative → ∞ as q → 0 or q → 1, meaning tiny changes in q produce
// large changes in k-space near the tails. A centroid is "full" when its
// size in k-space exceeds 1.0, so tail centroids stay very small.
//
// WHY ARCSIN?
// -----------
// The arcsin function has a nice property: its derivative 1/√(1-x²) blows up
// at the endpoints. When we compose this with 2q-1, we get a scale function
// that naturally compresses the tails. Other scale functions exist:
//
//   k₀(q) = δq/2         - uniform, no tail accuracy (don't use)
//   k₁(q) = δ·log(q/(1-q))/2π  - logit, moderate tail accuracy
//   k₂(q) = δ·arcsin(2q-1)/π   - arcsin, best tail accuracy (we use this)
//
// MERGING AND COMPRESSION
// -----------------------
// When we add a point, we find the nearest centroid and merge if the size
// constraint allows. When too many centroids accumulate (> δ), we sort and
// re-merge them greedily from one end to the other.
//
// The merge decision: two centroids at quantile q can merge if their combined
// weight w satisfies: k(q + w/2n) - k(q - w/2n) ≤ 1, where n is total count.
//
// QUANTILE ESTIMATION
// -------------------
// To estimate quantile q:
//   1. Find the two centroids that bracket quantile q (by cumulative weight)
//   2. Linear interpolate between their means
//
// TEMPLATE PARAMETERS
// -------------------
//   Compression - Controls accuracy vs memory (default 100)
//                 Higher = more centroids = more accuracy = more memory
//                 Typical: 50-500. Rule of thumb: error ≈ 3/Compression
//
// COMPLEXITY
// ----------
//   add():      O(n) worst case, but amortized O(1) with buffering
//   quantile(): O(n) where n is number of centroids (typically O(Compression))
//   merge():    O(n log n) due to sorting
//   Space:      O(Compression) centroids
//
// ACCURACY
// --------
// For tail quantiles (p99, p999), error is typically < 0.1% relative.
// For median, error can be 1-3% relative, depending on distribution.
// Accuracy improves with more data points, up to the compression limit.
//
template <std::size_t Compression = 100>
class TDigest {
  static_assert(Compression > 0, "Compression must be positive");

public:
  struct Centroid {
    double mean;
    double weight;
  };

  TDigest() = default;

  // Add a value with optional weight (default 1.0)
  void add(double value, double weight = 1.0) {
    assert(weight > 0.0 && "Weight must be positive");
    assert(std::isfinite(value) && "Value must be finite");
    assert(std::isfinite(weight) && "Weight must be finite");

    if (centroids_.empty()) {
      centroids_.push_back(Centroid{value, weight});
      total_weight_ += weight;
      return;
    }

    // Find nearest centroid that can absorb this point
    auto nearest_idx = findNearest(value);

    // Try to merge with nearest centroid if size constraint allows
    auto& nearest = centroids_[nearest_idx];
    double q = cumulativeWeight(nearest_idx) / total_weight_;
    double combined_weight = nearest.weight + weight;

    if (canMerge(q, combined_weight)) {
      // Weighted average for new mean
      nearest.mean = (nearest.mean * nearest.weight + value * weight) / combined_weight;
      nearest.weight = combined_weight;
      total_weight_ += weight;
    } else {
      // Insert as new centroid at sorted position
      insertSorted(value, weight);
      total_weight_ += weight;
    }

    // Compress if we have too many centroids
    if (centroids_.size() > maxCentroids()) {
      compress();
    }
  }

  // Estimate quantile q ∈ [0, 1]
  // Returns the estimated value at that quantile
  [[nodiscard]] auto quantile(double q) const -> double {
    assert(q >= 0.0 && q <= 1.0 && "Quantile must be in [0, 1]");

    if (centroids_.empty()) {
      return std::numeric_limits<double>::quiet_NaN();
    }

    if (centroids_.size() == 1) {
      return centroids_[0].mean;
    }

    double target_weight = q * total_weight_;

    // Handle edge cases
    if (target_weight <= centroids_[0].weight / 2.0) {
      return centroids_[0].mean;
    }
    if (target_weight >= total_weight_ - centroids_.back().weight / 2.0) {
      return centroids_.back().mean;
    }

    // Find the two centroids that bracket the target quantile
    double cumulative = 0.0;
    for (std::size_t i = 0; i < centroids_.size(); ++i) {
      double centroid_start = cumulative + centroids_[i].weight / 2.0;

      if (i > 0) {
        double prev_end = cumulative - centroids_[i - 1].weight / 2.0 + centroids_[i - 1].weight;
        if (target_weight >= prev_end && target_weight <= centroid_start) {
          // Interpolate between centroids i-1 and i
          double t = (target_weight - prev_end) / (centroid_start - prev_end);
          return centroids_[i - 1].mean + t * (centroids_[i].mean - centroids_[i - 1].mean);
        }
      }

      double centroid_end = cumulative + centroids_[i].weight / 2.0 + centroids_[i].weight / 2.0;
      if (target_weight >= centroid_start && target_weight <= centroid_end) {
        return centroids_[i].mean;
      }

      cumulative += centroids_[i].weight;
    }

    // Fallback: linear interpolation between bracketing centroids
    cumulative = 0.0;
    for (std::size_t i = 0; i < centroids_.size() - 1; ++i) {
      double mid_current = cumulative + centroids_[i].weight / 2.0;
      double mid_next = cumulative + centroids_[i].weight + centroids_[i + 1].weight / 2.0;

      if (target_weight >= mid_current && target_weight <= mid_next) {
        double t = (target_weight - mid_current) / (mid_next - mid_current);
        return centroids_[i].mean + t * (centroids_[i + 1].mean - centroids_[i].mean);
      }
      cumulative += centroids_[i].weight;
    }

    return centroids_.back().mean;
  }

  // Cumulative distribution function: P(X <= value)
  [[nodiscard]] auto cdf(double value) const -> double {
    if (centroids_.empty()) {
      return std::numeric_limits<double>::quiet_NaN();
    }

    if (centroids_.size() == 1) {
      return value < centroids_[0].mean ? 0.0 : (value > centroids_[0].mean ? 1.0 : 0.5);
    }

    // Value is below all centroids
    if (value <= centroids_[0].mean) {
      // Linear interpolation assuming min is at first centroid
      return 0.0;
    }

    // Value is above all centroids
    if (value >= centroids_.back().mean) {
      return 1.0;
    }

    // Find the two centroids that bracket the value
    double cumulative = 0.0;
    for (std::size_t i = 0; i < centroids_.size() - 1; ++i) {
      if (value >= centroids_[i].mean && value < centroids_[i + 1].mean) {
        // Interpolate between centroids i and i+1
        double mid_current = cumulative + centroids_[i].weight / 2.0;
        double mid_next = cumulative + centroids_[i].weight + centroids_[i + 1].weight / 2.0;
        double t = (value - centroids_[i].mean) / (centroids_[i + 1].mean - centroids_[i].mean);
        return (mid_current + t * (mid_next - mid_current)) / total_weight_;
      }
      cumulative += centroids_[i].weight;
    }

    return 1.0;
  }

  // Merge another T-Digest into this one
  void merge(const TDigest& other) {
    if (other.centroids_.empty()) {
      return;
    }

    // Add all centroids from other, then compress
    for (const auto& c : other.centroids_) {
      centroids_.push_back(c);
      total_weight_ += c.weight;
    }

    compress();
  }

  // Total weight (count) of all values added
  [[nodiscard]] auto count() const -> double {
    return total_weight_;
  }

  // Reset the digest
  void clear() {
    centroids_.clear();
    total_weight_ = 0.0;
  }

  // Number of centroids (for testing/debugging)
  [[nodiscard]] auto centroidCount() const -> std::size_t {
    return centroids_.size();
  }

  // Access centroids (for testing/debugging)
  [[nodiscard]] auto centroids() const -> const std::vector<Centroid>& {
    return centroids_;
  }

private:
  // Arcsin scale function: k(q) = (δ/π) · arcsin(2q - 1)
  // Maps quantile q ∈ [0,1] to scaled space
  [[nodiscard]] static auto scaleFunction(double q) -> double {
    constexpr double delta = static_cast<double>(Compression);
    return (delta / std::numbers::pi) * std::asin(2.0 * q - 1.0);
  }

  // Inverse scale function: q = (1 + sin(k·π/δ)) / 2
  [[nodiscard]] static auto inverseScaleFunction(double k) -> double {
    constexpr double delta = static_cast<double>(Compression);
    return (1.0 + std::sin(k * std::numbers::pi / delta)) / 2.0;
  }

  // Check if a centroid at quantile q can absorb additional weight
  // A centroid is "full" when its span in k-space exceeds 1.0
  [[nodiscard]] auto canMerge(double q, double combined_weight) const -> bool {
    if (total_weight_ == 0.0) return true;

    double half_width = combined_weight / (2.0 * total_weight_);
    double q_left = std::max(0.0, q - half_width);
    double q_right = std::min(1.0, q + half_width);

    double k_left = scaleFunction(q_left);
    double k_right = scaleFunction(q_right);

    return (k_right - k_left) <= 1.0;
  }

  // Find the index of the centroid nearest to value
  [[nodiscard]] auto findNearest(double value) const -> std::size_t {
    assert(!centroids_.empty());

    std::size_t best_idx = 0;
    double best_dist = std::abs(value - centroids_[0].mean);

    for (std::size_t i = 1; i < centroids_.size(); ++i) {
      double dist = std::abs(value - centroids_[i].mean);
      if (dist < best_dist) {
        best_dist = dist;
        best_idx = i;
      }
    }

    return best_idx;
  }

  // Cumulative weight up to and including centroid at index
  [[nodiscard]] auto cumulativeWeight(std::size_t index) const -> double {
    double cumulative = 0.0;
    for (std::size_t i = 0; i <= index && i < centroids_.size(); ++i) {
      cumulative += centroids_[i].weight;
    }
    // Return the midpoint of this centroid's weight range
    return cumulative - centroids_[index].weight / 2.0;
  }

  // Insert a new centroid at the correct sorted position
  void insertSorted(double value, double weight) {
    auto pos = std::lower_bound(
        centroids_.begin(), centroids_.end(), value,
        [](const Centroid& c, double v) { return c.mean < v; });
    centroids_.insert(pos, Centroid{value, weight});
  }

  // Maximum number of centroids before compression
  [[nodiscard]] static constexpr auto maxCentroids() -> std::size_t {
    // Allow some slack (2x) before forcing compression
    return Compression * 2;
  }

  // Compress centroids by merging neighbors
  void compress() {
    if (centroids_.size() <= 1) return;

    // Sort centroids by mean
    std::sort(centroids_.begin(), centroids_.end(),
              [](const Centroid& a, const Centroid& b) { return a.mean < b.mean; });

    std::vector<Centroid> compressed;
    compressed.reserve(centroids_.size());

    double cumulative_weight = 0.0;
    compressed.push_back(centroids_[0]);
    cumulative_weight += centroids_[0].weight;

    for (std::size_t i = 1; i < centroids_.size(); ++i) {
      auto& current = centroids_[i];
      auto& last = compressed.back();

      // Calculate quantile at the boundary between last and current
      double q = (cumulative_weight + current.weight / 2.0) / total_weight_;
      double combined_weight = last.weight + current.weight;

      if (canMerge(q, combined_weight)) {
        // Merge current into last
        last.mean = (last.mean * last.weight + current.mean * current.weight) / combined_weight;
        last.weight = combined_weight;
      } else {
        // Start a new centroid
        compressed.push_back(current);
      }
      cumulative_weight += current.weight;
    }

    centroids_ = std::move(compressed);
  }

  std::vector<Centroid> centroids_;
  double total_weight_ = 0.0;
};

}  // namespace tempura
