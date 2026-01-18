// Banana Curve Comparison Demo
// Shows the same banana-shaped distribution rendered with different character sets and scorers

#include <cmath>
#include <functional>
#include <numbers>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "block_gfx_patterns.h"
#include "block_gfx_scorer.h"
#include "block_graphics.h"
#include "plot.h"

using namespace tempura;
using namespace tempura::block_gfx;

// Generate banana distribution density analytically
// log_prob = -x²/200 - (y - x²/10)² / 2
auto generateBananaDensity(int width, int height) -> std::vector<DensityGrid> {
  std::vector<DensityGrid> grids(width * height);

  double x_min = -15.0, x_max = 15.0;
  double y_min = -5.0, y_max = 25.0;
  double data_width = x_max - x_min;
  double data_height = y_max - y_min;

  // First pass: find max density
  double max_log_prob = -1e9;
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      for (int dy = 0; dy < kDensityHeight; ++dy) {
        for (int dx = 0; dx < kDensityWidth; ++dx) {
          double cell_x = col + static_cast<double>(dx) / kDensityWidth;
          double cell_y = row + static_cast<double>(dy) / kDensityHeight;
          double x = x_min + (cell_x / width) * data_width;
          double y = y_max - (cell_y / height) * data_height;
          double log_prob = -x * x / 200.0 - std::pow(y - x * x / 10.0, 2) / 2.0;
          max_log_prob = std::max(max_log_prob, log_prob);
        }
      }
    }
  }

  // Second pass: compute normalized densities
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      DensityGrid& grid = grids[row * width + col];
      for (int dy = 0; dy < kDensityHeight; ++dy) {
        for (int dx = 0; dx < kDensityWidth; ++dx) {
          double cell_x = col + static_cast<double>(dx) / kDensityWidth;
          double cell_y = row + static_cast<double>(dy) / kDensityHeight;
          double x = x_min + (cell_x / width) * data_width;
          double y = y_max - (cell_y / height) * data_height;
          double log_prob = -x * x / 200.0 - std::pow(y - x * x / 10.0, 2) / 2.0;
          grid[densityIndex(dx, dy)] = std::exp(log_prob - max_log_prob);
        }
      }
    }
  }

  return grids;
}

// Generate sine waves of increasing frequency
auto generateSineWavesDensity(int width, int height) -> std::vector<DensityGrid> {
  std::vector<DensityGrid> grids(width * height);

  constexpr double kXRange = 4.0 * std::numbers::pi;
  constexpr int kNumWaves = 8;
  constexpr double kLineWidth = 0.15;

  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      DensityGrid& grid = grids[row * width + col];
      for (int dy = 0; dy < kDensityHeight; ++dy) {
        for (int dx = 0; dx < kDensityWidth; ++dx) {
          double cell_x = col + static_cast<double>(dx) / kDensityWidth;
          double cell_y = row + static_cast<double>(dy) / kDensityHeight;

          // Map to data coordinates
          double x = kXRange * cell_x / width;
          double y = kNumWaves + 1 - (kNumWaves + 2) * cell_y / height;

          // Check distance to each sine wave
          double max_density = 0.0;
          for (int wave = 1; wave <= kNumWaves; ++wave) {
            double freq = wave;
            double wave_y = wave + 0.4 * std::sin(freq * x);
            double dist = std::abs(y - wave_y);
            // Smooth falloff from line center
            double density = std::exp(-dist * dist / (2.0 * kLineWidth * kLineWidth));
            max_density = std::max(max_density, density);
          }

          grid[densityIndex(dx, dy)] = max_density;
        }
      }
    }
  }

  return grids;
}

// Build filtered character library based on codepoint filter
auto buildFilteredLibrary(std::function<bool(uint32_t)> filter)
    -> std::vector<BlockChar> {
  std::vector<BlockChar> result;

  for (const auto& ch : getCharacterLibrary()) {
    const auto* utf8 = reinterpret_cast<const unsigned char*>(ch.utf8);
    uint32_t cp = 0;

    if ((utf8[0] & 0x80) == 0) {
      cp = utf8[0];
    } else if ((utf8[0] & 0xE0) == 0xC0) {
      cp = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
    } else if ((utf8[0] & 0xF0) == 0xE0) {
      cp = ((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
    } else if ((utf8[0] & 0xF8) == 0xF0) {
      cp = ((utf8[0] & 0x07) << 18) | ((utf8[1] & 0x3F) << 12) |
           ((utf8[2] & 0x3F) << 6) | (utf8[3] & 0x3F);
    }

    // Always include space and full block
    if (cp == ' ' || cp == 0x2588 || filter(cp)) {
      result.push_back(ch);
    }
  }

  return result;
}

// Render heatmap with custom library and scorer
auto renderHeatmap(const std::vector<DensityGrid>& grids, int width, int height,
                   const std::string& title, const ScorerFn& scorer,
                   const std::vector<BlockChar>& lib, const RGB& bg,
                   const RGB& high_color) -> std::string {
  // Color interpolation in LAB space for perceptually uniform gradients
  auto lerpColor = [](const RGB& a, const RGB& b, double t) -> RGB {
    t = std::clamp(t, 0.0, 1.0);
    return lab::lerpLab(a, b, t);
  };

  std::string result;

  // Border top with title
  result += "┌";
  size_t title_len = title.size();
  size_t pad_left = (width - title_len) / 2;
  size_t pad_right = width - title_len - pad_left;
  for (size_t i = 0; i < pad_left; ++i) result += "─";
  result += title;
  for (size_t i = 0; i < pad_right; ++i) result += "─";
  result += "┐\n";

  // Render cells
  for (int row = 0; row < height; ++row) {
    result += "│";
    for (int col = 0; col < width; ++col) {
      const DensityGrid& grid = grids[row * width + col];
      auto best = scorer(grid, lib);

      RGB fg_color = lerpColor(bg, high_color, best.fg_intensity);
      RGB bg_color = lerpColor(bg, high_color, best.bg_intensity);

      if (best.inverted) std::swap(fg_color, bg_color);

      if (best.utf8[0] == ' ') {
        result += bg_color.ansiBgPrefix();
        result += " ";
        result += RGB::ansiSuffix();
      } else {
        result += fg_color.wrapFgBg(best.utf8, bg_color);
      }
    }
    result += "│\n";
  }

  // Border bottom
  result += "└";
  for (int i = 0; i < width; ++i) result += "─";
  result += "┘\n";

  return result;
}

int main() {
  std::println("╔══════════════════════════════════════════════════════════════════╗");
  std::println("║           Banana Curve - Character Set Comparison                ║");
  std::println("╚══════════════════════════════════════════════════════════════════╝\n");

  // Query terminal background for seamless blending
  RGB bg = queryTerminalBackground().value_or(RGB{30, 30, 30});
  std::println("Terminal background: RGB({}, {}, {})\n", bg.r, bg.g, bg.b);

  constexpr int kWidth = 45;
  constexpr int kHeight = 18;

  std::println("Generating {}×{} banana distribution...\n", kWidth, kHeight);
  auto grids = generateBananaDensity(kWidth, kHeight);

  RGB high_color{100, 200, 255};

  // Character set filters
  auto blocks_only = [](uint32_t cp) {
    return (cp >= 0x2580 && cp <= 0x259F);
  };

  auto sextants = [](uint32_t cp) {
    return (cp >= kFirstSextant && cp <= kLastSextant) ||
           (cp >= 0x2580 && cp <= 0x259F);
  };

  auto smooth_mosaics = [](uint32_t cp) {
    return (cp >= kFirstSmoothMosaic && cp <= kLastSmoothMosaic) ||
           (cp >= 0x2580 && cp <= 0x259F);
  };

  auto octants = [](uint32_t cp) {
    return (cp >= kFirstOctant && cp <= kLastOctant) ||
           (cp >= 0x2580 && cp <= 0x259F);
  };

  auto sixteenths = [](uint32_t cp) {
    return (cp >= kFirstSixteenth && cp <= kLastSixteenth) ||
           (cp >= 0x2580 && cp <= 0x259F);
  };

  auto sext_and_mosaics = [](uint32_t cp) {
    return (cp >= kFirstSextant && cp <= kLastSextant) ||
           (cp >= kFirstSmoothMosaic && cp <= kLastSmoothMosaic) ||
           (cp >= 0x2580 && cp <= 0x259F);
  };

  auto all_chars = [](uint32_t) { return true; };

  // Build libraries
  auto lib_blocks = buildFilteredLibrary(blocks_only);
  auto lib_sextants = buildFilteredLibrary(sextants);
  auto lib_mosaics = buildFilteredLibrary(smooth_mosaics);
  auto lib_octants = buildFilteredLibrary(octants);
  auto lib_sixteenths = buildFilteredLibrary(sixteenths);
  auto lib_sext_mosaics = buildFilteredLibrary(sext_and_mosaics);
  auto lib_all = buildFilteredLibrary(all_chars);

  // Scorers
  auto least_sq = scorerLeastSquares();
  auto smooth = scorerSmooth(0.5);

  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("1. Basic Block Elements (U+2580-259F) - {} chars", lib_blocks.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Blocks Only", least_sq, lib_blocks, bg, high_color));

  std::println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("2. Sextants (U+1FB00-1FB3B) - 2×3 grid - {} chars", lib_sextants.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Sextants", least_sq, lib_sextants, bg, high_color));

  std::println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("3. Smooth Mosaics (U+1FB3C-1FB67) - diagonal wedges - {} chars", lib_mosaics.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Smooth Mosaics", least_sq, lib_mosaics, bg, high_color));

  std::println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("4. Octants (U+1CD00-1CDE5) - 2×4 grid (Unicode 16) - {} chars", lib_octants.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Octants", least_sq, lib_octants, bg, high_color));

  std::println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("5. Sixteenths (U+1CE90-1CEAF) - 4×4 grid (Unicode 16) - {} chars", lib_sixteenths.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Sixteenths", least_sq, lib_sixteenths, bg, high_color));

  std::println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("6. Sextants + Smooth Mosaics - {} chars", lib_sext_mosaics.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Sext+Mosaic LeastSq", least_sq, lib_sext_mosaics, bg, high_color));
  std::println("");
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Sext+Mosaic Smooth", smooth, lib_sext_mosaics, bg, high_color));

  std::println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("7. All Characters - {} chars", lib_all.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "All Chars LeastSq", least_sq, lib_all, bg, high_color));
  std::println("");
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "All Chars Smooth", smooth, lib_all, bg, high_color));

  // Scoring Algorithm Comparison
  std::println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("8. Scoring Algorithm Comparison (All {} Symbols)", lib_all.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  std::println("LEAST SQUARES: Minimize Σ(target[i] - displayed[i])²");
  std::println("  - Picks fg/bg as mean density of filled/unfilled regions");
  std::println("  - Score = -Σ(error²) where error = target - displayed");
  std::println("");
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Least Squares", least_sq, lib_all, bg, high_color));

  std::println("\nBETWEEN-GROUP VARIANCE (Otsu-like): Maximize separation");
  std::println("  - Score = ωF × ωU × (μF - μU)²");
  std::println("  - Picks shape that best separates high-density from low-density");
  std::println("  - Similar to Otsu's thresholding - maximize between-class variance");
  std::println("");
  auto between_var = scorerBetweenVariance();
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Between-Grp Var", between_var, lib_all, bg, high_color));

  std::println("\nCORRELATION: Maximize pattern-density correlation");
  std::println("  - Score = |correlation(pattern, density)|");
  std::println("  - Pattern treated as binary (1=filled, 0=unfilled)");
  std::println("");
  auto corr = scorerCorrelation();
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Correlation", corr, lib_all, bg, high_color));

  std::println("\nVARIANCE-AWARE: Penalize high within-group variance");
  std::println("  - Score = -Σ(error²) - λ × (within-group variance)");
  std::println("  - Good when shape boundary should align with density gradient");
  std::println("");
  auto var_aware = scorerVarianceAware(0.5);
  std::print("{}", renderHeatmap(grids, kWidth, kHeight, "Variance-Aware", var_aware, lib_all, bg, high_color));

  // =========================================================================
  // Sine Waves Comparison
  // =========================================================================
  std::println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("9. Sine Waves of Increasing Frequency (All {} Symbols)", lib_all.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  constexpr int kSineWidth = 70;
  constexpr int kSineHeight = 20;
  auto sine_grids = generateSineWavesDensity(kSineWidth, kSineHeight);
  RGB sine_color{50, 220, 100};

  std::println("LEAST SQUARES:");
  std::print("{}", renderHeatmap(sine_grids, kSineWidth, kSineHeight, "Sine - Least Squares", least_sq, lib_all, bg, sine_color));

  std::println("\nVARIANCE-AWARE:");
  std::print("{}", renderHeatmap(sine_grids, kSineWidth, kSineHeight, "Sine - Variance-Aware", var_aware, lib_all, bg, sine_color));

  std::println("\nBETWEEN-GROUP VARIANCE (Otsu):");
  std::print("{}", renderHeatmap(sine_grids, kSineWidth, kSineHeight, "Sine - Between-Grp Var", between_var, lib_all, bg, sine_color));

  std::println("\nSMOOTH (edge complexity penalty):");
  std::print("{}", renderHeatmap(sine_grids, kSineWidth, kSineHeight, "Sine - Smooth", smooth, lib_all, bg, sine_color));

  std::println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Summary:");
  std::println("  Blocks:         {:3} chars (basic half/quarter blocks)", lib_blocks.size());
  std::println("  Sextants:       {:3} chars (2×3 rectangular grid)", lib_sextants.size());
  std::println("  Smooth Mosaics: {:3} chars (diagonal wedges)", lib_mosaics.size());
  std::println("  Octants:        {:3} chars (2×4 grid, Unicode 16)", lib_octants.size());
  std::println("  Sixteenths:     {:3} chars (4×4 grid, Unicode 16)", lib_sixteenths.size());
  std::println("  All:            {:3} chars (full library)", lib_all.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  return 0;
}
