#pragma once

#include "integrator.h"
#include "runge_kutta.h"

namespace tempura::ode {

// Convenience function: integrate ODE to t_end and return final state
// Uses RK4 by default as the workhorse method.
template <typename OdeFunc, typename State, typename T>
constexpr auto integrate(OdeFunc&& f, State initial, T t0, T t_end, T dt)
    -> State {
  auto solver = Integrator<RungeKutta4, OdeFunc, State, T>{f, initial, t0, dt};
  solver.stepTo(t_end);
  return solver.state();
}

// Integrate with a specific stepper
template <typename Stepper, typename OdeFunc, typename State, typename T>
constexpr auto integrateWith(OdeFunc&& f, State initial, T t0, T t_end, T dt)
    -> State {
  auto solver = Integrator<Stepper, OdeFunc, State, T>{f, initial, t0, dt};
  solver.stepTo(t_end);
  return solver.state();
}

}  // namespace tempura::ode
