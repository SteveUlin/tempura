#pragma once

// Block Graphics Scoring Algorithms
// Pluggable scoring system for character selection

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "block_gfx_patterns.h"

namespace tempura {

// =============================================================================
// High-Resolution Density Grid
// =============================================================================
// Use much finer resolution for density sampling than the character patterns.
// This reduces quantization artifacts when matching patterns.

constexpr int kDensityWidth = 32;   // 4x the pattern width (8)
constexpr int kDensityHeight = 96;  // 4x the pattern height (24)
constexpr int kTotalDensityCells = kDensityWidth * kDensityHeight;  // 3072

using DensityGrid = std::array<double, kTotalDensityCells>;

constexpr auto densityIndex(int x, int y) -> int {
  return y * kDensityWidth + x;
}

// Downsample density grid to pattern resolution for scoring
inline auto downsampleToPattern(const DensityGrid& density)
    -> std::array<double, kTotalSubpixels> {
  std::array<double, kTotalSubpixels> result{};

  // Each pattern cell covers (kDensityWidth/kSubpixelWidth) x (kDensityHeight/kSubpixelHeight) density cells
  constexpr int kScaleX = kDensityWidth / kSubpixelWidth;    // 4
  constexpr int kScaleY = kDensityHeight / kSubpixelHeight;  // 4
  constexpr double kCellCount = kScaleX * kScaleY;           // 16

  for (int py = 0; py < kSubpixelHeight; ++py) {
    for (int px = 0; px < kSubpixelWidth; ++px) {
      double sum = 0.0;
      for (int dy = 0; dy < kScaleY; ++dy) {
        for (int dx = 0; dx < kScaleX; ++dx) {
          int dx_idx = px * kScaleX + dx;
          int dy_idx = py * kScaleY + dy;
          sum += density[densityIndex(dx_idx, dy_idx)];
        }
      }
      result[subpixelIndex(px, py)] = sum / kCellCount;
    }
  }
  return result;
}

// =============================================================================
// Scoring Result
// =============================================================================

struct ScoringResult {
  const char* utf8;
  double fg_intensity;
  double bg_intensity;
  bool inverted;
  double score;
};

// =============================================================================
// Scorer Interface
// =============================================================================

using ScorerFn = std::function<ScoringResult(
    const DensityGrid& density,
    const std::vector<BlockChar>& library)>;

// =============================================================================
// Scoring Algorithms
// =============================================================================

// Algorithm 1: Least Squares (current algorithm)
// Minimizes sum of squared errors between target and displayed density
inline auto scorerLeastSquares() -> ScorerFn {
  return [](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    ScoringResult best{"", 0, 0, false, -1e9};

    for (const auto& ch : lib) {
      double filled_sum = 0.0, unfilled_sum = 0.0;
      int filled_count = 0, unfilled_count = 0;

      for (int i = 0; i < kTotalSubpixels; ++i) {
        if (ch.pattern[i]) {
          filled_sum += pattern_density[i];
          ++filled_count;
        } else {
          unfilled_sum += pattern_density[i];
          ++unfilled_count;
        }
      }

      double fg = filled_count > 0 ? filled_sum / filled_count : 0.0;
      double bg = unfilled_count > 0 ? unfilled_sum / unfilled_count : 0.0;

      double score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? fg : bg;
        double err = target - displayed;
        score -= err * err;
      }

      if (score > best.score) {
        best = {ch.utf8, fg, bg, false, score};
      }

      // Inverted
      double inv_fg = bg, inv_bg = fg;
      double inv_score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? inv_bg : inv_fg;
        double err = target - displayed;
        inv_score -= err * err;
      }

      if (inv_score > best.score) {
        best = {ch.utf8, inv_fg, inv_bg, true, inv_score};
      }
    }

    return best;
  };
}

// Algorithm 2: Variance-Aware Scoring
// Bonus for characters that capture high-variance regions
inline auto scorerVarianceAware(double variance_weight = 0.5) -> ScorerFn {
  return [variance_weight](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    ScoringResult best{"", 0, 0, false, -1e9};

    for (const auto& ch : lib) {
      double filled_sum = 0.0, unfilled_sum = 0.0;
      double filled_sq_sum = 0.0, unfilled_sq_sum = 0.0;
      int filled_count = 0, unfilled_count = 0;

      for (int i = 0; i < kTotalSubpixels; ++i) {
        double d = pattern_density[i];
        if (ch.pattern[i]) {
          filled_sum += d;
          filled_sq_sum += d * d;
          ++filled_count;
        } else {
          unfilled_sum += d;
          unfilled_sq_sum += d * d;
          ++unfilled_count;
        }
      }

      double fg = filled_count > 0 ? filled_sum / filled_count : 0.0;
      double bg = unfilled_count > 0 ? unfilled_sum / unfilled_count : 0.0;

      // Compute within-group variance
      double filled_var = filled_count > 1
          ? (filled_sq_sum / filled_count - fg * fg) : 0.0;
      double unfilled_var = unfilled_count > 1
          ? (unfilled_sq_sum / unfilled_count - bg * bg) : 0.0;

      // Low within-group variance = good (character boundary aligns with density boundary)
      double variance_penalty = (filled_var * filled_count + unfilled_var * unfilled_count)
                               / kTotalSubpixels;

      double score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? fg : bg;
        double err = target - displayed;
        score -= err * err;
      }

      // Subtract variance penalty (high variance = bad)
      score -= variance_weight * variance_penalty * kTotalSubpixels;

      if (score > best.score) {
        best = {ch.utf8, fg, bg, false, score};
      }

      // Inverted
      double inv_fg = bg, inv_bg = fg;
      double inv_score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? inv_bg : inv_fg;
        double err = target - displayed;
        inv_score -= err * err;
      }
      inv_score -= variance_weight * variance_penalty * kTotalSubpixels;

      if (inv_score > best.score) {
        best = {ch.utf8, inv_fg, inv_bg, true, inv_score};
      }
    }

    return best;
  };
}

// Algorithm 4: Correlation-based scoring
// Uses correlation between pattern and density instead of error
inline auto scorerCorrelation() -> ScorerFn {
  return [](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    // Compute mean density
    double density_mean = 0.0;
    for (int i = 0; i < kTotalSubpixels; ++i) {
      density_mean += pattern_density[i];
    }
    density_mean /= kTotalSubpixels;

    ScoringResult best{"", 0, 0, false, -1e9};

    for (const auto& ch : lib) {
      // Treat pattern as binary (1 for filled, 0 for unfilled)
      double pattern_mean = ch.fill_fraction;

      // Compute correlation
      double cov = 0.0, pattern_var = 0.0, density_var = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double p = ch.pattern[i] ? 1.0 : 0.0;
        double d = pattern_density[i];
        cov += (p - pattern_mean) * (d - density_mean);
        pattern_var += (p - pattern_mean) * (p - pattern_mean);
        density_var += (d - density_mean) * (d - density_mean);
      }

      double denom = std::sqrt(pattern_var * density_var);
      double corr = (denom > 1e-9) ? cov / denom : 0.0;

      // Compute optimal intensities
      double filled_sum = 0.0, unfilled_sum = 0.0;
      int filled_count = 0, unfilled_count = 0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        if (ch.pattern[i]) {
          filled_sum += pattern_density[i];
          ++filled_count;
        } else {
          unfilled_sum += pattern_density[i];
          ++unfilled_count;
        }
      }
      double fg = filled_count > 0 ? filled_sum / filled_count : 0.0;
      double bg = unfilled_count > 0 ? unfilled_sum / unfilled_count : 0.0;

      // Score is absolute correlation (both positive and negative are good)
      double score = std::abs(corr);
      bool inverted = corr < 0;

      if (score > best.score) {
        if (inverted) {
          best = {ch.utf8, bg, fg, true, score};
        } else {
          best = {ch.utf8, fg, bg, false, score};
        }
      }
    }

    return best;
  };
}

// Algorithm 5: Quantized Levels
// Quantize densities before scoring to reduce sensitivity
inline auto scorerQuantized(int levels = 4) -> ScorerFn {
  return [levels](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    // Quantize densities
    auto quantize = [levels](double d) {
      int q = static_cast<int>(d * levels);
      q = std::clamp(q, 0, levels - 1);
      return (q + 0.5) / levels;
    };

    std::array<double, kTotalSubpixels> quantized{};
    for (int i = 0; i < kTotalSubpixels; ++i) {
      quantized[i] = quantize(pattern_density[i]);
    }

    ScoringResult best{"", 0, 0, false, -1e9};

    for (const auto& ch : lib) {
      double filled_sum = 0.0, unfilled_sum = 0.0;
      int filled_count = 0, unfilled_count = 0;

      for (int i = 0; i < kTotalSubpixels; ++i) {
        if (ch.pattern[i]) {
          filled_sum += quantized[i];
          ++filled_count;
        } else {
          unfilled_sum += quantized[i];
          ++unfilled_count;
        }
      }

      double fg = filled_count > 0 ? filled_sum / filled_count : 0.0;
      double bg = unfilled_count > 0 ? unfilled_sum / unfilled_count : 0.0;

      double score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = quantized[i];
        double displayed = ch.pattern[i] ? fg : bg;
        double err = target - displayed;
        score -= err * err;
      }

      if (score > best.score) {
        best = {ch.utf8, fg, bg, false, score};
      }

      // Inverted
      double inv_fg = bg, inv_bg = fg;
      double inv_score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = quantized[i];
        double displayed = ch.pattern[i] ? inv_bg : inv_fg;
        double err = target - displayed;
        inv_score -= err * err;
      }

      if (inv_score > best.score) {
        best = {ch.utf8, inv_fg, inv_bg, true, inv_score};
      }
    }

    return best;
  };
}

// Algorithm 6: L1 norm (absolute error)
// Less sensitive to outliers than squared error
inline auto scorerL1() -> ScorerFn {
  return [](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    ScoringResult best{"", 0, 0, false, -1e9};

    for (const auto& ch : lib) {
      double filled_sum = 0.0;
      double unfilled_sum = 0.0;
      int filled_count = 0;
      int unfilled_count = 0;

      for (int i = 0; i < kTotalSubpixels; ++i) {
        if (ch.pattern[i]) {
          filled_sum += pattern_density[i];
          ++filled_count;
        } else {
          unfilled_sum += pattern_density[i];
          ++unfilled_count;
        }
      }

      double fg = filled_count > 0 ? filled_sum / filled_count : 0.0;
      double bg = unfilled_count > 0 ? unfilled_sum / unfilled_count : 0.0;

      // L1 score = negative sum of absolute errors
      double score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? fg : bg;
        score -= std::abs(target - displayed);
      }

      if (score > best.score) {
        best = {ch.utf8, fg, bg, false, score};
      }

      // Inverted
      double inv_fg = bg;
      double inv_bg = fg;
      double inv_score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? inv_bg : inv_fg;
        inv_score -= std::abs(target - displayed);
      }

      if (inv_score > best.score) {
        best = {ch.utf8, inv_fg, inv_bg, true, inv_score};
      }
    }

    return best;
  };
}

// Algorithm 7: Gradient alignment
// Prefer characters whose edges align with the density gradient
inline auto scorerGradientAlign() -> ScorerFn {
  return [](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    // Compute gradient direction of density
    double gx = 0.0, gy = 0.0;
    for (int y = 0; y < kSubpixelHeight; ++y) {
      for (int x = 0; x < kSubpixelWidth; ++x) {
        double d = pattern_density[subpixelIndex(x, y)];
        // Weight by position relative to center
        double wx = (x - kSubpixelWidth / 2.0 + 0.5) / kSubpixelWidth;
        double wy = (y - kSubpixelHeight / 2.0 + 0.5) / kSubpixelHeight;
        gx += d * wx;
        gy += d * wy;
      }
    }

    double grad_mag = std::sqrt(gx * gx + gy * gy);
    double grad_angle = std::atan2(gy, gx);  // -π to π

    ScoringResult best{"", 0, 0, false, -1e9};

    for (const auto& ch : lib) {
      double filled_sum = 0.0;
      double unfilled_sum = 0.0;
      int filled_count = 0;
      int unfilled_count = 0;

      // Also compute centroid of filled region
      double cx = 0.0, cy = 0.0;

      for (int i = 0; i < kTotalSubpixels; ++i) {
        int x = i % kSubpixelWidth;
        int y = i / kSubpixelWidth;
        if (ch.pattern[i]) {
          filled_sum += pattern_density[i];
          ++filled_count;
          cx += x;
          cy += y;
        } else {
          unfilled_sum += pattern_density[i];
          ++unfilled_count;
        }
      }

      if (filled_count == 0 || unfilled_count == 0) continue;

      double fg = filled_sum / filled_count;
      double bg = unfilled_sum / unfilled_count;

      // Character's "direction" - from center to centroid of filled region
      cx /= filled_count;
      cy /= filled_count;
      double char_angle = std::atan2(cy - kSubpixelHeight / 2.0, cx - kSubpixelWidth / 2.0);

      // Base score: least squares
      double score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? fg : bg;
        double err = target - displayed;
        score -= err * err;
      }

      // Bonus for gradient alignment (when gradient is significant)
      if (grad_mag > 0.1) {
        double angle_diff = std::abs(grad_angle - char_angle);
        if (angle_diff > M_PI) angle_diff = 2 * M_PI - angle_diff;
        double alignment = std::cos(angle_diff);  // 1 = aligned, -1 = opposite
        score += alignment * grad_mag * 10.0;  // Bonus proportional to gradient strength
      }

      if (score > best.score) {
        best = {ch.utf8, fg, bg, false, score};
      }

      // Inverted - opposite direction
      double inv_fg = bg;
      double inv_bg = fg;
      double inv_score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? inv_bg : inv_fg;
        double err = target - displayed;
        inv_score -= err * err;
      }

      if (grad_mag > 0.1) {
        double inv_char_angle = char_angle + M_PI;
        if (inv_char_angle > M_PI) inv_char_angle -= 2 * M_PI;
        double angle_diff = std::abs(grad_angle - inv_char_angle);
        if (angle_diff > M_PI) angle_diff = 2 * M_PI - angle_diff;
        double alignment = std::cos(angle_diff);
        inv_score += alignment * grad_mag * 10.0;
      }

      if (inv_score > best.score) {
        best = {ch.utf8, inv_fg, inv_bg, true, inv_score};
      }
    }

    return best;
  };
}

// Algorithm 8: Fill-first scoring
// First find characters with similar fill fraction, then pick best shape
inline auto scorerFillFirst(double fill_tolerance = 0.15) -> ScorerFn {
  return [fill_tolerance](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    // Compute target fill fraction
    double total = 0.0;
    for (int i = 0; i < kTotalSubpixels; ++i) {
      total += pattern_density[i];
    }
    double target_fill = total / kTotalSubpixels;

    ScoringResult best{"", 0, 0, false, -1e9};

    for (const auto& ch : lib) {
      // Skip if fill fraction is too different
      double fill_diff = std::abs(ch.fill_fraction - target_fill);
      if (fill_diff > fill_tolerance) continue;

      double filled_sum = 0.0;
      double unfilled_sum = 0.0;
      int filled_count = 0;
      int unfilled_count = 0;

      for (int i = 0; i < kTotalSubpixels; ++i) {
        if (ch.pattern[i]) {
          filled_sum += pattern_density[i];
          ++filled_count;
        } else {
          unfilled_sum += pattern_density[i];
          ++unfilled_count;
        }
      }

      double fg = filled_count > 0 ? filled_sum / filled_count : 0.0;
      double bg = unfilled_count > 0 ? unfilled_sum / unfilled_count : 0.0;

      double score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? fg : bg;
        double err = target - displayed;
        score -= err * err;
      }

      // Small bonus for closer fill fraction match
      score += (fill_tolerance - fill_diff) * 0.1;

      if (score > best.score) {
        best = {ch.utf8, fg, bg, false, score};
      }

      // Also consider inverted
      double inv_fill_diff = std::abs((1.0 - ch.fill_fraction) - target_fill);
      if (inv_fill_diff <= fill_tolerance) {
        double inv_fg = bg;
        double inv_bg = fg;
        double inv_score = 0.0;
        for (int i = 0; i < kTotalSubpixels; ++i) {
          double target = pattern_density[i];
          double displayed = ch.pattern[i] ? inv_bg : inv_fg;
          double err = target - displayed;
          inv_score -= err * err;
        }
        inv_score += (fill_tolerance - inv_fill_diff) * 0.1;

        if (inv_score > best.score) {
          best = {ch.utf8, inv_fg, inv_bg, true, inv_score};
        }
      }
    }

    // Fallback to least squares if nothing matched
    if (best.utf8 == nullptr || best.utf8[0] == '\0') {
      return scorerLeastSquares()(density, lib);
    }

    return best;
  };
}

// Algorithm 9: Adaptive contrast
// Contrast threshold based on local density range
inline auto scorerAdaptiveContrast() -> ScorerFn {
  return [](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    // Compute local min/max
    double d_min = 1.0, d_max = 0.0;
    for (int i = 0; i < kTotalSubpixels; ++i) {
      d_min = std::min(d_min, pattern_density[i]);
      d_max = std::max(d_max, pattern_density[i]);
    }
    double range = d_max - d_min;

    // Adaptive threshold: require at least 30% of local range
    double min_contrast = range * 0.3;

    // If range is very small, just use average color
    if (range < 0.05) {
      double avg = (d_min + d_max) / 2.0;
      return {" ", 0, avg, false, 0};
    }

    ScoringResult best{"", 0, 0, false, -1e9};

    for (const auto& ch : lib) {
      double filled_sum = 0.0;
      double unfilled_sum = 0.0;
      int filled_count = 0;
      int unfilled_count = 0;

      for (int i = 0; i < kTotalSubpixels; ++i) {
        if (ch.pattern[i]) {
          filled_sum += pattern_density[i];
          ++filled_count;
        } else {
          unfilled_sum += pattern_density[i];
          ++unfilled_count;
        }
      }

      if (filled_count == 0 || unfilled_count == 0) continue;

      double fg = filled_sum / filled_count;
      double bg = unfilled_sum / unfilled_count;

      // Skip if contrast too low relative to local range
      if (std::abs(fg - bg) < min_contrast) continue;

      double score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? fg : bg;
        double err = target - displayed;
        score -= err * err;
      }

      if (score > best.score) {
        best = {ch.utf8, fg, bg, false, score};
      }

      // Inverted
      double inv_fg = bg;
      double inv_bg = fg;
      double inv_score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? inv_bg : inv_fg;
        double err = target - displayed;
        inv_score -= err * err;
      }

      if (inv_score > best.score) {
        best = {ch.utf8, inv_fg, inv_bg, true, inv_score};
      }
    }

    // Fallback: use space with mid-range color
    if (best.utf8 == nullptr || best.utf8[0] == '\0') {
      double avg = (d_min + d_max) / 2.0;
      return {" ", 0, avg, false, 0};
    }

    return best;
  };
}

// Algorithm 10: Thresholded binary matching
// Find optimal threshold to binarize density, then match pattern
inline auto scorerThreshold() -> ScorerFn {
  return [](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    // Try multiple thresholds
    ScoringResult best{"", 0, 0, false, -1e9};

    for (double threshold = 0.1; threshold <= 0.9; threshold += 0.1) {
      // Binarize density at this threshold
      FillPattern binary_target;
      double high_sum = 0.0, low_sum = 0.0;
      int high_count = 0, low_count = 0;

      for (int i = 0; i < kTotalSubpixels; ++i) {
        if (pattern_density[i] >= threshold) {
          binary_target.set(i);
          high_sum += pattern_density[i];
          ++high_count;
        } else {
          low_sum += pattern_density[i];
          ++low_count;
        }
      }

      if (high_count == 0 || low_count == 0) continue;

      double fg = high_sum / high_count;
      double bg = low_sum / low_count;

      // Find best matching character pattern
      for (const auto& ch : lib) {
        // Count matching bits
        int matches = 0;
        for (int i = 0; i < kTotalSubpixels; ++i) {
          if (ch.pattern[i] == binary_target[i]) ++matches;
        }

        double score = static_cast<double>(matches) / kTotalSubpixels;

        if (score > best.score) {
          best = {ch.utf8, fg, bg, false, score};
        }

        // Inverted
        int inv_matches = kTotalSubpixels - matches;
        double inv_score = static_cast<double>(inv_matches) / kTotalSubpixels;
        if (inv_score > best.score) {
          best = {ch.utf8, bg, fg, true, inv_score};
        }
      }
    }

    return best;
  };
}

// Algorithm 11: Between-group variance maximization
// Maximize the variance explained by the character split
inline auto scorerBetweenVariance() -> ScorerFn {
  return [](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    // Compute overall mean
    double total_mean = 0.0;
    for (int i = 0; i < kTotalSubpixels; ++i) {
      total_mean += pattern_density[i];
    }
    total_mean /= kTotalSubpixels;

    ScoringResult best{"", 0, 0, false, -1e9};

    for (const auto& ch : lib) {
      double filled_sum = 0.0, unfilled_sum = 0.0;
      int filled_count = 0, unfilled_count = 0;

      for (int i = 0; i < kTotalSubpixels; ++i) {
        if (ch.pattern[i]) {
          filled_sum += pattern_density[i];
          ++filled_count;
        } else {
          unfilled_sum += pattern_density[i];
          ++unfilled_count;
        }
      }

      if (filled_count == 0 || unfilled_count == 0) continue;

      double fg = filled_sum / filled_count;
      double bg = unfilled_sum / unfilled_count;

      // Between-group variance (how well does the split separate the data?)
      double between_var = filled_count * (fg - total_mean) * (fg - total_mean)
                        + unfilled_count * (bg - total_mean) * (bg - total_mean);
      between_var /= kTotalSubpixels;

      double score = between_var;
      bool inverted = fg < bg;

      if (score > best.score) {
        if (inverted) {
          best = {ch.utf8, bg, fg, true, score};
        } else {
          best = {ch.utf8, fg, bg, false, score};
        }
      }
    }

    return best;
  };
}

// Algorithm: Edge complexity penalty
// Count transitions in the pattern - jagged patterns have more transitions
inline auto countPatternTransitions(const FillPattern& pattern) -> int {
  int transitions = 0;

  // Horizontal transitions (within each row)
  for (int y = 0; y < kSubpixelHeight; ++y) {
    for (int x = 0; x < kSubpixelWidth - 1; ++x) {
      if (pattern[subpixelIndex(x, y)] != pattern[subpixelIndex(x + 1, y)]) {
        ++transitions;
      }
    }
  }

  // Vertical transitions (within each column)
  for (int x = 0; x < kSubpixelWidth; ++x) {
    for (int y = 0; y < kSubpixelHeight - 1; ++y) {
      if (pattern[subpixelIndex(x, y)] != pattern[subpixelIndex(x, y + 1)]) {
        ++transitions;
      }
    }
  }

  return transitions;
}

// Smooth scorer: least squares + penalty for jagged patterns
inline auto scorerSmooth(double complexity_weight = 0.5) -> ScorerFn {
  return [complexity_weight](const DensityGrid& density, const std::vector<BlockChar>& lib) -> ScoringResult {
    auto pattern_density = downsampleToPattern(density);

    ScoringResult best{"", 0, 0, false, -1e9};

    for (const auto& ch : lib) {
      double filled_sum = 0.0;
      double unfilled_sum = 0.0;
      int filled_count = 0;
      int unfilled_count = 0;

      for (int i = 0; i < kTotalSubpixels; ++i) {
        if (ch.pattern[i]) {
          filled_sum += pattern_density[i];
          ++filled_count;
        } else {
          unfilled_sum += pattern_density[i];
          ++unfilled_count;
        }
      }

      double fg = filled_count > 0 ? filled_sum / filled_count : 0.0;
      double bg = unfilled_count > 0 ? unfilled_sum / unfilled_count : 0.0;

      // Base score: least squares
      double score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? fg : bg;
        double err = target - displayed;
        score -= err * err;
      }

      // Penalty for edge complexity (jagged patterns)
      // A clean horizontal/vertical split has ~8-12 transitions
      // A diagonal wedge has ~12-20 transitions
      // A scattered octant can have 30+ transitions
      int transitions = countPatternTransitions(ch.pattern);
      double complexity_penalty = complexity_weight * (transitions / 100.0);
      score -= complexity_penalty;

      if (score > best.score) {
        best = {ch.utf8, fg, bg, false, score};
      }

      // Inverted
      double inv_fg = bg;
      double inv_bg = fg;
      double inv_score = 0.0;
      for (int i = 0; i < kTotalSubpixels; ++i) {
        double target = pattern_density[i];
        double displayed = ch.pattern[i] ? inv_bg : inv_fg;
        double err = target - displayed;
        inv_score -= err * err;
      }
      inv_score -= complexity_penalty;  // Same pattern, same complexity

      if (inv_score > best.score) {
        best = {ch.utf8, inv_fg, inv_bg, true, inv_score};
      }
    }

    return best;
  };
}

// =============================================================================
// Named Scorer Registry
// =============================================================================

struct NamedScorer {
  std::string name;
  std::string description;
  ScorerFn scorer;
};

inline auto getAllScorers() -> std::vector<NamedScorer> {
  return {
    {"least_squares", "Minimize squared error (current)", scorerLeastSquares()},
    {"smooth", "Least squares + penalty for jagged patterns", scorerSmooth(0.5)},
    {"L1", "Minimize absolute error", scorerL1()},
    {"gradient_align", "Bonus for edge-gradient alignment", scorerGradientAlign()},
    {"fill_first", "Match fill fraction first, then shape", scorerFillFirst()},
    {"adaptive_contrast", "Contrast threshold based on local range", scorerAdaptiveContrast()},
    {"threshold", "Binarize density, match pattern", scorerThreshold()},
    {"variance_aware", "Penalize high within-group variance", scorerVarianceAware(0.5)},
    {"correlation", "Maximize pattern-density correlation", scorerCorrelation()},
    {"between_var", "Maximize between-group variance", scorerBetweenVariance()},
  };
}

}  // namespace tempura
