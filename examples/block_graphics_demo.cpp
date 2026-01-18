#include <array>
#include <cmath>
#include <iostream>
#include <print>
#include <random>
#include <vector>

#include "block_graphics.h"
#include "bayes/estimation/mcmc.h"

using namespace tempura;
using namespace tempura::bayes;
using namespace tempura::block_gfx;

// Print a FillPattern as ASCII art (8×24 grid)
void printPattern(const FillPattern& pattern, const char* label) {
  std::println("{}:", label);
  for (int y = 0; y < kSubpixelHeight; ++y) {
    std::print("  ");
    for (int x = 0; x < kSubpixelWidth; ++x) {
      std::print("{}", pattern[subpixelIndex(x, y)] ? "█" : "·");
    }
    std::println("");
  }
}

int main() {
  std::println("╔══════════════════════════════════════════════════════════════════╗");
  std::println("║          Block Graphics - Symbol Pattern Demo                    ║");
  std::println("╚══════════════════════════════════════════════════════════════════╝\n");

  // ============================================================================
  // Show some character patterns
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Sample Character Patterns (8×24 subpixel grid)");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  const auto& lib = getCharacterLibrary();

  // Show a few interesting characters (find by utf8 string)
  std::vector<std::pair<std::string, int>> samples;
  for (size_t i = 0; i < lib.size(); ++i) {
    std::string_view utf8 = lib[i].utf8;
    if (utf8 == "█") samples.push_back({"Full block █", static_cast<int>(i)});
    if (utf8 == "🬀") samples.push_back({"Sextant-1 🬀", static_cast<int>(i)});
    if (utf8 == "🬂") samples.push_back({"Sextant-12 🬂", static_cast<int>(i)});
    if (utf8 == "🬋") samples.push_back({"Sextant-34 🬋", static_cast<int>(i)});
    if (utf8 == "🭀") samples.push_back({"Mosaic 🭀", static_cast<int>(i)});
    if (utf8 == "🭑") samples.push_back({"Mosaic 🭑", static_cast<int>(i)});
    if (utf8 == "🭆") samples.push_back({"Mosaic 🭆", static_cast<int>(i)});
    if (utf8 == "𜴀") samples.push_back({"Octant 𜴀", static_cast<int>(i)});
    if (utf8 == "𜺠") samples.push_back({"Sixteenth 𜺠", static_cast<int>(i)});
  }

  for (const auto& [name, idx] : samples) {
    const auto& c = lib[idx];
    std::println("{} (fill: {:.1f}%)", name, c.fill_fraction * 100);
    printPattern(c.pattern, c.utf8);
    std::println("");
  }

  // ============================================================================
  // Demonstrate pattern matching
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Pattern Matching Demo");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  // Create a test pattern: bottom-left triangle
  std::println("Target: Bottom-left triangle");
  auto target1 = makePattern([](int x, int y) {
    double px = static_cast<double>(x) / kSubpixelWidth;
    double py = static_cast<double>(y) / kSubpixelHeight;
    return py > px;  // Below the diagonal
  });
  printPattern(target1, "Target pattern");

  const auto& best1 = findBestChar(target1);
  std::println("\nBest match: {} (fill: {:.1f}%)", best1.utf8, best1.fill_fraction * 100);
  printPattern(best1.pattern, "Matched pattern");
  std::println("");

  // Another test: top-right corner
  std::println("Target: Top-right quarter");
  auto target2 = makePattern([](int x, int y) {
    return x >= kSubpixelWidth / 2 && y < kSubpixelHeight / 3;
  });
  printPattern(target2, "Target pattern");

  const auto& best2 = findBestChar(target2);
  std::println("\nBest match: {} (fill: {:.1f}%)", best2.utf8, best2.fill_fraction * 100);
  printPattern(best2.pattern, "Matched pattern");
  std::println("");

  // ============================================================================
  // Show all diagonal characters
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Diagonal Characters Gallery");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  // Find diagonal chars (they're between sextants and standard blocks)
  std::println("Smooth diagonal characters:");
  for (size_t i = 0; i < lib.size(); ++i) {
    std::string_view utf8 = lib[i].utf8;
    // Diagonal chars are 🬼 through 🭯 (U+1FB3C - U+1FB6F)
    if (utf8.size() >= 4) {
      unsigned char b0 = static_cast<unsigned char>(utf8[0]);
      unsigned char b1 = static_cast<unsigned char>(utf8[1]);
      unsigned char b2 = static_cast<unsigned char>(utf8[2]);
      unsigned char b3 = static_cast<unsigned char>(utf8[3]);
      // Check if it's in the diagonal range (crude check)
      if (b0 == 0xF0 && b1 == 0x9F && b2 == 0xAD) {
        std::print("{} ", utf8);
      }
    }
  }
  std::println("\n");

  // Query terminal background for seamless blending
  auto terminal_bg = queryTerminalBackground();
  if (terminal_bg) {
    std::println("Detected terminal background: RGB({}, {}, {})",
                 terminal_bg->r, terminal_bg->g, terminal_bg->b);
  } else {
    std::println("Could not detect terminal background, using default");
    terminal_bg = RGB{30, 30, 30};  // Assume dark terminal
  }
  std::println("");

  // ============================================================================
  // Sine Waves of Increasing Frequency
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Sine Waves of Increasing Frequency");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  {
    std::vector<Point> sine_pts;
    constexpr double kXRange = 4.0 * std::numbers::pi;
    constexpr int kPointsPerWave = 2000;

    // Plot multiple sine waves with increasing frequency
    for (int wave = 1; wave <= 8; ++wave) {
      double freq = wave;
      double y_offset = 9 - wave;  // Stack waves vertically
      for (int i = 0; i < kPointsPerWave; ++i) {
        double x = kXRange * i / (kPointsPerWave - 1);
        double y = y_offset + 0.4 * std::sin(freq * x);
        // Add multiple points for thickness
        for (int t = 0; t < 50; ++t) {
          sine_pts.push_back({x, y});
        }
      }
    }

    std::println("Sine waves (frequencies 1-8, {} points):\n", sine_pts.size());

    HiresHeatmapOptions sine_opts{
        .width = 70,
        .height = 20,
        .title = " Sine Waves (f = 1, 2, 3, ... 8) ",
        .low_color = *terminal_bg,
        .high_color = {50, 220, 100},
        .bg_color = terminal_bg,
    };

    std::cout << hiresHeatmap(sine_pts, sine_opts);
    std::println("");
  }

  // ============================================================================
  // Smooth Analytical Heatmap (no sampling noise)
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Smooth Analytical Heatmap (2D Gaussian)");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  // Generate points from smooth 2D Gaussian density
  // More points where density is higher
  {
    std::vector<Point> smooth_pts;
    auto gaussian_2d = [](double x, double y) {
      return std::exp(-0.5 * (x * x + y * y));
    };

    // Sample proportional to density on a fine grid
    constexpr int kGridRes = 300;
    constexpr double kRange = 3.0;
    for (int iy = 0; iy < kGridRes; ++iy) {
      for (int ix = 0; ix < kGridRes; ++ix) {
        double x = -kRange + 2.0 * kRange * ix / (kGridRes - 1);
        double y = -kRange + 2.0 * kRange * iy / (kGridRes - 1);
        double density = gaussian_2d(x, y);
        // Add points proportional to density (scaled up)
        int n_pts = static_cast<int>(density * 200);
        for (int k = 0; k < n_pts; ++k) {
          smooth_pts.push_back({x, y});
        }
      }
    }

    std::println("Smooth 2D Gaussian ({} points):\n", smooth_pts.size());

    HiresHeatmapOptions smooth_opts{
        .width = 50,
        .height = 25,
        .title = " Smooth 2D Gaussian ",
        .low_color = *terminal_bg,
        .high_color = {100, 200, 255},
        .bg_color = terminal_bg,
    };

    std::cout << hiresHeatmap(smooth_pts, smooth_opts);
    std::println("");
  }

  // ============================================================================
  // Test with MCMC data
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("MCMC Heatmap with Block Graphics");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  // Bivariate normal
  constexpr double kMu1 = 0.0, kMu2 = 0.0;
  constexpr double kS1 = 1.0, kS2 = 1.0;
  constexpr double kRho = 0.7;

  auto bivariate_log_prob = [](const std::array<double, 2>& p) {
    double x = p[0], y = p[1];
    double z1 = (x - kMu1) / kS1;
    double z2 = (y - kMu2) / kS2;
    double q = (z1 * z1 - 2 * kRho * z1 * z2 + z2 * z2) / (1 - kRho * kRho);
    return -0.5 * q;
  };

  auto mcmc = chain<std::array<double, 2>>(
      bivariate_log_prob, GaussianWalkND<double, 2>{0.5}, std::mt19937{456},
      std::array{0.0, 0.0});

  std::vector<Point> samples_2d;
  int i = 0;
  for (const auto& s : mcmc | std::views::take(5000 + 100000)) {
    if (i >= 5000) {
      samples_2d.push_back({s.value[0], s.value[1]});
    }
    ++i;
  }

  std::println("Bivariate Normal (ρ = {}) - High-res block graphics:\n", kRho);

  HiresHeatmapOptions opts{
      .width = 50,
      .height = 18,
      .title = " Bivariate Normal ",
      .low_color = *terminal_bg,  // Start from terminal background
      .high_color = {255, 200, 50},
      .bg_color = terminal_bg,
  };

  std::cout << hiresHeatmap(samples_2d, opts);
  std::println("");

  // Banana distribution
  std::println("Banana Distribution - High-res block graphics:\n");

  auto banana_log_prob = [](const std::array<double, 2>& p) {
    double x = p[0], y = p[1];
    return -x * x / 200.0 - std::pow(y - x * x / 10.0, 2) / 2.0;
  };

  auto banana_chain = chain<std::array<double, 2>>(
      banana_log_prob, GaussianWalkND<double, 2>{2.0}, std::mt19937{2024},
      std::array{0.0, 0.0});

  std::vector<Point> banana_samples;
  for (const auto& s :
       banana_chain | std::views::drop(5000) | std::views::take(100000)) {
    banana_samples.push_back({s.value[0], s.value[1]});
  }

  HiresHeatmapOptions banana_opts{
      .width = 60,
      .height = 18,
      .title = " Banana Distribution ",
      .low_color = *terminal_bg,  // Start from terminal background
      .high_color = {255, 140, 30},
      .bg_color = terminal_bg,
  };

  std::cout << hiresHeatmap(banana_samples, banana_opts);
  std::println("");

  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Character library size: {} symbols", lib.size());
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  return 0;
}
