#include "plot.h"

#include <cmath>
#include <string_view>

#include "unit.h"

using namespace tempura;

// 256 distinct dot patterns ⇒ 256 distinct glyphs. The old hand-typed table had
// a duplicate (⣠ at two indices), which silently corrupted rendering; generating
// the table makes the bijection an invariant we can actually check.
constexpr bool octantsDistinct() {
  for (size_t i = 0; i < kOctant.size(); ++i) {
    for (size_t j = i + 1; j < kOctant.size(); ++j) {
      if (std::string_view{kOctant[i].bytes} == std::string_view{kOctant[j].bytes}) {
        return false;
      }
    }
  }
  return true;
}
static_assert(octantsDistinct(), "Braille octant glyphs must be unique");
static_assert(std::string_view{kOctant[0].bytes} == "⠀");    // U+2800, no dots
static_assert(std::string_view{kOctant[255].bytes} == "⣿");  // U+28FF, all dots

auto main() -> int {
  "empty histogram"_test = [] {
    std::vector<double> empty;
    expectEq(generateTextHistogram(empty), "");
  };

  "single value histogram"_test = [] {
    std::vector<double> single{5.0, 5.0, 5.0};
    auto result = generateTextHistogram(single);
    expectTrue(!result.empty());
  };

  "histogram basic"_test = [] {
    std::vector<double> data{0.1, 0.2, 0.3, 0.8, 0.9, 0.95};
    auto result = generateTextHistogram(data, {.bins = 5, .width = 20});
    expectTrue(!result.empty());
  };

  "plotFn invalid dimensions"_test = [] {
    auto fn = [](double x) { return x; };
    expectEq(plotFn(fn, 0, 10, 0, 10), "");   // width = 0
    expectEq(plotFn(fn, 0, 10, 10, 0), "");   // height = 0
    expectEq(plotFn(fn, 0, 10, -5, 10), "");  // negative width
    expectEq(plotFn(fn, 10, 0, 10, 10), "");  // min_x > max_x
  };

  "plotFn accepts a plain lambda (RealFunction, not std::function)"_test = [] {
    auto result = plotFn([](double x) { return x * x; }, -3, 3, 40, 10);
    expectTrue(!result.empty());
  };

  "plotFn sinc function"_test = [] {
    auto fn = [](double x) { return (std::abs(x) < 1e-10) ? 1.0 : std::sin(x) / x; };
    auto result = plotFn(fn, -10, 10, 40, 10);
    expectTrue(!result.empty());
    expectTrue(result.find("┌") != std::string::npos);  // has border
    expectTrue(result.find("┘") != std::string::npos);
  };

  "PlotOptions title"_test = [] {
    auto fn = [](double x) { return x * x; };
    PlotOptions opts{.width = 40, .height = 10, .title = "x^2"};
    auto result = plotFn(fn, -3, 3, opts);
    expectTrue(result.find("x^2") != std::string::npos);
  };

  "PlotOptions no border"_test = [] {
    auto fn = [](double x) { return x; };
    PlotOptions opts{.width = 20, .height = 5, .show_border = false};
    auto result = plotFn(fn, 0, 10, opts);
    expectTrue(result.find("┌") == std::string::npos);
    expectTrue(result.find("│") == std::string::npos);
  };

  "constant function"_test = [] {
    auto fn = [](double) { return 5.0; };
    auto result = plotFn(fn, 0, 10, 40, 10);
    expectTrue(!result.empty());  // should handle constant without crashing
  };

  "scatterPlot empty"_test = [] {
    std::vector<Point> empty;
    expectEq(scatterPlot(empty, {.width = 40, .height = 20}), "");
  };

  "scatterPlot basic"_test = [] {
    std::vector<Point> points{{0, 0}, {1, 1}, {2, 4}, {3, 9}};
    auto result = scatterPlot(points, {.width = 40, .height = 20});
    expectTrue(!result.empty());
  };

  "scatterPlot single point"_test = [] {
    std::vector<Point> points{{5, 5}};
    auto result = scatterPlot(points, {.width = 40, .height = 20});
    expectTrue(!result.empty());
  };

  "linesPlot empty series"_test = [] {
    std::vector<Series> empty;
    expectEq(linesPlot(empty, 0, 10), "");
  };

  "linesPlot multiple series with legend"_test = [] {
    std::vector<Series> series{
        {[](double x) { return std::sin(x); }, colors::kRed, "sin"},
        {[](double x) { return std::cos(x); }, colors::kBlue, "cos"},
    };
    auto result = linesPlot(series, 0, 6.28, {.width = 50, .height = 12});
    expectTrue(!result.empty());
    expectTrue(result.find("sin") != std::string::npos);
    expectTrue(result.find("cos") != std::string::npos);
  };

  "linesPlot title"_test = [] {
    std::vector<Series> series{
        {[](double x) { return x; }, colors::kGreen, "linear"},
    };
    auto result = linesPlot(series, 0, 10, {.width = 40, .height = 8, .title = "Test Title"});
    expectTrue(result.find("Test Title") != std::string::npos);
  };

  "heatmap point cloud"_test = [] {
    std::vector<Point> points;
    for (int i = 0; i < 200; ++i) {
      const double t = i * 0.05;
      points.push_back({std::cos(t), std::sin(t)});
    }
    auto result = heatmap(points, {.width = 30, .height = 12});
    expectTrue(!result.empty());
  };

  "heatmapFn scalar field"_test = [] {
    auto f = [](double x, double y) { return std::sin(x) * std::cos(y); };
    auto result = heatmapFn(f, -3.14, 3.14, -3.14, 3.14, {.width = 30, .height = 12});
    expectTrue(!result.empty());
    expectTrue(result.find("█") != std::string::npos);
  };

  "sparkline"_test = [] {
    std::vector<double> values{1, 3, 2, 5, 4, 8, 6, 7};
    auto result = sparkline(values);
    expectTrue(!result.empty());
    // lowest value → ▁, highest → █
    expectTrue(result.find("▁") != std::string::npos);
    expectTrue(result.find("█") != std::string::npos);
    expectEq(sparkline(std::vector<double>{}), "");
  };

  "barPlot"_test = [] {
    std::vector<double> values{3, 1, 4, 1, 5, 9, 2, 6};
    auto result = barPlot(values, {.height = 8});
    expectTrue(!result.empty());
    expectTrue(result.find("█") != std::string::npos);
  };

  "bar chart empty"_test = [] {
    std::vector<TextBar> empty;
    expectEq(buildBarChartText(empty), "");
  };

  "bar chart basic"_test = [] {
    std::vector<TextBar> bars{
        {10, "A", "10", colors::kRed},
        {20, "B", "20", colors::kBlue},
        {5, "C", "5", colors::kGreen},
    };
    auto result = buildBarChartText(bars);
    expectTrue(!result.empty());
    expectTrue(result.find("A") != std::string::npos);
    expectTrue(result.find("B") != std::string::npos);
    expectTrue(result.find("C") != std::string::npos);
  };

  "bar chart negative length handled"_test = [] {
    std::vector<TextBar> bars{
        {-5, "negative", "test", std::nullopt},
    };
    auto result = buildBarChartText(bars);
    expectTrue(!result.empty());  // should not crash
  };

  "RGB wrap"_test = [] {
    RGB color{255, 0, 0};
    auto wrapped = color.wrap("test");
    expectTrue(wrapped.find("255") != std::string::npos);
    expectTrue(wrapped.find("test") != std::string::npos);
    expectTrue(wrapped.find("\033[0m") != std::string::npos);
  };

  "predefined colors"_test = [] {
    expectEq(colors::kRed.r, 255);
    expectEq(colors::kGreen.g, 255);
    expectEq(colors::kBlue.b, 255);
  };

  return TestRegistry::result();
}
