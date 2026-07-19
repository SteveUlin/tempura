#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "dual.h"
#include "matrix/matrix.h"
#include "tape.h"

namespace tempura::autodiff {

// Jacobians of f: Tⁿ → Tᵐ (f returns a std::array of M differentiable outputs), landing in
// a src/matrix InlineDense<T, M, N> with the convention J[m, n] = ∂fₘ/∂xₙ.
//
// jacfwd seeds one input direction at a time (identity columns) and reads all M output
// tangents per pass → N forward passes, each filling a COLUMN. Cheap when M ≥ N (tall).
// jacrev seeds one output cotangent at a time and reads all N input adjoints per pass → M
// reverse passes, each filling a ROW. Cheap when N ≥ M (wide). They agree on the same J;
// which is cheaper is purely the map's shape.
//
// NOTE: each pass here is independent, so the seed loop is the natural place for a future
// task/-bulk parallel backend (see the autodiff plan) — kept serial until measured.

template <typename X, std::size_t>
using Repeat = X;

template <typename F, typename T, std::size_t N>
constexpr auto jacfwd(F&& f, const std::array<T, N>& x) {
  // M from f's return TYPE alone — shape discovery never calls f.
  constexpr std::size_t M = []<std::size_t... I>(std::index_sequence<I...>) {
    return std::tuple_size_v<std::invoke_result_t<F&, Repeat<Dual<T>, I>...>>;
  }(std::make_index_sequence<N>{});

  InlineDense<T, M, N> jac{};
  for (std::size_t j = 0; j < N; ++j) {
    auto out = [&]<std::size_t... I>(std::index_sequence<I...>) {
      return f(Dual<T>{x[I], (I == j ? T{1} : T{})}...);  // tangent = eⱼ
    }(std::make_index_sequence<N>{});
    for (std::size_t m = 0; m < M; ++m) jac[m, j] = out[m].gradient;  // column j = J·eⱼ
  }
  return jac;
}

template <typename F, typename T, std::size_t N>
auto jacrev(F&& f, const std::array<T, N>& x) {
  Tape<T> tape;
  auto vars = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return std::array{tape.variable(x[I])...};
  }(std::make_index_sequence<N>{});
  auto out = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return f(vars[I]...);
  }(std::make_index_sequence<N>{});
  constexpr std::size_t M = std::tuple_size_v<decltype(out)>;

  InlineDense<T, M, N> jac{};
  for (std::size_t m = 0; m < M; ++m) {  // cotangent = eₘ → row m = eₘᵀ·J
    const auto adj = tape.backward(out[m]);
    for (std::size_t n = 0; n < N; ++n) jac[m, n] = adj[vars[n].id];
  }
  return jac;
}

}  // namespace tempura::autodiff
