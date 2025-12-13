#pragma once

#include <concepts>
#include <type_traits>

namespace tempura::ode {

// An ODE function: f(t, state) -> derivative
// The derivative must be the same type as the state (or convertible to it)
template <typename F, typename T, typename State>
concept OdeFunction = std::invocable<F, T, State> &&
    std::convertible_to<std::invoke_result_t<F, T, State>, State>;

// A Stepper provides static step(f, state, t, dt) -> new_state
template <typename S, typename F, typename State, typename T>
concept Stepper = requires(F f, const State& state, T t, T dt) {
  { S::step(f, state, t, dt) } -> std::convertible_to<State>;
};

}  // namespace tempura::ode
