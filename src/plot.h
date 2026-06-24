#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "scalar.h"  // RealFunction

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

// Braille 2×4 cell glyphs, generated rather than transcribed: a hand-typed
// table of 256 lookalike characters had silent dupes (⣠ where ⣰ belonged).
// Octant bit layout: bit dy∈[0,4) is the left column row dy (top→bottom),
// bit dy+4 the right column. Unicode Braille (U+2800) numbers its dots
// 1-2-3-7 down the left, 4-5-6-8 down the right — hence the permutation.
struct Glyph {
  char bytes[4];  // up to 3 UTF-8 bytes + NUL
};

consteval auto makeOctants() -> std::array<Glyph, 256> {
  constexpr unsigned dot_bit[8] = {0x01, 0x02, 0x04, 0x40,   // left  rows 0-3
                                   0x08, 0x10, 0x20, 0x80};  // right rows 0-3
  std::array<Glyph, 256> table{};
  for (unsigned o = 0; o < 256; ++o) {
    unsigned cp = 0x2800;
    for (unsigned b = 0; b < 8; ++b) {
      if (o & (1u << b)) cp |= dot_bit[b];
    }
    table[o].bytes[0] = static_cast<char>(0xE0 | (cp >> 12));
    table[o].bytes[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    table[o].bytes[2] = static_cast<char>(0x80 | (cp & 0x3F));
    table[o].bytes[3] = '\0';
  }
  return table;
}

inline constexpr std::array<Glyph, 256> kOctant = makeOctants();

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
    // Sextant-34 fills the cell's middle third: a thick centered bar that still
    // aligns with the ├ connector. Box-drawing ─/━ are centered but too thin for
    // a bar; this is the deliberate trade of broad font coverage for thickness.
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
  bool show_labels = true;  // numeric y-gutter + x-axis tick row
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

// sRGB ↔ linear light (IEC 61966-2-1), both normalized to [0,1]. Color math —
// averaging, interpolation, LAB conversion — must happen in LINEAR space: sRGB
// is gamma-encoded (more code values in shadows, to match human vision), so
// blending the encoded values directly is ~2× too dark. These are the canonical
// transfer functions; byte-color and LAB code both build on them.
inline auto srgbToLinear(double s) -> double {
  return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
}

inline auto linearToSrgb(double v) -> double {
  return v <= 0.0031308 ? v * 12.92 : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
}

// 8-bit channel ↔ linear: the conveniences the byte-color blenders use.
inline auto channelToLinear(uint8_t c) -> double { return srgbToLinear(c / 255.0); }
inline auto linearToChannel(double v) -> uint8_t {
  return static_cast<uint8_t>(std::clamp(linearToSrgb(v), 0.0, 1.0) * 255.0 + 0.5);
}

// Interpolate between two colors at fraction t∈[0,1], mixed in linear light.
inline auto lerpColor(const RGB& a, const RGB& b, double t) -> RGB {
  t = std::clamp(t, 0.0, 1.0);
  const auto mix = [t](uint8_t x, uint8_t y) {
    const double lx = channelToLinear(x);
    return linearToChannel(lx + t * (channelToLinear(y) - lx));
  };
  return RGB{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
}

// Widen a degenerate range (hi ≤ lo) to a unit interval so constant data still
// renders a sane axis instead of dividing by zero.
inline void ensureRange(double& lo, double& hi) {
  if (hi <= lo) {
    const double c = (lo + hi) / 2;
    lo = c - 1.0;
    hi = c + 1.0;
  }
}

// Display columns of UTF-8 text. Layout math must count columns, not bytes:
// a code point that isn't a continuation byte (0b10xxxxxx) is one column — true
// for the Latin/Greek/math/box/Braille glyphs used here (not CJK double-width).
inline auto displayWidth(std::string_view s) -> int64_t {
  int64_t w = 0;
  for (const unsigned char c : s) w += static_cast<int64_t>((c & 0xC0) != 0x80);
  return w;
}

// Top/bottom frame around a width-column plot; the title is centered on top.
inline void frameTop(std::string& out, int64_t width, std::string_view title) {
  out += "┌";
  const int64_t tw = displayWidth(title);
  if (!title.empty() && tw < width) {
    const auto pad = (width - tw) / 2;
    for (int64_t i = 0; i < pad; ++i) out += "─";
    out += title;
    for (int64_t i = 0; i < width - pad - tw; ++i) out += "─";
  } else {
    for (int64_t i = 0; i < width; ++i) out += "─";
  }
  out += "┐\n";
}

inline void frameBottom(std::string& out, int64_t width) {
  out += "└";
  for (int64_t i = 0; i < width; ++i) out += "─";
  out += "┘\n";
}

// Decimals to render a tick value at, derived from the axis range so all ticks
// share a width: a span of ~2 wants 1 decimal (1.0/0.5/0.0), ~200 wants 0.
inline auto decimalsFor(double range) -> int {
  if (!(range > 0)) return 1;
  return std::clamp(1 - static_cast<int>(std::floor(std::log10(range))), 0, 6);
}

inline auto formatTick(double v, int decimals) -> std::string {
  if (v == 0.0) v = 0.0;  // -0.0 == 0.0 is true, so this normalizes the sign away
  return std::format("{:.{}f}", v, decimals);
}

// "Nice" axis ticks: round the spacing to 1/2/5×10ⁿ so labels read 0, 0.5, 1.0
// rather than landing on arbitrary fractions of the range. Returns the in-range
// tick values plus the decimal count they should print with (from the step, so
// every label shares a width). 0 is always a multiple of the step ⇒ always a tick.
inline auto axisTicks(double lo, double hi, int target)
    -> std::pair<std::vector<double>, int> {
  if (!(hi > lo) || target < 2) return {{lo}, decimalsFor(std::abs(hi - lo))};
  const double raw = (hi - lo) / (target - 1);
  const double mag = std::pow(10.0, std::floor(std::log10(raw)));
  const double norm = raw / mag;  // in [1, 10)
  const double step = (norm <= 1 ? 1.0 : norm <= 2 ? 2.0 : norm <= 5 ? 5.0 : 10.0) * mag;
  const int dec = std::clamp(-static_cast<int>(std::floor(std::log10(step))), 0, 6);
  std::vector<double> ticks;
  for (double t = std::ceil(lo / step) * step; t <= hi + step * 1e-6; t += step) {
    ticks.push_back(std::abs(t) < step * 1e-6 ? 0.0 : t);
  }
  return {ticks, dec};
}

// Data-space bounds a plot is drawn against; carried into render() so the frame
// can label its axes.
struct AxisSpec {
  double min_x, max_x, min_y, max_y;
};

// The x-axis tick row drawn below a plot body: nice-valued labels, each centered
// on the column it maps to, shifted right by the y-gutter (left_offset). Labels
// that would collide are pushed right to keep a ≥1-space gap.
inline auto xTickRow(double min_x, double max_x, int64_t cols, int64_t left_offset)
    -> std::string {
  const auto [ticks, dec] = axisTicks(min_x, max_x, std::clamp<int>(cols / 12 + 1, 2, 6));
  const int64_t den = std::max<int64_t>(cols - 1, 1);
  std::string line(static_cast<size_t>(left_offset), ' ');
  for (const double v : ticks) {
    const int64_t col = std::lround((v - min_x) / (max_x - min_x) * static_cast<double>(den));
    if (col < 0 || col >= cols) continue;
    const std::string lbl = formatTick(v, dec);
    const int64_t centered = left_offset + col - static_cast<int64_t>(lbl.size()) / 2;
    const int64_t floor_start = (static_cast<int64_t>(line.size()) <= left_offset)
                                    ? left_offset
                                    : static_cast<int64_t>(line.size()) + 1;  // ≥1 space gap
    const int64_t start = std::max(centered, floor_start);
    line.append(static_cast<size_t>(start - static_cast<int64_t>(line.size())), ' ');
    line += lbl;
  }
  line += "\n";
  return line;
}

// Maps data coordinates onto an integer pixel grid; row 0 is the TOP (y flips).
struct Viewport {
  double min_x, max_x, min_y, max_y;
  int64_t cols, rows;

  auto col(double x) const -> int64_t {
    if (max_x <= min_x) return cols / 2;
    return static_cast<int64_t>(std::lround((x - min_x) / (max_x - min_x) * (cols - 1)));
  }
  auto row(double y) const -> int64_t {
    if (max_y <= min_y) return rows / 2;
    return static_cast<int64_t>(std::lround((max_y - y) / (max_y - min_y) * (rows - 1)));
  }
};

// A 2×4 Braille dot grid: every character cell packs 8 sub-pixels, so an 80×20
// canvas addresses 160×80 dots. Per-dot color *accumulates*, so where two
// series cross, the cell shows their average — not whichever drew last.
class BrailleCanvas {
 public:
  BrailleCanvas(int64_t char_cols, int64_t char_rows)
      : cols_{char_cols},
        rows_{char_rows},
        dot_cols_{2 * char_cols},
        dot_rows_{4 * char_rows},
        on_(static_cast<size_t>(dot_cols_ * dot_rows_), false),
        acc_(static_cast<size_t>(dot_cols_ * dot_rows_)) {}

  auto dotCols() const -> int64_t { return dot_cols_; }
  auto dotRows() const -> int64_t { return dot_rows_; }

  void dot(int64_t dc, int64_t dr, const RGB& color) {
    if (dc < 0 || dc >= dot_cols_ || dr < 0 || dr >= dot_rows_) return;
    const auto i = static_cast<size_t>(dc + dr * dot_cols_);
    on_[i] = true;
    acc_[i].add(color);
  }

  // Bresenham segment in dot space — connects samples without the staircase a
  // pure vertical fill leaves on shallow slopes.
  void line(int64_t c0, int64_t r0, int64_t c1, int64_t r1, const RGB& color) {
    const int64_t dc = std::abs(c1 - c0);
    const int64_t dr = -std::abs(r1 - r0);
    const int64_t sc = c0 < c1 ? 1 : -1;
    const int64_t sr = r0 < r1 ? 1 : -1;
    int64_t err = dc + dr;
    while (true) {
      dot(c0, r0, color);
      if (c0 == c1 && r0 == r1) break;
      const int64_t e2 = 2 * err;
      if (e2 >= dr) { err += dr; c0 += sc; }
      if (e2 <= dc) { err += dc; r0 += sr; }
    }
  }

  struct Style {
    bool border = true;
    std::string title;
    std::optional<int64_t> axis_row;  // char row to underline with the x-axis
    RGB axis_color = colors::kAxis;
    std::string legend;               // appended verbatim after the frame
    std::optional<AxisSpec> labels;   // when set, draw numeric tick labels
  };

  auto render(const Style& s) const -> std::string {
    // y-gutter: a right-justified value label on a few rows (plus the zero row),
    // shared-width so every label and the spine line up.
    std::vector<std::string> row_label(static_cast<size_t>(rows_));
    int64_t gutter = 0;
    if (s.labels) {
      const auto [ticks, dec] =
          axisTicks(s.labels->min_y, s.labels->max_y, std::clamp<int>(rows_ / 3 + 1, 3, 6));
      const int64_t den = std::max<int64_t>(rows_ - 1, 1);
      const double span = s.labels->max_y - s.labels->min_y;
      for (const double v : ticks) {
        // snap the zero label onto the drawn axis line so ┼ and "0" share a row
        const int64_t r = (v == 0.0 && s.axis_row)
                              ? *s.axis_row
                              : std::lround((s.labels->max_y - v) / span * static_cast<double>(den));
        if (r < 0 || r >= rows_) continue;
        row_label[static_cast<size_t>(r)] = formatTick(v, dec);
      }
      for (const auto& l : row_label) gutter = std::max<int64_t>(gutter, static_cast<int64_t>(l.size()));
    }
    const bool lab = s.labels.has_value();

    // Left spine glyph: ┼ at the zero row, ┤ where a tick label sits, else │.
    auto spine = [&](int64_t r) -> const char* {
      if (s.axis_row && *s.axis_row == r) return "┼";
      return row_label[static_cast<size_t>(r)].empty() ? "│" : "┤";
    };

    std::string out;
    out.reserve(static_cast<size_t>((cols_ + gutter + 6) * (rows_ + 4)) * 3);

    if (lab) {
      out += std::string(static_cast<size_t>(gutter + 1), ' ');
      if (s.border) frameTop(out, cols_, s.title);
    } else if (s.border) {
      frameTop(out, cols_, s.title);
    }

    for (int64_t r = 0; r < rows_; ++r) {
      if (lab) {
        out += std::format("{:>{}} ", row_label[static_cast<size_t>(r)], gutter);
        out += s.axis_color.wrap(spine(r));
      } else if (s.border) {
        out += "│";
      }
      for (int64_t c = 0; c < cols_; ++c) {
        const auto [octant, color] = pack(c, r);
        if (octant != 0) {
          out += color.wrap(kOctant[static_cast<size_t>(octant)].bytes);
        } else if (s.axis_row && *s.axis_row == r) {
          out += s.axis_color.wrap("─");
        } else {
          out += " ";
        }
      }
      if (s.border) out += "│";
      out += "\n";
    }

    if (lab) {
      out += std::string(static_cast<size_t>(gutter + 1), ' ');
      if (s.border) {
        frameBottom(out, cols_);
      } else {
        out += s.axis_color.ansiPrefix();
        out += "└";
        for (int64_t i = 0; i < cols_; ++i) out += "─";
        out += RGB::ansiSuffix();
        out += "\n";
      }
      out += xTickRow(s.labels->min_x, s.labels->max_x, cols_, gutter + 2);
    } else if (s.border) {
      frameBottom(out, cols_);
    }

    out += s.legend;
    return out;
  }

 private:
  // Sums live in LINEAR light, not sRGB bytes, so the average is photometric
  // (a red×blue crossing reads as bright magenta, not a muddy dark purple).
  struct Accum {
    double r = 0, g = 0, b = 0;
    uint32_t n = 0;
    void add(const RGB& c) {
      r += channelToLinear(c.r);
      g += channelToLinear(c.g);
      b += channelToLinear(c.b);
      ++n;
    }
    void merge(const Accum& o) { r += o.r; g += o.g; b += o.b; n += o.n; }
    auto blend(const RGB& fallback) const -> RGB {
      if (n == 0) return fallback;
      const double d = n;
      return RGB{linearToChannel(r / d), linearToChannel(g / d), linearToChannel(b / d)};
    }
  };

  struct Cell {
    int octant;
    RGB color;
  };

  auto pack(int64_t c, int64_t r) const -> Cell {
    int octant = 0;
    Accum a;
    const int64_t ox = c * 2;
    const int64_t oy = r * 4;
    for (int64_t dy = 0; dy < 4; ++dy) {
      const auto il = static_cast<size_t>(ox + (oy + dy) * dot_cols_);
      const auto ir = static_cast<size_t>((ox + 1) + (oy + dy) * dot_cols_);
      if (on_[il]) { octant |= (1 << dy);       a.merge(acc_[il]); }
      if (on_[ir]) { octant |= (1 << (dy + 4)); a.merge(acc_[ir]); }
    }
    return {octant, a.blend(colors::kDefault)};
  }

  int64_t cols_, rows_, dot_cols_, dot_rows_;
  std::vector<bool> on_;
  std::vector<Accum> acc_;
};

inline auto seriesLegend(std::span<const Series> series) -> std::string {
  std::string out = "\n";
  for (const auto& s : series) {
    out += s.color.wrap("●");
    out += " ";
    out += s.label.empty() ? "series" : s.label;
    out += "  ";
  }
  out += "\n";
  return out;
}

// The char row to draw the x-axis on, or none if y=0 is off-screen.
inline auto axisRow(const Viewport& vp, double min_y, double max_y, bool show_axes)
    -> std::optional<int64_t> {
  if (!show_axes || min_y > 0.0 || max_y < 0.0) return std::nullopt;
  return std::clamp(vp.row(0.0), int64_t{0}, vp.rows - 1) / 4;
}

// The ─/┼ axis glyph renders at a character cell's vertical *center*, but the
// curve is drawn at 4× that resolution (Braille dots), so y=0 lands anywhere in
// a cell and the line floats up to half a cell off true zero. Fix it at the
// source: expand the y-range (never clipping data) so y=0 falls exactly on a
// cell center — dot row 4c+1.5 — where the glyph actually sits. Skip the rare
// blow-up when zero is inside the outermost half-cell (small error, near an edge).
inline void snapZeroToAxis(double& min_y, double& max_y, int64_t height) {
  if (!(min_y < 0.0 && max_y > 0.0) || height < 1) return;
  const double range = max_y - min_y;
  const double below = -min_y;
  const double f = max_y / range;  // zero's fraction of the way down from the top
  const double den = std::max<double>(4 * height - 1, 1);
  const int64_t c = std::clamp<int64_t>(std::lround((f * den - 1.5) / 4.0), 0, height - 1);
  const double p = (4.0 * c + 1.5) / den;  // target fraction: center of cell row c
  double lo = min_y;
  double hi = max_y;
  if (p >= f) {
    hi = p * below / (1.0 - p);  // expand upward to push zero down to the center
  } else {
    lo = -(max_y * (1.0 - p) / p);  // expand downward
  }
  if (hi - lo <= range * 1.5) {  // reject pathological expansion near an edge
    min_y = lo;
    max_y = hi;
  }
}

// High-resolution function plot. Samples one point per dot column and connects
// consecutive samples with a Bresenham segment.
template <RealFunction F>
inline auto plotFn(F&& fn, double min_x, double max_x, const PlotOptions& opts)
    -> std::string {
  if (opts.width <= 0 || opts.height <= 0 || max_x <= min_x) return "";

  BrailleCanvas canvas{opts.width, opts.height};
  const int64_t n = canvas.dotCols();

  std::vector<double> ys(static_cast<size_t>(n));
  double min_y = std::numeric_limits<double>::max();
  double max_y = std::numeric_limits<double>::lowest();
  for (int64_t i = 0; i < n; ++i) {
    const double x = min_x + (max_x - min_x) * static_cast<double>(i) / (n - 1);
    const double y = fn(x);
    ys[static_cast<size_t>(i)] = y;
    min_y = std::min(min_y, y);
    max_y = std::max(max_y, y);
  }
  ensureRange(min_y, max_y);
  if (opts.show_axes) snapZeroToAxis(min_y, max_y, opts.height);

  const Viewport vp{min_x, max_x, min_y, max_y, n, canvas.dotRows()};
  int64_t prev = std::clamp(vp.row(ys[0]), int64_t{0}, vp.rows - 1);
  for (int64_t i = 0; i < n; ++i) {
    const int64_t row = std::clamp(vp.row(ys[static_cast<size_t>(i)]), int64_t{0}, vp.rows - 1);
    if (i == 0) {
      canvas.dot(i, row, opts.color);
    } else {
      canvas.line(i - 1, prev, i, row, opts.color);
    }
    prev = row;
  }

  BrailleCanvas::Style style;
  style.border = opts.show_border;
  style.title = opts.title;
  style.axis_row = axisRow(vp, min_y, max_y, opts.show_axes);
  if (opts.show_labels) style.labels = AxisSpec{min_x, max_x, min_y, max_y};
  std::string out = canvas.render(style);

  if (!opts.x_label.empty()) {
    const auto pad = std::max(int64_t{0}, (opts.width - displayWidth(opts.x_label)) / 2 + 1);
    out += std::string(static_cast<size_t>(pad), ' ');
    out += opts.x_label;
    out += "\n";
  }
  return out;
}

template <RealFunction F>
inline auto plotFn(F&& fn, double min_x, double max_x, int64_t width,
                   int64_t height, std::optional<RGB> color = std::nullopt)
    -> std::string {
  PlotOptions opts;
  opts.width = width;
  opts.height = height;
  if (color) opts.color = *color;
  return plotFn(std::forward<F>(fn), min_x, max_x, opts);
}

// Several functions on shared axes; overlapping dots blend their colors, and a
// legend lists each series. Series.fn is type-erased — a heterogeneous list of
// callables is the one place type erasure earns its keep here.
inline auto linesPlot(std::span<const Series> series, double min_x, double max_x,
                      const PlotOptions& opts = {}) -> std::string {
  if (opts.width <= 0 || opts.height <= 0 || max_x <= min_x || series.empty()) return "";

  BrailleCanvas canvas{opts.width, opts.height};
  const int64_t n = canvas.dotCols();

  std::vector<std::vector<double>> ys(series.size());
  double min_y = std::numeric_limits<double>::max();
  double max_y = std::numeric_limits<double>::lowest();
  for (size_t s = 0; s < series.size(); ++s) {
    ys[s].resize(static_cast<size_t>(n));
    for (int64_t i = 0; i < n; ++i) {
      const double x = min_x + (max_x - min_x) * static_cast<double>(i) / (n - 1);
      const double y = series[s].fn(x);
      ys[s][static_cast<size_t>(i)] = y;
      min_y = std::min(min_y, y);
      max_y = std::max(max_y, y);
    }
  }
  ensureRange(min_y, max_y);
  if (opts.show_axes) snapZeroToAxis(min_y, max_y, opts.height);

  const Viewport vp{min_x, max_x, min_y, max_y, n, canvas.dotRows()};
  for (size_t s = 0; s < series.size(); ++s) {
    int64_t prev = std::clamp(vp.row(ys[s][0]), int64_t{0}, vp.rows - 1);
    for (int64_t i = 0; i < n; ++i) {
      const int64_t row = std::clamp(vp.row(ys[s][static_cast<size_t>(i)]), int64_t{0}, vp.rows - 1);
      if (i == 0) {
        canvas.dot(i, row, series[s].color);
      } else {
        canvas.line(i - 1, prev, i, row, series[s].color);
      }
      prev = row;
    }
  }

  BrailleCanvas::Style style;
  style.border = opts.show_border;
  style.title = opts.title;
  style.axis_row = axisRow(vp, min_y, max_y, opts.show_axes);
  if (opts.show_labels) style.labels = AxisSpec{min_x, max_x, min_y, max_y};
  style.legend = seriesLegend(series);
  return canvas.render(style);
}

// Discrete points, auto-scaled with 5% padding on each axis.
inline auto scatterPlot(std::span<const Point> points,
                        const PlotOptions& opts = {}) -> std::string {
  if (opts.width <= 0 || opts.height <= 0 || points.empty()) return "";

  double min_x = points[0].x, max_x = points[0].x;
  double min_y = points[0].y, max_y = points[0].y;
  for (const auto& p : points) {
    min_x = std::min(min_x, p.x);
    max_x = std::max(max_x, p.x);
    min_y = std::min(min_y, p.y);
    max_y = std::max(max_y, p.y);
  }
  const double x_pad = (max_x > min_x) ? (max_x - min_x) * 0.05 : 0.5;
  const double y_pad = (max_y > min_y) ? (max_y - min_y) * 0.05 : 0.5;
  min_x -= x_pad;
  max_x += x_pad;
  min_y -= y_pad;
  max_y += y_pad;
  if (opts.show_axes) snapZeroToAxis(min_y, max_y, opts.height);

  BrailleCanvas canvas{opts.width, opts.height};
  const Viewport vp{min_x, max_x, min_y, max_y, canvas.dotCols(), canvas.dotRows()};
  for (const auto& p : points) canvas.dot(vp.col(p.x), vp.row(p.y), opts.color);

  BrailleCanvas::Style style;
  style.border = opts.show_border;
  style.title = opts.title;
  style.axis_row = axisRow(vp, min_y, max_y, opts.show_axes);
  if (opts.show_labels) style.labels = AxisSpec{min_x, max_x, min_y, max_y};
  return canvas.render(style);
}

struct DensityPlotOptions {
  int64_t width = 60;
  int64_t height = 20;
  bool show_border = true;
  std::string title;

  RGB low_color{20, 20, 80};      // dark blue
  RGB high_color{255, 255, 100};  // bright yellow

  // Terminal cells are ~2× taller than wide; correcting keeps a round cloud
  // round instead of squashed. Set to 1.0 to disable.
  double char_aspect = 2.0;

  // Fixed bounds (auto-fit when unset).
  std::optional<double> min_x, max_x, min_y, max_y;
};

// Auto-fit point bounds with 5% padding, then expand the looser axis so equal
// data distances map to equal screen distances (after char_aspect correction).
// Any explicitly-set bound is honored and never expanded.
inline auto fitBounds(std::span<const Point> pts, const DensityPlotOptions& o)
    -> std::array<double, 4> {
  double dminx = pts[0].x, dmaxx = pts[0].x, dminy = pts[0].y, dmaxy = pts[0].y;
  for (const auto& p : pts) {
    dminx = std::min(dminx, p.x);
    dmaxx = std::max(dmaxx, p.x);
    dminy = std::min(dminy, p.y);
    dmaxy = std::max(dmaxy, p.y);
  }
  double min_x = o.min_x.value_or(dminx - (dmaxx - dminx) * 0.05 - 0.001);
  double max_x = o.max_x.value_or(dmaxx + (dmaxx - dminx) * 0.05 + 0.001);
  double min_y = o.min_y.value_or(dminy - (dmaxy - dminy) * 0.05 - 0.001);
  double max_y = o.max_y.value_or(dmaxy + (dmaxy - dminy) * 0.05 + 0.001);

  const double sw = static_cast<double>(o.width) / o.char_aspect;
  const double sh = static_cast<double>(o.height);
  const double xs = (max_x - min_x) / sw;
  const double ys = (max_y - min_y) / sh;
  if (xs > ys && !o.min_y && !o.max_y) {
    const double extra = (xs * sh - (max_y - min_y)) / 2;
    min_y -= extra;
    max_y += extra;
  } else if (ys > xs && !o.min_x && !o.max_x) {
    const double extra = (ys * sw - (max_x - min_x)) / 2;
    min_x -= extra;
    max_x += extra;
  }
  return {min_x, max_x, min_y, max_y};
}

// Density of a point cloud as a solid color field — log-compressed counts on a
// low→high gradient. The deliberately low-fidelity baseline; block_graphics.h's
// LAB-space hiresHeatmap is the high-quality variant.
inline auto heatmap(std::span<const Point> points,
                    const DensityPlotOptions& opts = {}) -> std::string {
  if (opts.width <= 0 || opts.height <= 0 || points.empty()) return "";
  const auto [min_x, max_x, min_y, max_y] = fitBounds(points, opts);
  const double xr = max_x - min_x;
  const double yr = max_y - min_y;
  if (xr <= 0 || yr <= 0) return "";

  const int64_t w = opts.width;
  const int64_t h = opts.height;
  std::vector<int64_t> counts(static_cast<size_t>(w * h), 0);
  int64_t max_count = 0;
  for (const auto& p : points) {
    auto col = static_cast<int64_t>((p.x - min_x) / xr * static_cast<double>(w));
    auto row = static_cast<int64_t>((max_y - p.y) / yr * static_cast<double>(h));
    col = std::clamp(col, int64_t{0}, w - 1);
    row = std::clamp(row, int64_t{0}, h - 1);
    max_count = std::max(max_count, ++counts[static_cast<size_t>(col + row * w)]);
  }
  if (max_count == 0) return "";
  const double log_max = std::log1p(static_cast<double>(max_count));

  std::string out;
  out.reserve(static_cast<size_t>((w + 4) * (h + 3)) * 3);
  if (opts.show_border) frameTop(out, w, opts.title);
  for (int64_t row = 0; row < h; ++row) {
    if (opts.show_border) out += "│";
    for (int64_t col = 0; col < w; ++col) {
      const int64_t count = counts[static_cast<size_t>(col + row * w)];
      if (count == 0) {
        out += " ";
      } else {
        const double t = std::sqrt(std::log1p(static_cast<double>(count)) / log_max);
        out += lerpColor(opts.low_color, opts.high_color, t).wrap("█");
      }
    }
    if (opts.show_border) out += "│";
    out += "\n";
  }
  if (opts.show_border) frameBottom(out, w);
  return out;
}

// A scalar field f(x, y) over [x0,x1]×[y0,y1] as a color heatmap: each value is
// normalized to the sampled [min,max] and mapped low→high. For loss surfaces,
// |f|, basins of attraction, a matrix entry-magnitude map, …
template <typename F>
  requires std::invocable<F, double, double>
inline auto heatmapFn(F&& f, double x0, double x1, double y0, double y1,
                      const DensityPlotOptions& opts = {}) -> std::string {
  if (opts.width <= 0 || opts.height <= 0 || x1 <= x0 || y1 <= y0) return "";
  const int64_t w = opts.width;
  const int64_t h = opts.height;

  std::vector<double> vals(static_cast<size_t>(w * h));
  double lo = std::numeric_limits<double>::max();
  double hi = std::numeric_limits<double>::lowest();
  for (int64_t row = 0; row < h; ++row) {
    const double y = y1 - (y1 - y0) * (static_cast<double>(row) + 0.5) / static_cast<double>(h);
    for (int64_t col = 0; col < w; ++col) {
      const double x = x0 + (x1 - x0) * (static_cast<double>(col) + 0.5) / static_cast<double>(w);
      const double v = f(x, y);
      vals[static_cast<size_t>(col + row * w)] = v;
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
  }
  ensureRange(lo, hi);

  std::string out;
  out.reserve(static_cast<size_t>((w + 4) * (h + 3)) * 3);
  if (opts.show_border) frameTop(out, w, opts.title);
  for (int64_t row = 0; row < h; ++row) {
    if (opts.show_border) out += "│";
    for (int64_t col = 0; col < w; ++col) {
      const double t = (vals[static_cast<size_t>(col + row * w)] - lo) / (hi - lo);
      out += lerpColor(opts.low_color, opts.high_color, t).wrap("█");
    }
    if (opts.show_border) out += "│";
    out += "\n";
  }
  if (opts.show_border) frameBottom(out, w);
  return out;
}

// Bottom-fill eighth blocks: index 0→7 is 1/8→8/8 of a cell filled from below.
inline constexpr const char* kBottomEighths[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

// One-line trend: each value picks an eighth-block by its height in [min,max].
// No frame — meant to sit inline in a log line or a table cell.
inline auto sparkline(std::span<const double> values,
                      std::optional<RGB> color = std::nullopt) -> std::string {
  if (values.empty()) return "";
  const double lo = std::ranges::min(values);
  const double hi = std::ranges::max(values);
  const double range = (hi > lo) ? hi - lo : 1.0;
  std::string out;
  for (const double v : values) {
    const auto level = std::clamp(
        static_cast<int64_t>((v - lo) / range * 7.0 + 0.5), int64_t{0}, int64_t{7});
    out += kBottomEighths[level];
  }
  return color ? color->wrap(out) : out;
}

struct BarPlotOptions {
  int64_t height = 12;
  RGB low_color{80, 90, 220};
  RGB high_color{255, 120, 90};
  bool show_border = true;
  std::string title;
};

// Vertical bars with eighth-block sub-row precision; each bar is colored by its
// height on a low→high gradient. Values ≤ 0 render as an empty column.
inline auto barPlot(std::span<const double> values, const BarPlotOptions& opts = {})
    -> std::string {
  if (values.empty() || opts.height <= 0) return "";
  const double hi = std::max(0.0, std::ranges::max(values));
  if (hi <= 0) return "";

  const int64_t w = static_cast<int64_t>(values.size());
  const int64_t h = opts.height;

  std::string out;
  out.reserve(static_cast<size_t>((w + 4) * (h + 3)) * 3);
  if (opts.show_border) frameTop(out, w, opts.title);
  for (int64_t row = 0; row < h; ++row) {
    if (opts.show_border) out += "│";
    for (const double v : values) {
      const double frac = std::clamp(v, 0.0, hi) / hi;
      const int64_t eighths = std::lround(frac * static_cast<double>(8 * h));
      const int64_t top = 8 * (h - row);      // eighths spanned at the top of this row
      const int64_t bot = 8 * (h - row - 1);  // …and at its bottom
      const RGB color = lerpColor(opts.low_color, opts.high_color, frac);
      if (eighths >= top) {
        out += color.wrap("█");
      } else if (eighths <= bot) {
        out += " ";
      } else {
        out += color.wrap(kBottomEighths[static_cast<size_t>(eighths - bot - 1)]);
      }
    }
    if (opts.show_border) out += "│";
    out += "\n";
  }
  if (opts.show_border) frameBottom(out, w);
  return out;
}

}  // namespace tempura
