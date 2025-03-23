#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <iterator>
#include <print>
#include <ranges>
#include <string>
#include <vector>

#include "root_finding.h"

// Unicode Plots
//
// TODO:
//  - Make plotting work with empty ranges
//  - Can I query for the size of the terminal and set the width of the plot
//    to a value that won't overflow the terminal boundary?

namespace tempura {
struct RGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  constexpr auto wrap(std::string_view text) const -> std::string {
    return std::format("{}{}{}", ansiPrefix(), text, ansiSuffix());
  }

  constexpr auto ansiPrefix() const -> std::string {
    return std::format("\033[38;2;{};{};{}m", r, g, b);
  }

  constexpr auto ansiSuffix() const -> std::string { return "\033[0m"; }
};

// A string containing the unicode characters breaking a cell into 4 quadrants
// it is orders such that in binary the bits are ordered as follows:
// TL, TR, BL, BR
constexpr auto kQuadrents = u8" ▘▝▀▖▌▞▛▗▚▐▜▄▙▟█";

// The same but for 3x2 cells
// TL, TR, ML, MR, BL, BR
constexpr const char* kSextant[] = {" ", "🬀", "🬁", "🬂", "🬃", "🬄", "🬅", "🬆",
                                    "🬇", "🬈", "🬉", "🬊", "🬋", "🬌", "🬍", "🬎",

                                    "🬏", "🬐", "🬑", "🬒", "🬓", "▌", "🬔", "🬕",
                                    "🬖", "🬗", "🬘", "🬙", "🬚", "🬛", "🬜", "🬝",

                                    "🬞", "🬟", "🬠", "🬡", "🬢", "🬣", "🬤", "🬥",
                                    "🬦", "🬧", "▐", "🬨", "🬩", "🬪", "🬫", "🬬",
                                    "🬭", "🬮", "🬯", "🬰", "🬱", "🬲", "🬳", "🬴",
                                    "🬵", "🬶", "🬷", "🬸", "🬹", "🬺", "🬻", "█"};

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
  // How many chars to use for the bar
  int64_t length;
  // Text to the left of the bar
  std::string label;
  // A string to display after the bar
  std::string end_text;
  // A color to use for the bar
  std::optional<RGB> color;
};

constexpr auto buildBartChartText(auto&& bars) -> std::string {
  const int64_t label_length =
      std::ranges::max(bars | std::views::transform([](const auto& x) {
                         return x.label.size();
                       }));

  std::string result;
  for (auto [i, bar] : bars | std::views::enumerate | std::views::as_const) {
    result += std::format("{:>{}} ", bar.label, label_length);

    if (i == 0) {
      result += "┌";
    } else if (static_cast<size_t>(i) == std::size(bars) - 1) {
      result += "└";
    } else {
      result += "├";
    }

    if (bar.color.has_value()) {
      result += std::format("\033[38;2;{};{};{}m", bar.color->r, bar.color->g,
                            bar.color->b);
    }
    for (int64_t j = 0; j < bar.length; ++j) {
      result += "🬋";
    }
    if (bar.color.has_value()) {
      result += "\033[0m";
    }

    result += std::format(" {}\n", bar.end_text);
  }

  return result;
}

struct TextHistogramOptions {
  int64_t bins = 10;
  // The width of the histogram bar region
  int64_t width = 60;
  std::optional<RGB> color = RGB{.r = 113, .g = 144, .b = 110};

  std::optional<double> min_x = std::nullopt;
  std::optional<double> max_x = std::nullopt;

  // TODO: turn this into a variant
  int64_t min_y = 0;
  std::optional<int64_t> max_y = std::nullopt;

  bool normalize = false;
};

constexpr auto generateTextHistogram(auto&& data,
                                     TextHistogramOptions options = {})
    -> std::string {
  if (std::empty(data)) return "";

  if (!options.min_x.has_value()) {
    options.min_x = std::ranges::min(data);
  }
  if (!options.max_x.has_value()) {
    options.max_x = std::ranges::max(data);
  }

  std::vector<int64_t> buckets(options.bins, 0);
  for (const auto& x : data) {
    if (x < *options.min_x) {
      continue;
    }
    if (x >= *options.max_x) {
      continue;
    }
    int64_t bucket =
        (x - *options.min_x) / (*options.max_x - *options.min_x) * options.bins;
    assert(0 <= bucket && bucket < options.bins);
    buckets[bucket]++;
  }

  if (!options.max_y.has_value()) {
    options.max_y = *std::ranges::max_element(buckets);
  }

  std::vector<TextBar> bars;
  for (size_t i = 0; i < buckets.size(); ++i) {
    double a = *options.min_x +
               (static_cast<double>(i) * (*options.max_x - *options.min_x) /
                static_cast<double>(options.bins));
    double b = *options.min_x + ((static_cast<double>(i) + 1) *
                                 (*options.max_x - *options.min_x) /
                                 static_cast<double>(options.bins));
    std::string label = std::format("{:.2f} - {:.2f}", a, b);
    auto length =
        static_cast<int64_t>(options.width * buckets[i]) / *options.max_y;
    std::string end_text;
    if (options.normalize) {
      end_text = std::format(
          "{:.2f}", static_cast<double>(buckets[i]) / std::ranges::size(data));
    } else {
      end_text = std::format("{}", buckets[i]);
    }
    bars.push_back({length, label, end_text, options.color});
  }
  return buildBartChartText(bars);
}

constexpr auto plotFn(std::function<double(double)>& fn, double min_x,
                      double max_x, int64_t width, int64_t height,
                      std::optional<RGB> color = std::nullopt) {
  // Using octants we get 2x the width of a char. Drawing very steep lines
  // may involve drawing multiple characters in the same column. We evaluate
  // the function at the endpoints of the columns and color in all cells
  // between the two y values.
  std::vector<std::pair<double, double>> data;
  data.reserve(2 * width + 1);
  for (int64_t i = 0; i < 2 * width + 1; ++i) {
    const double x = min_x + (max_x - min_x) * static_cast<double>(i) /
                                 static_cast<double>(2 * width);
    data.emplace_back(x, fn(x));
  }

  double max_y = std::ranges::max(data | std::views::elements<1>);
  double min_y = std::ranges::min(data | std::views::elements<1>);

  // add space so zero is always in the middle of grid cell so we can draw the
  // the x-axis with ―
  if (min_y <= 0 && max_y >= 0) {
    double length = max_y - min_y;
    double cell_height = length / (4 * height);
    double delta = std::fmod(max_y, cell_height);
    double offset = (delta + std::abs(delta) / delta * cell_height);
    if (offset < 0.) {
      double x = -offset / (1.0 - (max_y - offset) / length);
      max_y += x;
    } else {
      double x = offset / (1.0 - (min_y + offset) / length);
      min_y += x;
    }
  }

  // A bit representation of each "on" cell in the plot
  std::vector<bool> occupency((width * 2) * (height * 4), false);
  for (int64_t i = 0; i < 2 * width; ++i) {
    auto& [x0, y0] = data[i];
    auto& [x1, y1] = data[i + 1];
    // Note that the y-axis is flipped so higher rows are lower y values
    int64_t row0 =
        std::round((4 * height - 1) * (1.0 - (y0 - min_y) / (max_y - min_y)));
    int64_t row1 =
        std::round((4 * height - 1) * (1.0 - (y1 - min_y) / (max_y - min_y)));

    for (int64_t j = std::max(0L, std::min(row0, row1));
         j <= std::min(4 * height, std::max(row0, row1)); ++j) {
      occupency[i + j * 2 * width] = true;
    }
  }

  // Text representation of the plot
  std::vector<std::string> plot(width * height, " ");
  // Draw the y-axis
  // if (min_x <= 0 && max_x >= 0) {
  //   const int64_t col = width;
  //   for (int64_t i = 0; i < height; ++i) {
  //     plot[i * 3][col] = RGB{.r = 113, .g = 144, .b = 110}.wrap("│");
  //   }
  // }
  // If 0 is in the range we need to draw the x-axis
  if (min_y <= 0 && max_y >= 0) {
    // Which row to draw the x-axis on
    const auto row =
        static_cast<int64_t>(std::floor(height * (max_y) / (max_y - min_y)));
    if (0 <= row && row < height) {
      for (int64_t i = 0; i < width; ++i) {
        plot[i + row * width] = RGB{.r = 113, .g = 144, .b = 110}.wrap("―");
      }
    }
  }

  std::string result;
  for (int64_t i = 0; i < width * 2; i += 2) {
    for (int64_t j = 0; j < height * 4; j += 4) {
      const int64_t octant = occupency[i + j * 2 * width] +
                             occupency[i + (j + 1) * 2 * width] * 2 +
                             occupency[i + (j + 2) * 2 * width] * 4 +
                             occupency[i + (j + 3) * 2 * width] * 8 +
                             occupency[(i + 1) + j * 2 * width] * 16 +
                             occupency[(i + 1) + (j + 1) * 2 * width] * 32 +
                             occupency[(i + 1) + (j + 2) * 2 * width] * 64 +
                             occupency[(i + 1) + (j + 3) * 2 * width] * 128;

      if (octant != 0) {
        plot[(i / 2) + (j / 4 * width)] =
            RGB{.r = 200, .g = 200, .b = 200}.wrap(kOctant[octant]);
      }
    }
  }
  if (min_y <= 0 && max_y >= 0) {
    // Which row to draw the x-axis on
    const auto row =
        static_cast<int64_t>(std::floor(height * (max_y) / (max_y - min_y)));
    const std::vector<root_finding::Interval> intervals =
        root_finding::subIntervalSignChange(fn, {.a = min_x, .b = max_x},
                                            width);
    for (const auto& inteval : intervals) {
      const auto [a, b] = root_finding::bisectRoot(fn, inteval);
      auto x = (a + b) / 2;
      std::println("x: {}", x);
      auto col = std::floor((width) * (x - min_x) / (max_x - min_x));
      if (col == width) {
        col--;
      }
      plot[col + row * width] = RGB{.r = 200, .g = 200, .b = 200}.wrap("×");
    }
  }

  result += "┌";
  for (int64_t i = 0; i < width; ++i) {
    result += "―";
  }
  result += "┐\n";
  for (int64_t j = 0; j < height; ++j) {
    result += "│";
    for (int64_t i = 0; i < width; ++i) {
      result += plot[i + j * width];
    }
    result += "│\n";
  }
  result += "└";
  for (int64_t i = 0; i < width; ++i) {
    result += "―";
  }
  result += "┘\n";

  return result;
}

constexpr auto linePlot(std::function<double(double)>& fn, double min_x,
                        double max_x, int64_t width, int64_t height,
                        std::optional<RGB> color = std::nullopt) {
  std::vector<std::pair<double, double>> data;
  data.reserve(width + 1);
  for (int64_t i = 0; i < width + 1; ++i) {
    const double x = min_x + (max_x - min_x) * static_cast<double>(i) /
                                 static_cast<double>(width);
    data.emplace_back(x, fn(x));
  }
  auto max_y = std::ranges::max(data | std::views::elements<1>);
  auto min_y = std::ranges::min(data | std::views::elements<1>);

  std::vector<std::string> plot(width * height, " ");

  if (min_y <= 0 && max_y >= 0) {
    // Which row to draw the x-axis on
    const auto row = -1 + static_cast<int64_t>(
        std::floor((height - 1) * (max_y) / (max_y - min_y)));
    if (0 <= row && row < height) {
      for (int64_t i = 0; i < width; ++i) {
        plot[i + row * width] = RGB{.r = 113, .g = 144, .b = 110}.wrap("");
      }
    }
  }

  int64_t prev =
      std::floor((height - 1) * (max_y - data[0].second) / (max_y - min_y));
  for (int i = 0; i < width + 1; ++i) {
    const double y = data[i].second;
    const int64_t row =
        std::floor((height - 1) * ((max_y - y) / (max_y - min_y)));
    if (prev == row) {
      plot[i + prev * width] = RGB{.r = 200, .g = 200, .b = 200}.wrap("");
      prev = row;
      continue;
    }
    if (prev < row) {
      plot[i + prev * width] = RGB{.r = 200, .g = 200, .b = 200}.wrap("");
      plot[i + row * width] = RGB{.r = 200, .g = 200, .b = 200}.wrap("");
      for (int64_t j = prev + 1; j < row; ++j) {
        plot[i + j * width] = RGB{.r = 200, .g = 200, .b = 200}.wrap("");
      }
    } else {
      plot[i + prev * width] = RGB{.r = 200, .g = 200, .b = 200}.wrap("");
      plot[i + row * width] = RGB{.r = 200, .g = 200, .b = 200}.wrap("");
      for (int64_t j = row + 1; j < prev; ++j) {
        plot[i + j * width] = RGB{.r = 200, .g = 200, .b = 200}.wrap("");
      }
    }
    prev = row;
  }

  if (min_y <= 0 && max_y >= 0) {
    // Which row to draw the x-axis on
    const auto row = -1 + static_cast<int64_t>(
        std::floor((height - 1) * (max_y) / (max_y - min_y)));
    const std::vector<root_finding::Interval> intervals =
        root_finding::subIntervalSignChange(fn, {.a = min_x, .b = max_x},
                                            width);
    for (const auto& inteval : intervals) {
      const auto [a, b] = root_finding::bisectRoot(fn, inteval);
      auto x = (a + b) / 2;
      std::println("x: {}", x);
      auto col = std::floor((width) * (x - min_x) / (max_x - min_x));
      if (col == width) {
        col--;
      }
      plot[col + row * width] = RGB{.r = 200, .g = 200, .b = 200}.wrap("×");
    }
  }

  std::string result;
  for (int j = 0; j < height; ++j) {
    for (int i = 0; i < width; ++i) {
      result += plot[i + j * width];
    }
    result += "\n";
  }
  return result;
}

}  // namespace tempura
