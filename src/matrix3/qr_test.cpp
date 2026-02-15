#include "matrix3/qr.h"

#include <cmath>

#include "matrix3/lu_decomposition.h"
#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "QR - verify Q*R = A for 2x2"_test = [] {
    InlineDense<double, 2, 2> A{{4.0, 2.0}, {3.0, 1.0}};
    auto A_orig = A;

    auto qr = QR{std::move(A)};
    auto Q = qr.Q();
    auto R = qr.R();

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 2; ++k) {
          sum += Q[i, k] * R[k, j];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "QR - verify Q*R = A for 3x3"_test = [] {
    InlineDense<double, 3, 3> A{
        {1.0, 2.0, 3.0}, {2.0, 5.0, 2.0}, {6.0, -3.0, 1.0}};
    auto A_orig = A;

    auto qr = QR{std::move(A)};
    auto Q = qr.Q();
    auto R = qr.R();

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += Q[i, k] * R[k, j];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "QR - verify Q*R = A for 4x4"_test = [] {
    InlineDense<double, 4, 4> A{
        {4.0, 3.0, 2.0, 1.0},
        {3.0, 4.0, 3.0, 2.0},
        {2.0, 3.0, 4.0, 3.0},
        {1.0, 2.0, 3.0, 4.0}};
    auto A_orig = A;

    auto qr = QR{std::move(A)};
    auto Q = qr.Q();
    auto R = qr.R();

    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 4; ++k) {
          sum += Q[i, k] * R[k, j];
        }
        expectNear(A_orig[i, j], sum, 1e-9);
      }
    }
  };

  "QR - Q is orthogonal (QᵀQ ≈ I)"_test = [] {
    InlineDense<double, 3, 3> A{
        {1.0, 2.0, 3.0}, {2.0, 5.0, 2.0}, {6.0, -3.0, 1.0}};

    auto qr = QR{std::move(A)};
    auto Q = qr.Q();

    // QᵀQ should be identity
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += Q[k, i] * Q[k, j];  // Qᵀ[i,k] * Q[k,j]
        }
        double expected = (i == j) ? 1.0 : 0.0;
        expectNear(expected, sum, 1e-9);
      }
    }
  };

  "QR - R is upper triangular"_test = [] {
    InlineDense<double, 3, 3> A{
        {1.0, 2.0, 3.0}, {2.0, 5.0, 2.0}, {6.0, -3.0, 1.0}};

    auto qr = QR{std::move(A)};
    auto R = qr.R();

    // Below diagonal should be zero
    expectNear(0.0, R[1, 0], 1e-9);
    expectNear(0.0, R[2, 0], 1e-9);
    expectNear(0.0, R[2, 1], 1e-9);
  };

  "QR solve - known solution"_test = [] {
    // A = [4 2; 3 1], x = [1, 2], b = A*x = [8, 5]
    InlineDense<double, 2, 2> A{{4.0, 2.0}, {3.0, 1.0}};
    InlineDense<double, 2, 1> b{{8.0}, {5.0}};

    auto qr = QR{A};
    qr.solve(b);

    expectNear(1.0, b[0, 0], 1e-9);
    expectNear(2.0, b[1, 0], 1e-9);
  };

  "QR solve - verify A*x = b"_test = [] {
    InlineDense<double, 3, 3> A{
        {1.0, 2.0, 3.0}, {2.0, 5.0, 2.0}, {6.0, -3.0, 1.0}};
    auto A_orig = A;

    InlineDense<double, 3, 1> b{{1.0}, {2.0}, {3.0}};

    auto qr = QR{std::move(A)};
    qr.solve(b);

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

  "QR solve - identity matrix"_test = [] {
    InlineDense<double, 3, 3> I{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    auto qr = QR{I};

    InlineDense<double, 3, 1> b{{1.0}, {2.0}, {3.0}};
    qr.solve(b);

    expectNear(1.0, b[0, 0], 1e-9);
    expectNear(2.0, b[1, 0], 1e-9);
    expectNear(3.0, b[2, 0], 1e-9);
  };

  "QR - diagonal matrix"_test = [] {
    InlineDense<double, 3, 3> A{{4.0, 0.0, 0.0}, {0.0, 9.0, 0.0}, {0.0, 0.0, 16.0}};

    auto qr = QR{A};
    auto Q = qr.Q();
    auto R = qr.R();

    // Q should be ±I (diagonal with ±1), R should be diagonal with ±original values
    // Key invariant: Q*R = A regardless of sign convention
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
          sum += Q[i, k] * R[k, j];
        }
        expectNear(A[i, j], sum, 1e-9);
      }
    }

    // R below diagonal should be zero
    expectNear(0.0, R[1, 0], 1e-9);
    expectNear(0.0, R[2, 0], 1e-9);
    expectNear(0.0, R[2, 1], 1e-9);
  };

  "QR solve - multiple RHS"_test = [] {
    InlineDense<double, 2, 2> A{{4.0, 2.0}, {3.0, 1.0}};
    auto qr = QR{A};

    // x1 = [1, 2], b1 = A*x1 = [8, 5]
    // x2 = [3, 1], b2 = A*x2 = [14, 10]
    InlineDense<double, 2, 2> B{{8.0, 14.0}, {5.0, 10.0}};
    qr.solve(B);

    expectNear(1.0, B[0, 0], 1e-9);
    expectNear(2.0, B[1, 0], 1e-9);
    expectNear(3.0, B[0, 1], 1e-9);
    expectNear(1.0, B[1, 1], 1e-9);
  };

  "QR - near-singular matrix"_test = [] {
    // Rows nearly linearly dependent — high condition number
    InlineDense<double, 3, 3> A{
        {1.0, 1.0, 1.0}, {1.0, 1.0 + 1e-10, 1.0}, {1.0, 1.0, 1.0 + 1e-10}};
    auto A_orig = A;

    InlineDense<double, 3, 1> b{{3.0}, {3.0}, {3.0}};
    auto b_orig = b;

    auto qr = QR{std::move(A)};
    qr.solve(b);

    // Verify A * x ≈ original b (relaxed tolerance)
    auto result0 =
        A_orig[0, 0] * b[0, 0] + A_orig[0, 1] * b[1, 0] + A_orig[0, 2] * b[2, 0];
    auto result1 =
        A_orig[1, 0] * b[0, 0] + A_orig[1, 1] * b[1, 0] + A_orig[1, 2] * b[2, 0];
    auto result2 =
        A_orig[2, 0] * b[0, 0] + A_orig[2, 1] * b[1, 0] + A_orig[2, 2] * b[2, 0];

    expectNear(b_orig[0, 0], result0, 1e-3);
    expectNear(b_orig[1, 0], result1, 1e-3);
    expectNear(b_orig[2, 0], result2, 1e-3);
  };

  "QR determinant cross-check with LU"_test = [] {
    InlineDense<double, 3, 3> A{
        {1.0, 2.0, 3.0}, {2.0, 5.0, 2.0}, {6.0, -3.0, 1.0}};
    auto A_copy = A;

    auto qr = QR{std::move(A)};
    auto lu = LU{std::move(A_copy)};

    // |∏ R_ii| should equal |det(A)| from LU
    auto R = qr.R();
    double r_product = 1.0;
    for (int i = 0; i < 3; ++i) {
      r_product *= R[i, i];
    }

    using std::abs;
    expectNear(abs(lu.determinant()), abs(r_product), 1e-9);
  };

  "QR - CTAD"_test = [] {
    auto A = InlineDense{{4.0, 2.0}, {3.0, 1.0}};
    auto qr = QR{A};

    auto b = InlineDense{{8.0}, {5.0}};
    qr.solve(b);

    expectNear(1.0, b[0, 0], 1e-9);
    expectNear(2.0, b[1, 0], 1e-9);
  };

  return TestRegistry::result();
}
