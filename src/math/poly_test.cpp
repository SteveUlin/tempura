#include "poly.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

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

  "evalEven / evalOdd values and exact parity"_test = [] {
    // evalEven({1,2,3}, x) = 1 + 2x² + 3x⁴; at x=2 → 1+8+48 = 57
    expectClose((evalEven(std::array{1.0, 2.0, 3.0}, 2.0)), 57.0, {.rtol = 1e-14, .atol = 0});
    // evalOdd({1,2}, x) = x(1 + 2x²) = x + 2x³; at x=2 → 18
    expectClose((evalOdd(std::array{1.0, 2.0}, 2.0)), 18.0, {.rtol = 1e-14, .atol = 0});
    // bit-exact parity: even is symmetric, odd is antisymmetric
    const std::array ce{0.7, -1.1, 0.3, 0.02};
    const std::array co{1.0, -0.5, 0.08};
    for (double x : {0.3, 1.4, 2.9}) {
      expectWithinUlps(evalEven(ce, -x), evalEven(ce, x), 0);
      expectWithinUlps(evalOdd(co, -x), -evalOdd(co, x), 0);
    }
  };

  "hornerSecondOrder is tight on small arguments (its real regime)"_test = [] {
    // Evaluating in x² halves the argument's precision, so second-order Horner is for the
    // SMALL intervals libm uses it on (|r| ≲ 0.35); there it costs only a few ULP for a
    // ~halved critical path. (At larger |x| the x²-amplification grows — by design.)
    const std::array c{0.9, -1.3, 0.7, -0.21, 0.05, -0.011, 0.004};
    for (double x : {-0.34, -0.1, 0.05, 0.2, 0.34}) {
      long double ref = 0.0L;
      for (std::size_t i = c.size(); i-- > 0;) ref = ref * static_cast<long double>(x) + c[i];
      expectWithinUlps(hornerSecondOrder(c, x), static_cast<double>(ref), 4);
    }
  };

  "clenshaw evaluates a Chebyshev series"_test = [] {
    // Σ c_k T_k:  {2,3} → 2T₀+3T₁ = 2+3x;  {1,0,1} → T₀+T₂ = 1+(2x²−1) = 2x²
    for (double x : {-0.7, 0.0, 0.4, 0.9}) {
      expectClose((clenshaw(std::array{2.0, 3.0}, x)), 2.0 + 3.0 * x, {.rtol = 1e-14, .atol = 1e-15});
      expectClose((clenshaw(std::array{1.0, 0.0, 1.0}, x)), 2.0 * x * x, {.rtol = 1e-13, .atol = 1e-14});
    }
  };

  "error-free transforms are exact"_test = [] {
    // a+b = s+e and a·b = p+e to the bit (verified in long double)
    for (auto [a, b] : {std::pair{1.0, 1e-20}, std::pair{1.0 + 0x1p-52, 1.0 - 0x1p-52}, std::pair{3.7, -2.9}}) {
      auto [s, e] = twoSum(a, b);
      expectEq(static_cast<long double>(s) + e,
               static_cast<long double>(a) + static_cast<long double>(b));
      auto [p, pe] = twoProductFMA(a, b);
      expectEq(static_cast<long double>(p) + pe,
               static_cast<long double>(a) * static_cast<long double>(b));
    }
  };

  "compHorner recovers accuracy where plain Horner cancels"_test = [] {
    // (x−1)⁵ expanded — integer coeffs, exact. Evaluated at x=1.001 the terms (~O(1))
    // cancel down to (1e-3)⁵=1e-15: condition ~1e15, so plain Horner loses ~all digits.
    const std::array c{-1.0, 5.0, -10.0, 10.0, -5.0, 1.0};
    const double x = 1.0 + 1e-3;
    const double truth = static_cast<double>(std::pow(static_cast<long double>(x) - 1.0L, 5));
    const double err_horner = std::abs(horner(c, x) - truth);
    const double err_comp = std::abs(compHorner(c, x) - truth);
    expectLT(err_comp, err_horner);                               // compensation helps…
    expectClose(compHorner(c, x), truth, {.rtol = 1e-9, .atol = 1e-22});  // …and is actually accurate
  };

  return TestRegistry::result();
}
