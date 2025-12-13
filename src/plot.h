#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <vector>

#include "root_finding.h"

namespace tempura {

// ANSI color support for terminal output
struct RGB {
  uint8_t r{200};
  uint8_t g{200};
  uint8_t b{200};

  constexpr auto wrap(std::string_view text) const -> std::string {
    return std::format("{}{}{}", ansiPrefix(), text, ansiSuffix());
  }

  constexpr auto ansiPrefix() const -> std::string {
    return std::format("\033[38;2;{};{};{}m", r, g, b);
  }

  static constexpr auto ansiSuffix() -> std::string_view { return "\033[0m"; }
};

// Predefined colors for multi-series plots
namespace colors {
constexpr RGB kDefault{200, 200, 200};
constexpr RGB kAxis{113, 144, 110};
constexpr RGB kRed{255, 100, 100};
constexpr RGB kGreen{100, 255, 100};
constexpr RGB kBlue{100, 100, 255};
constexpr RGB kYellow{255, 255, 100};
constexpr RGB kCyan{100, 255, 255};
constexpr RGB kMagenta{255, 100, 255};
constexpr RGB kOrange{255, 165, 0};
}  // namespace colors

// Unicode characters for 2x2 cell rendering (quadrants)
constexpr auto kQuadrents = u8" ▘▝▀▖▌▞▛▗▚▐▜▄▙▟█";

// Unicode characters for 3x2 cell rendering (sextants)
constexpr const char* kSextant[] = {" ", "🬀", "🬁", "🬂", "🬃", "🬄", "🬅", "🬆",
                                    "🬇", "🬈", "🬉", "🬊", "🬋", "🬌", "🬍", "🬎",

                                    "🬏", "🬐", "🬑", "🬒", "🬓", "▌", "🬔", "🬕",
                                    "🬖", "🬗", "🬘", "🬙", "🬚", "🬛", "🬜", "🬝",

                                    "🬞", "🬟", "🬠", "🬡", "🬢", "🬣", "🬤", "🬥",
                                    "🬦", "🬧", "▐", "🬨", "🬩", "🬪", "🬫", "🬬",
                                    "🬭", "🬮", "🬯", "🬰", "🬱", "🬲", "🬳", "🬴",
                                    "🬵", "🬶", "🬷", "🬸", "🬹", "🬺", "🬻", "█"};

// Braille characters for 2x4 cell rendering (octants) - highest resolution
static constexpr const char* kOctant[] = {
    "⠀", "⠁", "⠂", "⠃", "⠄", "⠅", "⠆", "⠇", "⡀", "⡁", "⡂", "⡃", "⡄", "⡅", "⡆",
    "⡇", "⠈", "⠉", "⠊", "⠋", "⠌", "⠍", "⠎", "⠏", "⡈", "⡉", "⡊", "⡋", "⡌", "⡍",
    "⡎", "⡏", "⠐", "⠑", "⠒", "⠓", "⠔", "⠕", "⠖", "⠗", "⡐", "⡑", "⡒", "⡓", "⡔",
    "⡕", "⡖", "⡗", "⠘", "⠙", "⠚", "⠛", "⠜", "⠝", "⠞", "⠟", "⡘", "⡙", "⡚", "⡛",
    "⡜", "⡝", "⡞", "⡟", "⠠", "⠡", "⠢", "⠣", "⠤", "⠥", "⠦", "⠧", "⡠", "⡡", "⡢",
    "⡣", "⡤", "⡥", "⡦", "⡧", "⠨", "⠩", "⠪", "⠫", "⠬", "⠭", "⠮", "⠯", "⡨", "⡩",
    "⡪", "⡫", "⡬", "⡭", "⡮", "⡯", "⠰", "⠱", "⠲", "⠳", "⠴", "⠵", "⠶", "⠷", "⡰",
    "⡱", "⡲", "⡳", "⡴", "⡵", "⡶", "⡷", "⠸", "⠹", "⠺", "⠻", "⠼", "⠽", "⠾", "⠿",
    "⡸", "⡹", "⡺", "⡻", "⡼", "⡽", "⡾", "⡿", "⢀", "⢁", "⢂", "⢃", "⢄", "⢅", "⢆",
    "⢇", "⣀", "⣁", "⣂", "⣃", "⣄", "⣅", "⣆", "⣇", "⢈", "⢉", "⢊", "⢋", "⢌", "⢍",
    "⢎", "⢏", "⣈", "⣉", "⣊", "⣋", "⣌", "⣍", "⣎", "⣏", "⢐", "⢑", "⢒", "⢓", "⢔",
    "⢕", "⢖", "⢗", "⣐", "⣑", "⣒", "⣓", "⣔", "⣕", "⣖", "⣗", "⢘", "⢙", "⢚", "⢛",
    "⢜", "⢝", "⢞", "⢟", "⣘", "⣙", "⣚", "⣛", "⣜", "⣝", "⣞", "⣟", "⢠", "⢡", "⢢",
    "⢣", "⢤", "⢥", "⢦", "⢧", "⣠", "⣡", "⣢", "⣣", "⣤", "⣥", "⣦", "⣧", "⢨", "⢩",
    "⢪", "⢫", "⢬", "⢭", "⢮", "⢯", "⣨", "⣩", "⣪", "⣫", "⣬", "⣭", "⣮", "⣯", "⢰",
    "⢱", "⢲", "⢳", "⢴", "⢵", "⢶", "⢷", "⣠", "⣡", "⣢", "⣳", "⣴", "⣵", "⣶", "⣷",
    "⢸", "⢹", "⢺", "⢻", "⢼", "⢽", "⢾", "⢿", "⣸", "⣹", "⣺", "⣻", "⣼", "⣽", "⣾",
    "⣿"};

// A declarative representation of an element of a bar chart
struct TextBar {
  int64_t length;
  std::string label;
  std::string end_text;
  std::optional<RGB> color;
};

constexpr auto buildBarChartText(auto&& bars) -> std::string {
  if (std::ranges::empty(bars)) return "";

  const auto label_length = static_cast<int64_t>(
      std::ranges::max(bars | std::views::transform([](const auto& x) {
                         return x.label.size();
                       })));

  std::string result;
  const auto bar_count = std::ranges::size(bars);

  for (auto [i, bar] : bars | std::views::enumerate | std::views::as_const) {
    result += std::format("{:>{}} ", bar.label, label_length);

    if (i == 0) {
      result += "┌";
    } else if (static_cast<size_t>(i) == bar_count - 1) {
      result += "└";
    } else {
      result += "├";
    }

    if (bar.color.has_value()) {
      result += bar.color->ansiPrefix();
    }
    for (int64_t j = 0; j < std::max(int64_t{0}, bar.length); ++j) {
      result += "🬋";
    }
    if (bar.color.has_value()) {
      result += RGB::ansiSuffix();
    }

    result += std::format(" {}\n", bar.end_text);
  }

  return result;
}

// Backward compatibility alias
constexpr auto buildBartChartText(auto&& bars) -> std::string {
  return buildBarChartText(std::forward<decltype(bars)>(bars));
}

struct TextHistogramOptions {
  int64_t bins = 10;
  int64_t width = 60;
  std::optional<RGB> color = colors::kAxis;

  std::optional<double> min_x = std::nullopt;
  std::optional<double> max_x = std::nullopt;

  int64_t min_y = 0;
  std::optional<int64_t> max_y = std::nullopt;

  bool normalize = false;
};

constexpr auto generateTextHistogram(auto&& data,
                                     TextHistogramOptions options = {})
    -> std::string {
  if (std::ranges::empty(data)) return "";
  if (options.bins <= 0 || options.width <= 0) return "";

  if (!options.min_x.has_value()) {
    options.min_x = std::ranges::min(data);
  }
  if (!options.max_x.has_value()) {
    options.max_x = std::ranges::max(data);
  }

  const double range = *options.max_x - *options.min_x;
  if (range <= 0) {
    std::vector<TextBar> bars{{options.width,
                               std::format("{:.2f}", *options.min_x),
                               std::format("{}", std::ranges::size(data)),
                               options.color}};
    return buildBarChartText(bars);
  }

  std::vector<int64_t> buckets(static_cast<size_t>(options.bins), 0);
  for (const auto& x : data) {
    if (x < *options.min_x || x > *options.max_x) {
      continue;
    }
    int64_t bucket = static_cast<int64_t>(
        (x - *options.min_x) / range * static_cast<double>(options.bins));
    bucket = std::clamp(bucket, int64_t{0}, options.bins - 1);
    buckets[static_cast<size_t>(bucket)]++;
  }

  if (!options.max_y.has_value()) {
    options.max_y = *std::ranges::max_element(buckets);
  }
  if (*options.max_y <= 0) {
    *options.max_y = 1;
  }

  std::vector<TextBar> bars;
  bars.reserve(static_cast<size_t>(options.bins));

  for (int64_t i = 0; i < options.bins; ++i) {
    const double bin_width = range / static_cast<double>(options.bins);
    const double a = *options.min_x + static_cast<double>(i) * bin_width;
    const double b = a + bin_width;

    std::string label = std::format("{:.2f} - {:.2f}", a, b);
    const auto count = buckets[static_cast<size_t>(i)];
    const auto length =
        static_cast<int64_t>(options.width * count / *options.max_y);

    std::string end_text;
    if (options.normalize) {
      end_text = std::format(
          "{:.2f}", static_cast<double>(count) / std::ranges::size(data));
    } else {
      end_text = std::format("{}", count);
    }
    bars.push_back(
        {length, std::move(label), std::move(end_text), options.color});
  }
  return buildBarChartText(bars);
}

// Configuration for function plots
struct PlotOptions {
  int64_t width = 80;
  int64_t height = 20;
  RGB color = colors::kDefault;
  std::string title;
  std::string x_label;
  std::string y_label;
  bool show_border = true;
  bool show_axes = true;
  bool show_roots = true;
  bool verbose_roots = false;
};

// A point for scatter plots
struct Point {
  double x;
  double y;
};

// A series for multi-series plots
struct Series {
  std::function<double(double)> fn;
  RGB color = colors::kDefault;
  std::string label;
};

namespace detail {

// Safe index calculation with bounds checking
constexpr auto safeIndex(int64_t col, int64_t row, int64_t width, int64_t height)
    -> std::optional<size_t> {
  if (col < 0 || col >= width || row < 0 || row >= height) {
    return std::nullopt;
  }
  return static_cast<size_t>(col + row * width);
}

// Convert y-value to row index (y-axis is flipped)
constexpr auto yToRow(double y, double min_y, double max_y, int64_t height)
    -> int64_t {
  if (max_y <= min_y) return height / 2;
  const double normalized = (max_y - y) / (max_y - min_y);
  return static_cast<int64_t>(std::round(normalized * (height - 1)));
}

// Convert x-value to column index
constexpr auto xToCol(double x, double min_x, double max_x, int64_t width)
    -> int64_t {
  if (max_x <= min_x) return width / 2;
  const double normalized = (x - min_x) / (max_x - min_x);
  return static_cast<int64_t>(std::round(normalized * (width - 1)));
}

// Draw border around plot
inline void drawBorder(std::string& result, int64_t width,
                       const std::string& title) {
  result += "┌";
  if (!title.empty() && static_cast<int64_t>(title.size()) < width) {
    const auto padding = (width - static_cast<int64_t>(title.size())) / 2;
    for (int64_t i = 0; i < padding; ++i) result += "―";
    result += title;
    for (int64_t i = 0;
         i < width - padding - static_cast<int64_t>(title.size()); ++i)
      result += "―";
  } else {
    for (int64_t i = 0; i < width; ++i) result += "―";
  }
  result += "┐\n";
}

inline void drawBottomBorder(std::string& result, int64_t width) {
  result += "└";
  for (int64_t i = 0; i < width; ++i) result += "―";
  result += "┘\n";
}

// Draw x-axis if zero is in range
inline void drawXAxis(std::vector<std::string>& plot, double min_y,
                      double max_y, int64_t width, int64_t height) {
  if (min_y > 0 || max_y < 0 || max_y <= min_y) return;

  const auto row = yToRow(0.0, min_y, max_y, height);
  if (row < 0 || row >= height) return;

  for (int64_t i = 0; i < width; ++i) {
    if (auto idx = safeIndex(i, row, width, height)) {
      plot[*idx] = colors::kAxis.wrap("―");
    }
  }
}

// Mark roots on the plot
inline void markRoots(std::vector<std::string>& plot,
                      std::function<double(double)>& fn, double min_x,
                      double max_x, double min_y, double max_y, int64_t width,
                      int64_t height, const RGB& color, bool verbose) {
  if (min_y > 0 || max_y < 0 || max_y <= min_y || max_x <= min_x) return;

  const auto row = yToRow(0.0, min_y, max_y, height);
  if (row < 0 || row >= height) return;

  const auto intervals =
      root_finding::subIntervalSignChange(fn, {.a = min_x, .b = max_x}, width);

  for (const auto& interval : intervals) {
    const auto [a, b] = root_finding::bisectRoot(fn, interval);
    const double x = (a + b) / 2;
    if (verbose) {
      std::println("root: x = {}", x);
    }
    auto col = xToCol(x, min_x, max_x, width);
    col = std::clamp(col, int64_t{0}, width - 1);
    if (auto idx = safeIndex(col, row, width, height)) {
      plot[*idx] = color.wrap("×");
    }
  }
}

}  // namespace detail

// Forward declaration for overload
constexpr auto plotFn(std::function<double(double)>& fn, double min_x,
                      double max_x, const PlotOptions& opts) -> std::string;

// High-resolution function plot using Braille characters
constexpr auto plotFn(std::function<double(double)>& fn, double min_x,
                      double max_x, int64_t width, int64_t height,
                      std::optional<RGB> color = std::nullopt) -> std::string {
  PlotOptions opts;
  opts.width = width;
  opts.height = height;
  if (color) opts.color = *color;
  return plotFn(fn, min_x, max_x, opts);
}

// High-resolution function plot with full options
constexpr auto plotFn(std::function<double(double)>& fn, double min_x,
                      double max_x, const PlotOptions& opts) -> std::string {
  if (opts.width <= 0 || opts.height <= 0) return "";
  if (max_x <= min_x) return "";

  const int64_t width = opts.width;
  const int64_t height = opts.height;

  // Sample at 2x4 resolution for Braille rendering
  const int64_t sample_width = 2 * width;
  const int64_t sample_height = 4 * height;

  std::vector<double> y_values;
  y_values.reserve(static_cast<size_t>(sample_width + 1));

  for (int64_t i = 0; i <= sample_width; ++i) {
    const double x =
        min_x + (max_x - min_x) * static_cast<double>(i) / sample_width;
    y_values.push_back(fn(x));
  }

  double max_y = *std::ranges::max_element(y_values);
  double min_y = *std::ranges::min_element(y_values);

  // Ensure non-zero range
  if (max_y <= min_y) {
    const double center = (max_y + min_y) / 2;
    max_y = center + 1.0;
    min_y = center - 1.0;
  }

  // Adjust bounds to align zero with cell boundary for clean axis drawing
  if (opts.show_axes && min_y <= 0 && max_y >= 0) {
    const double range = max_y - min_y;
    const double cell_height = range / sample_height;
    if (cell_height > 0) {
      const double delta = std::fmod(max_y, cell_height);
      const double sign = (delta >= 0) ? 1.0 : -1.0;
      const double offset = delta + sign * cell_height;
      if (offset < 0 && range > std::abs(offset)) {
        max_y += -offset / (1.0 - (max_y - offset) / range);
      } else if (offset > 0 && range > std::abs(offset)) {
        min_y -= offset / (1.0 - (min_y + offset) / range);
      }
    }
  }

  // Occupancy grid for Braille rendering
  const auto grid_size = static_cast<size_t>(sample_width * sample_height);
  std::vector<bool> occupancy(grid_size, false);

  for (int64_t i = 0; i < sample_width; ++i) {
    const double y0 = y_values[static_cast<size_t>(i)];
    const double y1 = y_values[static_cast<size_t>(i + 1)];

    int64_t row0 = detail::yToRow(y0, min_y, max_y, sample_height);
    int64_t row1 = detail::yToRow(y1, min_y, max_y, sample_height);

    row0 = std::clamp(row0, int64_t{0}, sample_height - 1);
    row1 = std::clamp(row1, int64_t{0}, sample_height - 1);

    const int64_t start_row = std::min(row0, row1);
    const int64_t end_row = std::max(row0, row1);

    for (int64_t j = start_row; j <= end_row; ++j) {
      const auto idx = static_cast<size_t>(i + j * sample_width);
      if (idx < occupancy.size()) {
        occupancy[idx] = true;
      }
    }
  }

  // Convert occupancy to Braille characters
  std::vector<std::string> plot(static_cast<size_t>(width * height), " ");

  // Draw x-axis first
  if (opts.show_axes) {
    detail::drawXAxis(plot, min_y, max_y, width, height);
  }

  // Render Braille octants
  for (int64_t col = 0; col < width; ++col) {
    for (int64_t row = 0; row < height; ++row) {
      const int64_t ox = col * 2;
      const int64_t oy = row * 4;

      int64_t octant = 0;
      // Braille bit layout: rows 0-3 in left column (bits 0-3),
      // rows 0-3 in right column (bits 4-7)
      for (int64_t dy = 0; dy < 4; ++dy) {
        const auto idx_l = static_cast<size_t>(ox + (oy + dy) * sample_width);
        const auto idx_r =
            static_cast<size_t>((ox + 1) + (oy + dy) * sample_width);

        if (idx_l < occupancy.size() && occupancy[idx_l]) {
          octant |= (1 << dy);
        }
        if (idx_r < occupancy.size() && occupancy[idx_r]) {
          octant |= (1 << (dy + 4));
        }
      }

      if (octant != 0) {
        if (auto idx = detail::safeIndex(col, row, width, height)) {
          plot[*idx] = opts.color.wrap(kOctant[octant]);
        }
      }
    }
  }

  // Mark roots
  if (opts.show_roots) {
    detail::markRoots(plot, fn, min_x, max_x, min_y, max_y, width, height,
                      opts.color, opts.verbose_roots);
  }

  // Build result string
  std::string result;
  result.reserve(static_cast<size_t>((width + 10) * (height + 3)));

  if (opts.show_border) {
    detail::drawBorder(result, width, opts.title);
  }

  for (int64_t row = 0; row < height; ++row) {
    if (opts.show_border) result += "│";
    for (int64_t col = 0; col < width; ++col) {
      if (auto idx = detail::safeIndex(col, row, width, height)) {
        result += plot[*idx];
      }
    }
    if (opts.show_border) result += "│";
    result += "\n";
  }

  if (opts.show_border) {
    detail::drawBottomBorder(result, width);
  }

  if (!opts.x_label.empty()) {
    const auto padding =
        (width - static_cast<int64_t>(opts.x_label.size())) / 2;
    result += std::string(static_cast<size_t>(padding + 1), ' ');
    result += opts.x_label;
    result += "\n";
  }

  return result;
}

// Simple line plot (lower resolution, faster)
constexpr auto linePlot(std::function<double(double)>& fn, double min_x,
                        double max_x, int64_t width, int64_t height,
                        std::optional<RGB> color = std::nullopt) -> std::string {
  if (width <= 0 || height <= 0) return "";
  if (max_x <= min_x) return "";

  const RGB plot_color = color.value_or(colors::kDefault);

  // Sample function at each column
  std::vector<double> y_values;
  y_values.reserve(static_cast<size_t>(width));

  for (int64_t i = 0; i < width; ++i) {
    const double x =
        min_x + (max_x - min_x) * static_cast<double>(i) / (width - 1);
    y_values.push_back(fn(x));
  }

  double max_y = *std::ranges::max_element(y_values);
  double min_y = *std::ranges::min_element(y_values);

  if (max_y <= min_y) {
    const double center = (max_y + min_y) / 2;
    max_y = center + 1.0;
    min_y = center - 1.0;
  }

  std::vector<std::string> plot(static_cast<size_t>(width * height), " ");

  // Draw x-axis
  detail::drawXAxis(plot, min_y, max_y, width, height);

  // Draw line segments - iterate only within bounds
  for (int64_t i = 0; i < width; ++i) {
    const int64_t row =
        detail::yToRow(y_values[static_cast<size_t>(i)], min_y, max_y, height);
    const int64_t clamped_row = std::clamp(row, int64_t{0}, height - 1);

    if (auto idx = detail::safeIndex(i, clamped_row, width, height)) {
      plot[*idx] = plot_color.wrap("●");
    }

    // Connect to previous point with vertical line if needed
    if (i > 0) {
      const int64_t prev_row = detail::yToRow(
          y_values[static_cast<size_t>(i - 1)], min_y, max_y, height);
      const int64_t clamped_prev = std::clamp(prev_row, int64_t{0}, height - 1);

      const int64_t start = std::min(clamped_row, clamped_prev);
      const int64_t end = std::max(clamped_row, clamped_prev);

      for (int64_t j = start + 1; j < end; ++j) {
        if (auto idx = detail::safeIndex(i, j, width, height)) {
          plot[*idx] = plot_color.wrap("│");
        }
      }
    }
  }

  // Mark roots
  detail::markRoots(plot, fn, min_x, max_x, min_y, max_y, width, height,
                    plot_color, false);

  // Build result
  std::string result;
  result.reserve(static_cast<size_t>((width + 2) * height));

  for (int64_t row = 0; row < height; ++row) {
    for (int64_t col = 0; col < width; ++col) {
      if (auto idx = detail::safeIndex(col, row, width, height)) {
        result += plot[*idx];
      }
    }
    result += "\n";
  }

  return result;
}

// Scatter plot for discrete data points
inline auto scatterPlot(const std::vector<Point>& points, int64_t width,
                        int64_t height,
                        std::optional<RGB> color = std::nullopt) -> std::string {
  if (width <= 0 || height <= 0) return "";
  if (points.empty()) return "";

  const RGB plot_color = color.value_or(colors::kDefault);

  double min_x = points[0].x, max_x = points[0].x;
  double min_y = points[0].y, max_y = points[0].y;

  for (const auto& p : points) {
    min_x = std::min(min_x, p.x);
    max_x = std::max(max_x, p.x);
    min_y = std::min(min_y, p.y);
    max_y = std::max(max_y, p.y);
  }

  // Add 5% padding
  const double x_range = max_x - min_x;
  const double y_range = max_y - min_y;
  const double x_pad = (x_range > 0) ? x_range * 0.05 : 0.5;
  const double y_pad = (y_range > 0) ? y_range * 0.05 : 0.5;
  min_x -= x_pad;
  max_x += x_pad;
  min_y -= y_pad;
  max_y += y_pad;

  std::vector<std::string> plot(static_cast<size_t>(width * height), " ");

  // Draw axes
  detail::drawXAxis(plot, min_y, max_y, width, height);

  // Plot points
  for (const auto& p : points) {
    const int64_t col = detail::xToCol(p.x, min_x, max_x, width);
    const int64_t row = detail::yToRow(p.y, min_y, max_y, height);

    if (auto idx = detail::safeIndex(col, row, width, height)) {
      plot[*idx] = plot_color.wrap("●");
    }
  }

  // Build result
  std::string result;
  result.reserve(static_cast<size_t>((width + 2) * height));

  for (int64_t row = 0; row < height; ++row) {
    for (int64_t col = 0; col < width; ++col) {
      if (auto idx = detail::safeIndex(col, row, width, height)) {
        result += plot[*idx];
      }
    }
    result += "\n";
  }

  return result;
}

// Multi-series plot
inline auto multiPlot(std::vector<Series>& series_list, double min_x,
                      double max_x, int64_t width, int64_t height)
    -> std::string {
  if (width <= 0 || height <= 0) return "";
  if (max_x <= min_x) return "";
  if (series_list.empty()) return "";

  // Find global y range
  double min_y = std::numeric_limits<double>::max();
  double max_y = std::numeric_limits<double>::lowest();

  std::vector<std::vector<double>> all_y_values;
  all_y_values.reserve(series_list.size());

  for (auto& s : series_list) {
    std::vector<double> y_values;
    y_values.reserve(static_cast<size_t>(width));

    for (int64_t i = 0; i < width; ++i) {
      const double x =
          min_x + (max_x - min_x) * static_cast<double>(i) / (width - 1);
      const double y = s.fn(x);
      y_values.push_back(y);
      min_y = std::min(min_y, y);
      max_y = std::max(max_y, y);
    }
    all_y_values.push_back(std::move(y_values));
  }

  if (max_y <= min_y) {
    const double center = (max_y + min_y) / 2;
    max_y = center + 1.0;
    min_y = center - 1.0;
  }

  std::vector<std::string> plot(static_cast<size_t>(width * height), " ");

  // Draw x-axis
  detail::drawXAxis(plot, min_y, max_y, width, height);

  // Plot each series
  for (size_t s = 0; s < series_list.size(); ++s) {
    const auto& y_values = all_y_values[s];
    const RGB& plot_color = series_list[s].color;

    for (int64_t i = 0; i < width; ++i) {
      const int64_t row = detail::yToRow(y_values[static_cast<size_t>(i)],
                                         min_y, max_y, height);
      const int64_t clamped_row = std::clamp(row, int64_t{0}, height - 1);

      if (auto idx = detail::safeIndex(i, clamped_row, width, height)) {
        plot[*idx] = plot_color.wrap("●");
      }
    }
  }

  // Build result with legend
  std::string result;
  result.reserve(
      static_cast<size_t>((width + 20) * (height + series_list.size() + 2)));

  for (int64_t row = 0; row < height; ++row) {
    for (int64_t col = 0; col < width; ++col) {
      if (auto idx = detail::safeIndex(col, row, width, height)) {
        result += plot[*idx];
      }
    }
    result += "\n";
  }

  // Add legend
  result += "\n";
  for (const auto& s : series_list) {
    result += s.color.wrap("●");
    result += " ";
    result += s.label.empty() ? "series" : s.label;
    result += "  ";
  }
  result += "\n";

  return result;
}

}  // namespace tempura
