// Matrix3 Demo - Showcasing the mdspan-inspired matrix library
//
// This demo highlights the key features migrated to matrix3:
// - Storage types: InlineDense, InlineCoordinateList
// - Views: Transpose, Submatrix, RowPermuted
// - Algorithms: Addition, Multiplication, Gauss-Jordan, LU Decomposition
// - Utilities: toString, Kronecker product

#include <iostream>

#include "matrix3/addition.h"
#include "matrix3/gauss_jordan.h"
#include "matrix3/inline_coordinate_list.h"
#include "matrix3/kronecker_product.h"
#include "matrix3/lu_decomposition.h"
#include "matrix3/matrix.h"
#include "matrix3/multiplication.h"
#include "matrix3/permutation.h"
#include "matrix3/permuted.h"
#include "matrix3/submatrix.h"
#include "matrix3/to_string.h"
#include "matrix3/transpose.h"

using namespace tempura::matrix3;

void printSection(const char* title) {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "  " << title << "\n";
  std::cout << std::string(60, '=') << "\n\n";
}

void demoBasicMatrices() {
  printSection("Basic Matrix Types");

  // InlineDense - stack-allocated matrix with compile-time dimensions
  constexpr InlineDense A{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}};

  std::cout << "InlineDense 3×3 matrix A:\n" << toString(A) << "\n";

  // Identity matrix - print manually since it doesn't have extent()
  constexpr Identity<double, 3> I;
  std::cout << "Identity 3×3 matrix I:\n";
  std::cout << "⎡ 1.0000 0.0000 0.0000 ⎤\n";
  std::cout << "⎢ 0.0000 1.0000 0.0000 ⎥\n";
  std::cout << "⎣ 0.0000 0.0000 1.0000 ⎦\n\n";
}

void demoSparseStorage() {
  printSection("Sparse Storage (COO Format)");

  // InlineCoordinateList - COO sparse format
  using Sparse = InlineCoordinateList<double, 4, 4, 6>;
  Sparse sparse;
  sparse.insert(0, 0, 1.0);
  sparse.insert(1, 1, 2.0);
  sparse.insert(2, 2, 3.0);
  sparse.insert(3, 3, 4.0);
  sparse.insert(0, 3, 0.5);
  sparse.insert(3, 0, -0.5);

  std::cout << "Sparse 4×4 matrix (COO format, 6 non-zero elements):\n";
  std::cout << "  Diagonal: [1, 2, 3, 4]\n";
  std::cout << "  Off-diagonal: (0,3)=0.5, (3,0)=-0.5\n\n";

  // Materialize to show actual values
  InlineDense<double, 4, 4> sparse_view{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      sparse_view[i, j] = sparse[i, j];
    }
  }
  std::cout << "Materialized view:\n" << toString(sparse_view) << "\n";

  // Demonstrate constexpr sparse matrix
  std::cout << "Sparse matrices are fully constexpr - compile-time construction!\n";
}

void demoViews() {
  printSection("Matrix Views (Zero-Copy)");

  InlineDense<int, 3, 4> M{
      {1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};

  std::cout << "Original 3×4 matrix M:\n" << toString(M) << "\n";

  // Transpose view - swaps indices without copying
  Transpose<InlineDense<int, 3, 4>> Mt{M};
  std::cout << "Transpose view Mᵀ (4×3):\n";
  InlineDense<int, 4, 3> mt_view{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 3; ++j) {
      mt_view[i, j] = Mt[i, j];
    }
  }
  std::cout << toString(mt_view) << "\n";

  // Submatrix view - view into a rectangular region
  auto sub = makeSubmatrix(M, 0, 1, 2, 2);  // 2×2 starting at (0,1)
  std::cout << "Submatrix view (2×2 at offset [0,1]):\n";
  InlineDense<int, 2, 2> sub_view{};
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      sub_view[i, j] = sub[i, j];
    }
  }
  std::cout << toString(sub_view) << "\n";

  // Permutation - row/column reordering
  Permutation<3> perm{2, 0, 1};  // Cyclic permutation
  std::cout << "Permutation P = [2, 0, 1] (cyclic):\n";
  std::cout << "  Parity: " << perm.parity() << " (odd permutation)\n\n";

  InlineDense<int, 3, 3> P{
      {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
  RowPermuted rp{P, perm};
  std::cout << "Original P:\n" << toString(P);
  std::cout << "\nRow-permuted P (rows reordered by [2,0,1]):\n";
  InlineDense<int, 3, 3> rp_view{};
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      rp_view[i, j] = rp[i, j];
    }
  }
  std::cout << toString(rp_view) << "\n";
}

void demoArithmetic() {
  printSection("Matrix Arithmetic");

  constexpr InlineDense<double, 2, 2> A{{1.0, 2.0}, {3.0, 4.0}};
  constexpr InlineDense<double, 2, 2> B{{5.0, 6.0}, {7.0, 8.0}};

  std::cout << "Matrix A:\n" << toString(A);
  std::cout << "\nMatrix B:\n" << toString(B);

  // Addition
  auto C = A + B;
  std::cout << "\nA + B:\n" << toString(C);

  // Subtraction
  auto D = A - B;
  std::cout << "\nA - B:\n" << toString(D);

  // Matrix multiplication (cache-optimized tiled algorithm)
  auto E = A * B;
  std::cout << "\nA × B:\n" << toString(E);

  // Kronecker product
  constexpr InlineDense<int, 2, 2> K1{{1, 2}, {3, 4}};
  constexpr InlineDense<int, 2, 2> K2{{0, 5}, {6, 7}};

  std::cout << "\nKronecker product K1 ⊗ K2:\n";
  std::cout << "K1:\n" << toString(K1);
  std::cout << "\nK2:\n" << toString(K2);

  auto kron = kronecker(K1, K2);
  InlineDense<int, 4, 4> kron_view{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      kron_view[i, j] = kron[i, j];
    }
  }
  std::cout << "\nK1 ⊗ K2 (4×4):\n" << toString(kron_view) << "\n";
}

void demoLinearAlgebra() {
  printSection("Linear Algebra: Solving Ax = b");

  // System: Ax = b
  InlineDense<double, 3, 3> A{
      {2.0, 1.0, -1.0}, {-3.0, -1.0, 2.0}, {-2.0, 1.0, 2.0}};

  InlineDense<double, 3, 1> b{{8.0}, {-11.0}, {-3.0}};

  std::cout << "Solving linear system Ax = b\n\n";
  std::cout << "Matrix A:\n" << toString(A);
  std::cout << "\nVector b:\n" << toString(b);

  // Method 1: Gauss-Jordan elimination
  auto A_copy = A;
  auto x_gj = b;
  bool success = gaussJordan<Pivot::kRow>(A_copy, x_gj);

  std::cout << "\nGauss-Jordan solution x:\n" << toString(x_gj);
  std::cout << "Success: " << (success ? "yes" : "no (singular)") << "\n";

  // Method 2: LU Decomposition
  LU lu{A};
  auto x_lu = b;  // Copy b since solve() modifies in-place
  lu.solve(x_lu);

  std::cout << "\nLU Decomposition solution x:\n" << toString(x_lu);
  std::cout << "Determinant: " << lu.determinant() << "\n";

  // Verify: compute Ax
  auto Ax = A * x_lu;
  std::cout << "\nVerification A × x:\n" << toString(Ax);
}

void demoConstexpr() {
  printSection("Compile-Time Computation (constexpr)");

  // All of this is computed at compile time!
  constexpr InlineDense<int, 2, 2> A{{1, 2}, {3, 4}};
  constexpr InlineDense<int, 2, 2> B{{5, 6}, {7, 8}};
  constexpr auto C = A + B;
  constexpr auto D = A * B;

  // Compile-time matrix multiplication result
  static_assert(D[0, 0] == 19);  // 1*5 + 2*7 = 19
  static_assert(D[0, 1] == 22);  // 1*6 + 2*8 = 22
  static_assert(D[1, 0] == 43);  // 3*5 + 4*7 = 43
  static_assert(D[1, 1] == 50);  // 3*6 + 4*8 = 50

  std::cout << "Compile-time matrix multiplication verified!\n\n";
  std::cout << "constexpr A:\n" << toString(A);
  std::cout << "\nconstexpr B:\n" << toString(B);
  std::cout << "\nconstexpr A + B:\n" << toString(C);
  std::cout << "\nconstexpr A × B:\n" << toString(D);

  std::cout << "\nAll results computed at compile time (static_assert verified)\n";
}

auto main() -> int {
  std::cout << R"(
  ╔══════════════════════════════════════════════════════════╗
  ║                    Matrix3 Demo                          ║
  ║         mdspan-inspired C++26 Matrix Library             ║
  ╚══════════════════════════════════════════════════════════╝
)";

  demoBasicMatrices();
  demoSparseStorage();
  demoViews();
  demoArithmetic();
  demoLinearAlgebra();
  demoConstexpr();

  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "  Demo complete!\n";
  std::cout << std::string(60, '=') << "\n";

  return 0;
}
