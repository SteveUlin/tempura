#include "interpolate.h"

#include <array>
#include <cmath>
#include <ranges>

#include "unit.h"

using namespace tempura;

// The cubic spline is constexpr-friendly: ExponentialSearcher::find falls back to
// lower_bound under `if consteval`, so the whole construct-and-evaluate runs at
// compile time. Values are the canonical NR spline/splint on y = x² over a
// non-uniform grid — an oracle independent of this implementation.
constexpr auto splineRef() -> bool {
  std::array sx{0.0, 1.0, 2.5, 4.0, 6.0};
  std::array sy{0.0, 1.0, 6.25, 16.0, 36.0};
  CubicSplineInterpolator spline{std::views::zip(sx, sy)};
  return isClose(spline(0.5), 0.34477459016393441, {.rtol = 1e-12}) &&
         isClose(spline(3.25), 10.512935450819672, {.rtol = 1e-12});
}
static_assert(splineRef());

auto main() -> int {
  "exponential searcher: lower-bound semantics, constexpr path"_test = [] consteval {
    constexpr static std::array x{1., 2., 3., 4., 5.};
    constexpr ExponentialSearcher w{std::ranges::views::all(x)};
    static_assert(*w.find(2.5) == 3.);  // first element ≥ 2.5
    static_assert(*w.find(2.0) == 2.);  // exact hit returns itself
  };

  "exponential searcher: galloping path with locality cache"_test = [] {
    std::array x{1., 2., 3., 4., 5.};
    ExponentialSearcher w{std::ranges::views::all(x)};
    // Runtime path hunts outward from the previous query, so exercise both
    // directions: forward then backward.
    expectEq(2., *w.find(2.0));
    expectEq(5., *w.find(4.5));
    expectEq(2., *w.find(1.5));  // backward jump from the cached position
    expectEq(1., *w.find(0.5));  // clamps to begin
  };

  "linear interpolator: exact on a straight line"_test = [] {
    std::array x{1.0, 3.0};
    std::array y{10.0, 20.0};  // slope 5
    LinearInterpolator lin{std::views::zip(x, y)};
    expectClose(lin(2.0), 15.0, {.rtol = 1e-15});
    expectClose(lin(1.0), 10.0, {.rtol = 1e-15});
  };

  "piecewise linear: picks the right bracket, including the edges"_test = [] {
    std::array x{1., 2., 3., 4., 5.};
    std::array y{1., 4., 9., 16., 25.};  // y = x²
    auto interp = makePiecewiseInterpolator<LinearInterpolator>(2, std::views::zip(x, y));
    expectClose(interp(2.5), 6.5, {.rtol = 1e-15});   // chord of (2,4)-(3,9)
    expectClose(interp(1.5), 2.5, {.rtol = 1e-15});   // first bracket
    expectClose(interp(4.5), 20.5, {.rtol = 1e-15});  // last bracket
    expectEq(1.0, interp(1.0));                        // left endpoint node
    expectEq(9.0, interp(3.0));                        // interior node
    expectEq(25.0, interp(5.0));                       // right endpoint node
  };

  "piecewise polynomial: 4-point window reproduces a cubic"_test = [] {
    std::array x{0., 1., 2., 3., 4., 5., 6.};
    std::array y{0., 1., 8., 27., 64., 125., 216.};  // y = x³
    auto interp = makePiecewiseInterpolator<PolynomialInterpolator>(4, std::views::zip(x, y));
    expectClose(interp(2.5), 15.625, {.rtol = 1e-12});
    expectClose(interp(4.5), 91.125, {.rtol = 1e-12});
  };

  "polynomial (Neville): value and error estimate"_test = [] {
    std::array x{0., 1., 2., 3., 4.};
    std::array y{0., 1., 8., 27., 64.};  // y = x³, exactly representable here
    PolynomialInterpolator poly{std::views::zip(x, y)};
    auto exact = poly.evaluate(2.5);
    expectClose(exact.value, 15.625, {.rtol = 1e-12});
    expectClose(poly(0.5), 0.125, {.atol = 1e-12});
    expectClose(exact.error, 0.0, {.atol = 1e-9});  // exact fit → vanishing correction

    // For a function the interpolant cannot represent, the estimate goes live.
    std::array ex{0.0, 0.5, 1.0, 1.5, 2.0};
    std::array ey{std::exp(0.0), std::exp(0.5), std::exp(1.0), std::exp(1.5), std::exp(2.0)};
    PolynomialInterpolator expInterp{std::views::zip(ex, ey)};
    auto est = expInterp.evaluate(1.1);
    expectClose(est.value, std::exp(1.1), {.atol = 1e-3});
    expectGT(est.error, 0.0);
    expectFinite(est.error);
  };

  "cubic spline: matches reference, passes through nodes"_test = [] {
    std::array sx{0.0, 1.0, 2.5, 4.0, 6.0};
    std::array sy{0.0, 1.0, 6.25, 16.0, 36.0};  // y = x²
    CubicSplineInterpolator spline{std::views::zip(sx, sy)};
    expectClose(spline(0.5), 0.34477459016393441, {.rtol = 1e-12});
    expectClose(spline(1.75), 3.0336834016393444, {.rtol = 1e-12});
    expectClose(spline(3.25), 10.512935450819672, {.rtol = 1e-12});
    expectClose(spline(5.0), 25.342213114754099, {.rtol = 1e-12});
    // The interpolation basis vanishes at the knots, so node values are exact.
    for (auto [xi, yi] : std::views::zip(sx, sy)) expectEq(yi, spline(xi));
  };

  "cubic spline: natural spline of a line is the line"_test = [] {
    std::array x{0.0, 1.0, 2.0, 3.0};
    std::array y{1.0, 3.0, 5.0, 7.0};  // y = 2x + 1, has y'' = 0 everywhere
    CubicSplineInterpolator spline{std::views::zip(x, y)};
    expectClose(spline(0.5), 2.0, {.rtol = 1e-14});
    expectClose(spline(2.5), 6.0, {.rtol = 1e-14});
  };

  return TestRegistry::result();
}
