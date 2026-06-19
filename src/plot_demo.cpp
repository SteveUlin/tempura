// Visual spotcheck for every plot type in plot.h. Not a test — it renders to a
// real terminal, so eyeball the output rather than asserting on it. Build with
// -DTEMPURA_BUILD_DEMOS=ON and run ./plot_demo.

#include <cmath>
#include <numbers>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "plot.h"

using namespace tempura;

namespace {

void header(std::string_view title) {
  // title.size() counts bytes (an em-dash is 3); keep the subtraction signed.
  const int dashes = std::max(2, 64 - static_cast<int>(title.size()));
  std::println("\n\033[1m── {} {}\033[0m", title, std::string(static_cast<size_t>(dashes), '-'));
}

// A horizontal low→high color ramp, so a heatmap's gradient can be read.
auto colorbar(const RGB& lo, const RGB& hi, int64_t n = 32) -> std::string {
  std::string out = "low ";
  for (int64_t i = 0; i < n; ++i) {
    out += lerpColor(lo, hi, static_cast<double>(i) / (n - 1)).wrap("█");
  }
  out += " high";
  return out;
}

}  // namespace

auto main() -> int {
  constexpr double kPi = std::numbers::pi;

  header("plotFn — damped sine (single function, Braille + zero axis)");
  std::print("{}", plotFn([](double x) { return std::exp(-0.15 * x) * std::sin(x); },
                          0, 30,
                          {.width = 72, .height = 16, .color = colors::kCyan,
                           .title = "e^(-x/7)·sin(x)", .x_label = "x"}));

  header("plotFn — sinc (function with a sharp feature)");
  std::print("{}", plotFn([](double x) { return std::abs(x) < 1e-9 ? 1.0 : std::sin(x) / x; },
                          -4 * kPi, 4 * kPi,
                          {.width = 72, .height = 14, .color = colors::kYellow,
                           .title = "sinc(x)"}));

  header("linesPlot — three series share axes; overlaps blend their colors");
  std::vector<Series> series{
      {[](double x) { return std::sin(x); }, colors::kRed, "sin"},
      {[](double x) { return std::cos(x); }, colors::kBlue, "cos"},
      {[](double x) { return 0.5 * std::sin(2 * x); }, colors::kGreen, "½sin2x"},
  };
  std::print("{}", linesPlot(series, 0, 2 * kPi,
                             {.width = 72, .height = 16, .title = "trig family"}));

  header("scatterPlot — noisy quadratic (auto-scaled with padding)");
  {
    std::mt19937_64 gen{0xABCDEF};
    std::normal_distribution<double> noise{0.0, 2.5};
    std::vector<Point> pts;
    for (int i = 0; i < 120; ++i) {
      const double x = -5.0 + 10.0 * i / 119.0;
      pts.push_back({x, x * x + noise(gen)});
    }
    std::print("{}", scatterPlot(pts, {.width = 64, .height = 18, .color = colors::kMagenta,
                                       .title = "y = x² + 𝒩(0, 2.5)"}));
  }

  header("heatmap — point-cloud density (two Gaussian blobs)");
  {
    std::mt19937_64 gen{0x1234};
    std::normal_distribution<double> a{-2.0, 0.8}, b{2.0, 1.1};
    std::vector<Point> pts;
    for (int i = 0; i < 6000; ++i) pts.push_back({a(gen), a(gen)});
    for (int i = 0; i < 6000; ++i) pts.push_back({b(gen), b(gen)});
    std::print("{}", heatmap(pts, {.width = 56, .height = 22, .title = "two blobs"}));
    std::println("{}", colorbar(RGB{20, 20, 80}, RGB{255, 255, 100}));
  }

  header("heatmapFn — scalar field sin(x)·cos(y) over [-π,π]²");
  std::print("{}", heatmapFn([](double x, double y) { return std::sin(x) * std::cos(y); },
                             -kPi, kPi, -kPi, kPi,
                             {.width = 64, .height = 28, .title = "sin(x)·cos(y)",
                              .low_color = {30, 30, 120}, .high_color = {255, 180, 60}}));
  std::println("{}", colorbar(RGB{30, 30, 120}, RGB{255, 180, 60}));

  header("heatmapFn — Himmelblau (four minima; a classic optimization surface)");
  std::print("{}", heatmapFn(
                       [](double x, double y) {
                         const double a = x * x + y - 11.0;
                         const double b = x + y * y - 7.0;
                         return -std::log1p(a * a + b * b);  // negate+log so minima glow
                       },
                       -5, 5, -5, 5,
                       {.width = 64, .height = 28, .title = "−log(1+Himmelblau)"}));

  header("sparkline — inline trends");
  {
    std::vector<double> req{3, 5, 4, 8, 9, 7, 12, 15, 11, 14, 18, 22, 19, 25};
    std::vector<double> lat{42, 40, 45, 38, 50, 47, 41, 39, 44, 60, 55, 43, 41, 40};
    std::println("requests/s  {}  ({}…{})", sparkline(req, colors::kGreen),
                 req.front(), req.back());
    std::println("latency ms  {}  (p100 = {})", sparkline(lat, colors::kRed),
                 std::ranges::max(lat));
  }

  header("barPlot — vertical bars with eighth-block sub-rows (digits of π)");
  {
    std::vector<double> digits{3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3, 2, 3, 8, 4};
    std::print("{}", barPlot(digits, {.height = 10, .title = "first 20 digits of π"}));
  }

  header("generateTextHistogram — 4000 samples from 𝒩(0,1)");
  {
    std::mt19937_64 gen{0x55};
    std::normal_distribution<double> nd{0.0, 1.0};
    std::vector<double> data(4000);
    for (auto& v : data) v = nd(gen);
    std::print("{}", generateTextHistogram(data, {.bins = 15, .width = 50,
                                                  .color = colors::kCyan, .normalize = true}));
  }

  header("buildBarChartText — labeled horizontal bars");
  {
    std::vector<TextBar> bars{
        {32, "linear", "32 ms", colors::kCyan},
        {18, "robin-hood", "18 ms", colors::kGreen},
        {44, "chaining", "44 ms", colors::kYellow},
        {9, "perfect", "9 ms", colors::kMagenta},
    };
    std::print("{}", buildBarChartText(bars));
  }

  std::println("");
  return 0;
}
