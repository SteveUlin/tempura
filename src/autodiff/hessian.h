#pragma once

#include <array>
#include <cstddef>
#include <utility>

#include "dual.h"
#include "matrix/matrix.h"
#include "tape.h"

namespace tempura::autodiff {

// Second order by COMPOSING forward and reverse — the payoff of making every core generic
// over its scalar. Record on Dual<T> elements, seeding the forward (dual) tangent with v:
// each input's adjoint returns as a Dual whose .value is ∂f/∂xᵢ and whose .gradient is
// (H·v)ᵢ. No bespoke second-order machinery — Tape<Dual<T>> just works.
//
// hvp gives a Hessian-vector product in ONE forward-over-reverse pass (H never
// materialized — the matrix-free primitive Newton/CG methods want). hessian assembles the
// full matrix as N hvps over the basis (column j = H·eⱼ).

// (∇f(x), H·v) — value AND Hessian-vector product, one pass. f is the usual generic
// callable (here instantiated on Var<Dual<T>>).
template <typename F, typename T, std::size_t N>
auto gradAndHvp(F&& f, const std::array<T, N>& x, const std::array<T, N>& v)
    -> std::pair<std::array<T, N>, std::array<T, N>> {
  using D = Dual<T>;
  Tape<D> tape;
  auto vars = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return std::array{tape.variable(D{x[I], v[I]})...};  // value x, forward tangent v
  }(std::make_index_sequence<N>{});
  Var<D> y = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return f(vars[I]...);
  }(std::make_index_sequence<N>{});
  const auto adj = tape.backward(y, D{T{1}, T{}});  // reverse cotangent 1, no forward part
  std::array<T, N> g{};
  std::array<T, N> hv{};
  for (std::size_t i = 0; i < N; ++i) {
    g[i] = adj[vars[i].id].value;      // ∂f/∂xᵢ
    hv[i] = adj[vars[i].id].gradient;  // (H·v)ᵢ
  }
  return {g, hv};
}

template <typename F, typename T, std::size_t N>
auto hvp(F&& f, const std::array<T, N>& x, const std::array<T, N>& v) -> std::array<T, N> {
  return gradAndHvp(std::forward<F>(f), x, v).second;
}

// Full Hessian as InlineDense<T,N,N>: column j = H·eⱼ (N forward-over-reverse passes).
template <typename F, typename T, std::size_t N>
auto hessian(F&& f, const std::array<T, N>& x) -> InlineDense<T, N, N> {
  InlineDense<T, N, N> hess{};
  for (std::size_t j = 0; j < N; ++j) {
    std::array<T, N> e{};
    e[j] = T{1};
    const auto col = hvp(f, x, e);
    for (std::size_t i = 0; i < N; ++i) hess[i, j] = col[i];
  }
  return hess;
}

}  // namespace tempura::autodiff
