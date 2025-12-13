#include <cmath>
#include <iostream>
#include <limits>
#include <print>
#include <vector>

#include "plot.h"

using namespace tempura;

auto main() -> int {
  std::println("=== ASCII Plotting Library Demo ===\n");

  // 1. High-resolution function plot using Braille characters
  std::println("1. High-Resolution Plot (sinc function using Braille)");
  std::println("   Uses 2x4 subpixel resolution per character\n");

  std::function<double(double)> sinc = [](double x) {
    return (std::abs(x) < 1e-10) ? 1.0 : std::sin(x) / x;
  };

  std::cout << plotFn(sinc, -20., 20., 80, 12);
  std::println("");

  // 2. Plot with title and options
  std::println("2. Plot with Title and Custom Options");

  std::function<double(double)> sine = [](double x) { return std::sin(x); };

  PlotOptions opts{
      .width = 60,
      .height = 10,
      .color = colors::kCyan,
      .title = " sin(x) ",
      .x_label = "x (radians)",
      .show_border = true,
      .show_axes = true,
      .show_roots = true,
  };
  std::cout << plotFn(sine, -6.28, 6.28, opts);
  std::println("");

  // 3. Simple line plot (lower resolution, faster)
  std::println("3. Simple Line Plot (quadratic function)");

  std::function<double(double)> quadratic = [](double x) {
    return x * x - 4;
  };

  std::cout << linePlot(quadratic, -4., 4., 50, 12, colors::kGreen);
  std::println("");

  // 4. Scatter plot
  std::println("4. Scatter Plot (random-ish data points)");

  std::vector<Point> points;
  for (int i = 0; i < 30; ++i) {
    double x = i * 0.5;
    double y = std::sin(x * 0.3) * 5 + (i % 3) - 1;
    points.push_back({x, y});
  }

  std::cout << scatterPlot(points, 60, 15, colors::kYellow);
  std::println("");

  // 5. Multi-series plot (simple)
  std::println("5. Multi-Series Plot - Simple (sin, cos, and tan clipped)");

  std::vector<Series> series{
      {[](double x) { return std::sin(x); }, colors::kRed, "sin(x)"},
      {[](double x) { return std::cos(x); }, colors::kBlue, "cos(x)"},
      {[](double x) { return std::clamp(std::tan(x), -2.0, 2.0); },
       colors::kGreen,
       "tan(x)"},
  };

  std::cout << multiPlot(series, 0., 6.28, 70, 15);
  std::println("");

  // 5b. High-resolution Braille multi-series with color mixing
  std::println("5b. Multi-Series Braille Plot with Color Mixing");
  std::println("    When lines cross, colors blend (red+blue=purple, etc.)\n");

  std::vector<Series> braille_series{
      {[](double x) { return std::sin(x); }, colors::kRed, "sin(x)"},
      {[](double x) { return std::cos(x); }, colors::kBlue, "cos(x)"},
  };

  std::cout << multiPlotBraille(braille_series, 0., 12.56, 80, 15, true, true,
                                " sin & cos with color mixing ");
  std::println("");

  // 5c. Three-way color mixing
  std::println("5c. Three-Way Color Mixing (RGB waves)");

  std::vector<Series> rgb_series{
      {[](double x) { return std::sin(x); }, colors::kRed, "R"},
      {[](double x) { return std::sin(x + 2.09); }, colors::kGreen, "G"},
      {[](double x) { return std::sin(x + 4.19); }, colors::kBlue, "B"},
  };

  std::cout << multiPlotBraille(rgb_series, 0., 12.56, 80, 12, true, true,
                                " RGB phase-shifted sines ");
  std::println("");

  // 6. Histogram
  std::println("6. Text Histogram (simulated normal distribution)");

  std::vector<double> data;
  data.reserve(1000);
  // Simple pseudo-random normal-ish distribution using Box-Muller-like approach
  for (int i = 0; i < 1000; ++i) {
    double sum = 0;
    for (int j = 0; j < 12; ++j) {
      sum += (i * 17 + j * 31) % 100 / 100.0;
    }
    data.push_back((sum - 6) * 2);  // Roughly centered around 0
  }

  TextHistogramOptions hist_opts{
      .bins = 15,
      .width = 40,
      .color = colors::kMagenta,
      .normalize = false,
  };
  std::cout << generateTextHistogram(data, hist_opts);
  std::println("");

  // 7. Bar chart
  std::println("7. Bar Chart");

  std::vector<TextBar> bars{
      {35, "Category A", "175 items", colors::kRed},
      {50, "Category B", "250 items", colors::kGreen},
      {20, "Category C", "100 items", colors::kBlue},
      {45, "Category D", "225 items", colors::kYellow},
      {30, "Category E", "150 items", colors::kCyan},
  };
  std::cout << buildBarChartText(bars);
  std::println("");

  // 8. Plot without border
  std::println("8. Borderless Plot (exponential decay)");

  std::function<double(double)> decay = [](double x) {
    return std::exp(-x * 0.5);
  };

  PlotOptions borderless{
      .width = 50,
      .height = 8,
      .color = colors::kOrange,
      .show_border = false,
      .show_axes = true,
      .show_roots = false,
  };
  std::cout << plotFn(decay, 0., 10., borderless);
  std::println("");

  // 9. Constant function (edge case)
  std::println("9. Constant Function (y = 5)");

  std::function<double(double)> constant = [](double) { return 5.0; };

  std::cout << plotFn(constant, 0., 10., 40, 5);
  std::println("");

  // 10. Available colors
  std::println("10. Available Predefined Colors:");
  std::println("    {} kDefault (gray)", colors::kDefault.wrap("●"));
  std::println("    {} kAxis (muted green)", colors::kAxis.wrap("●"));
  std::println("    {} kRed", colors::kRed.wrap("●"));
  std::println("    {} kGreen", colors::kGreen.wrap("●"));
  std::println("    {} kBlue", colors::kBlue.wrap("●"));
  std::println("    {} kYellow", colors::kYellow.wrap("●"));
  std::println("    {} kCyan", colors::kCyan.wrap("●"));
  std::println("    {} kMagenta", colors::kMagenta.wrap("●"));
  std::println("    {} kOrange", colors::kOrange.wrap("●"));

  return 0;
}
