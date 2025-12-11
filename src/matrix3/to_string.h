#pragma once

#include <matrix3/matrix.h>

#include <complex>
#include <format>
#include <string>
#include <type_traits>
#include <vector>

template <>
struct std::formatter<std::complex<double>> : std::formatter<std::string> {
  auto format(std::complex<double> num, std::format_context& ctx) const {
    return std::formatter<std::string>::format(
        std::format("{:.2f}e^({:.4f}i)", std::abs(num), std::arg(num)), ctx);
  }
};

namespace tempura::matrix3 {

// Helper to format a value based on type
template <typename T>
auto formatValue(const T& val) -> std::string {
  if constexpr (std::is_floating_point_v<std::remove_cvref_t<T>>) {
    return std::format("{:.4f}", val);
  } else {
    return std::format("{}", val);
  }
}

// Helper to format a value with width based on type
template <typename T>
auto formatValueWithWidth(const T& val, std::size_t width) -> std::string {
  if constexpr (std::is_floating_point_v<std::remove_cvref_t<T>>) {
    return std::format("{:{}.4f}", val, width);
  } else {
    return std::format("{:>{}}", val, width);
  }
}

template <typename MatT>
  requires requires(const MatT& m) {
    { m.extent() };
    { m.extent().rank() } -> std::same_as<std::size_t>;
    { m.extent().extent(0) };
    { m[0, 0] };
  }
auto toString(const MatT& m) -> std::string {
  const auto rows = m.extent().extent(0);
  const auto cols = m.extent().extent(1);

  // Pre-scan to determine column widths for alignment
  std::vector<std::size_t> widths(cols, 0);
  for (std::size_t i = 0; i < rows; ++i) {
    for (std::size_t j = 0; j < cols; ++j) {
      widths[j] = std::max(widths[j], formatValue(m[i, j]).length());
    }
  }

  std::string out;
  if (rows == 1) {
    // Single row: [ ... ]
    out += "[ ";
    for (std::size_t j = 0; j < cols; ++j) {
      out += formatValueWithWidth(m[0, j], widths[j]);
      out += ' ';
    }
    out += "]";
  } else {
    // Multi-row: ⎡ ⎤ ⎢ ⎥ ⎣ ⎦
    out += "⎡ ";
    for (std::size_t j = 0; j < cols; ++j) {
      out += formatValueWithWidth(m[0, j], widths[j]);
      out += ' ';
    }
    out += "⎤\n";
    for (std::size_t i = 1; i < rows - 1; ++i) {
      out += "⎢ ";
      for (std::size_t j = 0; j < cols; ++j) {
        out += formatValueWithWidth(m[i, j], widths[j]);
        out += ' ';
      }
      out += "⎥\n";
    }
    out += "⎣ ";
    for (std::size_t j = 0; j < cols; ++j) {
      out += formatValueWithWidth(m[rows - 1, j], widths[j]);
      out += ' ';
    }
    out += "⎦";
  }
  return out;
}

}  // namespace tempura::matrix3
