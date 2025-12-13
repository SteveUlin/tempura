#pragma once

#include <utility>

namespace tempura::ode {

// Classical 4th-order Runge-Kutta method
// dy/dt = f(t, y)
//
// k1 = f(t, y)
// k2 = f(t + dt/2, y + dt/2 * k1)
// k3 = f(t + dt/2, y + dt/2 * k2)
// k4 = f(t + dt, y + dt * k3)
// y_{n+1} = y_n + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
//
// Fourth-order accurate, the "workhorse" of ODE solvers.
struct RungeKutta4 {
  template <typename OdeFunc, typename State, typename T>
  static constexpr auto step(OdeFunc&& f, const State& state, T t, T dt)
      -> State {
    auto k1 = f(t, state);
    auto k2 = f(t + dt / 2, state + (dt / 2) * k1);
    auto k3 = f(t + dt / 2, state + (dt / 2) * k2);
    auto k4 = f(t + dt, state + dt * k3);
    return state + (dt / 6) * (k1 + k2 * 2 + k3 * 2 + k4);
  }
};

}  // namespace tempura::ode
