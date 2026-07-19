#pragma once

#include <array>
#include <cstddef>
#include <utility>

#include "dual.h"
#include "tape.h"

namespace tempura::autodiff {

// The transform layer. Forward (jvp) and reverse (vjp) are the only primitives; grad and
// valueAndGrad are reverse mode with a standard seed. Jacobians (jacobian.h) and Hessians
// (hessian.h) compose from these. The function f is written ONCE as a generic callable
// (e.g. `[](auto... x){ ... }`) and differentiated either way — jvp instantiates it on
// Dual, the reverse transforms on Var. Seeds are ALWAYS caller-supplied tangent/cotangent
// vectors, never a baked-in `1.` literal (which would break non-double scalars).

// Forward JVP, scalar f: T → T. Returns (f(x), f'(x)·v).
template <typename F, typename T>
constexpr auto jvp(F&& f, const T& x, const T& v) -> std::pair<T, T> {
  auto r = f(Dual<T>{x, v});
  return {r.value, r.gradient};
}

// Forward JVP, f: Tⁿ → T. One pass gives (f(x), J·v) — the directional derivative along v.
template <typename F, typename T, std::size_t N>
constexpr auto jvp(F&& f, const std::array<T, N>& x, const std::array<T, N>& v) -> std::pair<T, T> {
  auto r = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return f(Dual<T>{x[I], v[I]}...);
  }(std::make_index_sequence<N>{});
  return {r.value, r.gradient};
}

// Reverse value-and-gradient, f: Tⁿ → T. One recording, one backward sweep → ∇f.
template <typename F, typename T, std::size_t N>
auto valueAndGrad(F&& f, const std::array<T, N>& x) -> std::pair<T, std::array<T, N>> {
  Tape<T> tape;
  auto vars = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return std::array{tape.variable(x[I])...};
  }(std::make_index_sequence<N>{});
  Var<T> y = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return f(vars[I]...);
  }(std::make_index_sequence<N>{});
  const auto adj = tape.backward(y);
  std::array<T, N> g{};
  for (std::size_t i = 0; i < N; ++i) g[i] = adj[vars[i].id];
  return {y.value, g};
}

// Just the gradient.
template <typename F, typename T, std::size_t N>
auto grad(F&& f, const std::array<T, N>& x) -> std::array<T, N> {
  return valueAndGrad(std::forward<F>(f), x).second;
}

// Reverse VJP, f: Tⁿ → T. Returns (f(x), pullback) where pullback(cotangent) = cotangent·J
// (the input adjoints). The pullback OWNS the recording, so it outlives this call and can
// be re-invoked with any cotangent — the forward pass is recorded once, reused per
// cotangent.
template <typename F, typename T, std::size_t N>
auto vjp(F&& f, const std::array<T, N>& x) {
  Tape<T> tape;
  auto vars = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return std::array{tape.variable(x[I])...};
  }(std::make_index_sequence<N>{});
  Var<T> y = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return f(vars[I]...);
  }(std::make_index_sequence<N>{});
  auto ids = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return std::array{vars[I].id...};
  }(std::make_index_sequence<N>{});
  // The tape moves into the closure; the Vars' dag pointers dangle after, so
  // only ids cross.
  auto pullback = [tape = std::move(tape), output = y.id,
                   ids](const T& cotangent) -> std::array<T, N> {
    const auto adj = tape.backward(output, cotangent);
    std::array<T, N> g{};
    for (std::size_t i = 0; i < N; ++i) g[i] = adj[ids[i]];
    return g;
  };
  return std::pair{y.value, std::move(pullback)};
}

}  // namespace tempura::autodiff
