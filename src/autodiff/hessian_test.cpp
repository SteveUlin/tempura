#include "hessian.h"

#include <array>
#include <cmath>
#include <cstddef>

#include "unit.h"

using namespace tempura;
using namespace tempura::autodiff;

auto main() -> int {
  // f(x,y) = x²y + sin(x)
  //   ∇f = [2xy + cos x, x²]
  //   H  = [[2y − sin x, 2x], [2x, 0]]
  auto f = [](auto x, auto y) { return x * x * y + sin(x); };

  "gradAndHvp: gradient and H·v in one forward-over-reverse pass"_test = [&] {
    const std::array x{1.0, 2.0};
    const std::array v{1.0, 0.0};  // H·e₀ = first column of H
    auto [g, hv] = gradAndHvp(f, x, v);
    expectClose((g[0]), 2 * 1.0 * 2.0 + std::cos(1.0), {.rtol = 1e-11, .atol = 1e-11});
    expectClose((g[1]), 1.0, {.rtol = 1e-11, .atol = 1e-11});           // x² = 1
    expectClose((hv[0]), 2 * 2.0 - std::sin(1.0), {.rtol = 1e-11, .atol = 1e-11});  // 2y − sin x
    expectClose((hv[1]), 2 * 1.0, {.rtol = 1e-11, .atol = 1e-11});      // 2x
  };

  "full Hessian: values, symmetry, and a closed-form check"_test = [&] {
    const std::array x{1.0, 2.0};
    auto h = hessian(f, x);
    const double expected[2][2] = {{2 * 2.0 - std::sin(1.0), 2 * 1.0}, {2 * 1.0, 0.0}};
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j)
        expectClose((h[i, j]), expected[i][j], {.rtol = 1e-11, .atol = 1e-11});
    expectClose((h[0, 1]), (h[1, 0]), {.rtol = 1e-12, .atol = 1e-12});  // symmetry
  };

  "scalar literals lift through Var<Dual>: f = 2x²y + 1/x"_test = [] {
    // H = [[4y + 2/x³, 4x], [4x, 0]], at (1, 2)
    auto g = [](auto x, auto y) { return 2.0 * x * x * y + 1.0 / x; };
    auto h = hessian(g, std::array{1.0, 2.0});
    const double expected[2][2] = {{10.0, 4.0}, {4.0, 0.0}};
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j)
        expectClose((h[i, j]), expected[i][j], {.rtol = 1e-11, .atol = 1e-11});
  };

  "HVP matches H·v without materializing H"_test = [&] {
    const std::array x{1.0, 2.0};
    const std::array v{0.3, 0.7};
    auto h = hessian(f, x);
    auto hv = hvp(f, x, v);
    for (std::size_t i = 0; i < 2; ++i) {
      const double hrow = h[i, 0] * v[0] + h[i, 1] * v[1];
      expectClose((hv[i]), hrow, {.rtol = 1e-11, .atol = 1e-11});
    }
  };

  return TestRegistry::result();
}
