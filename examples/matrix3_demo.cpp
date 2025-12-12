// Matrix3 Demo - Showcasing the mdspan-inspired matrix library
//
// This demo highlights the key features of matrix3:
// - Storage types: InlineDense, Dense, InlineCoordinateList, Banded
// - Views: Transpose, HermitianTranspose, Submatrix, RowPermuted, BlockRow/Col
// - Algorithms: Addition, Multiplication, Gauss-Jordan, LU, Tridiagonal Solver
// - Utilities: toString, Kronecker product, materialize

#include <complex>
#include <iostream>

#include "matrix3/addition.h"
#include "matrix3/banded.h"
#include "matrix3/block.h"
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

  // rows() and cols() convenience methods
  std::cout << "A.rows() = " << A.rows() << ", A.cols() = " << A.cols() << "\n";
  std::cout << "A.extent().size() = " << A.extent().size() << " elements\n\n";

  // Identity matrix
  constexpr Identity<double, 3> I;
  std::cout << "Identity 3×3 matrix I:\n";
  std::cout << "I[0,0] = " << I[0, 0] << ", I[0,1] = " << I[0, 1]
            << ", I[1,1] = " << I[1, 1] << "\n\n";
}

void demoSparseStorage() {
  printSection("Sparse Storage (COO Format)");

  // InlineCoordinateList - COO sparse format with iteration support
  using Sparse = InlineCoordinateList<double, 4, 4, 6>;
  Sparse sparse;
  sparse.insert(0, 0, 1.0);
  sparse.insert(1, 1, 2.0);
  sparse.insert(2, 2, 3.0);
  sparse.insert(3, 3, 4.0);
  sparse.insert(0, 3, 0.5);
  sparse.insert(3, 0, -0.5);

  std::cout << "Sparse 4×4 matrix (COO format, 6 non-zero elements):\n";

  // NEW: Iterate over non-zero entries
  std::cout << "Non-zero entries (via iteration):\n";
  for (const auto& entry : sparse) {
    std::cout << "  (" << entry.row << ", " << entry.col << ") = " << entry.value
              << "\n";
  }
  std::cout << "\n";
}

void demoBandedMatrices() {
  printSection("Banded Matrices & Tridiagonal Solver");

  // Create a tridiagonal matrix (3 bands) manually
  // Layout: each row stores [lower, main, upper] diagonal entries
  Banded<InlineDense<double, 4, 3>, 1> tri{};
  // Row 0: main=2, upper=-1
  tri[0, 0] = 2.0;
  tri[0, 1] = -1.0;
  // Row 1: lower=-1, main=2, upper=-1
  tri[1, 0] = -1.0;
  tri[1, 1] = 2.0;
  tri[1, 2] = -1.0;
  // Row 2: lower=-1, main=2, upper=-1
  tri[2, 1] = -1.0;
  tri[2, 2] = 2.0;
  tri[2, 3] = -1.0;
  // Row 3: lower=-1, main=2
  tri[3, 2] = -1.0;
  tri[3, 3] = 2.0;

  std::cout << "Tridiagonal 4×4 matrix (3 bands, center=1):\n";
  std::cout << "  Main diagonal: [2, 2, 2, 2]\n";
  std::cout << "  Off-diagonals: [-1, -1, -1]\n\n";

  // NEW: Solve tridiagonal system using Thomas algorithm
  InlineDense<double, 4, 1> b{{1.0}, {0.0}, {0.0}, {1.0}};

  std::cout << "Solving tridiagonal system Ax = b\n";
  std::cout << "b = [1, 0, 0, 1]ᵀ\n\n";

  auto x = solveTridiagonal(tri, b);
  std::cout << "Solution x (Thomas algorithm, O(n)):\n" << toString(x);

  // Verify with banded multiplication
  auto Ax = multiplyBanded(tri, x);
  std::cout << "\nVerification Ax:\n" << toString(Ax);
}

void demoViews() {
  printSection("Matrix Views (Zero-Copy)");

  InlineDense<int, 3, 4> M{{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};

  std::cout << "Original 3×4 matrix M:\n" << toString(M) << "\n";

  // Transpose view - now stores by reference (zero-copy)
  auto Mt = Transpose{M};
  std::cout << "Transpose view Mᵀ (4×3) - zero copy:\n" << toString(Mt) << "\n";

  // Modify through transpose, affects original
  Mt[0, 2] = 99;  // This is M[2, 0]
  std::cout << "After Mt[0,2] = 99, original M:\n" << toString(M) << "\n";
  M[2, 0] = 9;  // Restore

  // Submatrix view
  auto sub = makeSubmatrix(M, 0, 1, 2, 3);  // 2×3 starting at (0,1)
  std::cout << "Submatrix view (2×3 at offset [0,1]):\n" << toString(sub) << "\n";

  // NEW: materialize() creates independent copy
  auto sub_copy = sub.materialize();
  sub_copy[0, 0] = 999;
  std::cout << "Materialized submatrix (independent copy, modified [0,0]=999):\n"
            << toString(sub_copy) << "\n";
  std::cout << "Original M unchanged:\n" << toString(M) << "\n";
}

void demoBlockMatrices() {
  printSection("Block Matrices (Horizontal & Vertical)");

  InlineDense<int, 2, 2> A{{1, 2}, {3, 4}};
  InlineDense<int, 2, 3> B{{5, 6, 7}, {8, 9, 10}};

  std::cout << "Block A (2×2):\n" << toString(A);
  std::cout << "\nBlock B (2×3):\n" << toString(B);

  // BlockRow - horizontal concatenation
  auto row_block = BlockRow{A, B};
  std::cout << "\nBlockRow [A | B] (2×5):\n";
  for (std::size_t i = 0; i < row_block.rows(); ++i) {
    std::cout << "  ";
    for (std::size_t j = 0; j < row_block.cols(); ++j) {
      std::cout << row_block[i, j] << " ";
    }
    std::cout << "\n";
  }
  std::cout << "\n";

  // NEW: BlockCol - vertical concatenation
  InlineDense<int, 2, 2> C{{11, 12}, {13, 14}};
  InlineDense<int, 1, 2> D{{15, 16}};

  std::cout << "Block C (2×2):\n" << toString(C);
  std::cout << "\nBlock D (1×2):\n" << toString(D);

  auto col_block = BlockCol{A, C, D};
  std::cout << "\nBlockCol [A; C; D] (5×2):\n";
  for (std::size_t i = 0; i < col_block.rows(); ++i) {
    std::cout << "  ";
    for (std::size_t j = 0; j < col_block.cols(); ++j) {
      std::cout << col_block[i, j] << " ";
    }
    std::cout << "\n";
  }
  std::cout << "\n";
}

void demoHermitianTranspose() {
  printSection("Hermitian Transpose (Conjugate Transpose)");

  // Complex matrix
  using Complex = std::complex<double>;
  InlineDense<Complex, 2, 2> H{
      {Complex{1.0, 2.0}, Complex{3.0, 4.0}},
      {Complex{5.0, 6.0}, Complex{7.0, 8.0}}};

  std::cout << "Complex matrix H:\n";
  std::cout << "  H[0,0] = " << H[0, 0] << "  H[0,1] = " << H[0, 1] << "\n";
  std::cout << "  H[1,0] = " << H[1, 0] << "  H[1,1] = " << H[1, 1] << "\n\n";

  // NEW: Hermitian transpose (conjugate transpose)
  auto Hh = HermitianTranspose{H};
  std::cout << "Hermitian transpose Hᴴ:\n";
  std::cout << "  Hᴴ[0,0] = " << Hh[0, 0] << "  Hᴴ[0,1] = " << Hh[0, 1] << "\n";
  std::cout << "  Hᴴ[1,0] = " << Hh[1, 0] << "  Hᴴ[1,1] = " << Hh[1, 1] << "\n\n";

  // For real matrices, Hermitian = regular transpose
  InlineDense<double, 2, 2> R{{1.0, 2.0}, {3.0, 4.0}};
  auto Rh = HermitianTranspose{R};
  std::cout << "For real matrix R, Rᴴ = Rᵀ:\n" << toString(Rh) << "\n";
}

void demoScalarMultiplication() {
  printSection("Scalar Multiplication");

  constexpr InlineDense<int, 2, 3> M{{1, 2, 3}, {4, 5, 6}};

  std::cout << "Matrix M:\n" << toString(M);

  // NEW: Scalar multiplication operators
  auto M2 = 2 * M;  // scalar * matrix
  std::cout << "\n2 * M:\n" << toString(M2);

  auto M3 = M * 3;  // matrix * scalar
  std::cout << "\nM * 3:\n" << toString(M3);

  // Type promotion: int matrix * double scalar = double matrix
  auto Md = M * 0.5;
  std::cout << "\nM * 0.5 (type promotion to double):\n" << toString(Md);
}

void demoArithmetic() {
  printSection("Matrix Arithmetic");

  constexpr InlineDense<double, 2, 2> A{{1.0, 2.0}, {3.0, 4.0}};
  constexpr InlineDense<double, 2, 2> B{{5.0, 6.0}, {7.0, 8.0}};

  std::cout << "Matrix A:\n" << toString(A);
  std::cout << "\nMatrix B:\n" << toString(B);

  auto C = A + B;
  std::cout << "\nA + B:\n" << toString(C);

  auto D = A - B;
  std::cout << "\nA - B:\n" << toString(D);

  auto E = A * B;
  std::cout << "\nA × B:\n" << toString(E);

  // Kronecker product
  constexpr InlineDense<int, 2, 2> K1{{1, 2}, {3, 4}};
  constexpr InlineDense<int, 2, 2> K2{{0, 5}, {6, 7}};

  auto kron = kronecker(K1, K2);
  InlineDense<int, 4, 4> kron_mat{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      kron_mat[i, j] = kron[i, j];
    }
  }
  std::cout << "\nKronecker product K1 ⊗ K2 (4×4):\n" << toString(kron_mat) << "\n";
}

void demoLinearAlgebra() {
  printSection("Linear Algebra: Solving Ax = b");

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
  auto x_lu = b;
  lu.solve(x_lu);

  std::cout << "\nLU Decomposition solution x:\n" << toString(x_lu);
  std::cout << "Determinant: " << lu.determinant() << "\n";

  // Verify
  auto Ax = A * x_lu;
  std::cout << "\nVerification A × x:\n" << toString(Ax);
}

void demoConstexpr() {
  printSection("Compile-Time Computation (constexpr)");

  // All computed at compile time!
  constexpr InlineDense<int, 2, 2> A{{1, 2}, {3, 4}};
  constexpr InlineDense<int, 2, 2> B{{5, 6}, {7, 8}};
  constexpr auto C = A + B;
  constexpr auto D = A * B;
  constexpr auto S = 2 * A;  // NEW: constexpr scalar multiplication

  // Compile-time verification
  static_assert(D[0, 0] == 19);  // 1*5 + 2*7 = 19
  static_assert(D[0, 1] == 22);  // 1*6 + 2*8 = 22
  static_assert(D[1, 0] == 43);  // 3*5 + 4*7 = 43
  static_assert(D[1, 1] == 50);  // 3*6 + 4*8 = 50
  static_assert(S[0, 0] == 2);   // 2*1 = 2
  static_assert(S[1, 1] == 8);   // 2*4 = 8

  // NEW: Extents comparison at compile time
  static_assert(A.extent() == B.extent());
  static_assert(A.extent().size() == 4);

  std::cout << "Compile-time matrix operations verified!\n\n";
  std::cout << "constexpr A:\n" << toString(A);
  std::cout << "\nconstexpr B:\n" << toString(B);
  std::cout << "\nconstexpr A + B:\n" << toString(C);
  std::cout << "\nconstexpr A × B:\n" << toString(D);
  std::cout << "\nconstexpr 2 * A:\n" << toString(S);

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
  demoBandedMatrices();
  demoViews();
  demoBlockMatrices();
  demoHermitianTranspose();
  demoScalarMultiplication();
  demoArithmetic();
  demoLinearAlgebra();
  demoConstexpr();

  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "  Demo complete!\n";
  std::cout << std::string(60, '=') << "\n";

  return 0;
}
