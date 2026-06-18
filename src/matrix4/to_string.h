#pragma once

#include <cstddef>
#include <format>
#include <string>
#include <type_traits>
#include <vector>

#include "matrix.h"  // view, MatrixView
#include "vec.h"     // VecView

namespace tempura {

// Render a matrix view as a Unicode box (⎡⎢⎣ … ⎤⎥⎦), or a single ASCII row "[ … ]"
// for a 1-row matrix. Columns right-align to their widest cell; floats print at 4
// decimals (two-pass: the measuring pass must format identically to the rendering pass
// modulo width). No trailing newline. NOT constexpr — std::format / float to_chars
// aren't constexpr. Works on any MatrixView (owners and transposed/permuted views).
template <MatrixView M>
auto toString(const M& m) -> std::string {
  auto v = view(m);
  using T = typename decltype(v)::value_type;
  auto cell = [](const T& val) -> std::string {
    if constexpr (std::is_floating_point_v<T>)
      return std::format("{:.4f}", val);
    else
      return std::format("{}", val);
  };
  auto cellW = [](const T& val, std::size_t w) -> std::string {
    if constexpr (std::is_floating_point_v<T>)
      return std::format("{:{}.4f}", val, w);
    else
      return std::format("{:>{}}", val, w);
  };
  const std::size_t rows = v.extent(0);
  const std::size_t cols = v.extent(1);
  if (rows == 0) return "[ ]";  // no rows: nothing to render (and avoids the rows-1 underflow)

  std::vector<std::size_t> width(cols, 0);
  for (std::size_t i = 0; i < rows; ++i)
    for (std::size_t j = 0; j < cols; ++j) {
      const std::size_t w = cell(v[i, j]).size();
      if (w > width[j]) width[j] = w;
    }

  auto renderRow = [&](std::size_t i, const char* open, const char* close) -> std::string {
    std::string s = open;
    for (std::size_t j = 0; j < cols; ++j) {
      s += cellW(v[i, j], width[j]);
      s += ' ';
    }
    s += close;
    return s;
  };

  if (rows == 1) return renderRow(0, "[ ", "]");
  std::string out = renderRow(0, "⎡ ", "⎤");
  for (std::size_t i = 1; i + 1 < rows; ++i) {
    out += '\n';
    out += renderRow(i, "⎢ ", "⎥");
  }
  out += '\n';
  out += renderRow(rows - 1, "⎣ ", "⎦");
  return out;
}

// Render a vector view as a single ASCII row "[ a b c ]" (floats at 4 decimals).
template <VecView M>
auto toString(const M& m) -> std::string {
  auto v = view(m);
  using T = typename decltype(v)::value_type;
  auto cell = [](const T& val) -> std::string {
    if constexpr (std::is_floating_point_v<T>)
      return std::format("{:.4f}", val);
    else
      return std::format("{}", val);
  };
  const std::size_t n = v.extent(0);
  std::string out = "[ ";
  for (std::size_t i = 0; i < n; ++i) {
    out += cell(v[i]);
    out += ' ';
  }
  out += "]";
  return out;
}

}  // namespace tempura
