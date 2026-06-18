#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>

#include "matrix.h"
#include "mdarray.h"
#include "vec.h"

namespace tempura {

// A square N×N band matrix storing only `Bands` diagonals in a compact N×Bands buffer:
// logical (i,j) lives in band b = j − i + center, where center is the main-diagonal band.
//
// This is a STRUCTURED-STORAGE type, NOT a dense view: the out-of-band cells are
// structurally zero and are never stored, so there is no mdspan of the logical matrix to
// hand the dense kernels (a layout must map every (i,j) to a real offset). It carries its
// own O(N·Bands) kernels instead. Out-of-band reads return 0 BY VALUE — matrix3 returned a
// reference to a shared zero member, so an out-of-band write silently corrupted every
// later zero read; here a write asserts in-band. Static (stack) for now; dynamic can follow.
template <typename T, std::size_t N, std::size_t Bands>
  requires(Bands >= 1 && Bands <= 2 * N - 1)
class Banded {
 public:
  static constexpr std::size_t center = Bands / 2;  // main-diagonal band index

  constexpr Banded() = default;

  // Read: 0 outside the band, by value (no shared-zero aliasing hazard).
  constexpr auto operator[](std::size_t i, std::size_t j) const -> T {
    const std::ptrdiff_t b = bandOf(i, j);
    if (b < 0 || b >= static_cast<std::ptrdiff_t>(Bands)) return T{};
    return band_[i, static_cast<std::size_t>(b)];
  }
  // Write: must land in the band — fail loudly rather than corrupt a structural zero.
  constexpr void set(std::size_t i, std::size_t j, const T& v) {
    const std::ptrdiff_t b = bandOf(i, j);
    assert(b >= 0 && b < static_cast<std::ptrdiff_t>(Bands) && "write outside the band");
    band_[i, static_cast<std::size_t>(b)] = v;
  }

  static constexpr auto rows() -> std::size_t { return N; }
  static constexpr auto cols() -> std::size_t { return N; }
  static constexpr auto bandCount() -> std::size_t { return Bands; }
  constexpr auto buffer() -> InlineDense<T, N, Bands>& { return band_; }  // raw compact store
  constexpr auto buffer() const -> const InlineDense<T, N, Bands>& { return band_; }

 private:
  static constexpr auto bandOf(std::size_t i, std::size_t j) -> std::ptrdiff_t {
    return static_cast<std::ptrdiff_t>(j) - static_cast<std::ptrdiff_t>(i) +
           static_cast<std::ptrdiff_t>(center);
  }
  InlineDense<T, N, Bands> band_{};
};

// Materialize the full dense matrix (out-of-band filled with zeros) — for interop and for
// cross-checking against the dense kernels.
template <typename T, std::size_t N, std::size_t Bands>
constexpr auto toDense(const Banded<T, N, Bands>& a) {
  InlineDense<T, N, N> d{};
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = 0; j < N; ++j) d[i, j] = a[i, j];
  return d;
}

// y = A·x in O(N·Bands): each row sums only its in-band columns.
template <typename T, std::size_t N, std::size_t Bands, VecView X>
constexpr auto multiply(const Banded<T, N, Bands>& a, const X& x) {
  auto vx = view(x);
  assert(static_cast<std::size_t>(vx.extent(0)) == N && "matrix-vector size mismatch");
  using Acc = Accumulator<T, typename decltype(vx)::value_type>;
  constexpr auto c = static_cast<std::ptrdiff_t>(Banded<T, N, Bands>::center);
  InlineVec<T, N> y{};
  for (std::size_t i = 0; i < N; ++i) {
    // in-band: b = j − i + center ∈ [0, Bands) ⇒ j ∈ [i − center, i − center + Bands)
    const std::ptrdiff_t lo = std::max<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(i) - c, 0);
    const std::ptrdiff_t hi = std::min<std::ptrdiff_t>(
        static_cast<std::ptrdiff_t>(i) - c + static_cast<std::ptrdiff_t>(Bands),
        static_cast<std::ptrdiff_t>(N));
    Acc sum{};
    for (std::ptrdiff_t j = lo; j < hi; ++j)
      sum += static_cast<Acc>(a[i, static_cast<std::size_t>(j)]) *
             static_cast<Acc>(vx[static_cast<std::size_t>(j)]);
    y[i] = static_cast<T>(sum);
  }
  return y;
}

// Solve a tridiagonal system A·x = b via the Thomas algorithm in O(N). PRECONDITION:
// diagonally dominant or SPD (no pivoting). Floating-point only.
template <std::floating_point T, std::size_t N, VecView B>
constexpr auto solveTridiagonal(const Banded<T, N, 3>& a, const B& b) {
  auto vb = view(b);
  assert(static_cast<std::size_t>(vb.extent(0)) == N && "RHS size must match the system");
  InlineVec<T, N> x{};
  if constexpr (N == 1) {
    x[0] = static_cast<T>(vb[0]) / a[0, 0];
    return x;
  } else {
    InlineVec<T, N> cp{};  // modified superdiagonal
    InlineVec<T, N> dp{};  // modified RHS
    cp[0] = a[0, 1] / a[0, 0];
    dp[0] = static_cast<T>(vb[0]) / a[0, 0];
    for (std::size_t i = 1; i < N; ++i) {
      const T denom = a[i, i] - a[i, i - 1] * cp[i - 1];
      if (i < N - 1) cp[i] = a[i, i + 1] / denom;
      dp[i] = (static_cast<T>(vb[i]) - a[i, i - 1] * dp[i - 1]) / denom;
    }
    x[N - 1] = dp[N - 1];
    for (std::size_t i = N - 1; i-- > 0;) x[i] = dp[i] - cp[i] * x[i + 1];
    return x;
  }
}

}  // namespace tempura
