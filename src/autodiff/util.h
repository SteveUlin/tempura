#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "autodiff/dual.h"
#include "autodiff/node.h"
#include "broadcast_array.h"
#include "matrix/storage/dense.h"

namespace tempura::autodiff {

template <typename... Ts, typename... Gs>
void oneHotGradients(std::tuple<Dual<Ts, Gs>...>& duals) {
  [&]<size_t... I>(std::index_sequence<I...> /*unused*/) {
    ((std::get<I>(duals).gradient[I] = 1.), ...);
  }(std::make_index_sequence<sizeof...(Ts)>{});
}

template <typename F, typename... Args>
auto valueAndForwardGradient(F&& f, Args&&... args) {
  constexpr auto N = sizeof...(Args);

  auto variables =
      std::tuple{Dual{std::forward<Args>(args),
                      BroadcastArray<std::remove_cvref_t<Args>, N>{}}...};
  oneHotGradients(variables);

  auto result = std::apply(std::forward<F>(f), variables);

  return [&]<size_t... I>(std::index_sequence<I...> /*unused*/) {
    return std::tuple{result.value, get<I>(result.gradient)...};
  }(std::make_index_sequence<N>{});
}

// Helper function to remove the first N elements of a tuple
template <size_t N, typename... Ts>
auto dropFront(std::tuple<Ts...>&& tuple) {
  return [&]<size_t... I>(std::index_sequence<I...> /*unused*/) {
    return std::tuple{std::get<I + N>(tuple)...};
  }(std::make_index_sequence<sizeof...(Ts) - N>{});
}

// Same as above but without the value
template <typename F, typename... Args>
auto forwardGradient(F&& f, Args&&... args) {
  return dropFront<1>(
      valueAndForwardGradient(std::forward<F>(f), std::forward<Args>(args)...));
}

template <typename F, typename... Args>
auto valueAndReverseGradient(F&& f, Args&&... args) {
  auto varaibles = std::tuple{Variable<std::remove_cvref_t<Args>>{}...};
  auto result = std::apply(std::forward<F>(f), varaibles);
  return std::apply(
      [&](auto&... var) { return differentiate(result, (var = args)...); },
      varaibles);
}

template <typename F, typename Scalar, matrix::RowCol extent,
          matrix::IndexOrder order>
auto jacobianReverse(F&& f, const matrix::Dense<Scalar, extent, order>& arg) {
  constexpr int64_t M = extent.row;
  matrix::Dense<Variable<Scalar>, {M, 1}, order> variables;
  for (int64_t i = 0; i < M; ++i) {
    variables[i].bind(arg[i]);
  }

  auto result = f(variables);
  constexpr int64_t N = decltype(result)::kExtent.row;

  auto jacobian_matrix = matrix::Dense<Scalar, {N, M}, order>{};

  for (int64_t i = 0; i < N; ++i) {
    result[i].node->propagate(1.);
    for (int64_t j = 0; j < M; ++j) {
      jacobian_matrix[i, j] = variables[j].derivative();
      variables[j].clearDerivative();
    }
  }
  return jacobian_matrix;
}

template <typename F, typename Scalar, matrix::RowCol extent,
          matrix::IndexOrder order>
auto jacobianForward(F&& f, const matrix::Dense<Scalar, extent, order>& arg) {
  constexpr int64_t M = extent.row;
  matrix::Dense<Dual<Scalar, BroadcastArray<Scalar, M>>, {M, 1}, order>
      variables;
  [&]<size_t... I>(std::index_sequence<I...> /*unused*/) {
    (
        [&]() {
          variables[I] = Dual{arg[I], BroadcastArray<Scalar, M>{}};
          variables[I].gradient[I] = 1.;
        }(),
        ...);
  }(std::make_index_sequence<M>{});

  auto result = f(variables);
  constexpr int64_t N = decltype(result)::kExtent.row;
  auto jacobian_matrix = matrix::Dense<Scalar, {N, M}, order>{};

  for (int64_t i = 0; i < N; ++i) {
    for (int64_t j = 0; j < M; ++j) {
      jacobian_matrix[i, j] = result[i].gradient[j];
    }
  }
  return jacobian_matrix;
}

// Derivative of a function at a point with respect to a single output
// variable.
// template <typename F, typename... Args>
// auto valueAndForwardGradiant(F f, Args&&... args) {
//   auto variables = std::tuple{
//       Dual<std::remove_cvref_t<Args>,
//            BroadcastArray<std::remove_cvref_t<Args>, sizeof...(Args)>>{
//           args}...};
//   return variables;
// }

// template <typename F, typename... Args>
// auto hessian(F func, Args&&... args) {
//   constexpr auto N = sizeof...(Args);
//
//   auto varaibles =
//       std::tuple{Variable<Dual<Args, BroadcastArray<Args, N>>>{}...};
//
//   auto output_node = std::apply(func, varaibles);
//
//   auto inputs = std::tuple{
//     Dual<Args, BroadcastArray<Args, N>>{std::forward<Args>(args), {}}...};
//
//   [&]<size_t... I>(std::index_sequence<I...> /*unused*/) {
//     ((std::get<I>(inputs).gradient[I] = 1.), ...);
//   }(std::make_index_sequence<N>{});
//
//   auto result = std::apply([&](auto&... var) {
//     return std::apply([&](auto&... input) {
//       return differentiate(output_node, (var = input)...);
//     }, inputs);
//   }, varaibles);
// }

}  // namespace tempura::autodiff
