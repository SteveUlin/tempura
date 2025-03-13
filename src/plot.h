#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

// Unicode Plots

namespace tempura {
struct RGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

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

}  // namespace tempura
