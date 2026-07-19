#include "objectives.h"

#include "autodiff/dual.h"
#include "autodiff/tape.h"
#include "unit.h"

using namespace tempura;

// f(x, y) = ½(x² + 10y²): κ = 10, minimum 0 at the origin, ∇f = (x, 10y).
constexpr auto kQuadratic = QuadraticFn{1.0, 10.0};
constexpr auto kRosenbrock = RosenbrockFn{1.0, 100.0};
constexpr auto kHimmelblau = HimmelblauFn{-11.0, -7.0};
constexpr auto kBeale = BealeFn{1.5, 2.25, 2.625};

// Each canonical instance is exactly zero at its known minimum.
static_assert(kQuadratic(0.0, 0.0) == 0.0);
static_assert(kQuadratic(3.0, 2.0) == 0.5 * (9.0 + 40.0));
static_assert(kRosenbrock(1.0, 1.0) == 0.0);
static_assert(kHimmelblau(3.0, 2.0) == 0.0);
static_assert(kBeale(3.0, 0.5) == 0.0);

// Gradient oracles evaluate at points where every intermediate is binary-exact,
// so tape-vs-hand comparisons use expectEq — any mismatch is structural, not rounding.
auto main() -> int {
  "quadratic, dual seed beside a plain constant: ∂f/∂x₀ = d₀x₀"_test = [] {
    auto f = kQuadratic(Dual<double>{3.0, 1.0}, 2.0);
    expectEq(f.value, 0.5 * (9.0 + 40.0));
    expectEq(f.gradient, 3.0);
  };

  "quadratic, tape: one backward sweep gives ∇f = dᵢxᵢ"_test = [] {
    Tape<double> tape;
    auto x = tape.variable(3.0);
    auto y = tape.variable(2.0);
    auto f = kQuadratic(x, y);
    auto adj = tape.backward(f);
    expectEq(f.value, 0.5 * (9.0 + 40.0));
    expectEq(adj[x.id], 3.0);
    expectEq(adj[y.id], 20.0);
  };

  "rosenbrock: ∇f = (−2(a−x) − 4bx(y−x²), 2b(y−x²)); zero at the minimum"_test = [] {
    // at (2, 1): u = −1, v = −3 ⇒ f = 1 + 100·9 = 901, ∇f = (2 + 2400, −600)
    Tape<double> tape;
    auto x = tape.variable(2.0);
    auto y = tape.variable(1.0);
    auto f = kRosenbrock(x, y);
    auto adj = tape.backward(f);
    expectEq(f.value, 901.0);
    expectEq(adj[x.id], 2402.0);
    expectEq(adj[y.id], -600.0);

    Tape<double> tape_min;
    auto xm = tape_min.variable(1.0);
    auto ym = tape_min.variable(1.0);
    auto adj_min = tape_min.backward(kRosenbrock(xm, ym));
    expectEq(adj_min[xm.id], 0.0);
    expectEq(adj_min[ym.id], 0.0);
  };

  "himmelblau: ∇f = (4xu + 2v, 2u + 4yv); zero at the exact minimum (3, 2)"_test = [] {
    // at (1, 1): u = −9, v = −5 ⇒ f = 81 + 25, ∇f = (−36 − 10, −18 − 20)
    Tape<double> tape;
    auto x = tape.variable(1.0);
    auto y = tape.variable(1.0);
    auto f = kHimmelblau(x, y);
    auto adj = tape.backward(f);
    expectEq(f.value, 106.0);
    expectEq(adj[x.id], -46.0);
    expectEq(adj[y.id], -38.0);

    Tape<double> tape_min;
    auto xm = tape_min.variable(3.0);
    auto ym = tape_min.variable(2.0);
    auto adj_min = tape_min.backward(kHimmelblau(xm, ym));
    expectEq(adj_min[xm.id], 0.0);
    expectEq(adj_min[ym.id], 0.0);
  };

  "beale: ∇f = (∑2tᵢ·∂ₓtᵢ, ∑2tᵢ·∂ᵧtᵢ); zero at the minimum (3, ½)"_test = [] {
    // at (1, 2): t = (2.5, 5.25, 9.625) ⇒ f = 6.25 + 27.5625 + 92.640625
    // ∂ₓt = (y−1, y²−1, y³−1) = (1, 3, 7) ⇒ ∂ₓf = 5 + 31.5 + 134.75 = 171.25
    // ∂ᵧt = (x, 2xy, 3xy²)    = (1, 4, 12) ⇒ ∂ᵧf = 5 + 42 + 231 = 278
    Tape<double> tape;
    auto x = tape.variable(1.0);
    auto y = tape.variable(2.0);
    auto f = kBeale(x, y);
    auto adj = tape.backward(f);
    expectEq(f.value, 126.453125);
    expectEq(adj[x.id], 171.25);
    expectEq(adj[y.id], 278.0);

    Tape<double> tape_min;
    auto xm = tape_min.variable(3.0);
    auto ym = tape_min.variable(0.5);
    auto adj_min = tape_min.backward(kBeale(xm, ym));
    expectEq(adj_min[xm.id], 0.0);
    expectEq(adj_min[ym.id], 0.0);
  };

  return TestRegistry::result();
}
