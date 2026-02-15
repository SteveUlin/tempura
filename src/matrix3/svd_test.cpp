#include "matrix3/svd.h"

#include <cmath>

#include "matrix3/lu_decomposition.h"
#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "SVD - verify U*Σ*Vᵀ = A for 2x2"_test = [] {
    InlineDense<double, 2, 2> A{{4.0, 3.0}, {2.0, 1.0}};
    auto A_orig = A;

    auto svd = SVD{std::move(A)};
    const auto& U = svd.U();
    const auto& V = svd.V();
    const auto& s = svd.singularValues();

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 2; ++k) {
          sum += U[i, k] * s[k] * V[j, k];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "SVD - verify U*Σ*Vᵀ = A for 3x3"_test = [] {
    InlineDense<double, 3, 3> A{
        {1.0, 2.0, 3.0}, {2.0, 5.0, 2.0}, {6.0, -3.0, 1.0}};
    auto A_orig = A;

    auto svd = SVD{std::move(A)};
    const auto& U = svd.U();
    const auto& V = svd.V();
    const auto& s = svd.singularValues();

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += U[i, k] * s[k] * V[j, k];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "SVD - verify U*Σ*Vᵀ = A for 4x4"_test = [] {
    InlineDense<double, 4, 4> A{
        {4.0, 3.0, 2.0, 1.0},
        {3.0, 4.0, 3.0, 2.0},
        {2.0, 3.0, 4.0, 3.0},
        {1.0, 2.0, 3.0, 4.0}};
    auto A_orig = A;

    auto svd = SVD{std::move(A)};
    const auto& U = svd.U();
    const auto& V = svd.V();
    const auto& s = svd.singularValues();

    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 4; ++k) {
          sum += U[i, k] * s[k] * V[j, k];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "SVD - U is orthogonal"_test = [] {
    InlineDense<double, 3, 3> A{
        {1.0, 2.0, 3.0}, {2.0, 5.0, 2.0}, {6.0, -3.0, 1.0}};

    auto svd = SVD{std::move(A)};
    const auto& U = svd.U();

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += U[k, i] * U[k, j];
        }
        expectNear((i == j) ? 1.0 : 0.0, sum, 1e-9);
      }
    }
  };

  "SVD - V is orthogonal"_test = [] {
    InlineDense<double, 3, 3> A{
        {1.0, 2.0, 3.0}, {2.0, 5.0, 2.0}, {6.0, -3.0, 1.0}};

    auto svd = SVD{std::move(A)};
    const auto& V = svd.V();

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += V[k, i] * V[k, j];
        }
        expectNear((i == j) ? 1.0 : 0.0, sum, 1e-9);
      }
    }
  };

  "SVD - singular values non-negative and descending"_test = [] {
    InlineDense<double, 4, 4> A{
        {4.0, 3.0, 2.0, 1.0},
        {3.0, 4.0, 3.0, 2.0},
        {2.0, 3.0, 4.0, 3.0},
        {1.0, 2.0, 3.0, 4.0}};

    auto svd = SVD{std::move(A)};
    const auto& s = svd.singularValues();

    for (std::size_t i = 0; i < s.size(); ++i) {
      expectGE(s[i], 0.0);
    }
    for (std::size_t i = 0; i + 1 < s.size(); ++i) {
      expectGE(s[i], s[i + 1]);
    }
  };

  "SVD solve - known solution"_test = [] {
    // A = [4 2; 3 1], x = [1, 2], b = A*x = [8, 5]
    InlineDense<double, 2, 2> A{{4.0, 2.0}, {3.0, 1.0}};
    InlineDense<double, 2, 1> b{{8.0}, {5.0}};

    auto svd = SVD{A};
    svd.solve(b);

    expectNear(1.0, b[0, 0], 1e-9);
    expectNear(2.0, b[1, 0], 1e-9);
  };

  "SVD solve - verify A*x = b"_test = [] {
    InlineDense<double, 3, 3> A{
        {1.0, 2.0, 3.0}, {2.0, 5.0, 2.0}, {6.0, -3.0, 1.0}};
    auto A_orig = A;
    InlineDense<double, 3, 1> b{{1.0}, {2.0}, {3.0}};

    auto svd = SVD{std::move(A)};
    svd.solve(b);

    auto r0 = A_orig[0, 0] * b[0, 0] + A_orig[0, 1] * b[1, 0] + A_orig[0, 2] * b[2, 0];
    auto r1 = A_orig[1, 0] * b[0, 0] + A_orig[1, 1] * b[1, 0] + A_orig[1, 2] * b[2, 0];
    auto r2 = A_orig[2, 0] * b[0, 0] + A_orig[2, 1] * b[1, 0] + A_orig[2, 2] * b[2, 0];

    expectNear(1.0, r0, 1e-9);
    expectNear(2.0, r1, 1e-9);
    expectNear(3.0, r2, 1e-9);
  };

  "SVD solve - identity matrix"_test = [] {
    InlineDense<double, 3, 3> I{
        {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    auto svd = SVD{I};

    InlineDense<double, 3, 1> b{{1.0}, {2.0}, {3.0}};
    svd.solve(b);

    expectNear(1.0, b[0, 0], 1e-9);
    expectNear(2.0, b[1, 0], 1e-9);
    expectNear(3.0, b[2, 0], 1e-9);
  };

  "SVD - diagonal matrix"_test = [] {
    InlineDense<double, 3, 3> A{
        {5.0, 0.0, 0.0}, {0.0, 2.0, 0.0}, {0.0, 0.0, 8.0}};

    auto svd = SVD{std::move(A)};
    const auto& s = svd.singularValues();

    // Singular values = absolute diagonal entries, sorted descending
    expectNear(8.0, s[0], 1e-9);
    expectNear(5.0, s[1], 1e-9);
    expectNear(2.0, s[2], 1e-9);
  };

  "SVD - determinant cross-check with LU"_test = [] {
    InlineDense<double, 3, 3> A{
        {1.0, 2.0, 3.0}, {2.0, 5.0, 2.0}, {6.0, -3.0, 1.0}};
    auto A_copy = A;

    auto svd = SVD{std::move(A)};
    auto lu = LU{std::move(A_copy)};

    // |det(A)| = product of singular values
    double svd_det = 1.0;
    for (const auto& v : svd.singularValues()) {
      svd_det *= v;
    }

    using std::abs;
    expectNear(abs(lu.determinant()), svd_det, 1e-9);
  };

  "SVD - 1x1"_test = [] {
    InlineDense<double, 1, 1> A{{-7.0}};
    auto svd = SVD{A};

    expectNear(7.0, svd.singularValues()[0], 1e-9);
  };

  "SVD - CTAD"_test = [] {
    auto A = InlineDense{{4.0, 2.0}, {3.0, 1.0}};
    auto svd = SVD{A};

    auto b = InlineDense{{8.0}, {5.0}};
    svd.solve(b);

    expectNear(1.0, b[0, 0], 1e-9);
    expectNear(2.0, b[1, 0], 1e-9);
  };

  return TestRegistry::result();
}
