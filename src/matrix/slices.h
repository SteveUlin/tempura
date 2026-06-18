#pragma once

#include <cstddef>
#include <mdspan>
#include <ranges>

#include "matrix.h"

namespace tempura {

// Lazy ranges of rank-1 slices over a 2D view: `rows(m)`/`cols(m)` yield one mdspan per
// row/column, so a matrix iterates by line and each slice is a VecView that flows
// straight through dot/norm2/matvec. mdspan has no native row/col iteration; this is
// std::submdspan + a transform over the index range — the iterator is std::views::iota's.
//
// A row of a row-major owner is contiguous; a column is std::layout_stride (stride =
// #cols) — correct, but a hot column sweep pays the strided-access tax, so materialize()
// the column panel when it is reused. DANGER: like every view, a slice borrows the
// source storage — it dangles if the source (e.g. a temporary) dies first.

template <MatrixView M>
constexpr auto rows(M&& m) {
  auto v = view(m);
  return std::views::iota(std::size_t{0}, static_cast<std::size_t>(v.extent(0))) |
         std::views::transform(
             [v](std::size_t i) { return std::submdspan(v, i, std::full_extent); });
}

template <MatrixView M>
constexpr auto cols(M&& m) {
  auto v = view(m);
  return std::views::iota(std::size_t{0}, static_cast<std::size_t>(v.extent(1))) |
         std::views::transform(
             [v](std::size_t j) { return std::submdspan(v, std::full_extent, j); });
}

}  // namespace tempura
