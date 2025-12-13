#include "plot.h"

#include <cmath>

#include "unit.h"

using namespace tempura;

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
    std::function<double(double)> fn = [](double x) { return x; };
    expectEq(plotFn(fn, 0, 10, 0, 10), "");   // width = 0
    expectEq(plotFn(fn, 0, 10, 10, 0), "");   // height = 0
    expectEq(plotFn(fn, 0, 10, -5, 10), "");  // negative width
    expectEq(plotFn(fn, 10, 0, 10, 10), "");  // min_x > max_x
  };

  "linePlot invalid dimensions"_test = [] {
    std::function<double(double)> fn = [](double x) { return x; };
    expectEq(linePlot(fn, 0, 10, 0, 10), "");
    expectEq(linePlot(fn, 0, 10, 10, 0), "");
    expectEq(linePlot(fn, 10, 0, 10, 10), "");
  };

  "plotFn sinc function"_test = [] {
    std::function<double(double)> fn = [](double x) {
      return (std::abs(x) < 1e-10) ? 1.0 : std::sin(x) / x;
    };
    auto result = plotFn(fn, -10, 10, 40, 10);
    expectTrue(!result.empty());
    expectTrue(result.find("┌") != std::string::npos);  // Has border
    expectTrue(result.find("┘") != std::string::npos);
  };

  "linePlot linear function"_test = [] {
    std::function<double(double)> fn = [](double x) { return 2 * x + 1; };
    auto result = linePlot(fn, -5, 5, 30, 10);
    expectTrue(!result.empty());
  };

  "scatterPlot empty"_test = [] {
    std::vector<Point> empty;
    expectEq(scatterPlot(empty, 40, 20), "");
  };

  "scatterPlot basic"_test = [] {
    std::vector<Point> points{{0, 0}, {1, 1}, {2, 4}, {3, 9}};
    auto result = scatterPlot(points, 40, 20);
    expectTrue(!result.empty());
  };

  "scatterPlot single point"_test = [] {
    std::vector<Point> points{{5, 5}};
    auto result = scatterPlot(points, 40, 20);
    expectTrue(!result.empty());
  };

  "multiPlot empty series"_test = [] {
    std::vector<Series> empty;
    expectEq(multiPlot(empty, 0, 10, 40, 20), "");
  };

  "multiPlot multiple series"_test = [] {
    std::vector<Series> series{
        {[](double x) { return std::sin(x); }, colors::kRed, "sin"},
        {[](double x) { return std::cos(x); }, colors::kBlue, "cos"},
    };
    auto result = multiPlot(series, 0, 6.28, 40, 15);
    expectTrue(!result.empty());
    expectTrue(result.find("sin") != std::string::npos);
    expectTrue(result.find("cos") != std::string::npos);
  };

  "PlotOptions title"_test = [] {
    std::function<double(double)> fn = [](double x) { return x * x; };
    PlotOptions opts{.width = 40, .height = 10, .title = "x^2"};
    auto result = plotFn(fn, -3, 3, opts);
    expectTrue(result.find("x^2") != std::string::npos);
  };

  "PlotOptions no border"_test = [] {
    std::function<double(double)> fn = [](double x) { return x; };
    PlotOptions opts{.width = 20, .height = 5, .show_border = false};
    auto result = plotFn(fn, 0, 10, opts);
    expectTrue(result.find("┌") == std::string::npos);
    expectTrue(result.find("│") == std::string::npos);
  };

  "constant function"_test = [] {
    std::function<double(double)> fn = [](double) { return 5.0; };
    auto result = plotFn(fn, 0, 10, 40, 10);
    expectTrue(!result.empty());  // Should handle constant without crash
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
    expectTrue(!result.empty());  // Should not crash
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

  "multiPlotBraille empty"_test = [] {
    std::vector<Series> empty;
    expectEq(multiPlotBraille(empty, 0, 10, 40, 10), "");
  };

  "multiPlotBraille single series"_test = [] {
    std::vector<Series> series{
        {[](double x) { return std::sin(x); }, colors::kRed, "sin"},
    };
    auto result = multiPlotBraille(series, 0, 6.28, 40, 10);
    expectTrue(!result.empty());
    expectTrue(result.find("sin") != std::string::npos);
  };

  "multiPlotBraille color mixing"_test = [] {
    std::vector<Series> series{
        {[](double x) { return std::sin(x); }, colors::kRed, "R"},
        {[](double x) { return std::cos(x); }, colors::kBlue, "B"},
    };
    auto result = multiPlotBraille(series, 0, 6.28, 50, 12);
    expectTrue(!result.empty());
    // Should have legend
    expectTrue(result.find("R") != std::string::npos);
    expectTrue(result.find("B") != std::string::npos);
  };

  "multiPlotBraille with title"_test = [] {
    std::vector<Series> series{
        {[](double x) { return x; }, colors::kGreen, "linear"},
    };
    auto result =
        multiPlotBraille(series, 0, 10, 40, 8, true, true, "Test Title");
    expectTrue(result.find("Test Title") != std::string::npos);
  };

  return TestRegistry::result();
}
