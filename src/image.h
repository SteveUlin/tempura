#pragma once

#include <cassert>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <ranges>

namespace tempura {

struct GreyScalePixel {
  // The value of the pixel in the range [0, 1].
  double value;
};

// https://netpbm.sourceforge.net/doc/pgm.html
struct PgmOptions {
  size_t width;
  size_t height;
  // Maximum value of the output pixel in the range [0, 65535].
  uint16_t max_val = 255;
};

template <typename R>
  requires std::ranges::range<R> &&
           std::is_same_v<std::ranges::range_value_t<R>, GreyScalePixel>
auto encodePgm(PgmOptions options, R&& range) -> std::string {
  const size_t num_pixels = options.width * options.height;
  if (std::ranges::size(range) != num_pixels) {
    abort();
  }
  std::string width = std::to_string(options.width);
  std::string height = std::to_string(options.height);
  std::string max_val = std::to_string(options.max_val);

  bool two_bytes_pixels = options.max_val > 255;
  std::string image;
  image.reserve(/*magic_number=*/2 + /*white space=*/4 + width.size() +
                height.size() + max_val.size() +
                num_pixels * (two_bytes_pixels ? 2 : 1));

  image += "P5\n";
  image += width;
  image += ' ';
  image += height;
  image += '\n';
  image += max_val;
  image += '\n';

  for (const auto& pixel : range) {
    if (pixel.value < 0.0 || 1.0 < pixel.value) {
      abort();
    }
    uint16_t value = pixel.value * options.max_val;
    if (two_bytes_pixels) {
      image += static_cast<char>(value >> 8);
    }
    image += static_cast<char>(value & 0xFF);
  }

  return image;
}

}  // namespace tempura
