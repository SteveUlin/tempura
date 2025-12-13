#pragma once

#include <utility>

namespace tempura::ode {

// Explicit Euler method (first-order)
// dy/dt = f(t, y)  →  y_{n+1} = y_n + dt * f(t_n, y_n)
//
// Simple but only first-order accurate. Useful for testing and
// as a building block for higher-order methods.
struct Euler {
  template <typename OdeFunc, typename State, typename T>
  static constexpr auto step(OdeFunc&& f, const State& state, T t, T dt)
      -> State {
    return state + dt * std::forward<OdeFunc>(f)(t, state);
  }
};

}  // namespace tempura::ode
