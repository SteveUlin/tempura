#pragma once

#include <utility>

#include "matrix2/matrix.h"
#include "matrix2/storage/dense.h"
#include "optimization/util.h"

namespace tempura::optimization {

// Downhill Simplex
// Given a N-Simplex, a matrix of N+1 points in N dimensional
// space, find the minimum of a function defined on that space.
//
// Downhill Simplex takes the point with the highest function value
// and "pushes" it through the opposing face to get to a hopefully
// lower value.
template <int64_t N>
auto downhillSimplex(matrix::MatrixT auto& mat, auto&& Func) {
  matrix::Dense<double, 1, N + 1> f_vals;
  for (int64_t i = 0; i < mat.shape().row; ++i) {
    [&]<std::int64_t... Is>(std::index_sequence<Is...>) {
      f_vals[i] = Func(mat[i, Is]...);
    }(std::make_index_sequence<N + 1>());
  }

  constexpr auto scale = [&](int64_t index, double fac) {
    matrix::Dense<double, N, 1> next;
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < N + 1; ++j) {
        next[j, 0] += fac * (mat[i, j] - mat[i, index]) / N + mat[i, index];
      }
    }
    return next;
  };

  int64_t count = 0;
  while (true) {
    int64_t high = 0;
    double f_low = f_vals[0];
    int64_t low = 0;
    double f_high = f_vals[0];
    for (int64_t i = 1; i < mat.shape().row; ++i) {
      if (f_vals[i] < f_low) {
        low = i;
        f_low = f_vals[i];
      }
      if (f_vals[i] > f_high) {
        high = i;
        f_high = f_vals[i];
      }
    }
    // TODO: Pass tolerance as argument
    if (f_high - f_low < 1e-4) {
      return mat;
    }

    ++count;
  }
}

}  // namespace tempura::optimization
