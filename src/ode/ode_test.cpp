#include "ode/euler.h"
#include "ode/integrate.h"
#include "ode/integrator.h"
#include "ode/runge_kutta.h"

#include <cmath>

#include "unit.h"

using namespace tempura;
using namespace tempura::ode;

// dy/dt = y  →  y(t) = y₀ * e^t
constexpr auto exponentialGrowth = [](auto /*t*/, auto y) { return y; };

// dy/dt = -y  →  y(t) = y₀ * e^{-t}
constexpr auto exponentialDecay = [](auto /*t*/, auto y) { return -y; };

// dy/dt = 1  →  y(t) = y₀ + t
constexpr auto linear = [](auto /*t*/, auto /*y*/) { return 1.0; };

auto main() -> int {
  // ============================================================================
  // Euler tests
  // ============================================================================

  "euler single step on linear ODE"_test = [] {
    double y = Euler::step(linear, 0.0, 0.0, 0.5);
    expectNear(0.5, y, 1e-10);
  };

  "euler single step on exponential"_test = [] {
    // Euler: y_{n+1} = y + dt * y = 1 + 0.1 * 1 = 1.1
    double y = Euler::step(exponentialGrowth, 1.0, 0.0, 0.1);
    expectNear(1.1, y, 1e-10);
  };

  "euler multi-step convergence"_test = [] {
    // Integrate dy/dt = y from t=0 to t=1, y(0)=1
    // Exact solution: y(1) = e ≈ 2.718...
    double y = 1.0;
    double dt = 0.001;
    for (double t = 0.0; t < 1.0; t += dt) {
      y = Euler::step(exponentialGrowth, y, t, dt);
    }
    double exact = std::exp(1.0);
    // Euler is O(dt), expect ~0.1% error with dt=0.001
    expectNear(exact, y, 0.01);
  };

  // ============================================================================
  // Runge-Kutta 4 tests
  // ============================================================================

  "rk4 single step on linear ODE"_test = [] {
    // RK4 is exact for polynomials up to degree 4
    double y = RungeKutta4::step(linear, 0.0, 0.0, 0.5);
    expectNear(0.5, y, 1e-10);
  };

  "rk4 single step accuracy"_test = [] {
    // For dy/dt = y, RK4 should give very accurate result
    double y = RungeKutta4::step(exponentialGrowth, 1.0, 0.0, 0.1);
    double exact = std::exp(0.1);
    expectNear(exact, y, 1e-6);
  };

  "rk4 vs exact exponential"_test = [] {
    // Integrate dy/dt = y from t=0 to t=1, y(0)=1
    double y = 1.0;
    double dt = 0.01;
    for (double t = 0.0; t < 1.0; t += dt) {
      y = RungeKutta4::step(exponentialGrowth, y, t, dt);
    }
    double exact = std::exp(1.0);
    // RK4 is O(dt^4), very accurate
    expectNear(exact, y, 1e-8);
  };

  // ============================================================================
  // Integrator class tests
  // ============================================================================

  "integrator initial state"_test = [] {
    Integrator<RungeKutta4, decltype(exponentialGrowth), double, double> solver{
        exponentialGrowth, 1.0, 0.0, 0.1};
    expectNear(0.0, solver.t(), 1e-10);
    expectNear(1.0, solver.state(), 1e-10);
    expectNear(0.1, solver.dt(), 1e-10);
  };

  "integrator step advances time"_test = [] {
    Integrator<RungeKutta4, decltype(exponentialGrowth), double, double> solver{
        exponentialGrowth, 1.0, 0.0, 0.1};
    solver.step();
    expectNear(0.1, solver.t(), 1e-10);
    expectTrue(solver.state() > 1.0);  // State should have grown

    solver.step();
    expectNear(0.2, solver.t(), 1e-10);
  };

  "integrator stepTo"_test = [] {
    Integrator<RungeKutta4, decltype(exponentialGrowth), double, double> solver{
        exponentialGrowth, 1.0, 0.0, 0.01};
    solver.stepTo(1.0);
    expectTrue(solver.t() >= 1.0);

    double exact = std::exp(1.0);
    expectNear(exact, solver.state(), 1e-6);
  };

  "integrator with euler stepper"_test = [] {
    Integrator<Euler, decltype(exponentialGrowth), double, double> solver{
        exponentialGrowth, 1.0, 0.0, 0.001};
    solver.stepTo(1.0);

    double exact = std::exp(1.0);
    expectNear(exact, solver.state(), 0.01);
  };

  // ============================================================================
  // Convenience function tests
  // ============================================================================

  "integrate convenience function"_test = [] {
    auto result = integrate(exponentialGrowth, 1.0, 0.0, 1.0, 0.01);
    double exact = std::exp(1.0);
    expectNear(exact, result, 1e-6);
  };

  "integrateWith euler"_test = [] {
    auto result =
        integrateWith<Euler>(exponentialGrowth, 1.0, 0.0, 1.0, 0.001);
    double exact = std::exp(1.0);
    expectNear(exact, result, 0.01);
  };

  "integrate decay ODE"_test = [] {
    auto result = integrate(exponentialDecay, 1.0, 0.0, 2.0, 0.01);
    double exact = std::exp(-2.0);
    expectNear(exact, result, 1e-6);
  };

  // ============================================================================
  // constexpr tests
  // ============================================================================

  "euler constexpr"_test = [] {
    constexpr auto result = Euler::step(exponentialGrowth, 1.0, 0.0, 0.1);
    static_assert(result > 1.0);
    expectNear(1.1, result, 1e-10);
  };

  "rk4 constexpr"_test = [] {
    constexpr auto result = RungeKutta4::step(exponentialGrowth, 1.0, 0.0, 0.1);
    static_assert(result > 1.0);
    expectNear(std::exp(0.1), result, 1e-6);
  };

  return TestRegistry::result();
}
