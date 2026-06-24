// Visual spotcheck for the mosaic drawing lib. drawMosaic renders implicit shapes
// (coverage fields) with continuity-aware best-fit glyphs + anti-aliased fg/bg
// color, so curved and diagonal edges come out smooth. Then the LAB hiresHeatmap.
// Build with -DTEMPURA_BUILD_DEMOS=ON, run ./block_graphics_demo.

#include <cmath>
#include <numbers>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "block_graphics.h"

using namespace tempura;

namespace {

void header(std::string_view title) {
  const int dashes = std::max(2, 64 - static_cast<int>(title.size()));
  std::println("\n\033[1m── {} {}\033[0m", title, std::string(static_cast<size_t>(dashes), '-'));
}

}  // namespace

auto main() -> int {
  constexpr double kPi = std::numbers::pi;
  const int W = 48, H = 22;
  const double ax = W, ay = H * 2.0;  // cells are ~2:1; scale y so shapes aren't squashed

  std::println("\033[1mMosaic drawing — implicit shapes via continuity-aware best-fit glyphs\033[0m");
  std::println("The library has {} glyphs (sextants, eighths, diagonal wedges, …).",
               getCharacterLibrary().size());

  header("a disc — diagonal-wedge glyphs + fg/bg color smooth the curved edge");
  std::print("{}", drawMosaic(
      [=](double x, double y) -> double {
        const double dx = (x - 0.5) * ax, dy = (y - 0.5) * ay;
        return dx * dx + dy * dy <= std::pow(std::min(ax, ay) * 0.45, 2) ? 1.0 : 0.0;
      },
      {.width = W, .height = H, .color = colors::kCyan, .title = "x² + y² ≤ r²"}));

  header("a filled triangle — straight diagonal edges");
  std::print("{}", drawMosaic(
      [](double x, double y) -> double {
        return (y >= 0.12 && y <= 0.88 && std::abs(x - 0.5) <= (y - 0.12) * 0.62) ? 1.0 : 0.0;
      },
      {.width = W, .height = H, .color = colors::kGreen, .title = "△"}));

  header("a sine band — a thick curve tracked smoothly");
  std::print("{}", drawMosaic(
      [=](double x, double y) -> double {
        const double s = 0.5 - 0.34 * std::sin(x * 2.0 * kPi * 1.5);
        return std::abs(y - s) < 0.09 ? 1.0 : 0.0;
      },
      {.width = W, .height = H, .color = colors::kYellow, .title = "y = sin(x)"}));

  header("hiresHeatmap — two Gaussian blobs in perceptual LAB color");
  {
    std::mt19937_64 gen{0xB10C};
    std::normal_distribution<double> a{-1.6, 0.7}, b{1.6, 1.0};
    std::vector<Point> pts;
    for (int i = 0; i < 8000; ++i) pts.push_back({a(gen), a(gen)});
    for (int i = 0; i < 8000; ++i) pts.push_back({b(gen), b(gen)});
    std::print("{}", hiresHeatmap(pts, {.width = 50, .height = 24, .title = "two blobs (LAB)"}));
  }

  header("hiresHeatmap — a spiral");
  {
    std::vector<Point> pts;
    for (int i = 0; i < 6000; ++i) {
      const double t = i * 0.01;
      const double r = t * 0.06;
      pts.push_back({r * std::cos(t), r * std::sin(t)});
    }
    std::print("{}", hiresHeatmap(pts, {.width = 50, .height = 24, .title = "spiral"}));
  }

  std::println("");
  return 0;
}
