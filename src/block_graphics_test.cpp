#include "block_graphics.h"

#include <string_view>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "character library + best-fit matching"_test = [] {
    const auto& lib = getCharacterLibrary();
    expectTrue(lib.size() > 10);
    // The two anchor glyphs must round-trip exactly through the scorer.
    expectEq(std::string_view{findBestChar(emptyPattern()).utf8}, std::string_view{" "});
    expectEq(std::string_view{findBestChar(fullPattern()).utf8}, std::string_view{"█"});
    // A left-half target should pick a left-weighted glyph, not the full block.
    const auto& left = findBestChar(fillRect(0.0, 0.0, 0.5, 1.0));
    expectTrue(left.fill_fraction > 0.3 && left.fill_fraction < 0.7);
  };

  "shape fills rasterize into the 8×24 subpixel grid"_test = [] {
    expectEq(fillRect(0.0, 0.0, 1.0, 1.0).count(), size_t{kTotalSubpixels});  // whole cell
    expectEq(fillRect(0.0, 0.0, 0.5, 1.0).count(), size_t{96});              // left 4 of 8 cols
    const auto disc = fillCircle(0.5, 0.5, 0.3);
    expectTrue(disc.test(static_cast<size_t>(subpixelIndex(4, 12))));  // center filled
    expectTrue(!disc.test(static_cast<size_t>(subpixelIndex(0, 0))));  // corner empty
  };

  "LAB round-trips through sRGB (perceptual color path)"_test = [] {
    for (const RGB c : {RGB{0, 0, 0}, RGB{255, 255, 255}, RGB{255, 0, 0},
                        RGB{100, 150, 200}, RGB{30, 30, 80}}) {
      const RGB back = labToRgb(rgbToLab(c));
      expectTrue(std::abs(int{back.r} - int{c.r}) <= 2);
      expectTrue(std::abs(int{back.g} - int{c.g}) <= 2);
      expectTrue(std::abs(int{back.b} - int{c.b}) <= 2);
    }
    // A LAB-space midpoint of black→white is a balanced mid-gray.
    const RGB mid = lerpLab(RGB{0, 0, 0}, RGB{255, 255, 255}, 0.5);
    expectEq(mid.r, mid.g);
    expectEq(mid.g, mid.b);
    expectTrue(mid.r > 100 && mid.r < 140);
  };

  "hiresHeatmap renders a point cloud"_test = [] {
    std::vector<Point> pts;
    for (int i = 0; i < 300; ++i) {
      const double t = i * 0.05;
      pts.push_back({std::cos(t), std::sin(t)});
    }
    const auto out = hiresHeatmap(pts, {.width = 30, .height = 12});
    expectTrue(!out.empty());
    expectEq(hiresHeatmap(std::vector<Point>{}), std::string{});
  };

  "drawMosaic renders a coverage field"_test = [] {
    // A constant-1 field fills every cell with the full block; constant-0 leaves
    // none (the binary shape-match selects ' ' there, so continuity can't leak).
    const auto full = drawMosaic([](double, double) { return 1.0; },
                                 {.width = 8, .height = 4, .show_border = false});
    const auto empty = drawMosaic([](double, double) { return 0.0; },
                                  {.width = 8, .height = 4, .show_border = false});
    expectTrue(full.find("█") != std::string::npos);
    expectTrue(empty.find("█") == std::string::npos);
    expectTrue(!drawMosaic([](double x, double) { return x < 0.5 ? 1.0 : 0.0; },
                           {.width = 12, .height = 6}).empty());
  };

  return TestRegistry::result();
}
