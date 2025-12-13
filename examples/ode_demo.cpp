// ODE Solver Demos - Visualizing solutions to differential equations

#include <cmath>
#include <iostream>
#include <print>

#include "ode/euler.h"
#include "ode/integrate.h"
#include "plot.h"

using namespace tempura;
using namespace tempura::ode;

// 2D state for second-order ODEs converted to systems
struct Vec2 {
  double x;
  double y;

  constexpr auto operator+(const Vec2& other) const -> Vec2 {
    return {x + other.x, y + other.y};
  }
  constexpr auto operator*(double s) const -> Vec2 { return {x * s, y * s}; }
  friend constexpr auto operator*(double s, const Vec2& v) -> Vec2 {
    return v * s;
  }
};

auto main() -> int {
  // ============================================================================
  // 1. Exponential Growth: dy/dt = 0.5y
  // ============================================================================
  std::println("═══════════════════════════════════════════════════════════════");
  std::println("  Exponential Growth: dy/dt = 0.5y,  y(0) = 1");
  std::println("  Exact solution: y(t) = e^(0.5t)");
  std::println("═══════════════════════════════════════════════════════════════");

  constexpr auto expGrowth = [](double /*t*/, double y) { return 0.5 * y; };

  std::function<double(double)> expSolution = [&](double t) {
    return integrate(expGrowth, 1.0, 0.0, t, 0.01);
  };
  std::cout << plotFn(expSolution, 0.0, 5.0, 70, 12) << "\n";

  // ============================================================================
  // 2. Comparison: Euler vs RK4 accuracy
  // ============================================================================
  std::println("═══════════════════════════════════════════════════════════════");
  std::println("  Accuracy comparison: Euler vs RK4 on dy/dt = y");
  std::println("  y(0) = 1, integrating to t = 2, exact = e² ≈ 7.389");
  std::println("═══════════════════════════════════════════════════════════════");

  constexpr auto expOde = [](double /*t*/, double y) { return y; };
  double exact = std::exp(2.0);

  std::println("\n  {:>8}  {:>14}  {:>14}  {:>14}", "dt", "Euler error",
               "RK4 error", "Ratio");
  std::println("  {:─>8}  {:─>14}  {:─>14}  {:─>14}", "", "", "", "");

  for (double dt : {0.1, 0.05, 0.02, 0.01}) {
    double euler_result = integrateWith<Euler>(expOde, 1.0, 0.0, 2.0, dt);
    double rk4_result = integrate(expOde, 1.0, 0.0, 2.0, dt);
    double euler_err = std::abs(euler_result - exact);
    double rk4_err = std::abs(rk4_result - exact);

    std::println("  {:>8.3f}  {:>14.6e}  {:>14.6e}  {:>14.0f}x", dt, euler_err,
                 rk4_err, euler_err / rk4_err);
  }
  std::println("\n  Euler is O(dt), RK4 is O(dt⁴) - RK4 converges much faster!");

  // ============================================================================
  // 3. Damped Oscillator: d²x/dt² = -x - 0.2·dx/dt
  // ============================================================================
  std::println("\n═══════════════════════════════════════════════════════════════");
  std::println("  Damped Oscillator: d²x/dt² = -x - 0.2·dx/dt");
  std::println("  x(0) = 1, v(0) = 0  (decaying sinusoid)");
  std::println("═══════════════════════════════════════════════════════════════");

  constexpr double damping = 0.2;
  constexpr auto dampedOsc = [](double /*t*/, Vec2 s) -> Vec2 {
    return {.x = s.y, .y = -s.x - damping * s.y};
  };

  // Print trajectory as ASCII
  Vec2 state{.x = 1.0, .y = 0.0};
  double t = 0.0;
  double dt = 0.1;

  std::println("\n  t       x(t)");
  std::println("  -----   ---------------------------------------------------");
  for (int i = 0; i <= 50; ++i) {
    int bar_pos = static_cast<int>((state.x + 1.0) * 25);  // map [-1,1] to [0,50]
    bar_pos = std::clamp(bar_pos, 0, 50);

    std::string bar(51, ' ');
    bar[25] = '|';  // center line (x=0)
    bar[bar_pos] = '*';

    std::println("  {:5.1f}   {}", t, bar);

    state = RungeKutta4::step(dampedOsc, state, t, dt);
    t += dt;
  }

  std::println("\n  The oscillation decays exponentially due to damping.\n");

  return 0;
}
