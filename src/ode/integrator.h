#pragma once

#include <utility>

namespace tempura::ode {

// Stateful ODE integrator that wraps any stepper
// Tracks time, state, and step size while delegating stepping to the Stepper.
template <typename Stepper, typename OdeFunc, typename State, typename T>
class Integrator {
 public:
  constexpr Integrator(OdeFunc f, State initial, T t0, T dt)
      : f_{std::move(f)}, state_{std::move(initial)}, t_{t0}, dt_{dt} {}

  constexpr auto t() const -> T { return t_; }
  constexpr auto state() const -> const State& { return state_; }
  constexpr auto dt() const -> T { return dt_; }

  // Advance one time step
  constexpr void step() {
    state_ = Stepper::step(f_, state_, t_, dt_);
    t_ += dt_;
  }

  // Advance until t >= t_target
  constexpr void stepTo(T t_target) {
    while (t_ < t_target) {
      step();
    }
  }

 private:
  OdeFunc f_;
  State state_;
  T t_;
  T dt_;
};

}  // namespace tempura::ode
