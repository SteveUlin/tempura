#include "transform.h"

#include <array>
#include <cmath>

#include "unit.h"

using namespace tempura;
using namespace tempura::autodiff;

auto main() -> int {
  // One generic function, differentiated every way — the point of the transform layer.
  auto f = [](auto x, auto y) { return x * y + sin(x); };  // f(x,y) = xy + sin x

  "jvp scalar: directional derivative of a 1-D function"_test = [] {
    auto g = [](auto x) { return sin(x); };
    auto [val, dv] = jvp(g, 0.5, 2.0);  // (sin 0.5, 2·cos 0.5)
    expectClose(val, std::sin(0.5), {.rtol = 1e-12, .atol = 1e-12});
    expectClose(dv, 2.0 * std::cos(0.5), {.rtol = 1e-12, .atol = 1e-12});
  };

  "valueAndGrad: reverse-mode gradient of f(x,y)=xy+sin x"_test = [&] {
    auto [val, g] = valueAndGrad(f, std::array{1.3, 2.1});
    expectClose(val, 1.3 * 2.1 + std::sin(1.3), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((g[0]), 2.1 + std::cos(1.3), {.rtol = 1e-12, .atol = 1e-12});  // ∂/∂x = y + cos x
    expectClose((g[1]), 1.3, {.rtol = 1e-12, .atol = 1e-12});                  // ∂/∂y = x
  };

  "vjp: pullback reused across cotangents"_test = [] {
    auto prod = [](auto x, auto y) { return x * y; };
    auto [val, pull] = vjp(prod, std::array{3.0, 4.0});
    expectClose(val, 12.0, {.rtol = 1e-12, .atol = 1e-12});
    auto g1 = pull(1.0);  // 1·J = [y, x] = [4, 3]
    expectClose((g1[0]), 4.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((g1[1]), 3.0, {.rtol = 1e-12, .atol = 1e-12});
    auto g2 = pull(2.0);  // 2·J = [8, 6] — same recorded forward pass, new cotangent
    expectClose((g2[0]), 8.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((g2[1]), 6.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  "forward and reverse agree: J·v (jvp) == grad·v"_test = [&] {
    const std::array x{1.0, 2.0};
    const std::array v{0.5, 1.0};
    auto [fx, jv] = jvp(f, x, v);
    auto g = grad(f, x);
    const double gdotv = g[0] * v[0] + g[1] * v[1];
    expectClose(jv, gdotv, {.rtol = 1e-12, .atol = 1e-12});
  };

  return TestRegistry::result();
}
