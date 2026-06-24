#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "plot.h"  // For RGB, Point, queryTerminalBackground
#include "block_gfx_patterns.h"  // Ghostty-based patterns for sextants and diagonals
#include "block_gfx_scorer.h"  // Scoring algorithms for character selection

namespace tempura {

// =============================================================================
// LAB Color Space - Perceptually Uniform Color Interpolation
// =============================================================================
//
// CIELAB (L*a*b*) is a perceptually uniform color space where equal distances
// correspond to equal perceived color differences. This produces much smoother
// gradients than RGB interpolation, especially for heatmaps.

struct Lab {
  double L;  // Lightness [0, 100]
  double a;  // Green-Red [-128, 127]
  double b;  // Blue-Yellow [-128, 127]
};

// srgbToLinear / linearToSrgb come from plot.h (the canonical transfer functions).

// XYZ to LAB helper
constexpr double xyzToLabF(double t) {
  constexpr double delta = 6.0 / 29.0;
  constexpr double delta_cubed = delta * delta * delta;
  return t > delta_cubed ? std::cbrt(t) : t / (3 * delta * delta) + 4.0 / 29.0;
}

// LAB to XYZ helper (inverse of xyzToLabF)
constexpr double labToXyzF(double t) {
  constexpr double delta = 6.0 / 29.0;
  return t > delta ? t * t * t : 3 * delta * delta * (t - 4.0 / 29.0);
}

// D65 white point
constexpr double kXn = 0.95047;
constexpr double kYn = 1.0;
constexpr double kZn = 1.08883;

inline auto rgbToLab(const RGB& rgb) -> Lab {
  // sRGB to linear RGB
  double r = srgbToLinear(rgb.r / 255.0);
  double g = srgbToLinear(rgb.g / 255.0);
  double b = srgbToLinear(rgb.b / 255.0);

  // Linear RGB to XYZ (sRGB matrix)
  double x = 0.4124564 * r + 0.3575761 * g + 0.1804375 * b;
  double y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
  double z = 0.0193339 * r + 0.1191920 * g + 0.9503041 * b;

  // XYZ to LAB
  double fx = xyzToLabF(x / kXn);
  double fy = xyzToLabF(y / kYn);
  double fz = xyzToLabF(z / kZn);

  return Lab{
      116.0 * fy - 16.0,
      500.0 * (fx - fy),
      200.0 * (fy - fz)};
}

inline auto labToRgb(const Lab& lab) -> RGB {
  // LAB to XYZ
  double fy = (lab.L + 16.0) / 116.0;
  double fx = lab.a / 500.0 + fy;
  double fz = fy - lab.b / 200.0;

  double x = kXn * labToXyzF(fx);
  double y = kYn * labToXyzF(fy);
  double z = kZn * labToXyzF(fz);

  // XYZ to linear RGB (inverse sRGB matrix)
  double r =  3.2404542 * x - 1.5371385 * y - 0.4985314 * z;
  double g = -0.9692660 * x + 1.8760108 * y + 0.0415560 * z;
  double b =  0.0556434 * x - 0.2040259 * y + 1.0572252 * z;

  // Linear RGB to sRGB with clamping
  r = std::clamp(linearToSrgb(r), 0.0, 1.0);
  g = std::clamp(linearToSrgb(g), 0.0, 1.0);
  b = std::clamp(linearToSrgb(b), 0.0, 1.0);

  return RGB{
      static_cast<uint8_t>(std::round(r * 255)),
      static_cast<uint8_t>(std::round(g * 255)),
      static_cast<uint8_t>(std::round(b * 255))};
}

// Interpolate in LAB space for perceptually uniform gradients
inline auto lerpLab(const RGB& from, const RGB& to, double t) -> RGB {
  Lab a = rgbToLab(from);
  Lab b = rgbToLab(to);
  Lab result{
      a.L + t * (b.L - a.L),
      a.a + t * (b.a - a.a),
      a.b + t * (b.b - a.b)};
  return labToRgb(result);
}


// =============================================================================
// Block Graphics - High Resolution Terminal Heatmaps
// =============================================================================
//
// Uses Unicode "Symbols for Legacy Computing" (U+1FB00-U+1FBFF) for
// high-resolution terminal graphics. Key character sets:
//
// 1. Sextants (U+1FB00-U+1FB3B): 2×3 grid = 6 "pixels" per character cell
//    Numbering:  1 | 2
//                3 | 4
//                5 | 6
//
// 2. Smooth Diagonal Characters (U+1FB3C-U+1FB67): Triangular fills with
//    diagonal edges, enabling smooth gradients at boundaries.
//
// 3. Block Eighths (U+1FB70-U+1FB8B): Fine fractional blocks for edges.
//
// Approach: Each character has a "fill pattern" - which subpixels it fills.
// Given a desired density pattern, we score each character and pick the best.
// Combined with fg/bg coloring, this achieves very high visual fidelity.


// BlockChar is defined in block_gfx_patterns.h

// Helper to create FillPattern from a lambda
template <typename Fn>
constexpr auto makePattern(Fn&& fn) -> FillPattern {
  FillPattern p;
  for (int y = 0; y < kSubpixelHeight; ++y) {
    for (int x = 0; x < kSubpixelWidth; ++x) {
      if (fn(x, y)) {
        p.set(subpixelIndex(x, y));
      }
    }
  }
  return p;
}

// =============================================================================
// Sextant Characters (U+1FB00-U+1FB3B)
// =============================================================================
//
// Layout:  1 | 2     Segment boundaries:
//          -----     x: [0,4) = left, [4,8) = right
//          3 | 4     y: [0,4) = top, [4,8) = middle, [8,12) = bottom
//          -----
//          5 | 6

constexpr bool inSextant(int x, int y, int segment) {
  // Segments 1-6 (1-indexed as per Unicode naming)
  bool left = x < 4;
  bool right = x >= 4;
  bool top = y < 4;
  bool mid = y >= 4 && y < 8;
  bool bot = y >= 8;

  switch (segment) {
    case 1: return left && top;
    case 2: return right && top;
    case 3: return left && mid;
    case 4: return right && mid;
    case 5: return left && bot;
    case 6: return right && bot;
    default: return false;
  }
}

// Generate pattern for sextant with given segments filled
template <int... Segments>
constexpr auto sextantPattern() -> FillPattern {
  return makePattern([](int x, int y) {
    return ((inSextant(x, y, Segments) || ...));
  });
}

// Empty pattern
constexpr auto emptyPattern() -> FillPattern {
  return FillPattern{};
}

// Full pattern
constexpr auto fullPattern() -> FillPattern {
  FillPattern p;
  p.set();
  return p;
}

// =============================================================================
// Diagonal Character Patterns
// =============================================================================
//
// Diagonal characters fill triangular regions. The Unicode names describe
// the diagonal line: "LOWER LEFT BLOCK DIAGONAL UPPER LEFT TO LOWER CENTRE"
// means the lower-left area bounded by a line from (0,0) to (0.5,1).
//
// We use normalized coordinates: (0,0) = top-left, (1,1) = bottom-right
// The pattern checks if a subpixel is on the filled side of the diagonal.

// Check if point (px, py) is below/left of line from (x1,y1) to (x2,y2)
// Returns true if point is in the "lower-left" half-plane
constexpr bool belowLine(double px, double py,
                         double x1, double y1, double x2, double y2) {
  // Cross product of (p - p1) and (p2 - p1)
  // Positive = left of line (when going from p1 to p2)
  double cross = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1);
  return cross >= 0;
}

// Create diagonal pattern: fills pixels on one side of a line
// Line defined from (x1,y1) to (x2,y2) in normalized [0,1] coordinates
// fill_below = true: fill the lower-left side
constexpr auto diagonalPattern(double x1, double y1, double x2, double y2,
                                bool fill_below) -> FillPattern {
  return makePattern([=](int x, int y) {
    // Convert subpixel to normalized coordinates (center of subpixel)
    double px = (x + 0.5) / kSubpixelWidth;
    double py = (y + 0.5) / kSubpixelHeight;
    bool below = belowLine(px, py, x1, y1, x2, y2);
    return fill_below ? below : !below;
  });
}

// =============================================================================
// Block Eighth Patterns
// =============================================================================

// Left N eighths filled (N = 1..7)
constexpr auto leftEighthsPattern(int n) -> FillPattern {
  int threshold = n;  // subpixels 0..n-1 are filled
  return makePattern([=](int x, int /*y*/) { return x < threshold; });
}

// Right N eighths filled
constexpr auto rightEighthsPattern(int n) -> FillPattern {
  int threshold = kSubpixelWidth - n;
  return makePattern([=](int x, int /*y*/) { return x >= threshold; });
}

// Bottom N twelfths filled (for vertical eighths, we have 12 rows)
constexpr auto bottomTwelfthsPattern(int n) -> FillPattern {
  int threshold = kSubpixelHeight - n;
  return makePattern([=](int /*x*/, int y) { return y >= threshold; });
}

// Top N twelfths filled
constexpr auto topTwelfthsPattern(int n) -> FillPattern {
  int threshold = n;
  return makePattern([=](int /*x*/, int y) { return y < threshold; });
}

// Upper half (▀)
constexpr auto upperHalfPattern() -> FillPattern {
  return makePattern([](int /*x*/, int y) { return y < kSubpixelHeight / 2; });
}

// Lower half (▄)
constexpr auto lowerHalfPattern() -> FillPattern {
  return makePattern([](int /*x*/, int y) { return y >= kSubpixelHeight / 2; });
}


// =============================================================================
// Character Library
// =============================================================================

// Storage for dynamically generated UTF-8 strings
// Uses unique_ptr to string to avoid pointer invalidation on vector growth
struct Utf8Storage {
  std::vector<std::unique_ptr<std::string>> strings;

  auto add(uint32_t cp) -> const char* {
    char buf[5] = {};
    int len = codepointToUtf8(cp, buf);
    strings.push_back(std::make_unique<std::string>(buf, static_cast<size_t>(len)));
    return strings.back()->c_str();
  }
};

inline const std::vector<BlockChar>& getCharacterLibrary() {
  static Utf8Storage storage;
  static const std::vector<BlockChar> chars = []() {
    std::vector<BlockChar> result;
    result.reserve(1000);

    auto add = [&](const char* utf8, FillPattern pattern) {
      double fraction = static_cast<double>(pattern.count()) / kTotalSubpixels;
      result.push_back({utf8, pattern, fraction});
    };

    auto addRange = [&](uint32_t first, uint32_t last) {
      for (uint32_t cp = first; cp <= last; ++cp) {
        FillPattern pattern = getPattern(cp);
        if (pattern.none()) continue;  // Skip empty patterns
        add(storage.add(cp), pattern);
      }
    };

    // Space and full block (special - use string literals)
    add(" ", emptyPattern());
    add("█", fullPattern());

    // -------------------------------------------------------------------------
    // ACTIVE: High-resolution character sets for smooth radial gradients
    // -------------------------------------------------------------------------
    addRange(kFirstSextant, kLastSextant);           // Sextants (2×3 grid)
    addRange(kFirstSmoothMosaic, kLastSmoothMosaic); // Smooth Mosaics (diagonals)
    addRange(kFirstOctant, kLastOctant);             // Octants (2×4 grid)
    addRange(kFirstSixteenth, kLastSixteenth);       // Sixteenth Blocks (4×4 grid)

    // -------------------------------------------------------------------------
    // DISABLED: Block Elements cause vertical striping artifacts
    // -------------------------------------------------------------------------
    // addRange(kFirstBlockElement, kLastBlockElement); // Vertical bars ▉▊▋▌▍▎▏

    // -------------------------------------------------------------------------
    // DISABLED: Other categories turned off for now
    // -------------------------------------------------------------------------
    // addRange(kFirstEdgeTriangle, kLastEdgeTriangle);
    // addRange(kFirstEighthBlock, kLastEighthBlock);
    // addRange(kFirstCombinedBlock, kLastCombinedBlock);
    // addRange(kFirstDiagonalFill, kLastDiagonalFill);
    // addRange(kFirstTriangleShade, kLastTriangleShade);
    // addRange(kFirstCornerDiagonal, kLastCornerDiagonal);
    // addRange(kFirstCellDiagonal, kLastCellDiagonal);
    // addRange(kFirstCircle, kLastCircle);
    // addRange(kFirstSepQuadrant, kLastSepQuadrant);
    // addRange(kFirstSepSextant, kLastSepSextant);

    return result;
  }();

  return chars;
}

// =============================================================================
// Pattern Scoring
// =============================================================================
//
// Given a target pattern (which subpixels should be filled with high density),
// score how well each character matches. Higher score = better match.

// Simple overlap score: count matching bits
inline auto overlapScore(const FillPattern& target,
                         const FillPattern& candidate) -> int {
  // XOR gives differing bits, then count and subtract from total
  auto diff = target ^ candidate;
  return kTotalSubpixels - static_cast<int>(diff.count());
}

// Weighted score that penalizes false positives more than false negatives
// (better to have empty space than spurious fills in low-density areas)
inline auto weightedScore(const FillPattern& target,
                          const FillPattern& candidate,
                          double false_positive_penalty = 1.5) -> double {
  int true_positives = 0;   // target=1, candidate=1
  int false_positives = 0;  // target=0, candidate=1
  int false_negatives = 0;  // target=1, candidate=0

  for (int i = 0; i < kTotalSubpixels; ++i) {
    bool t = target[i];
    bool c = candidate[i];
    if (t && c) ++true_positives;
    else if (!t && c) ++false_positives;
    else if (t && !c) ++false_negatives;
  }

  return static_cast<double>(true_positives)
       - false_positive_penalty * false_positives
       - false_negatives;
}

// Find the best matching character for a target pattern
inline auto findBestChar(const FillPattern& target) -> const BlockChar& {
  const auto& library = getCharacterLibrary();

  const BlockChar* best = &library[0];
  double best_score = weightedScore(target, library[0].pattern);

  for (size_t i = 1; i < library.size(); ++i) {
    double score = weightedScore(target, library[i].pattern);
    if (score > best_score) {
      best_score = score;
      best = &library[i];
    }
  }

  return *best;
}

// Find best character for a given fill fraction (simple density-based lookup)
inline auto findCharByDensity(double density) -> const BlockChar& {
  const auto& library = getCharacterLibrary();

  const BlockChar* best = &library[0];
  double best_diff = std::abs(density - library[0].fill_fraction);

  for (size_t i = 1; i < library.size(); ++i) {
    double diff = std::abs(density - library[i].fill_fraction);
    if (diff < best_diff) {
      best_diff = diff;
      best = &library[i];
    }
  }

  return *best;
}


// =============================================================================
// High-Resolution Heatmap
// =============================================================================

struct HiresHeatmapOptions {
  int64_t width = 60;
  int64_t height = 20;
  bool show_border = true;
  std::string title;
  RGB low_color{20, 20, 80};
  RGB high_color{255, 255, 100};
  std::optional<RGB> bg_color;
  double char_aspect = 2.0;  // Terminal char height/width ratio
  std::optional<double> min_x, max_x, min_y, max_y;
};

// High-resolution density heatmap using legacy computing block characters
template <std::ranges::input_range R>
  requires std::convertible_to<std::ranges::range_value_t<R>, Point>
auto hiresHeatmap(R&& points, const HiresHeatmapOptions& opts = {})
    -> std::string {

  std::vector<Point> pts;
  for (const auto& p : points) {
    pts.push_back(static_cast<Point>(p));
  }

  if (pts.empty()) return "";

  // Determine bounds
  double x_min = opts.min_x.value_or(pts[0].x);
  double x_max = opts.max_x.value_or(pts[0].x);
  double y_min = opts.min_y.value_or(pts[0].y);
  double y_max = opts.max_y.value_or(pts[0].y);

  if (!opts.min_x || !opts.max_x || !opts.min_y || !opts.max_y) {
    for (const auto& p : pts) {
      if (!opts.min_x) x_min = std::min(x_min, p.x);
      if (!opts.max_x) x_max = std::max(x_max, p.x);
      if (!opts.min_y) y_min = std::min(y_min, p.y);
      if (!opts.max_y) y_max = std::max(y_max, p.y);
    }
    // Add margins
    double x_margin = (x_max - x_min) * 0.05;
    double y_margin = (y_max - y_min) * 0.05;
    if (!opts.min_x) x_min -= x_margin;
    if (!opts.max_x) x_max += x_margin;
    if (!opts.min_y) y_min -= y_margin;
    if (!opts.max_y) y_max += y_margin;
  }

  // Account for character aspect ratio
  double x_range = x_max - x_min;
  double y_range = y_max - y_min;
  double data_aspect = y_range / x_range;
  double char_cells_aspect = static_cast<double>(opts.height) / opts.width;
  double visual_aspect = char_cells_aspect * opts.char_aspect;

  if (visual_aspect > data_aspect) {
    double extra = y_range * (visual_aspect / data_aspect - 1.0);
    y_min -= extra / 2;
    y_max += extra / 2;
    y_range = y_max - y_min;
  } else {
    double extra = x_range * (data_aspect / visual_aspect - 1.0);
    x_min -= extra / 2;
    x_max += extra / 2;
    x_range = x_max - x_min;
  }

  // High-resolution density grid using scorer's resolution
  int grid_w = static_cast<int>(opts.width) * kDensityWidth;
  int grid_h = static_cast<int>(opts.height) * kDensityHeight;
  std::vector<int> counts(grid_w * grid_h, 0);

  for (const auto& p : pts) {
    int gx = static_cast<int>((p.x - x_min) / x_range * grid_w);
    int gy = static_cast<int>((y_max - p.y) / y_range * grid_h);  // Flip Y
    gx = std::clamp(gx, 0, grid_w - 1);
    gy = std::clamp(gy, 0, grid_h - 1);
    ++counts[gy * grid_w + gx];
  }

  // Color interpolation in LAB space for perceptually uniform gradients
  auto lerpColor = [](const RGB& a, const RGB& b, double t) -> RGB {
    return lerpLab(a, b, t);
  };

  RGB bg = opts.bg_color.value_or(opts.low_color);

  std::string result;

  // Border top
  if (opts.show_border) {
    result += "┌";
    if (!opts.title.empty()) {
      size_t title_len = opts.title.size();
      size_t pad_left = (opts.width - title_len) / 2;
      size_t pad_right = opts.width - title_len - pad_left;
      for (size_t i = 0; i < pad_left; ++i) result += "―";
      result += opts.title;
      for (size_t i = 0; i < pad_right; ++i) result += "―";
    } else {
      for (int64_t i = 0; i < opts.width; ++i) result += "―";
    }
    result += "┐\n";
  }

  // Get the character library and scorer
  const auto& lib = getCharacterLibrary();
  auto scorer = scorerVarianceAware(0.5);

  // Cell result: stores chosen shape + raw fg/bg densities
  struct CellResult {
    const char* utf8;
    const FillPattern* pattern;
    bool inverted;
    double raw_fg;  // Raw average density in foreground region
    double raw_bg;  // Raw average density in background region
  };
  std::vector<CellResult> cells(opts.width * opts.height);

  // Gaussian kernel for smoothing (3x3)
  constexpr std::array<std::array<double, 3>, 3> kKernel = {{
      {{1.0 / 16, 2.0 / 16, 1.0 / 16}},
      {{2.0 / 16, 4.0 / 16, 2.0 / 16}},
      {{1.0 / 16, 2.0 / 16, 1.0 / 16}}}};

  // =========================================================================
  // PASS 1: Pick best shape for each cell (using locally-normalized density)
  // =========================================================================
  for (int64_t row = 0; row < opts.height; ++row) {
    for (int64_t col = 0; col < opts.width; ++col) {
      // Build DensityGrid with Gaussian smoothing (raw values)
      DensityGrid raw_grid{};
      double cell_max = 0.0;

      for (int dy = 0; dy < kDensityHeight; ++dy) {
        for (int dx = 0; dx < kDensityWidth; ++dx) {
          double sum = 0.0;
          for (int ky = -1; ky <= 1; ++ky) {
            for (int kx = -1; kx <= 1; ++kx) {
              int gx = static_cast<int>(col) * kDensityWidth + dx + kx;
              int gy = static_cast<int>(row) * kDensityHeight + dy + ky;
              if (gx >= 0 && gx < grid_w && gy >= 0 && gy < grid_h) {
                sum += counts[gy * grid_w + gx] * kKernel[ky + 1][kx + 1];
              }
            }
          }
          raw_grid[densityIndex(dx, dy)] = sum;
          cell_max = std::max(cell_max, sum);
        }
      }

      auto& cell = cells[row * opts.width + col];

      if (cell_max < 0.001) {
        // Empty cell
        cell = {" ", nullptr, false, 0.0, 0.0};
      } else {
        // Normalize for shape selection
        DensityGrid norm_grid = raw_grid;
        for (auto& v : norm_grid) {
          v /= cell_max;
        }

        // Run scorer to pick best shape
        auto res = scorer(norm_grid, lib);

        // Find the pattern for the chosen character
        const FillPattern* pattern = nullptr;
        for (const auto& ch : lib) {
          if (ch.utf8 == res.utf8) {
            pattern = &ch.pattern;
            break;
          }
        }

        cell.utf8 = res.utf8;
        cell.pattern = pattern;
        cell.inverted = res.inverted;

        // Compute raw average density in fg/bg using original (un-normalized) grid
        // Downsample raw_grid to pattern resolution
        auto raw_pattern = downsampleToPattern(raw_grid);

        double fg_sum = 0.0, bg_sum = 0.0;
        int fg_count = 0, bg_count = 0;
        if (pattern) {
          for (int i = 0; i < kTotalSubpixels; ++i) {
            bool is_fg = res.inverted ? !(*pattern)[i] : (*pattern)[i];
            if (is_fg) {
              fg_sum += raw_pattern[i];
              ++fg_count;
            } else {
              bg_sum += raw_pattern[i];
              ++bg_count;
            }
          }
        }
        cell.raw_fg = fg_count > 0 ? fg_sum / fg_count : 0.0;
        cell.raw_bg = bg_count > 0 ? bg_sum / bg_count : 0.0;
      }
    }
  }

  // =========================================================================
  // PASS 2: Find global min/max density
  // =========================================================================
  double global_min = std::numeric_limits<double>::max();
  double global_max = 0.0;

  for (const auto& cell : cells) {
    if (cell.utf8[0] != ' ') {
      global_min = std::min(global_min, std::min(cell.raw_fg, cell.raw_bg));
      global_max = std::max(global_max, std::max(cell.raw_fg, cell.raw_bg));
    }
  }

  // Handle edge case: all empty
  if (global_max < 0.001) {
    global_min = 0.0;
    global_max = 1.0;
  }

  double global_range = global_max - global_min;
  if (global_range < 0.001) global_range = 1.0;

  // =========================================================================
  // PASS 3: Render with colors based on global density range
  // =========================================================================
  for (int64_t row = 0; row < opts.height; ++row) {
    if (opts.show_border) result += "│";

    for (int64_t col = 0; col < opts.width; ++col) {
      const auto& cell = cells[row * opts.width + col];

      // Map raw density to [0,1] using global range
      double fg_t = (cell.raw_fg - global_min) / global_range;
      double bg_t = (cell.raw_bg - global_min) / global_range;
      fg_t = std::clamp(fg_t, 0.0, 1.0);
      bg_t = std::clamp(bg_t, 0.0, 1.0);

      RGB fg_color = lerpColor(bg, opts.high_color, fg_t);
      RGB bg_color_cell = lerpColor(bg, opts.high_color, bg_t);

      if (cell.utf8[0] == ' ') {
        result += bg_color_cell.ansiBgPrefix();
        result += " ";
        result += RGB::ansiSuffix();
      } else {
        result += fg_color.wrapFgBg(cell.utf8, bg_color_cell);
      }
    }

    if (opts.show_border) result += "│";
    result += "\n";
  }

  // Border bottom
  if (opts.show_border) {
    result += "└";
    for (int64_t i = 0; i < opts.width; ++i) result += "―";
    result += "┘\n";
  }

  return result;
}

struct MosaicOptions {
  int64_t width = 48;
  int64_t height = 22;
  RGB color{120, 220, 255};    // shape / foreground
  RGB background{18, 18, 26};   // canvas the edges anti-alias against
  double continuity = 8.0;      // weight of neighbor-edge continuity vs density fit
  bool show_border = true;
  std::string title;
};

// Render a coverage field cover(nx, ny) ∈ [0,1] over a character canvas (nx, ny
// are normalized [0,1] canvas coordinates) as mosaic glyphs with anti-aliased
// fg/bg color. Glyph selection is CONTINUITY-AWARE: cells are chosen greedily
// left→right, top→bottom, and each prefers a glyph whose edge subpixels line up
// with its already-chosen left/top neighbor — so a diagonal tiles into a smooth
// line instead of the lumps a per-cell argmax leaves where it tips between wedge
// families. `continuity` trades that smoothness against raw density fit.
template <typename Cover>
auto drawMosaic(Cover&& cover, const MosaicOptions& opts) -> std::string {
  if (opts.width <= 0 || opts.height <= 0) return "";
  const auto& lib = getCharacterLibrary();
  const int64_t W = opts.width, H = opts.height;
  const auto cell = [W](int64_t cx, int64_t cy) { return static_cast<size_t>(cy * W + cx); };

  std::vector<const BlockChar*> chosen(static_cast<size_t>(W * H), &lib[0]);
  std::vector<double> fg_int(static_cast<size_t>(W * H), 0.0);
  std::vector<double> bg_int(static_cast<size_t>(W * H), 0.0);

  for (int64_t cy = 0; cy < H; ++cy) {
    for (int64_t cx = 0; cx < W; ++cx) {
      std::array<double, kTotalSubpixels> target{};
      for (int py = 0; py < kSubpixelHeight; ++py) {
        const double ny = (static_cast<double>(cy) + (py + 0.5) / kSubpixelHeight) / static_cast<double>(H);
        for (int px = 0; px < kSubpixelWidth; ++px) {
          const double nx = (static_cast<double>(cx) + (px + 0.5) / kSubpixelWidth) / static_cast<double>(W);
          target[static_cast<size_t>(subpixelIndex(px, py))] = std::clamp(cover(nx, ny), 0.0, 1.0);
        }
      }

      const BlockChar* left = (cx > 0) ? chosen[cell(cx - 1, cy)] : nullptr;
      const BlockChar* top = (cy > 0) ? chosen[cell(cx, cy - 1)] : nullptr;

      const BlockChar* best = &lib[0];
      double best_score = -1e18, best_fg = 0.0, best_bg = 0.0;
      for (const auto& ch : lib) {
        double fsum = 0.0, bsum = 0.0;
        int fcount = 0, bcount = 0;
        for (int i = 0; i < kTotalSubpixels; ++i) {
          if (ch.pattern[static_cast<size_t>(i)]) { fsum += target[static_cast<size_t>(i)]; ++fcount; }
          else { bsum += target[static_cast<size_t>(i)]; ++bcount; }
        }
        const double fg = fcount ? fsum / fcount : 0.0;  // for AA color only
        const double bg = bcount ? bsum / bcount : 0.0;
        // Select by binary shape match. Using the fg/bg means here instead would
        // make every glyph tie (error 0) on a uniform cell — then continuity has
        // nothing to push against and propagates fill into empty cells.
        double score = 0.0;
        for (int i = 0; i < kTotalSubpixels; ++i) {
          const double err = target[static_cast<size_t>(i)] - (ch.pattern[static_cast<size_t>(i)] ? 1.0 : 0.0);
          score -= err * err;
        }
        // Reward edge agreement with the already-placed neighbors.
        if (left != nullptr) {
          int match = 0;
          for (int y = 0; y < kSubpixelHeight; ++y) {
            match += left->pattern[static_cast<size_t>(subpixelIndex(kSubpixelWidth - 1, y))]
                  == ch.pattern[static_cast<size_t>(subpixelIndex(0, y))];
          }
          score += opts.continuity * (static_cast<double>(match) / kSubpixelHeight);
        }
        if (top != nullptr) {
          int match = 0;
          for (int x = 0; x < kSubpixelWidth; ++x) {
            match += top->pattern[static_cast<size_t>(subpixelIndex(x, kSubpixelHeight - 1))]
                  == ch.pattern[static_cast<size_t>(subpixelIndex(x, 0))];
          }
          score += opts.continuity * (static_cast<double>(match) / kSubpixelWidth);
        }
        if (score > best_score) { best_score = score; best = &ch; best_fg = fg; best_bg = bg; }
      }
      chosen[cell(cx, cy)] = best;
      fg_int[cell(cx, cy)] = best_fg;
      bg_int[cell(cx, cy)] = best_bg;
    }
  }

  std::string out;
  if (opts.show_border) frameTop(out, W, opts.title);
  for (int64_t cy = 0; cy < H; ++cy) {
    if (opts.show_border) out += "│";
    for (int64_t cx = 0; cx < W; ++cx) {
      const RGB fg = lerpColor(opts.background, opts.color, std::clamp(fg_int[cell(cx, cy)], 0.0, 1.0));
      const RGB bg = lerpColor(opts.background, opts.color, std::clamp(bg_int[cell(cx, cy)], 0.0, 1.0));
      out += fg.wrapFgBg(chosen[cell(cx, cy)]->utf8, bg);
    }
    if (opts.show_border) out += "│";
    out += "\n";
  }
  if (opts.show_border) frameBottom(out, W);
  return out;
}

}  // namespace tempura
