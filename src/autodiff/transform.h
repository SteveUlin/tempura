#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "dual.h"
#include "tape.h"
#include "var.h"

namespace tempura::autodiff {

// The transform layer. Forward (jvp) and reverse (vjp) are the only primitives; grad and
// valueAndGrad are reverse mode with a standard seed. Jacobians (jacobian.h) and Hessians
// (hessian.h) compose from these. The function f is written ONCE as a generic callable
// (e.g. `[](auto... x){ ... }`) and differentiated either way — jvp instantiates it on
// Dual, the reverse transforms on Var. Seeds are ALWAYS caller-supplied tangent/cotangent
// vectors, never a baked-in `1.` literal (which would break non-double scalars).

// Forward JVP, scalar f: T → T. Returns (f(x), f'(x)·v).
template <typename F, typename T>
auto jvp(F&& f, const T& x, const T& v) -> std::pair<T, T> {
  auto r = f(Dual<T>{x, v});
  return {r.value, r.gradient};
}

// Forward JVP, f: Tⁿ → T. One pass gives (f(x), J·v) — the directional derivative along v.
template <typename F, typename T, std::size_t N>
auto jvp(F&& f, const std::array<T, N>& x, const std::array<T, N>& v) -> std::pair<T, T> {
  auto r = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return f(Dual<T>{x[I], v[I]}...);
  }(std::make_index_sequence<N>{});
  return {r.value, r.gradient};
}

// Reverse value-and-gradient, f: Tⁿ → T. One tape, one backward sweep → ∇f.
template <typename F, typename T, std::size_t N>
auto valueAndGrad(F&& f, const std::array<T, N>& x) -> std::pair<T, std::array<T, N>> {
  Tape<T> tape;
  std::array<Var<T>, N> vars{};
  for (std::size_t i = 0; i < N; ++i) vars[i] = variable(tape, x[i]);
  Var<T> y = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return f(vars[I]...);
  }(std::make_index_sequence<N>{});
  const auto adj = tape.backward(y.idx);
  std::array<T, N> g{};
  for (std::size_t i = 0; i < N; ++i) g[i] = adj[vars[i].idx];
  return {y.value, g};
}

// Just the gradient.
template <typename F, typename T, std::size_t N>
auto grad(F&& f, const std::array<T, N>& x) -> std::array<T, N> {
  return valueAndGrad(std::forward<F>(f), x).second;
}

// Reverse VJP, f: Tⁿ → T. Returns (f(x), pullback) where pullback(cotangent) = cotangent·J
// (the input adjoints). The pullback OWNS the tape, so it outlives this call and can be
// re-invoked with any cotangent — the forward pass is recorded once, reused per cotangent.
template <typename F, typename T, std::size_t N>
auto vjp(F&& f, const std::array<T, N>& x) {
  Tape<T> tape;
  std::array<std::uint32_t, N> in{};
  std::array<Var<T>, N> vars{};
  for (std::size_t i = 0; i < N; ++i) {
    vars[i] = variable(tape, x[i]);
    in[i] = vars[i].idx;
  }
  Var<T> y = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return f(vars[I]...);
  }(std::make_index_sequence<N>{});
  const std::uint32_t output = y.idx;
  // Move the tape into the closure; only integer indices of `vars` are needed afterward,
  // so the now-dangling Var pointers are never touched again.
  auto pullback = [tape = std::move(tape), output, in](const T& cotangent) -> std::array<T, N> {
    const auto adj = tape.backward(output, cotangent);
    std::array<T, N> g{};
    for (std::size_t i = 0; i < N; ++i) g[i] = adj[in[i]];
    return g;
  };
  return std::pair{y.value, std::move(pullback)};
}

}  // namespace tempura::autodiff
