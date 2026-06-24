// Visual spotcheck for the mosaic drawing lib. Renders shapes by rasterizing
// them into per-cell sub-pixel patterns and letting findBestChar pick the
// closest Unicode mosaic glyph — so edges come out smooth, not blocky — plus the
// LAB-color hiresHeatmap. Build with -DTEMPURA_BUILD_DEMOS=ON, run ./block_graphics_demo.

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

// Rasterize an implicit shape over a w×h character canvas with anti-aliasing.
// Per cell: sample the predicate across the fine 32×96 density grid, let the
// variance-aware scorer pick the glyph whose filled region best *aligns* with
// the shape's edge (its variance penalty is the anti-jaggedness term), then
// render fg/bg so partial-coverage cells blend shape-color into the background.
// Binary point-sampling + the scorer's 4×4 downsample gives 17 levels of edge AA.
template <typename Inside>
auto drawShape(int w, int h, Inside inside, RGB color, std::string_view title) -> std::string {
  const auto& lib = getCharacterLibrary();
  const auto scorer = scorerVarianceAware(0.5);
  const RGB canvas{18, 18, 26};  // dark background the edges anti-alias against

  std::string out;
  frameTop(out, w, title);
  for (int cy = 0; cy < h; ++cy) {
    out += "│";
    for (int cx = 0; cx < w; ++cx) {
      DensityGrid grid{};
      for (int dy = 0; dy < kDensityHeight; ++dy) {
        const double ny = (cy + (dy + 0.5) / kDensityHeight) / h;
        for (int dx = 0; dx < kDensityWidth; ++dx) {
          const double nx = (cx + (dx + 0.5) / kDensityWidth) / w;
          grid[static_cast<size_t>(densityIndex(dx, dy))] = inside(nx, ny) ? 1.0 : 0.0;
        }
      }
      const auto res = scorer(grid, lib);
      const RGB fg = lerpColor(canvas, color, std::clamp(res.fg_intensity, 0.0, 1.0));
      const RGB bg = lerpColor(canvas, color, std::clamp(res.bg_intensity, 0.0, 1.0));
      out += fg.wrapFgBg(res.utf8, bg);
    }
    out += "│\n";
  }
  frameBottom(out, w);
  return out;
}

}  // namespace

auto main() -> int {
  constexpr double kPi = std::numbers::pi;
  const int W = 48, H = 22;
  // Terminal cells are ~2:1 (h:w); scale y by 2 so shapes aren't squashed.
  const double ax = W, ay = H * 2.0;

  std::println("\033[1mMosaic drawing — shapes rendered with best-fit Unicode glyphs\033[0m");
  std::println("The library has {} glyphs (sextants, eighths, diagonal wedges, …).",
               getCharacterLibrary().size());

  header("a disc — the diagonal-wedge glyphs smooth the curved edge");
  std::print("{}", drawShape(W, H, [=](double x, double y) {
    const double dx = (x - 0.5) * ax, dy = (y - 0.5) * ay;
    return dx * dx + dy * dy <= std::pow(std::min(ax, ay) * 0.45, 2);
  }, colors::kCyan, "x² + y² ≤ r²"));

  header("a filled triangle — straight diagonal edges");
  std::print("{}", drawShape(W, H, [](double x, double y) {
    return y >= 0.12 && y <= 0.88 && std::abs(x - 0.5) <= (y - 0.12) * 0.62;
  }, colors::kGreen, "△"));

  header("a sine band — a thick curve tracked smoothly");
  std::print("{}", drawShape(W, H, [=](double x, double y) {
    const double s = 0.5 - 0.34 * std::sin(x * 2.0 * kPi * 1.5);
    return std::abs(y - s) < 0.09;
  }, colors::kYellow, "y = sin(x)"));

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
