#include "poly.h"

#include <array>
#include <cstddef>

#include "unit.h"

using namespace tempura;

// constexpr: p(x) = 1 + 2x + 3x² at x=2 → 17; p'(x) = 2 + 6x → 14.
static_assert(horner(std::array{1.0, 2.0, 3.0}, 2.0) == 17.0);
static_assert(hornerWithDeriv(std::array{1.0, 2.0, 3.0}, 2.0).second == 14.0);

auto main() -> int {
  "horner value, edge sizes"_test = [] {
    expectClose((horner(std::array{1.0, 2.0, 3.0}, 2.0)), 17.0, {.rtol = 1e-15, .atol = 0});
    expectEq((horner(std::array{7.0}, 123.0)), 7.0);          // constant
    expectEq((horner(std::array<double, 0>{}, 5.0)), 0.0);    // empty
  };

  "hornerWithDeriv matches separate value and derivative"_test = [] {
    // p(x) = 2 − x + 4x³  (note the zero x² term)
    const std::array c{2.0, -1.0, 0.0, 4.0};
    for (double x : {-1.5, 0.0, 0.3, 2.7}) {
      auto [v, d] = hornerWithDeriv(c, x);
      expectClose(v, (horner(c, x)), {.rtol = 1e-14, .atol = 1e-14});
      expectClose(d, (horner(derivative(c), x)), {.rtol = 1e-14, .atol = 1e-14});
      // closed form: p' = −1 + 12x²
      expectClose(d, -1.0 + 12.0 * x * x, {.rtol = 1e-13, .atol = 1e-13});
    }
  };

  "derivative coefficients"_test = [] {
    auto d = derivative(std::array{5.0, 3.0, 7.0, 2.0});  // 5+3x+7x²+2x³ → 3+14x+6x²
    expectEq(d.size(), std::size_t{3});
    expectClose((d[0]), 3.0, {.rtol = 1e-15, .atol = 0});
    expectClose((d[1]), 14.0, {.rtol = 1e-15, .atol = 0});
    expectClose((d[2]), 6.0, {.rtol = 1e-15, .atol = 0});
    expectEq((derivative(std::array{42.0}).size()), std::size_t{0});  // constant → empty
  };

  "horner is within ~1 ULP of the correctly-rounded value"_test = [] {
    // Oracle = Horner in long double, then rounded to double (the reference, not another
    // double approximation). FMA-Horner should track it to ≤2 ULP on a well-conditioned poly.
    const std::array c{0.9, -1.3, 0.7, -0.21, 0.05, -0.011};
    for (double x : {-0.8, -0.2, 0.1, 0.6, 0.95}) {
      long double ref = 0.0L;
      for (std::size_t i = c.size(); i-- > 0;) ref = ref * static_cast<long double>(x) + c[i];
      expectWithinUlps(horner(c, x), static_cast<double>(ref), 2);
    }
  };

  return TestRegistry::result();
}
