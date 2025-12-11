#include "matrix3/permutation.h"

#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "Identity permutation (static)"_test = [] {
  constexpr Permutation<3> p;

  static_assert(p.rows() == 3);
  static_assert(p.cols() == 3);
  static_assert(p.staticRows() == 3);
  static_assert(p.staticCols() == 3);

  // Identity has diagonal elements
  static_assert(p[0, 0] == true);
  static_assert(p[1, 1] == true);
  static_assert(p[2, 2] == true);

  // Off-diagonal elements are false
  static_assert(p[0, 1] == false);
  static_assert(p[0, 2] == false);
  static_assert(p[1, 0] == false);
  static_assert(p[1, 2] == false);
  static_assert(p[2, 0] == false);
  static_assert(p[2, 1] == false);

  // Identity permutation has even parity
  static_assert(p.parity() == false);
  };

  "Identity permutation (dynamic)"_test = [] {
  Permutation<> p(3);

  expectEq(p.rows(), 3);
  expectEq(p.cols(), 3);

  expectTrue(p[0, 0]);
  expectTrue(p[1, 1]);
  expectTrue(p[2, 2]);

  expectFalse(p[0, 1]);
  expectFalse(p[1, 0]);
  expectFalse(p.parity());
  };

  "Simple transposition (swap two elements)"_test = [] {
  // Swap first and last: (0 2) in cycle notation
  constexpr Permutation<3> p{2, 1, 0};

  static_assert(p[2, 0] == true);  // Row 2 at column 0
  static_assert(p[1, 1] == true);  // Row 1 at column 1
  static_assert(p[0, 2] == true);  // Row 0 at column 2

  // Transposition has odd parity
  static_assert(p.parity() == true);
  };

  "Cyclic permutation"_test = [] {
  // 3-cycle: 0→1→2→0
  constexpr Permutation<3> p{1, 2, 0};

  static_assert(p[1, 0] == true);  // Row 1 at column 0
  static_assert(p[2, 1] == true);  // Row 2 at column 1
  static_assert(p[0, 2] == true);  // Row 0 at column 2

  // 3-cycle = 2 transpositions, even parity
  static_assert(p.parity() == false);
  };

  "Parity calculation - complex permutation"_test = [] {
  // Permutation with two 2-cycles: (0 1)(2 3)
  // Parity = even (two transpositions)
  constexpr Permutation<4> p{1, 0, 3, 2};

  static_assert(p.parity() == false);
  };

  "Parity calculation - single 3-cycle"_test = [] {
  // 3-cycle = 2 transpositions = even
  constexpr Permutation<4> p{1, 2, 0, 3};

  static_assert(p.parity() == false);
  };

  "swap() updates parity"_test = [] {
  Permutation<3> p;  // Identity, even parity

  expectFalse(p.parity());

  // First swap: even → odd
  p.swap(0, 1);
  expectTrue(p.parity());

  // Second swap: odd → even
  p.swap(1, 2);
  expectFalse(p.parity());

  // Third swap: even → odd
  p.swap(0, 2);
  expectTrue(p.parity());
  };

  "swap() updates permutation array"_test = [] {
  Permutation<3> p;  // Identity: [0, 1, 2]

  p.swap(0, 2);  // Now: [2, 1, 0]

  expectTrue(p[2, 0]);  // order_[0] = 2
  expectTrue(p[1, 1]);  // order_[1] = 1
  expectTrue(p[0, 2]);  // order_[2] = 0
  };

  "permuteRows() on dense matrix"_test = [] {
  // Permutation that reverses rows: [2, 1, 0]
  Permutation<3> perm{2, 1, 0};

  // Matrix:
  // ⎡ 1 2 3 ⎤
  // ⎢ 4 5 6 ⎥
  // ⎣ 7 8 9 ⎦
  InlineDense<int, 3, 3> mat{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

  perm.permuteRows(mat);

  // After permutation:
  // ⎡ 7 8 9 ⎤
  // ⎢ 4 5 6 ⎥
  // ⎣ 1 2 3 ⎦
  expectEq(mat[0, 0], 7);
  expectEq(mat[0, 1], 8);
  expectEq(mat[0, 2], 9);
  expectEq(mat[1, 0], 4);
  expectEq(mat[1, 1], 5);
  expectEq(mat[1, 2], 6);
  expectEq(mat[2, 0], 1);
  expectEq(mat[2, 1], 2);
  expectEq(mat[2, 2], 3);
  };

  "permuteRows() with cyclic permutation"_test = [] {
  // 3-cycle: 0→1→2→0
  Permutation<3> perm{1, 2, 0};

  InlineDense<int, 3, 3> mat{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

  perm.permuteRows(mat);

  // After cyclic permutation:
  // Row 0 → Row 1 → Row 2 → Row 0
  // ⎡ 7 8 9 ⎤  (was row 2)
  // ⎢ 1 2 3 ⎥  (was row 0)
  // ⎣ 4 5 6 ⎦  (was row 1)
  expectEq(mat[0, 0], 7);
  expectEq(mat[0, 1], 8);
  expectEq(mat[0, 2], 9);
  expectEq(mat[1, 0], 1);
  expectEq(mat[1, 1], 2);
  expectEq(mat[1, 2], 3);
  expectEq(mat[2, 0], 4);
  expectEq(mat[2, 1], 5);
  expectEq(mat[2, 2], 6);
  };

  "permuteRows() identity does nothing"_test = [] {
  Permutation<3> perm;  // Identity

  InlineDense<int, 3, 3> mat{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

  perm.permuteRows(mat);

  // Matrix unchanged
  expectEq(mat[0, 0], 1);
  expectEq(mat[1, 1], 5);
  expectEq(mat[2, 2], 9);
  };

  "Dynamic permutation"_test = [] {
  Permutation<> perm(4);  // Dynamic size 4, identity

  expectEq(perm.rows(), 4);
  expectEq(perm.cols(), 4);
  expectFalse(perm.parity());

  perm.swap(0, 3);
  expectTrue(perm.parity());

  expectTrue(perm[3, 0]);
  expectTrue(perm[1, 1]);
  expectTrue(perm[2, 2]);
  expectTrue(perm[0, 3]);
  };

  "Dynamic permutation from initializer list"_test = [] {
  Permutation<> perm{2, 1, 0};  // Reverse permutation

  expectEq(perm.rows(), 3);
  expectTrue(perm.parity());  // One transposition
  expectTrue(perm[2, 0]);
  expectTrue(perm[0, 2]);
  };

  "data() returns permutation array"_test = [] {
  constexpr Permutation<3> p{1, 2, 0};

  static_assert(p.data()[0] == 1);
  static_assert(p.data()[1] == 2);
  static_assert(p.data()[2] == 0);
  };

  return TestRegistry::result();
}
