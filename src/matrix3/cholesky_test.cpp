#include "matrix3/cholesky.h"

#include "matrix3/lu_decomposition.h"
#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "Cholesky - verify L*Lᵀ = A for 2x2"_test = [] {
    // SPD matrix: A = [4 2; 2 5]
    InlineDense<double, 2, 2> A{{4.0, 2.0}, {2.0, 5.0}};
    auto A_orig = A;

    auto chol = Cholesky{std::move(A)};
    expectTrue(chol.isPositiveDefinite());

    const auto& L = chol.factor();

    // Verify L * Lᵀ = A
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 2; ++k) {
          sum += L[i, k] * L[j, k];  // L[i,k] * Lᵀ[k,j] = L[i,k] * L[j,k]
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "Cholesky - verify L*Lᵀ = A for 3x3"_test = [] {
    // SPD matrix: A = [4 2 2; 2 10 5; 2 5 6]
    InlineDense<double, 3, 3> A{{4.0, 2.0, 2.0}, {2.0, 10.0, 5.0}, {2.0, 5.0, 6.0}};
    auto A_orig = A;

    auto chol = Cholesky{std::move(A)};
    expectTrue(chol.isPositiveDefinite());

    const auto& L = chol.factor();

    // Verify L * Lᵀ = A
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += L[i, k] * L[j, k];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "Cholesky - verify L*Lᵀ = A for 4x4"_test = [] {
    // SPD matrix with positive eigenvalues
    InlineDense<double, 4, 4> A{
        {10.0, 2.0, 1.0, 0.0},
        {2.0, 8.0, 2.0, 1.0},
        {1.0, 2.0, 6.0, 2.0},
        {0.0, 1.0, 2.0, 4.0}};
    auto A_orig = A;

    auto chol = Cholesky{std::move(A)};
    expectTrue(chol.isPositiveDefinite());

    const auto& L = chol.factor();

    // Verify L * Lᵀ = A
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 4; ++k) {
          sum += L[i, k] * L[j, k];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "Cholesky solve - known solution"_test = [] {
    // A = [4 2; 2 5], x = [1, 2], b = A*x = [8, 12]
    InlineDense<double, 2, 2> A{{4.0, 2.0}, {2.0, 5.0}};
    InlineDense<double, 2, 1> b{{8.0}, {12.0}};

    auto chol = Cholesky{A};
    chol.solve(b);

    expectNear(1.0, b[0, 0], 1e-9);
    expectNear(2.0, b[1, 0], 1e-9);
  };

  "Cholesky solve - verify A*x = b"_test = [] {
    // SPD matrix: A = [4 2 2; 2 10 5; 2 5 6]
    InlineDense<double, 3, 3> A{{4.0, 2.0, 2.0}, {2.0, 10.0, 5.0}, {2.0, 5.0, 6.0}};
    auto A_orig = A;

    InlineDense<double, 3, 1> b{{1.0}, {2.0}, {3.0}};

    auto chol = Cholesky{std::move(A)};
    chol.solve(b);

    // Verify A * x = original b
    auto result0 =
        A_orig[0, 0] * b[0, 0] + A_orig[0, 1] * b[1, 0] + A_orig[0, 2] * b[2, 0];
    auto result1 =
        A_orig[1, 0] * b[0, 0] + A_orig[1, 1] * b[1, 0] + A_orig[1, 2] * b[2, 0];
    auto result2 =
        A_orig[2, 0] * b[0, 0] + A_orig[2, 1] * b[1, 0] + A_orig[2, 2] * b[2, 0];

    expectNear(1.0, result0, 1e-9);
    expectNear(2.0, result1, 1e-9);
    expectNear(3.0, result2, 1e-9);
  };

  "Cholesky solve - identity matrix"_test = [] {
    // Identity should solve to x = b
    InlineDense<double, 3, 3> I{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    auto chol = Cholesky{I};

    expectTrue(chol.isPositiveDefinite());

    InlineDense<double, 3, 1> b{{1.0}, {2.0}, {3.0}};
    chol.solve(b);

    expectNear(1.0, b[0, 0], 1e-9);
    expectNear(2.0, b[1, 0], 1e-9);
    expectNear(3.0, b[2, 0], 1e-9);
  };

  "Cholesky logDeterminant"_test = [] {
    // A = [4 2; 2 5], det(A) = 4*5 - 2*2 = 16
    // log(16) ≈ 2.772588722
    InlineDense<double, 2, 2> A{{4.0, 2.0}, {2.0, 5.0}};
    auto chol = Cholesky{A};

    auto log_det = chol.logDeterminant();
    expectNear(std::log(16.0), log_det, 1e-9);
  };

  "Cholesky determinant - matches LU"_test = [] {
    InlineDense<double, 3, 3> A{{4.0, 2.0, 2.0}, {2.0, 10.0, 5.0}, {2.0, 5.0, 6.0}};
    auto A_copy = A;

    auto chol = Cholesky{std::move(A)};
    auto lu = LU{std::move(A_copy)};

    expectNear(lu.determinant(), chol.determinant(), 1e-9);
  };

  "Cholesky determinant - 2x2"_test = [] {
    // det([4 2; 2 5]) = 16
    InlineDense<double, 2, 2> A{{4.0, 2.0}, {2.0, 5.0}};
    auto chol = Cholesky{A};

    expectNear(16.0, chol.determinant(), 1e-9);
  };

  "Cholesky inverse - verify A * A⁻¹ ≈ I"_test = [] {
    InlineDense<double, 3, 3> A{{4.0, 2.0, 2.0}, {2.0, 10.0, 5.0}, {2.0, 5.0, 6.0}};
    auto A_orig = A;

    auto chol = Cholesky{std::move(A)};
    auto inv = chol.inverse();

    // Verify A * A⁻¹ = I
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += A_orig[i, k] * inv[k, j];
        }
        double expected = (i == j) ? 1.0 : 0.0;
        expectNear(expected, sum, 1e-9);
      }
    }
  };

  "Cholesky - non-positive-definite detection"_test = [] {
    // Matrix with negative eigenvalue (not SPD)
    // [1 2; 2 1] has eigenvalues 3 and -1
    InlineDense<double, 2, 2> A{{1.0, 2.0}, {2.0, 1.0}};
    auto chol = Cholesky{A};

    expectFalse(chol.isPositiveDefinite());
  };

  "Cholesky - indefinite 3x3 detection"_test = [] {
    // Indefinite matrix: not positive definite
    InlineDense<double, 3, 3> A{{1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}};
    auto chol = Cholesky{A};

    expectFalse(chol.isPositiveDefinite());
  };

  "Cholesky solve - multiple RHS"_test = [] {
    // Solve for two RHS vectors simultaneously
    InlineDense<double, 2, 2> A{{4.0, 2.0}, {2.0, 5.0}};
    auto chol = Cholesky{A};

    // Two systems with known solutions
    // x1 = [1, 2], b1 = A*x1 = [8, 12]
    // x2 = [3, 1], b2 = A*x2 = [14, 11]
    InlineDense<double, 2, 2> B{{8.0, 14.0}, {12.0, 11.0}};
    chol.solve(B);

    expectNear(1.0, B[0, 0], 1e-9);
    expectNear(2.0, B[1, 0], 1e-9);
    expectNear(3.0, B[0, 1], 1e-9);
    expectNear(1.0, B[1, 1], 1e-9);
  };

  "Cholesky - 1x1 special case"_test = [] {
    InlineDense<double, 1, 1> A{{4.0}};
    auto chol = Cholesky{A};

    expectTrue(chol.isPositiveDefinite());
    expectNear(2.0, chol.factor()[0, 0], 1e-9);  // L = sqrt(4) = 2
    expectNear(4.0, chol.determinant(), 1e-9);
    expectNear(std::log(4.0), chol.logDeterminant(), 1e-9);

    InlineDense<double, 1, 1> b{{8.0}};
    chol.solve(b);
    expectNear(2.0, b[0, 0], 1e-9);  // 4*x = 8 => x = 2

    auto inv = chol.inverse();
    expectNear(0.25, inv[0, 0], 1e-9);  // 1/4
  };

  "Cholesky - CTAD"_test = [] {
    // Test class template argument deduction
    auto A = InlineDense{{4.0, 2.0}, {2.0, 5.0}};
    auto chol = Cholesky{A};

    auto b = InlineDense{{8.0}, {12.0}};
    chol.solve(b);

    expectNear(1.0, b[0, 0], 1e-9);
    expectNear(2.0, b[1, 0], 1e-9);
  };

  "Cholesky - symmetric input produces correct L"_test = [] {
    // For symmetric A, L should have L[i,j] = 0 for j > i (lower triangular)
    InlineDense<double, 3, 3> A{{9.0, 6.0, 3.0}, {6.0, 13.0, 5.0}, {3.0, 5.0, 3.0}};
    auto chol = Cholesky{A};

    expectTrue(chol.isPositiveDefinite());

    const auto& L = chol.factor();
    // Upper triangle should be zero
    expectNear(0.0, L[0, 1], 1e-9);
    expectNear(0.0, L[0, 2], 1e-9);
    expectNear(0.0, L[1, 2], 1e-9);

    // Diagonal should be positive
    expectGT(L[0, 0], 0.0);
    expectGT(L[1, 1], 0.0);
    expectGT(L[2, 2], 0.0);
  };

  "Cholesky - large condition number (near-singular SPD)"_test = [] {
    // Near-singular but still positive definite
    // Condition number ≈ 1e8
    InlineDense<double, 2, 2> A{{1.0, 0.99999999}, {0.99999999, 1.0}};
    auto A_orig = A;

    auto chol = Cholesky{std::move(A)};
    // Should still be positive definite (barely)
    expectTrue(chol.isPositiveDefinite());

    // Test solve with a known RHS
    InlineDense<double, 2, 1> b{{1.0}, {1.0}};
    chol.solve(b);

    // Verify A * x = original b (with larger tolerance due to conditioning)
    auto result0 = A_orig[0, 0] * b[0, 0] + A_orig[0, 1] * b[1, 0];
    auto result1 = A_orig[1, 0] * b[0, 0] + A_orig[1, 1] * b[1, 0];

    expectNear(1.0, result0, 1e-6);
    expectNear(1.0, result1, 1e-6);
  };

  "Cholesky - diagonal matrix"_test = [] {
    // Diagonal SPD: L should also be diagonal with sqrt values
    InlineDense<double, 3, 3> A{{4.0, 0.0, 0.0}, {0.0, 9.0, 0.0}, {0.0, 0.0, 16.0}};
    auto chol = Cholesky{A};

    expectTrue(chol.isPositiveDefinite());

    const auto& L = chol.factor();
    expectNear(2.0, L[0, 0], 1e-9);
    expectNear(3.0, L[1, 1], 1e-9);
    expectNear(4.0, L[2, 2], 1e-9);

    // Off-diagonals should be zero
    expectNear(0.0, L[1, 0], 1e-9);
    expectNear(0.0, L[2, 0], 1e-9);
    expectNear(0.0, L[2, 1], 1e-9);

    expectNear(4.0 * 9.0 * 16.0, chol.determinant(), 1e-9);
  };

  return TestRegistry::result();
}
