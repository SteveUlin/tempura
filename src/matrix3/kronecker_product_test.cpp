#include "matrix3/kronecker_product.h"

#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "kronecker 2x2"_test = [] {
  InlineDense<int, 2, 2> a{{1, 2}, {3, 4}};
  InlineDense<int, 2, 2> b{{5, 6}, {7, 8}};

  auto result = kronecker(a, b);

  // Expected result:
  // [1·B  2·B]   [5  6  10 12]
  // [3·B  4·B] = [7  8  14 16]
  //              [15 18 20 24]
  //              [21 24 28 32]

  expectEq(4, result.extent().extent(0));
  expectEq(4, result.extent().extent(1));

  expectEq(5, result[0, 0]);
  expectEq(6, result[0, 1]);
  expectEq(10, result[0, 2]);
  expectEq(12, result[0, 3]);

  expectEq(7, result[1, 0]);
  expectEq(8, result[1, 1]);
  expectEq(14, result[1, 2]);
  expectEq(16, result[1, 3]);

  expectEq(15, result[2, 0]);
  expectEq(18, result[2, 1]);
  expectEq(20, result[2, 2]);
  expectEq(24, result[2, 3]);

  expectEq(21, result[3, 0]);
  expectEq(24, result[3, 1]);
  expectEq(28, result[3, 2]);
  expectEq(32, result[3, 3]);
};

// Rectangular matrices test
"kronecker rectangular"_test = [] {
  InlineDense<int, 2, 3> a{{1, 2, 3}, {4, 5, 6}};
  InlineDense<int, 2, 1> b{{7}, {8}};

  auto result = kronecker(a, b);

  // Expected: 4×3 result
  // [7  14 21]
  // [8  16 24]
  // [28 35 42]
  // [32 40 48]

  expectEq(4, result.extent().extent(0));
  expectEq(3, result.extent().extent(1));

  expectEq(7, result[0, 0]);
  expectEq(14, result[0, 1]);
  expectEq(21, result[0, 2]);

  expectEq(8, result[1, 0]);
  expectEq(16, result[1, 1]);
  expectEq(24, result[1, 2]);

  expectEq(28, result[2, 0]);
  expectEq(35, result[2, 1]);
  expectEq(42, result[2, 2]);

  expectEq(32, result[3, 0]);
  expectEq(40, result[3, 1]);
  expectEq(48, result[3, 2]);
};

// Identity property: I ⊗ A
"kronecker identity left"_test = [] {
  Identity<int, 2, 2> eye;
  InlineDense<int, 2, 2> a{{3, 4}, {5, 6}};

  auto result = kronecker(eye, a);

  // I₂ ⊗ A = [A  0]
  //          [0  A]

  expectEq(4, result.extent().extent(0));
  expectEq(4, result.extent().extent(1));

  expectEq(3, result[0, 0]);
  expectEq(4, result[0, 1]);
  expectEq(0, result[0, 2]);
  expectEq(0, result[0, 3]);

  expectEq(5, result[1, 0]);
  expectEq(6, result[1, 1]);
  expectEq(0, result[1, 2]);
  expectEq(0, result[1, 3]);

  expectEq(0, result[2, 0]);
  expectEq(0, result[2, 1]);
  expectEq(3, result[2, 2]);
  expectEq(4, result[2, 3]);

  expectEq(0, result[3, 0]);
  expectEq(0, result[3, 1]);
  expectEq(5, result[3, 2]);
  expectEq(6, result[3, 3]);
};

// Identity property: A ⊗ I
"kronecker identity right"_test = [] {
  InlineDense<int, 2, 2> a{{3, 4}, {5, 6}};
  Identity<int, 2, 2> eye;

  auto result = kronecker(a, eye);

  // A ⊗ I₂ = [3·I  4·I]   [3 0 4 0]
  //          [5·I  6·I] = [0 3 0 4]
  //                       [5 0 6 0]
  //                       [0 5 0 6]

  expectEq(4, result.extent().extent(0));
  expectEq(4, result.extent().extent(1));

  expectEq(3, result[0, 0]);
  expectEq(0, result[0, 1]);
  expectEq(4, result[0, 2]);
  expectEq(0, result[0, 3]);

  expectEq(0, result[1, 0]);
  expectEq(3, result[1, 1]);
  expectEq(0, result[1, 2]);
  expectEq(4, result[1, 3]);

  expectEq(5, result[2, 0]);
  expectEq(0, result[2, 1]);
  expectEq(6, result[2, 2]);
  expectEq(0, result[2, 3]);

  expectEq(0, result[3, 0]);
  expectEq(5, result[3, 1]);
  expectEq(0, result[3, 2]);
  expectEq(6, result[3, 3]);
};

// Scalar multiplication equivalence: (cA) ⊗ B = c(A ⊗ B) = A ⊗ (cB)
"kronecker scalar multiplication"_test = [] {
  InlineDense<int, 2, 2> a{{1, 2}, {3, 4}};
  InlineDense<int, 2, 2> b{{5, 6}, {7, 8}};
  InlineDense<int, 2, 2> a_scaled{{2, 4}, {6, 8}};  // 2·A

  auto result1 = kronecker(a_scaled, b);
  auto result2 = kronecker(a, b);

  // Verify (2A) ⊗ B = 2(A ⊗ B)
  for (std::size_t i = 0; i < 4; ++i) {
    for (std::size_t j = 0; j < 4; ++j) {
      expectEq(result1[i, j], 2 * result2[i, j]);
    }
  }
};

// Mixed types: int ⊗ double
"kronecker mixed types"_test = [] {
  InlineDense<int, 2, 2> a{{1, 2}, {3, 4}};
  InlineDense<double, 2, 2> b{{0.5, 1.0}, {1.5, 2.0}};

  auto result = kronecker(a, b);

  // Result type should be double (int * double = double)
  expectEq(0.5, result[0, 0]);
  expectEq(1.0, result[0, 1]);
  expectEq(1.0, result[0, 2]);
  expectEq(2.0, result[0, 3]);

  expectEq(1.5, result[1, 0]);
  expectEq(2.0, result[1, 1]);
  expectEq(3.0, result[1, 2]);
  expectEq(4.0, result[1, 3]);
};

// Single element matrices
"kronecker 1x1"_test = [] {
  InlineDense<int, 1, 1> a{{3}};
  InlineDense<int, 1, 1> b{{7}};

  auto result = kronecker(a, b);

  expectEq(1, result.extent().extent(0));
  expectEq(1, result.extent().extent(1));
  expectEq(21, result[0, 0]);
};

// Edge case: zeros
"kronecker with zeros"_test = [] {
  InlineDense<int, 2, 2> a{{0, 1}, {2, 0}};
  InlineDense<int, 2, 2> b{{3, 4}, {5, 6}};

  auto result = kronecker(a, b);

  // First block (0·B) should be all zeros
  expectEq(0, result[0, 0]);
  expectEq(0, result[0, 1]);
  expectEq(0, result[1, 0]);
  expectEq(0, result[1, 1]);

  // Second block (1·B) should equal B
  expectEq(3, result[0, 2]);
  expectEq(4, result[0, 3]);
  expectEq(5, result[1, 2]);
  expectEq(6, result[1, 3]);
};

  // Compile-time extent verification
  static_assert(
      decltype(kronecker(std::declval<InlineDense<int, 2, 3>>(),
                         std::declval<InlineDense<int, 4, 5>>()))::ExtentsType::
              staticExtent(0) == 8,
      "2×3 ⊗ 4×5 should have 8 rows");

  static_assert(
      decltype(kronecker(std::declval<InlineDense<int, 2, 3>>(),
                         std::declval<InlineDense<int, 4, 5>>()))::ExtentsType::
              staticExtent(1) == 15,
      "2×3 ⊗ 4×5 should have 15 columns");

  return TestRegistry::result();
}
