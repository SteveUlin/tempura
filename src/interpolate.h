#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace tempura {

// std::projected_value_t is specified for C++26 but not yet in this libstdc++.
template <std::indirectly_readable I, std::indirectly_regular_unary_invocable<I> Proj>
using projected_value_t = std::remove_cvref_t<std::invoke_result_t<Proj&, std::iter_value_t<I>&>>;

// Projects the nth tuple element: GetFunctor<0> → xs, GetFunctor<1> → ys.
template <size_t n>
struct GetFunctor {
  template <typename T>
  constexpr auto operator()(T&& t) const -> decltype(auto) {
    using std::get;
    return get<n>(std::forward<T>(t));
  }
};

// Like binary search, but remembers the last queried point and uses it as a
// starting point for the next query.
template <
    std::ranges::forward_range R, typename Proj = std::identity,
    typename T = projected_value_t<std::ranges::iterator_t<R>, Proj>,
    std::indirect_strict_weak_order<const T*, std::projected<std::ranges::iterator_t<R>, Proj>>
        Comp = std::ranges::less>
  requires(std::ranges::borrowed_range<R>)
class ExponentialSearcher {
 public:
  constexpr ExponentialSearcher(R data, Comp comp = {}, Proj proj = {})
      : data_{std::move(data)}, comp_{comp}, proj_{proj}, prev_{std::ranges::begin(data_)} {}

  // Yields the first position whose key is ≥ value (lower_bound semantics), so it
  // returns end() when value exceeds every element — deref only after a range check.
  constexpr auto find(const T& value) const {
    if consteval {
      return std::ranges::lower_bound(data_, value, comp_, proj_);
    } else {
      if (prev_ == std::ranges::end(data_) or comp_(value, proj_(*prev_))) {
        auto right = prev_;
        std::ptrdiff_t delta = 1;
        while (right != std::ranges::begin(data_)) {
          const auto left = std::ranges::prev(right, delta, std::ranges::begin(data_));
          if (comp_(proj_(*left), value)) {
            prev_ = std::ranges::lower_bound(left, right, value, comp_, proj_);
            return prev_;
          }
          right = left;
          delta *= 2;
        }
        prev_ = std::ranges::begin(data_);
        return prev_;
      }
      auto left = prev_;
      std::ptrdiff_t delta = 1;
      while (left != std::ranges::end(data_)) {
        const auto right = std::ranges::next(left, delta, std::ranges::end(data_));
        if (right == std::ranges::end(data_) or comp_(value, proj_(*right))) {
          prev_ = std::ranges::lower_bound(left, right, value, comp_, proj_);
          return prev_;
        }
        left = right;
        delta *= 2;
      }
    }
    std::unreachable();
  }

  constexpr auto data() const -> const R& { return data_; }

 private:
  R data_;
  Comp comp_;
  Proj proj_;
  mutable std::ranges::iterator_t<R> prev_;
};

template <std::ranges::borrowed_range Range, template <typename> typename Interpolator>
class PiecewiseInterpolator {
 public:
  constexpr PiecewiseInterpolator(int64_t n, Range data)
      : n_{n},
        searcher_{data},
        start_{std::ranges::next(std::ranges::begin(searcher_.data()), n / 2)},
        end_{std::ranges::prev(std::ranges::end(searcher_.data()), (n + 1) / 2)} {
    assert(n > 0);
  }

  constexpr auto operator()(const auto& arg) const {
    // Extrapolation is unreliable, so require arg inside the data range rather
    // than silently clamping the window and returning a plausible-looking lie.
    const auto& data = searcher_.data();
    assert(std::get<0>(*std::ranges::begin(data)) <= arg &&
           arg <= std::get<0>(*std::ranges::prev(std::ranges::end(data))));
    auto iter = searcher_.find(arg);
    iter = std::clamp(iter, start_, end_);
    std::advance(iter, -n_ / 2);
    return Interpolator{std::ranges::subrange(iter, std::ranges::next(iter, n_))}(arg);
  }

 private:
  int64_t n_;
  ExponentialSearcher<Range, GetFunctor<0>> searcher_;
  std::ranges::iterator_t<Range> start_;
  std::ranges::iterator_t<Range> end_;
};

template <std::ranges::borrowed_range Range>
class LinearInterpolator {
 public:
  constexpr LinearInterpolator(Range data) : data_{std::move(data)} {
    assert(std::ranges::size(data_) == 2);
  }

  constexpr auto operator()(const auto& arg) const {
    const auto& [x0, y0] = data_[0];
    const auto& [x1, y1] = data_[1];
    assert(x0 <= arg && arg <= x1);
    return y0 + ((y1 - y0) * (arg - x0) / (x1 - x0));
  }

 private:
  Range data_;
};

template <template <typename> typename Interpolator>
constexpr auto makePiecewiseInterpolator(int64_t n, std::ranges::borrowed_range auto data) {
  return PiecewiseInterpolator<decltype(data), Interpolator>{n, data};
}

template <std::ranges::borrowed_range Range>
LinearInterpolator(Range data) -> LinearInterpolator<Range>;

// Polynomial Interpolator
//
// This class uses a context window of N points to interpolate a polynomial.
// The window is center around the point closest to the query point.
//
// This class does not calculate the coefficients of the polynomial. Rather, it
// calculates the interpolated point directly through successive differences.
//
// Set P_i = y_i
// Now let P_{i, ... j} be the polynomial of degree j-i passing through the
// points i, i+1, ..., j.
//
// Note that P_{i, ... j} and P_{i + 1, ... j + 1} agree at points i + 1, i + 2, ..., j.
// If we take a weight average of these two polynomials, we get a polynomial
// of one degree higher that passes through the points i, i + 1, ..., j + 1.
//
// P_{i .. j + 1} = ((x_{j + 1} - x)P_{i, ... j} + (x - x_{i})P_{i, .. j}) / (x_{j + 1} - x_i)
//
// As an optimization, instead of calculating P_{i, ... j} we can instead store the small
// differences between P{i ... j} and its parents
//
// c{m, i} = P_{i, ... i + m} - P_{i, ... i + m - 1}
// d{m, i} = P_{i, ... i + m} - P_{i + 1, ... i + m}
//
// We then derive the following recursive formulae:
// d{m + 1, i} = (x_{i + m + 1} - x) / (x_i - x_{i + m + 1}) * (c{m, i + 1} - d{m, i})
// c{m + 1, i} = (x - :x_i) / (x_{i + m + 1} - x_i) * (c{m, i + 1} - d{m, i})
//
// Now we start at the point closest to the query point and apply the C and
// D correction terms in as straight a path as possible to reach the end state.
//
// References: Numerical Recipes V3 Ch 3.2
template <std::ranges::borrowed_range Range>
class PolynomialInterpolator {
 public:
  using ValueType =
      projected_value_t<std::ranges::iterator_t<Range>, GetFunctor<1>>;

  constexpr PolynomialInterpolator(Range data)
      : xs_{data | std::views::elements<0>}, ys_{data | std::views::elements<1>} {}

  // The last correction Neville adds estimates the truncation error — the reason
  // the c/d formulation is worth more than a plain Lagrange evaluation.
  struct Estimate {
    ValueType value;
    ValueType error;
  };

  constexpr auto evaluate(auto arg) const -> Estimate {
    const size_t n = std::ranges::size(ys_);
    using std::abs;
    assert(xs_[0] <= arg && arg <= xs_[n - 1]);

    std::vector<ValueType> c(n, 0.0);
    std::vector<ValueType> d(n, 0.0);

    size_t idx = 0;
    auto diff = abs(xs_[idx] - arg);
    c[0] = ys_[0];
    d[0] = ys_[0];
    for (size_t i = 1; i < n; ++i) {
      if (const auto curr = abs(xs_[i] - arg); curr < diff) {
        idx = i;
        diff = curr;
      }

      c[i] = ys_[i];
      d[i] = ys_[i];
    }

    ValueType y = ys_[idx];
    ValueType dy{};
    --idx;

    for (size_t m = 1; m < n; ++m) {
      for (size_t i = 0; i < n - m; ++i) {
        const ValueType w = c[i + 1] - d[i];
        const ValueType den = xs_[i] - xs_[i + m];

        d[i] = w / den * (xs_[i + m] - arg);
        c[i] = w / den * (xs_[i] - arg);
      }

      if (2 * (idx + 1) < (n - m)) {
        dy = c[idx + 1];
      } else {
        dy = d[idx];
        --idx;
      }
      y += dy;
    }

    return {y, abs(dy)};
  }

  constexpr auto operator()(auto arg) const -> ValueType { return evaluate(arg).value; }

 private:
  std::ranges::elements_view<Range, 0> xs_;
  std::ranges::elements_view<Range, 1> ys_;
};

template <std::ranges::borrowed_range Range>
PolynomialInterpolator(Range data) -> PolynomialInterpolator<Range>;

// Cubic Spline Interpolator
//
// The goal of cubic spline interpolation is to find a piecewise cubic function
// that has:
// - smooth in the first derivative
// - continuous in the second derivative
//
// Suppose that we had a table of both yᵢ and y''ᵢ.
// We can create a basic linear interpolation for the ys:
// fᵢ(x) = yᵢ Aᵢ(x) + yᵢ₊₁ Bᵢ(x)
// where
// Aᵢ(x) = (xᵢ₊₁ - x) / (xᵢ₊₁ - xᵢ)
// Bᵢ(x) = 1 - Aᵢ(x)
//
// Now:
// -- If we can define a cubic function s(x) that is zero at the endpoints but
//    matches the second derivative at the endpoints.
// -> Then fᵢ(x) + sᵢ(x) will be our cubic spline.
//
// Let:
// Cᵢ(x) = 1/6 * (A(x)³ - A(x)) * (xᵢ₊₁ - xᵢ)²
// Dᵢ(x) = 1/6 * (B(x)³ - B(x)) * (xᵢ₊₁ - xᵢ)²
//
// We have that:
// sᵢ(x) = y''ᵢ Cᵢ(x) + y''ᵢ₊₁ Dᵢ(x)
//
// But we don't know what the y''ᵢ are. We now add the restriction that the
// first derivative of the spline is the same for each connected interval.
// f'ᵢ₋₁(xᵢ₊₁) + sᵢ₋₁'(xᵢ₊₁) = f'ᵢ(xᵢ) + s'ᵢ(xᵢ)
//
// ->  yᵢ₋₁ (-1 / hᵢ₋₁) + yᵢ   (1 / hᵢ₋₁) + y''ᵢ₋₁ (1 / 6hᵢ₋₁) hᵢ₋₁² + y''ᵢ   (1 / 3hᵢ₋₁) hᵢ₋₁²
//  == yᵢ   (-1 / hᵢ  ) + yᵢ₊₁ (1 / hᵢ  ) - y''ᵢ   (1 / 3hᵢ  ) hᵢ² - y''ᵢ₊₁ (1 / 6hᵢ  ) hᵢ²
//
// -> y''ᵢ₋₁ hᵢ₋₁ + 2 y''ᵢ (hᵢ₋₁ + hᵢ) + y''ᵢ₊₁ hᵢ
//  == 6 ((yᵢ₊₁ - yᵢ) / hᵢ - (yᵢ - yᵢ₋₁) / hᵢ₋₁)
//
// We now have n - 1 equations with n + 1 unknowns. For now we will assume the
// natural boundary conditions: y''₀ = y''ₙ = 0.
//
// This creates a tri-diagonal matrix that we can directly solve inline.
template <std::ranges::borrowed_range Range>
class CubicSplineInterpolator {
 public:
  using ValueType =
      projected_value_t<std::ranges::iterator_t<Range>, GetFunctor<1>>;

  constexpr CubicSplineInterpolator(Range data)
      : data_{std::move(data)},
        y2_(std::ranges::size(data_)),
        searcher_{data_, {}, GetFunctor<0>{}} {
    const size_t n = std::ranges::size(data_);
    assert(n >= 3);
    const auto x = [&](size_t i) { return std::get<0>(data_[i]); };
    const auto y = [&](size_t i) { return std::get<1>(data_[i]); };

    // Tridiagonal solve (Thomas) for the interior y''; natural BC pins y''₀ =
    // y''ₙ₋₁ = 0 (already set by value-init). The system is symmetric, so the
    // upper diagonal of row i−1, the lower diagonal of row i, and the knot
    // spacing hᵢ₋₁ are all the same value — the cancellation the old code missed.
    std::vector<ValueType> diag(n);
    std::vector<ValueType> rhs(n);
    for (size_t i = 1; i + 1 < n; ++i) {
      const auto h_lo = x(i) - x(i - 1);
      const auto h_hi = x(i + 1) - x(i);
      diag[i] = 2 * (h_lo + h_hi);
      rhs[i] = 6 * ((y(i + 1) - y(i)) / h_hi - (y(i) - y(i - 1)) / h_lo);
    }
    // Forward-eliminate the lower diagonal against upper[i−1] == hᵢ₋₁.
    for (size_t i = 2; i + 1 < n; ++i) {
      const auto h_lo = x(i) - x(i - 1);
      const auto factor = h_lo / diag[i - 1];
      diag[i] -= factor * h_lo;
      rhs[i] -= factor * rhs[i - 1];
    }
    // Back-substitute; upper[i] is the spacing hᵢ.
    for (size_t i = n - 2; i > 0; --i) {
      y2_[i] = (rhs[i] - (x(i + 1) - x(i)) * y2_[i + 1]) / diag[i];
    }
  }

  constexpr auto operator()(const auto& arg) const {
    assert(std::get<0>(data_[0]) <= arg &&
           arg <= std::get<0>(data_[std::ranges::size(data_) - 1]));
    auto iter = searcher_.find(arg);
    iter = std::ranges::clamp(iter, ++std::ranges::begin(searcher_.data()),
                              --std::ranges::end(searcher_.data()));
    std::ranges::advance(iter, -1, std::ranges::begin(searcher_.data()));
    const auto& [x0, y0] = *iter;
    const auto& [x1, y1] = *std::ranges::next(iter);
    const size_t idx = std::distance(std::ranges::begin(searcher_.data()), iter);
    const auto& y2_0 = y2_[idx];
    const auto& y2_1 = y2_[idx + 1];

    const auto h = x1 - x0;
    const auto a = (x1 - arg) / h;
    const auto b = 1 - a;

    return (a * y0) + (b * y1) + (((a * a * a - a) * y2_0 + (b * b * b - b) * y2_1) * h * h / 6);
  }

 private:
  Range data_;
  std::vector<ValueType> y2_;
  ExponentialSearcher<Range, GetFunctor<0>> searcher_;
};

template <std::ranges::borrowed_range Range>
CubicSplineInterpolator(Range data) -> CubicSplineInterpolator<Range>;

}  // namespace tempura
