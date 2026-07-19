#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <utility>

namespace tempura {

// f(x₀…x_{N−1}) = ½·∑ dᵢxᵢ² — diagonal quadratic: minimum 0 at the origin.
template <std::size_t N>
struct QuadraticFn {
  static_assert(N > 0);

  std::array<double, N> diag;

  constexpr auto operator()(auto... xs) const
    requires(sizeof...(xs) == N)
  {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return 0.5 * (... + (diag[Is] * xs...[Is] * xs...[Is]));
    }(std::make_index_sequence<N>{});
  }
};

QuadraticFn(std::convertible_to<double> auto... diag)
    -> QuadraticFn<sizeof...(diag)>;

// f(x, y) = (a − x)² + b·(y − x²)²
// min = (a, a²)
struct RosenbrockFn {
  double a;
  double b;

  constexpr auto operator()(auto x, auto y) const {
    const auto u = a - x;
    const auto v = y - x * x;
    return u * u + b * (v * v);
  }
};

// f(x, y) = (x² + y + a)² + (x + y² + b)²
// Himmelblau's function is {a, b} = {−11, −7}: four zero minima, (3, 2) exact
struct HimmelblauFn {
  double a;
  double b;

  constexpr auto operator()(auto x, auto y) const {
    const auto u = x * x + y + a;
    const auto v = x + y * y + b;
    return u * u + v * v;
  }
};

// f(x, y) = (a − x + x·y)² + (b − x + x·y²)² + (c − x + x·y³)²
// Beale's function is {a, b, c} = {1.5, 2.25, 2.625}: min (3, ½)
struct BealeFn {
  double a;
  double b;
  double c;

  constexpr auto operator()(auto x, auto y) const {
    const auto y2 = y * y;
    const auto u = a - x + x * y;
    const auto v = b - x + x * y2;
    const auto w = c - x + x * (y2 * y);
    return u * u + v * v + w * w;
  }
};

}  // namespace tempura
