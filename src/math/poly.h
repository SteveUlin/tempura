#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

namespace tempura {

// A bidirectional range of floating-point polynomial COEFFICIENTS.
template <typename R>
concept RealCoeffs = std::ranges::bidirectional_range<R> &&
                     std::floating_point<std::ranges::range_value_t<R>>;

template <typename R>
using CoeffT = std::ranges::range_value_t<R>;

// Horner — evaluate p(x)= Σ cᵢ xⁱ as a nested product
//     p(x) = c₀ + x·(c₁ + x·(c₂ + x·c₃))          (a right fold)
//     acc = c₃ → c₃·x+c₂ → (c₃·x+c₂)·x+c₁ → …·x+c₀
template <RealCoeffs R, typename X>
constexpr auto horner(R&& c, X x) -> X {
  return std::ranges::fold_right(c, X{}, [x](CoeffT<R> ci, X acc) {
    using std::fma;  // std::fma for scalars; ADL finds fma(Dual,…) for a dual
                     // argument
    return fma(acc, x, ci);
  });
}

// Coefficients of the N-th derivative p⁽ᴺ⁾ as a lazy view
template <std::size_t N = 1, RealCoeffs R>
  requires std::ranges::sized_range<R>
constexpr auto polyDifferentiate(R&& c) {
  using T = CoeffT<R>;
  const std::size_t len = std::ranges::size(c);
  const std::size_t out = len > N ? len - N : 0;
  return std::views::zip_transform(
      [](std::size_t k, T ck) {
        T m{1};
        for (std::size_t i = 1; i <= N; ++i) m *= static_cast<T>(k + i);
        return m * ck;
      },
      std::views::iota(std::size_t{0}, out),
      std::forward<R>(c) | std::views::drop(N));
}

// Evaluate an even polynomial: ax² + bx⁴ + cx⁶ + …
template <RealCoeffs R>
constexpr auto evalEven(R&& c, CoeffT<R> x) -> CoeffT<R> {
  return horner(std::forward<R>(c), x * x);
}
// Evaluate an odd polynomial: ax + bx³ + cx\^5 + …
template <RealCoeffs R>
constexpr auto evalOdd(R&& c, CoeffT<R> x) -> CoeffT<R> {
  return x * horner(std::forward<R>(c), x * x);
}

// Second-order Horner (2-way): split coefficients by index parity into two
// independent Horner chains in x², recombine E(x²) + x·O(x²). Same op count,
// ~half the critical path (measured ~2× by degree 128). This is the k=2 split,
// NOT Estrin's scheme (a log-depth tree over x^(2ᵏ)). Costs accuracy near roots
// (x² amplification) — prefer horner there.
template <RealCoeffs R>
constexpr auto horner2(R&& c, CoeffT<R> x) -> CoeffT<R> {
  const CoeffT<R> x2 = x * x;
  const CoeffT<R> even = horner(c | std::views::stride(2), x2);
  const CoeffT<R> odd =
      horner(c | std::views::drop(1) | std::views::stride(2), x2);
  return std::fma(x, odd, even);
}

// Clenshaw recurrence for a CHEBYSHEV-basis series Σ c[k]·Tₖ(x) — evaluate
// without converting to monomials, more stable when coefficients live in that
// basis. CONVENTION: full c[0] (not the c₀/2 some references use). Footgun:
// passing monomial coeffs here is silent garbage. Chebyshev basis, not monomial
// — its natural home is the Chebyshev machinery, not poly.h.
template <RealCoeffs R>
constexpr auto clenshaw(R&& c, CoeffT<R> x) -> CoeffT<R> {
  using T = CoeffT<R>;
  auto rit = std::ranges::rbegin(c);
  const auto rend = std::ranges::rend(c);
  if (rit == rend) return T{0};
  const T two_x = T{2} * x;
  T b1{0};  // bₖ₊₁
  T b2{0};  // bₖ₊₂
  for (const auto last = std::ranges::prev(rend); rit != last; ++rit) {
    const T bk = std::fma(two_x, b1, *rit - b2);  // bₖ = 2x·bₖ₊₁ − bₖ₊₂ + cₖ
    b2 = b1;
    b1 = bk;
  }
  return std::fma(x, b1, *rit - b2);  // c₀ + x·b₁ − b₂
}

}  // namespace tempura
