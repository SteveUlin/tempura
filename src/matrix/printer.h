#pragma once

#include <matrix/matrix.h>

#include <format>
#include <sstream>
#include <type_traits>
#include <vector>

namespace tempura::matrix {

template <MatrixT MatT>
auto toString(const MatT& m) -> std::string {
  constexpr std::string_view base_fmt_str =
      (std::is_floating_point_v<std::remove_cvref_t<decltype(m[0, 0])>>) ? "{:.4f}" : "{}";
  std::cout << MatT::kExtent.row << std::endl;
  std::cout << MatT::kExtent.col << std::endl;
  const int64_t row = m.shape().row;
  const int64_t col = m.shape().col;
  std::cout << m.shape().row << std::endl;
  std::cout << m.shape().col << std::endl;
  std::vector<size_t> widths(col, 0);
  for (int64_t i = 0; i < row; ++i) {
    for (int64_t j = 0; j < col; ++j) {
      widths[j] = std::max(widths[j], std::format(base_fmt_str, m[i, j]).length());
    }
  }
  constexpr std::string_view fmt_str =
      (std::is_floating_point_v<std::remove_cvref_t<decltype(m[0, 0])>>) ? "{:{}.4f}" : "{:{}}";
  std::stringstream ss;
  if (row == 1) {
    ss << "[ ";
    for (int64_t j = 0; j < col; ++j) {
      ss << std::format(fmt_str, m[0, j], widths[j]) << ' ';
    }
    ss << "]";
  } else {
    ss << "⎡ ";
    for (int64_t j = 0; j < col; ++j) {
      ss << std::format(fmt_str, m[0, j], widths[j]) << ' ';
    }
    ss << "⎤\n";
    for (int64_t i = 1; i < row - 1; ++i) {
      ss << "⎢ ";
      for (int64_t j = 0; j < col; ++j) {
        ss << std::format(fmt_str, m[i, j], widths[j]) << ' ';
      }
      ss << "⎥\n";
    }
    ss << "⎣ ";
    for (int64_t j = 0; j < col; ++j) {
      ss << std::format(fmt_str, m[row - 1, j], widths[j]) << ' ';
    }
    ss << "⎦";
  }
  return ss.str();
}

}  // namespace tempura::matrix
