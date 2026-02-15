#include "matrix3/symmetric_eigen.h"

#include <cmath>

#include "matrix3/lu_decomposition.h"
#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "SymmetricEigen - 2x2 known eigenvalues"_test = [] {
    // A = [3 1; 1 3], eigenvalues = 2, 4
    InlineDense<double, 2, 2> A{{3.0, 1.0}, {1.0, 3.0}};

    auto eig = SymmetricEigen{std::move(A)};
    const auto& vals = eig.eigenvalues();

    expectNear(2.0, vals[0], 1e-9);
    expectNear(4.0, vals[1], 1e-9);
  };

  "SymmetricEigen - 2x2 eigenvectors"_test = [] {
    // A = [3 1; 1 3]
    // λ=2: eigenvector [1, -1]/√2
    // λ=4: eigenvector [1, 1]/√2
    InlineDense<double, 2, 2> A{{3.0, 1.0}, {1.0, 3.0}};

    auto eig = SymmetricEigen{std::move(A)};
    const auto& V = eig.eigenvectors();

    // Eigenvectors may be ± flipped; check absolute values
    using std::abs;
    expectNear(1.0 / std::sqrt(2.0), abs(V[0, 0]), 1e-9);
    expectNear(1.0 / std::sqrt(2.0), abs(V[1, 0]), 1e-9);
    // Signs should be opposite for λ=2
    expectNear(-V[0, 0], V[1, 0], 1e-9);

    expectNear(1.0 / std::sqrt(2.0), abs(V[0, 1]), 1e-9);
    expectNear(1.0 / std::sqrt(2.0), abs(V[1, 1]), 1e-9);
    // Signs should be equal for λ=4
    expectNear(V[0, 1], V[1, 1], 1e-9);
  };

  "SymmetricEigen - 3x3 reconstruction VDVᵀ = A"_test = [] {
    InlineDense<double, 3, 3> A{
        {4.0, 2.0, 2.0}, {2.0, 5.0, 3.0}, {2.0, 3.0, 6.0}};
    auto A_orig = A;

    auto eig = SymmetricEigen{std::move(A)};
    const auto& V = eig.eigenvectors();
    const auto& d = eig.eigenvalues();

    // Reconstruct: A = V · diag(d) · Vᵀ
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += V[i, k] * d[k] * V[j, k];  // V·D·Vᵀ = Σ d_k * v_k * v_kᵀ
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "SymmetricEigen - 4x4 reconstruction VDVᵀ = A"_test = [] {
    InlineDense<double, 4, 4> A{
        {10.0, 2.0, 1.0, 0.0},
        {2.0, 8.0, 2.0, 1.0},
        {1.0, 2.0, 6.0, 2.0},
        {0.0, 1.0, 2.0, 4.0}};
    auto A_orig = A;

    auto eig = SymmetricEigen{std::move(A)};
    const auto& V = eig.eigenvectors();
    const auto& d = eig.eigenvalues();

    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 4; ++k) {
          sum += V[i, k] * d[k] * V[j, k];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "SymmetricEigen - V is orthogonal (VᵀV ≈ I)"_test = [] {
    InlineDense<double, 3, 3> A{
        {4.0, 2.0, 2.0}, {2.0, 5.0, 3.0}, {2.0, 3.0, 6.0}};

    auto eig = SymmetricEigen{std::move(A)};
    const auto& V = eig.eigenvectors();

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += V[k, i] * V[k, j];
        }
        double expected = (i == j) ? 1.0 : 0.0;
        expectNear(expected, sum, 1e-9);
      }
    }
  };

  "SymmetricEigen - identity matrix"_test = [] {
    InlineDense<double, 3, 3> I{
        {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};

    auto eig = SymmetricEigen{I};
    const auto& vals = eig.eigenvalues();

    expectNear(1.0, vals[0], 1e-9);
    expectNear(1.0, vals[1], 1e-9);
    expectNear(1.0, vals[2], 1e-9);
  };

  "SymmetricEigen - diagonal matrix"_test = [] {
    InlineDense<double, 3, 3> A{
        {5.0, 0.0, 0.0}, {0.0, 2.0, 0.0}, {0.0, 0.0, 8.0}};

    auto eig = SymmetricEigen{std::move(A)};
    const auto& vals = eig.eigenvalues();

    // Should be sorted ascending
    expectNear(2.0, vals[0], 1e-9);
    expectNear(5.0, vals[1], 1e-9);
    expectNear(8.0, vals[2], 1e-9);
  };

  "SymmetricEigen - eigenvalues sorted ascending"_test = [] {
    InlineDense<double, 4, 4> A{
        {10.0, 2.0, 1.0, 0.0},
        {2.0, 8.0, 2.0, 1.0},
        {1.0, 2.0, 6.0, 2.0},
        {0.0, 1.0, 2.0, 4.0}};

    auto eig = SymmetricEigen{std::move(A)};
    const auto& vals = eig.eigenvalues();

    for (std::size_t i = 0; i + 1 < vals.size(); ++i) {
      expectLE(vals[i], vals[i + 1]);
    }
  };

  "SymmetricEigen - indefinite matrix (negative eigenvalues)"_test = [] {
    // A = [1 2; 2 1] has eigenvalues -1, 3
    InlineDense<double, 2, 2> A{{1.0, 2.0}, {2.0, 1.0}};
    auto A_orig = A;

    auto eig = SymmetricEigen{std::move(A)};
    const auto& vals = eig.eigenvalues();

    expectNear(-1.0, vals[0], 1e-9);
    expectNear(3.0, vals[1], 1e-9);

    // Verify reconstruction
    const auto& V = eig.eigenvectors();
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 2; ++k) {
          sum += V[i, k] * vals[k] * V[j, k];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "SymmetricEigen - near-degenerate eigenvalues"_test = [] {
    // Near-identity perturbation: eigenvalues close to 1
    InlineDense<double, 3, 3> A{
        {1.0, 1e-8, 0.0}, {1e-8, 1.0, 1e-8}, {0.0, 1e-8, 1.0}};
    auto A_orig = A;

    auto eig = SymmetricEigen{std::move(A)};
    const auto& V = eig.eigenvectors();
    const auto& d = eig.eigenvalues();

    // All eigenvalues ≈ 1
    for (int i = 0; i < 3; ++i) {
      expectNear(1.0, d[i], 1e-6);
    }

    // Reconstruction should still hold
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += V[i, k] * d[k] * V[j, k];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "SymmetricEigen - determinant cross-check with LU"_test = [] {
    InlineDense<double, 3, 3> A{
        {4.0, 2.0, 2.0}, {2.0, 5.0, 3.0}, {2.0, 3.0, 6.0}};
    auto A_copy = A;

    auto eig = SymmetricEigen{std::move(A)};
    auto lu = LU{std::move(A_copy)};

    // Product of eigenvalues = det(A)
    double eig_det = 1.0;
    for (const auto& v : eig.eigenvalues()) {
      eig_det *= v;
    }

    expectNear(lu.determinant(), eig_det, 1e-9);
  };

  "SymmetricEigen - 1x1"_test = [] {
    InlineDense<double, 1, 1> A{{7.0}};
    auto eig = SymmetricEigen{A};

    expectNear(7.0, eig.eigenvalues()[0], 1e-9);
    expectNear(1.0, eig.eigenvectors()[0, 0], 1e-9);
  };

  "SymmetricEigen - CTAD"_test = [] {
    auto A = InlineDense{{3.0, 1.0}, {1.0, 3.0}};
    auto eig = SymmetricEigen{A};

    expectNear(2.0, eig.eigenvalues()[0], 1e-9);
    expectNear(4.0, eig.eigenvalues()[1], 1e-9);
  };

  return TestRegistry::result();
}
