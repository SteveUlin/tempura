#include "interpolate.h"
#include "unit.h"

#include <print>

using namespace tempura;

auto main() -> int {
  "interpolate"_test = [] consteval {
    constexpr static std::array x{1., 2., 3., 4., 5.};
    constexpr ExponentialSearcher w{std::ranges::views::all(x)};
    static_assert(*w.find(2.5) == 3.);
  };

  "interpolate not constexpr"_test = [] {
    static std::array x{1., 2., 3., 4., 5.};
    ExponentialSearcher w{std::ranges::views::all(x)};
    expectEq(3., *w.find(2.));
  };

  "linear interpolator"_test = [] {
    std::array x{1., 2., 3., 4., 5.};
    std::array y{1., 4., 9., 16., 25.};
    auto data = std::views::zip(x, y);
    auto interp = makePiecewiseInterpolator<LinearInterpolator>(2, data);
    std::println("{}", interp(2.5));
  };
}
