#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#ifdef __unix__
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#endif

namespace tempura {

// ANSI color support for terminal output
struct RGB {
  uint8_t r{200};
  uint8_t g{200};
  uint8_t b{200};

  constexpr auto wrap(std::string_view text) const -> std::string {
    return std::format("{}{}{}", ansiPrefix(), text, ansiSuffix());
  }

  // Wrap with both foreground and background colors
  constexpr auto wrapFgBg(std::string_view text, const RGB& bg) const
      -> std::string {
    return std::format("\033[38;2;{};{};{};48;2;{};{};{}m{}\033[0m", r, g, b,
                       bg.r, bg.g, bg.b, text);
  }

  constexpr auto ansiPrefix() const -> std::string {
    return std::format("\033[38;2;{};{};{}m", r, g, b);
  }

  constexpr auto ansiBgPrefix() const -> std::string {
    return std::format("\033[48;2;{};{};{}m", r, g, b);
  }

  static constexpr auto ansiSuffix() -> std::string_view { return "\033[0m"; }

  constexpr bool operator==(const RGB&) const = default;
};

// Query the terminal for its background color using OSC 11
// Returns std::nullopt if the query fails (unsupported terminal, timeout, etc.)
// This function temporarily modifies terminal settings and restores them after
#ifdef __unix__
inline auto queryTerminalBackground(int timeout_ms = 100) -> std::optional<RGB> {
  // Check if we have a terminal
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    return std::nullopt;
  }

  // Save terminal settings
  struct termios old_settings{};
  struct termios new_settings{};
  if (tcgetattr(STDIN_FILENO, &old_settings) < 0) {
    return std::nullopt;
  }

  // Set raw mode for reading response
  new_settings = old_settings;
  new_settings.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
  new_settings.c_cc[VMIN] = 0;
  new_settings.c_cc[VTIME] = 0;

  if (tcsetattr(STDIN_FILENO, TCSANOW, &new_settings) < 0) {
    return std::nullopt;
  }

  // Flush any pending input
  tcflush(STDIN_FILENO, TCIFLUSH);

  // Send OSC 11 query: \033]11;?\033\\  (or \007 as terminator)
  const char* query = "\033]11;?\033\\";
  if (write(STDOUT_FILENO, query, 7) != 7) {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
    return std::nullopt;
  }

  // Wait for response with timeout using poll()
  struct pollfd pfd{};
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;

  std::string response;
  response.reserve(64);

  while (true) {
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0) break;  // Timeout or error

    char c{};
    if (read(STDIN_FILENO, &c, 1) != 1) break;
    response += c;

    // Response ends with ST (String Terminator): ESC \ or BEL
    if (response.size() >= 2) {
      auto len = response.size();
      if ((response[len - 2] == '\033' && response[len - 1] == '\\') ||
          response[len - 1] == '\007') {
        break;
      }
    }
    if (response.size() > 100) break;  // Safety limit
  }

  // Restore terminal settings
  tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);

  // Parse response: \033]11;rgb:RRRR/GGGG/BBBB\033\\
  // The RGB values are 16-bit hex (0000-FFFF), we want 8-bit (00-FF)
  auto pos = response.find("rgb:");
  if (pos == std::string::npos) {
    return std::nullopt;
  }

  // Extract hex values
  unsigned int r16 = 0, g16 = 0, b16 = 0;
  if (sscanf(response.c_str() + pos, "rgb:%x/%x/%x", &r16, &g16, &b16) != 3) {
    return std::nullopt;
  }

  // Convert 16-bit to 8-bit (take high byte)
  return RGB{
      static_cast<uint8_t>(r16 >> 8),
      static_cast<uint8_t>(g16 >> 8),
      static_cast<uint8_t>(b16 >> 8),
  };
}
#else
inline auto queryTerminalBackground(int = 100) -> std::optional<RGB> {
  return std::nullopt;  // Not supported on non-Unix
}
#endif

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

// Braille characters for 2x4 cell rendering (octants) — highest resolution
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

  size_t i = 0;
  for (const auto& bar : bars) {
    result += std::format("{:>{}} ", bar.label, label_length);

    if (i == 0) {
      result += "┌";
    } else if (i == bar_count - 1) {
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
    ++i;
  }

  return result;
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

// Multi-series plot (simple, low resolution)
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

// High-resolution multi-series Braille plot with color mixing
// When multiple series occupy the same Braille dot, their colors are blended
inline auto multiPlotBraille(std::vector<Series>& series_list, double min_x,
                             double max_x, int64_t width, int64_t height,
                             bool show_border = true, bool show_axes = true,
                             const std::string& title = "") -> std::string {
  if (width <= 0 || height <= 0) return "";
  if (max_x <= min_x) return "";
  if (series_list.empty()) return "";

  // Sample at 2x4 resolution for Braille rendering
  const int64_t sample_width = 2 * width;
  const int64_t sample_height = 4 * height;
  const auto grid_size = static_cast<size_t>(sample_width * sample_height);

  // Find global y range by sampling all series
  double min_y = std::numeric_limits<double>::max();
  double max_y = std::numeric_limits<double>::lowest();

  // Store y values for each series at each sample point
  std::vector<std::vector<double>> all_y_values;
  all_y_values.reserve(series_list.size());

  for (auto& s : series_list) {
    std::vector<double> y_values;
    y_values.reserve(static_cast<size_t>(sample_width + 1));

    for (int64_t i = 0; i <= sample_width; ++i) {
      const double x =
          min_x + (max_x - min_x) * static_cast<double>(i) / sample_width;
      const double y = s.fn(x);
      y_values.push_back(y);
      min_y = std::min(min_y, y);
      max_y = std::max(max_y, y);
    }
    all_y_values.push_back(std::move(y_values));
  }

  // Ensure non-zero range
  if (max_y <= min_y) {
    const double center = (max_y + min_y) / 2;
    max_y = center + 1.0;
    min_y = center - 1.0;
  }

  // For each Braille dot, track which series contribute (as bitmask per series)
  // We store accumulated RGB values and count for averaging
  struct CellColor {
    uint32_t r_sum = 0;
    uint32_t g_sum = 0;
    uint32_t b_sum = 0;
    uint8_t count = 0;

    void add(const RGB& color) {
      r_sum += color.r;
      g_sum += color.g;
      b_sum += color.b;
      ++count;
    }

    [[nodiscard]] auto blend() const -> RGB {
      if (count == 0) return colors::kDefault;
      return RGB{static_cast<uint8_t>(r_sum / count),
                 static_cast<uint8_t>(g_sum / count),
                 static_cast<uint8_t>(b_sum / count)};
    }
  };

  // For each sample cell, track accumulated color
  std::vector<CellColor> cell_colors(grid_size);
  // Occupancy grid - which cells are "on"
  std::vector<bool> occupancy(grid_size, false);

  // Fill occupancy and colors for each series
  for (size_t s = 0; s < series_list.size(); ++s) {
    const auto& y_values = all_y_values[s];
    const RGB& color = series_list[s].color;

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
        if (idx < grid_size) {
          if (!occupancy[idx]) {
            occupancy[idx] = true;
          }
          cell_colors[idx].add(color);
        }
      }
    }
  }

  // For each character cell, compute the blended color from all contributing
  // Braille dots
  struct CharCell {
    int64_t octant = 0;
    CellColor blended_color;
  };

  std::vector<CharCell> char_cells(static_cast<size_t>(width * height));

  for (int64_t col = 0; col < width; ++col) {
    for (int64_t row = 0; row < height; ++row) {
      const int64_t ox = col * 2;
      const int64_t oy = row * 4;

      auto& cell = char_cells[static_cast<size_t>(col + row * width)];

      // Braille bit layout: rows 0-3 in left column (bits 0-3),
      // rows 0-3 in right column (bits 4-7)
      for (int64_t dy = 0; dy < 4; ++dy) {
        const auto idx_l = static_cast<size_t>(ox + (oy + dy) * sample_width);
        const auto idx_r =
            static_cast<size_t>((ox + 1) + (oy + dy) * sample_width);

        if (idx_l < occupancy.size() && occupancy[idx_l]) {
          cell.octant |= (1 << dy);
          // Add all colors that contributed to this dot
          const auto& cc = cell_colors[idx_l];
          cell.blended_color.r_sum += cc.r_sum;
          cell.blended_color.g_sum += cc.g_sum;
          cell.blended_color.b_sum += cc.b_sum;
          cell.blended_color.count += cc.count;
        }
        if (idx_r < occupancy.size() && occupancy[idx_r]) {
          cell.octant |= (1 << (dy + 4));
          const auto& cc = cell_colors[idx_r];
          cell.blended_color.r_sum += cc.r_sum;
          cell.blended_color.g_sum += cc.g_sum;
          cell.blended_color.b_sum += cc.b_sum;
          cell.blended_color.count += cc.count;
        }
      }
    }
  }

  // Build the plot
  std::vector<std::string> plot(static_cast<size_t>(width * height), " ");

  // Draw x-axis first
  if (show_axes) {
    detail::drawXAxis(plot, min_y, max_y, width, height);
  }

  // Render Braille characters with blended colors
  for (int64_t col = 0; col < width; ++col) {
    for (int64_t row = 0; row < height; ++row) {
      const auto& cell = char_cells[static_cast<size_t>(col + row * width)];
      if (cell.octant != 0) {
        if (auto idx = detail::safeIndex(col, row, width, height)) {
          const RGB blended = cell.blended_color.blend();
          plot[*idx] = blended.wrap(kOctant[cell.octant]);
        }
      }
    }
  }

  // Build result string
  std::string result;
  result.reserve(
      static_cast<size_t>((width + 10) * (height + series_list.size() + 5)));

  if (show_border) {
    detail::drawBorder(result, width, title);
  }

  for (int64_t row = 0; row < height; ++row) {
    if (show_border) result += "│";
    for (int64_t col = 0; col < width; ++col) {
      if (auto idx = detail::safeIndex(col, row, width, height)) {
        result += plot[*idx];
      }
    }
    if (show_border) result += "│";
    result += "\n";
  }

  if (show_border) {
    detail::drawBottomBorder(result, width);
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

// =============================================================================
// 2D Density Plot - Heatmap visualization for point clouds
// =============================================================================

struct DensityPlotOptions {
  int64_t width = 60;
  int64_t height = 20;
  bool show_border = true;
  std::string title;

  // Color gradient from low to high density
  RGB low_color{20, 20, 80};      // Dark blue
  RGB high_color{255, 255, 100};  // Bright yellow

  // Terminal background color - used for empty cells and partial block backgrounds
  // Set this to match your terminal's background for seamless blending
  std::optional<RGB> bg_color;  // If not set, uses low_color

  // Terminal character aspect ratio (height/width). Most terminals ~2.0
  // Set to 1.0 to disable aspect correction
  double char_aspect = 2.0;

  // Optional fixed bounds (auto-computed if not set)
  std::optional<double> min_x;
  std::optional<double> max_x;
  std::optional<double> min_y;
  std::optional<double> max_y;
};

namespace detail {

// Interpolate between two colors
constexpr auto lerpColor(const RGB& a, const RGB& b, double t) -> RGB {
  t = std::clamp(t, 0.0, 1.0);
  return RGB{
      static_cast<uint8_t>(a.r + t * (b.r - a.r)),
      static_cast<uint8_t>(a.g + t * (b.g - a.g)),
      static_cast<uint8_t>(a.b + t * (b.b - a.b)),
  };
}

// Shading characters from empty to full (8 levels)
constexpr const char* kShadeChars[] = {" ", "░", "▒", "▓", "█"};
constexpr size_t kNumShades = 5;

}  // namespace detail

// High-resolution density plot using Braille with color intensity
inline auto densityPlot(const std::vector<Point>& points,
                        const DensityPlotOptions& opts = {}) -> std::string {
  if (opts.width <= 0 || opts.height <= 0) return "";
  if (points.empty()) return "";

  const int64_t width = opts.width;
  const int64_t height = opts.height;

  // Compute data bounds
  double data_min_x = points[0].x, data_max_x = points[0].x;
  double data_min_y = points[0].y, data_max_y = points[0].y;
  for (const auto& p : points) {
    data_min_x = std::min(data_min_x, p.x);
    data_max_x = std::max(data_max_x, p.x);
    data_min_y = std::min(data_min_y, p.y);
    data_max_y = std::max(data_max_y, p.y);
  }

  // Use provided bounds or data bounds with padding
  double min_x = opts.min_x.value_or(data_min_x - (data_max_x - data_min_x) * 0.05 - 0.001);
  double max_x = opts.max_x.value_or(data_max_x + (data_max_x - data_min_x) * 0.05 + 0.001);
  double min_y = opts.min_y.value_or(data_min_y - (data_max_y - data_min_y) * 0.05 - 0.001);
  double max_y = opts.max_y.value_or(data_max_y + (data_max_y - data_min_y) * 0.05 + 0.001);

  double x_range = max_x - min_x;
  double y_range = max_y - min_y;
  if (x_range <= 0 || y_range <= 0) return "";

  // Braille resolution: 2 dots wide × 4 dots tall per character
  // Terminal chars are ~2:1 (H:W), so Braille dots are roughly square on screen
  // Adjust for char aspect: screen_width = width/aspect, screen_height = height
  // For equal visual scaling: x_range/screen_width = y_range/screen_height
  // x_range/(width/aspect) = y_range/height → x_range*aspect/width = y_range/height

  const double screen_width = static_cast<double>(width) / opts.char_aspect;
  const double screen_height = static_cast<double>(height);
  const double x_scale = x_range / screen_width;
  const double y_scale = y_range / screen_height;

  // Expand the smaller dimension to match aspect ratios
  if (x_scale > y_scale && !opts.min_y && !opts.max_y) {
    const double extra = (x_scale * screen_height - y_range) / 2;
    min_y -= extra;
    max_y += extra;
    y_range = max_y - min_y;
  } else if (y_scale > x_scale && !opts.min_x && !opts.max_x) {
    const double extra = (y_scale * screen_width - x_range) / 2;
    min_x -= extra;
    max_x += extra;
    x_range = max_x - min_x;
  }

  // Braille grid dimensions
  const int64_t grid_w = 2 * width;
  const int64_t grid_h = 4 * height;
  const auto grid_size = static_cast<size_t>(grid_w * grid_h);

  // Count samples in each sub-cell
  std::vector<int64_t> counts(grid_size, 0);
  int64_t max_count = 0;

  for (const auto& p : points) {
    auto col = static_cast<int64_t>((p.x - min_x) / x_range * static_cast<double>(grid_w - 1));
    auto row = static_cast<int64_t>((max_y - p.y) / y_range * static_cast<double>(grid_h - 1));
    col = std::clamp(col, int64_t{0}, grid_w - 1);
    row = std::clamp(row, int64_t{0}, grid_h - 1);

    const auto idx = static_cast<size_t>(col + row * grid_w);
    if (idx < counts.size()) {
      ++counts[idx];
      max_count = std::max(max_count, counts[idx]);
    }
  }

  if (max_count == 0) return "";

  // Render each character cell
  std::string result;
  result.reserve(static_cast<size_t>((width + 10) * (height + 3)));

  if (opts.show_border) {
    detail::drawBorder(result, width, opts.title);
  }

  for (int64_t row = 0; row < height; ++row) {
    if (opts.show_border) result += "│";

    for (int64_t col = 0; col < width; ++col) {
      const int64_t bx = col * 2;
      const int64_t by = row * 4;

      // Compute Braille pattern and total density for this cell
      int64_t octant = 0;
      int64_t cell_count = 0;

      for (int64_t dy = 0; dy < 4; ++dy) {
        const auto idx_l = static_cast<size_t>(bx + (by + dy) * grid_w);
        const auto idx_r = static_cast<size_t>((bx + 1) + (by + dy) * grid_w);

        if (idx_l < counts.size() && counts[idx_l] > 0) {
          octant |= (1 << dy);
          cell_count += counts[idx_l];
        }
        if (idx_r < counts.size() && counts[idx_r] > 0) {
          octant |= (1 << (dy + 4));
          cell_count += counts[idx_r];
        }
      }

      if (octant == 0) {
        result += " ";
      } else {
        // Color intensity based on density
        // Use log scale for better visual distribution
        const double density =
            std::log1p(static_cast<double>(cell_count)) /
            std::log1p(static_cast<double>(max_count * 8));
        const RGB color = detail::lerpColor(opts.low_color, opts.high_color,
                                            std::sqrt(density));
        result += color.wrap(kOctant[octant]);
      }
    }

    if (opts.show_border) result += "│";
    result += "\n";
  }

  if (opts.show_border) {
    detail::drawBottomBorder(result, width);
  }

  return result;
}

// Block-based density plot with shading characters
inline auto densityPlotBlocks(const std::vector<Point>& points,
                              const DensityPlotOptions& opts = {})
    -> std::string {
  if (opts.width <= 0 || opts.height <= 0) return "";
  if (points.empty()) return "";

  const int64_t width = opts.width;
  const int64_t height = opts.height;

  // Compute data bounds
  double data_min_x = points[0].x, data_max_x = points[0].x;
  double data_min_y = points[0].y, data_max_y = points[0].y;
  for (const auto& p : points) {
    data_min_x = std::min(data_min_x, p.x);
    data_max_x = std::max(data_max_x, p.x);
    data_min_y = std::min(data_min_y, p.y);
    data_max_y = std::max(data_max_y, p.y);
  }

  double min_x = opts.min_x.value_or(data_min_x - (data_max_x - data_min_x) * 0.05 - 0.001);
  double max_x = opts.max_x.value_or(data_max_x + (data_max_x - data_min_x) * 0.05 + 0.001);
  double min_y = opts.min_y.value_or(data_min_y - (data_max_y - data_min_y) * 0.05 - 0.001);
  double max_y = opts.max_y.value_or(data_max_y + (data_max_y - data_min_y) * 0.05 + 0.001);

  double x_range = max_x - min_x;
  double y_range = max_y - min_y;
  if (x_range <= 0 || y_range <= 0) return "";

  // Aspect ratio correction
  const double screen_width = static_cast<double>(width) / opts.char_aspect;
  const double screen_height = static_cast<double>(height);
  const double x_scale = x_range / screen_width;
  const double y_scale = y_range / screen_height;

  if (x_scale > y_scale && !opts.min_y && !opts.max_y) {
    const double extra = (x_scale * screen_height - y_range) / 2;
    min_y -= extra;
    max_y += extra;
    y_range = max_y - min_y;
  } else if (y_scale > x_scale && !opts.min_x && !opts.max_x) {
    const double extra = (y_scale * screen_width - x_range) / 2;
    min_x -= extra;
    max_x += extra;
    x_range = max_x - min_x;
  }

  // Count samples per cell
  const auto grid_size = static_cast<size_t>(width * height);
  std::vector<int64_t> counts(grid_size, 0);
  int64_t max_count = 0;

  for (const auto& p : points) {
    auto col = static_cast<int64_t>((p.x - min_x) / x_range * static_cast<double>(width));
    auto row = static_cast<int64_t>((max_y - p.y) / y_range * static_cast<double>(height));
    col = std::clamp(col, int64_t{0}, width - 1);
    row = std::clamp(row, int64_t{0}, height - 1);

    const auto idx = static_cast<size_t>(col + row * width);
    ++counts[idx];
    max_count = std::max(max_count, counts[idx]);
  }

  if (max_count == 0) return "";

  std::string result;
  result.reserve(static_cast<size_t>((width + 10) * (height + 3)));

  if (opts.show_border) {
    detail::drawBorder(result, width, opts.title);
  }

  for (int64_t row = 0; row < height; ++row) {
    if (opts.show_border) result += "│";

    for (int64_t col = 0; col < width; ++col) {
      const auto idx = static_cast<size_t>(col + row * width);
      const int64_t count = counts[idx];

      if (count == 0) {
        result += " ";
      } else {
        // Map count to shade level (log scale)
        const double t = std::log1p(static_cast<double>(count)) /
                         std::log1p(static_cast<double>(max_count));
        const auto shade_idx = std::min(
            static_cast<size_t>(std::lround(t * (detail::kNumShades - 1))),
            detail::kNumShades - 1);
        const RGB color = detail::lerpColor(opts.low_color, opts.high_color, t);
        result += color.wrap(detail::kShadeChars[shade_idx]);
      }
    }

    if (opts.show_border) result += "│";
    result += "\n";
  }

  if (opts.show_border) {
    detail::drawBottomBorder(result, width);
  }

  return result;
}

// =============================================================================
// Partial Block Characters for Smooth Shading
// =============================================================================
//
// Unicode provides partial blocks for sub-cell precision:
//
// Horizontal (left fill): ▏▎▍▌▋▊▉█  (1/8 to 8/8)
// Vertical (bottom fill): ▁▂▃▄▅▆▇█  (1/8 to 8/8)
// Half blocks: ▀ (upper), ▄ (lower), ▌ (left), ▐ (right)
//
// Missing from Unicode: right-side eighths, upper eighths (except half)
//
// With fg+bg coloring, we can use these to interpolate between adjacent cells:
// - ▀ with fg=top_color, bg=bottom_color gives 2x vertical resolution
// - ▌ with fg=left_color, bg=right_color gives 2x horizontal resolution
// - Eighth blocks give up to 8x resolution on one edge

namespace detail {

// Horizontal eighth blocks: index 0-7 maps to 1/8 through 8/8 fill from left
constexpr const char* kLeftEighths[] = {"▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"};

// Vertical eighth blocks: index 0-7 maps to 1/8 through 8/8 fill from bottom
constexpr const char* kBottomEighths[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

// Half blocks
constexpr const char* kUpperHalf = "▀";
constexpr const char* kLowerHalf = "▄";
constexpr const char* kLeftHalf = "▌";
constexpr const char* kRightHalf = "▐";
constexpr const char* kFullBlock = "█";

}  // namespace detail

// Smooth heatmap using half blocks for 2x vertical resolution
// Each character cell shows two density values (top and bottom halves)
// Uses fg+bg coloring for smooth gradients
inline auto smoothHeatmap(const std::vector<Point>& points,
                          const DensityPlotOptions& opts = {}) -> std::string {
  if (opts.width <= 0 || opts.height <= 0) return "";
  if (points.empty()) return "";

  const int64_t width = opts.width;
  const int64_t height = opts.height;

  // Compute data bounds
  double data_min_x = points[0].x, data_max_x = points[0].x;
  double data_min_y = points[0].y, data_max_y = points[0].y;
  for (const auto& p : points) {
    data_min_x = std::min(data_min_x, p.x);
    data_max_x = std::max(data_max_x, p.x);
    data_min_y = std::min(data_min_y, p.y);
    data_max_y = std::max(data_max_y, p.y);
  }

  double min_x = opts.min_x.value_or(data_min_x - (data_max_x - data_min_x) * 0.05 - 0.001);
  double max_x = opts.max_x.value_or(data_max_x + (data_max_x - data_min_x) * 0.05 + 0.001);
  double min_y = opts.min_y.value_or(data_min_y - (data_max_y - data_min_y) * 0.05 - 0.001);
  double max_y = opts.max_y.value_or(data_max_y + (data_max_y - data_min_y) * 0.05 + 0.001);

  double x_range = max_x - min_x;
  double y_range = max_y - min_y;
  if (x_range <= 0 || y_range <= 0) return "";

  // Aspect ratio correction
  const double screen_width = static_cast<double>(width) / opts.char_aspect;
  const double screen_height = static_cast<double>(height);
  const double x_scale = x_range / screen_width;
  const double y_scale = y_range / screen_height;

  if (x_scale > y_scale && !opts.min_y && !opts.max_y) {
    const double extra = (x_scale * screen_height - y_range) / 2;
    min_y -= extra;
    max_y += extra;
    y_range = max_y - min_y;
  } else if (y_scale > x_scale && !opts.min_x && !opts.max_x) {
    const double extra = (y_scale * screen_width - x_range) / 2;
    min_x -= extra;
    max_x += extra;
    x_range = max_x - min_x;
  }

  // Grid at 2x vertical resolution (2 rows per character)
  const int64_t grid_w = width;
  const int64_t grid_h = height * 2;
  const auto grid_size = static_cast<size_t>(grid_w * grid_h);

  // Count samples per sub-cell
  std::vector<int64_t> counts(grid_size, 0);
  int64_t max_count = 0;

  for (const auto& p : points) {
    auto col = static_cast<int64_t>((p.x - min_x) / x_range * static_cast<double>(grid_w));
    auto row = static_cast<int64_t>((max_y - p.y) / y_range * static_cast<double>(grid_h));
    col = std::clamp(col, int64_t{0}, grid_w - 1);
    row = std::clamp(row, int64_t{0}, grid_h - 1);

    const auto idx = static_cast<size_t>(col + row * grid_w);
    ++counts[idx];
    max_count = std::max(max_count, counts[idx]);
  }

  if (max_count == 0) return "";

  const double log_max = std::log1p(static_cast<double>(max_count));
  const RGB bg = opts.bg_color.value_or(opts.low_color);

  // Helper to compute color from count
  auto countToColor = [&](int64_t count) -> RGB {
    if (count == 0) return bg;
    const double t = std::sqrt(std::log1p(static_cast<double>(count)) / log_max);
    return detail::lerpColor(opts.low_color, opts.high_color, t);
  };

  std::string result;
  result.reserve(static_cast<size_t>((width + 10) * (height + 3)));

  if (opts.show_border) {
    detail::drawBorder(result, width, opts.title);
  }

  for (int64_t row = 0; row < height; ++row) {
    if (opts.show_border) result += "│";

    for (int64_t col = 0; col < width; ++col) {
      // Top and bottom sub-rows for this character cell
      const auto top_idx = static_cast<size_t>(col + (row * 2) * grid_w);
      const auto bot_idx = static_cast<size_t>(col + (row * 2 + 1) * grid_w);

      const int64_t top_count = counts[top_idx];
      const int64_t bot_count = counts[bot_idx];

      const RGB top_color = countToColor(top_count);
      const RGB bot_color = countToColor(bot_count);

      if (top_count == 0 && bot_count == 0) {
        result += " ";
      } else if (top_color == bot_color) {
        // Same color - use full block
        result += top_color.wrap(detail::kFullBlock);
      } else {
        // Different colors - use upper half block
        // ▀ : foreground = top, background = bottom
        result += top_color.wrapFgBg(detail::kUpperHalf, bot_color);
      }
    }

    if (opts.show_border) result += "│";
    result += "\n";
  }

  if (opts.show_border) {
    detail::drawBottomBorder(result, width);
  }

  return result;
}

// Ultra-smooth heatmap with 8x vertical resolution using eighth blocks
// Uses bottom-fill eighth blocks (▁▂▃▄▅▆▇█) for fine vertical gradients
// Only works well at edges (where background is empty/dark)
inline auto smoothHeatmap8(const std::vector<Point>& points,
                           const DensityPlotOptions& opts = {}) -> std::string {
  if (opts.width <= 0 || opts.height <= 0) return "";
  if (points.empty()) return "";

  const int64_t width = opts.width;
  const int64_t height = opts.height;

  // Compute data bounds
  double data_min_x = points[0].x, data_max_x = points[0].x;
  double data_min_y = points[0].y, data_max_y = points[0].y;
  for (const auto& p : points) {
    data_min_x = std::min(data_min_x, p.x);
    data_max_x = std::max(data_max_x, p.x);
    data_min_y = std::min(data_min_y, p.y);
    data_max_y = std::max(data_max_y, p.y);
  }

  double min_x = opts.min_x.value_or(data_min_x - (data_max_x - data_min_x) * 0.05 - 0.001);
  double max_x = opts.max_x.value_or(data_max_x + (data_max_x - data_min_x) * 0.05 + 0.001);
  double min_y = opts.min_y.value_or(data_min_y - (data_max_y - data_min_y) * 0.05 - 0.001);
  double max_y = opts.max_y.value_or(data_max_y + (data_max_y - data_min_y) * 0.05 + 0.001);

  double x_range = max_x - min_x;
  double y_range = max_y - min_y;
  if (x_range <= 0 || y_range <= 0) return "";

  // Aspect ratio correction
  const double screen_width = static_cast<double>(width) / opts.char_aspect;
  const double screen_height = static_cast<double>(height);
  const double x_scale = x_range / screen_width;
  const double y_scale = y_range / screen_height;

  if (x_scale > y_scale && !opts.min_y && !opts.max_y) {
    const double extra = (x_scale * screen_height - y_range) / 2;
    min_y -= extra;
    max_y += extra;
    y_range = max_y - min_y;
  } else if (y_scale > x_scale && !opts.min_x && !opts.max_x) {
    const double extra = (y_scale * screen_width - x_range) / 2;
    min_x -= extra;
    max_x += extra;
    x_range = max_x - min_x;
  }

  // Grid at 8x vertical resolution
  const int64_t grid_w = width;
  const int64_t grid_h = height * 8;
  const auto grid_size = static_cast<size_t>(grid_w * grid_h);

  // Count samples per sub-cell
  std::vector<int64_t> counts(grid_size, 0);
  int64_t max_count = 0;

  for (const auto& p : points) {
    auto col = static_cast<int64_t>((p.x - min_x) / x_range * static_cast<double>(grid_w));
    auto row = static_cast<int64_t>((max_y - p.y) / y_range * static_cast<double>(grid_h));
    col = std::clamp(col, int64_t{0}, grid_w - 1);
    row = std::clamp(row, int64_t{0}, grid_h - 1);

    const auto idx = static_cast<size_t>(col + row * grid_w);
    ++counts[idx];
    max_count = std::max(max_count, counts[idx]);
  }

  if (max_count == 0) return "";

  const double log_max = std::log1p(static_cast<double>(max_count));
  const RGB bg = opts.bg_color.value_or(opts.low_color);

  auto countToColor = [&](int64_t count) -> RGB {
    if (count == 0) return bg;
    const double t = std::sqrt(std::log1p(static_cast<double>(count)) / log_max);
    return detail::lerpColor(opts.low_color, opts.high_color, t);
  };

  std::string result;
  result.reserve(static_cast<size_t>((width + 10) * (height + 3)));

  if (opts.show_border) {
    detail::drawBorder(result, width, opts.title);
  }

  for (int64_t row = 0; row < height; ++row) {
    if (opts.show_border) result += "│";

    for (int64_t col = 0; col < width; ++col) {
      // Find the 8 sub-rows for this character cell
      // Count how many have samples and find dominant color
      int64_t total_count = 0;
      int64_t filled_eighths = 0;  // How many eighth-rows from bottom have samples

      // Scan from bottom to top (row*8+7 is top, row*8 is bottom in screen coords)
      // But in our grid, higher row index = lower on screen
      for (int64_t dy = 7; dy >= 0; --dy) {
        const auto idx = static_cast<size_t>(col + (row * 8 + dy) * grid_w);
        if (counts[idx] > 0) {
          total_count += counts[idx];
          // Fill from bottom: if sub-row 7 has data, that's bottom of char
          filled_eighths = std::max(filled_eighths, int64_t{8} - dy);
        }
      }

      if (total_count == 0) {
        result += " ";
      } else {
        const RGB color = countToColor(total_count);
        if (filled_eighths >= 8) {
          result += color.wrap(detail::kFullBlock);
        } else {
          // Use eighth block with fg=color, bg=background
          result += color.wrapFgBg(
              detail::kBottomEighths[static_cast<size_t>(filled_eighths - 1)],
              bg);
        }
      }
    }

    if (opts.show_border) result += "│";
    result += "\n";
  }

  if (opts.show_border) {
    detail::drawBottomBorder(result, width);
  }

  return result;
}

// Solid heatmap using full blocks with color intensity
// Best for showing density gradients without pattern artifacts
inline auto heatmap(const std::vector<Point>& points,
                    const DensityPlotOptions& opts = {}) -> std::string {
  if (opts.width <= 0 || opts.height <= 0) return "";
  if (points.empty()) return "";

  const int64_t width = opts.width;
  const int64_t height = opts.height;

  // Compute data bounds
  double data_min_x = points[0].x, data_max_x = points[0].x;
  double data_min_y = points[0].y, data_max_y = points[0].y;
  for (const auto& p : points) {
    data_min_x = std::min(data_min_x, p.x);
    data_max_x = std::max(data_max_x, p.x);
    data_min_y = std::min(data_min_y, p.y);
    data_max_y = std::max(data_max_y, p.y);
  }

  double min_x = opts.min_x.value_or(data_min_x - (data_max_x - data_min_x) * 0.05 - 0.001);
  double max_x = opts.max_x.value_or(data_max_x + (data_max_x - data_min_x) * 0.05 + 0.001);
  double min_y = opts.min_y.value_or(data_min_y - (data_max_y - data_min_y) * 0.05 - 0.001);
  double max_y = opts.max_y.value_or(data_max_y + (data_max_y - data_min_y) * 0.05 + 0.001);

  double x_range = max_x - min_x;
  double y_range = max_y - min_y;
  if (x_range <= 0 || y_range <= 0) return "";

  // Aspect ratio correction
  const double screen_width = static_cast<double>(width) / opts.char_aspect;
  const double screen_height = static_cast<double>(height);
  const double x_scale = x_range / screen_width;
  const double y_scale = y_range / screen_height;

  if (x_scale > y_scale && !opts.min_y && !opts.max_y) {
    const double extra = (x_scale * screen_height - y_range) / 2;
    min_y -= extra;
    max_y += extra;
    y_range = max_y - min_y;
  } else if (y_scale > x_scale && !opts.min_x && !opts.max_x) {
    const double extra = (y_scale * screen_width - x_range) / 2;
    min_x -= extra;
    max_x += extra;
    x_range = max_x - min_x;
  }

  // Count samples per cell
  const auto grid_size = static_cast<size_t>(width * height);
  std::vector<int64_t> counts(grid_size, 0);
  int64_t max_count = 0;

  for (const auto& p : points) {
    auto col = static_cast<int64_t>((p.x - min_x) / x_range * static_cast<double>(width));
    auto row = static_cast<int64_t>((max_y - p.y) / y_range * static_cast<double>(height));
    col = std::clamp(col, int64_t{0}, width - 1);
    row = std::clamp(row, int64_t{0}, height - 1);

    const auto idx = static_cast<size_t>(col + row * width);
    ++counts[idx];
    max_count = std::max(max_count, counts[idx]);
  }

  if (max_count == 0) return "";

  // Use log scale for better visual distribution
  const double log_max = std::log1p(static_cast<double>(max_count));

  std::string result;
  result.reserve(static_cast<size_t>((width + 10) * (height + 3)));

  if (opts.show_border) {
    detail::drawBorder(result, width, opts.title);
  }

  for (int64_t row = 0; row < height; ++row) {
    if (opts.show_border) result += "│";

    for (int64_t col = 0; col < width; ++col) {
      const auto idx = static_cast<size_t>(col + row * width);
      const int64_t count = counts[idx];

      if (count == 0) {
        result += " ";
      } else {
        // Map count to color intensity (log scale with sqrt for visual spread)
        const double t = std::sqrt(std::log1p(static_cast<double>(count)) / log_max);
        const RGB color = detail::lerpColor(opts.low_color, opts.high_color, t);
        result += color.wrap("█");
      }
    }

    if (opts.show_border) result += "│";
    result += "\n";
  }

  if (opts.show_border) {
    detail::drawBottomBorder(result, width);
  }

  return result;
}

}  // namespace tempura
