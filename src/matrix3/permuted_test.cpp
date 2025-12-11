#include "matrix3/permuted.h"

#include "matrix3/matrix.h"
#include "matrix3/permutation.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "RowPermuted with identity permutation"_test = [] {
  InlineDense<int, 3, 2> mat{
      {1, 2},
      {3, 4},
      {5, 6},
  };

  RowPermuted rp{mat};

  expectEq(3uz, rp.rows());
  expectEq(2uz, rp.cols());

  // Identity permutation: should match original matrix
  expectEq(1, rp[0, 0]);
  expectEq(2, rp[0, 1]);
  expectEq(3, rp[1, 0]);
  expectEq(4, rp[1, 1]);
  expectEq(5, rp[2, 0]);
  expectEq(6, rp[2, 1]);
};

"RowPermuted with explicit permutation"_test = [] {
  InlineDense<int, 3, 2> mat{
      {1, 2},
      {3, 4},
      {5, 6},
  };

  // Permutation: [2, 0, 1] maps row 0→2, 1→0, 2→1
  Permutation<3> perm{{2, 0, 1}};
  RowPermuted rp{mat, perm};

  // Row 0 should show original row 2
  expectEq(5, rp[0, 0]);
  expectEq(6, rp[0, 1]);

  // Row 1 should show original row 0
  expectEq(1, rp[1, 0]);
  expectEq(2, rp[1, 1]);

  // Row 2 should show original row 1
  expectEq(3, rp[2, 0]);
  expectEq(4, rp[2, 1]);
};

"RowPermuted swap operations"_test = [] {
  InlineDense<int, 3, 2> mat{
      {1, 2},
      {3, 4},
      {5, 6},
  };

  RowPermuted rp{mat};

  // Swap rows 0 and 2
  rp.swap(0, 2);

  // Row 0 should now show original row 2
  expectEq(5, rp[0, 0]);
  expectEq(6, rp[0, 1]);

  // Row 2 should now show original row 0
  expectEq(1, rp[2, 0]);
  expectEq(2, rp[2, 1]);

  // Row 1 unchanged
  expectEq(3, rp[1, 0]);
  expectEq(4, rp[1, 1]);

  // Swap again to verify parity tracking
  rp.swap(1, 2);

  // Row 1 should now show original row 0
  expectEq(1, rp[1, 0]);
  expectEq(2, rp[1, 1]);

  // Row 2 should now show original row 1
  expectEq(3, rp[2, 0]);
  expectEq(4, rp[2, 1]);

  // Check parity: two swaps = even parity
  expectEq(false, rp.permutation().parity());
};

"RowPermuted mutable access"_test = [] {
  InlineDense<int, 3, 2> mat{
      {1, 2},
      {3, 4},
      {5, 6},
  };

  RowPermuted rp{mat};

  // Modify through permuted view
  rp[0, 0] = 10;
  rp[1, 1] = 20;

  // Modifications persist within the permuted view
  expectEq(10, rp[0, 0]);
  expectEq(20, rp[1, 1]);

  // Swap rows and verify modified values move with the permutation
  rp.swap(0, 1);
  expectEq(3, rp[0, 0]);
  expectEq(20, rp[0, 1]);
  expectEq(10, rp[1, 0]);
  expectEq(2, rp[1, 1]);
};

"ColPermuted with identity permutation"_test = [] {
  InlineDense<int, 2, 3> mat{
      {1, 2, 3},
      {4, 5, 6},
  };

  ColPermuted cp{mat};

  expectEq(2uz, cp.rows());
  expectEq(3uz, cp.cols());

  // Identity permutation: should match original matrix
  expectEq(1, cp[0, 0]);
  expectEq(2, cp[0, 1]);
  expectEq(3, cp[0, 2]);
  expectEq(4, cp[1, 0]);
  expectEq(5, cp[1, 1]);
  expectEq(6, cp[1, 2]);
};

"ColPermuted with explicit permutation"_test = [] {
  InlineDense<int, 2, 3> mat{
      {1, 2, 3},
      {4, 5, 6},
  };

  // Permutation: [2, 0, 1] maps col 0→2, 1→0, 2→1
  Permutation<3> perm{{2, 0, 1}};
  ColPermuted cp{mat, perm};

  // Col 0 should show original col 2
  expectEq(3, cp[0, 0]);
  expectEq(6, cp[1, 0]);

  // Col 1 should show original col 0
  expectEq(1, cp[0, 1]);
  expectEq(4, cp[1, 1]);

  // Col 2 should show original col 1
  expectEq(2, cp[0, 2]);
  expectEq(5, cp[1, 2]);
};

"ColPermuted swap operations"_test = [] {
  InlineDense<int, 2, 3> mat{
      {1, 2, 3},
      {4, 5, 6},
  };

  ColPermuted cp{mat};

  // Swap cols 0 and 2
  cp.swap(0, 2);

  // Col 0 should now show original col 2
  expectEq(3, cp[0, 0]);
  expectEq(6, cp[1, 0]);

  // Col 2 should now show original col 0
  expectEq(1, cp[0, 2]);
  expectEq(4, cp[1, 2]);

  // Col 1 unchanged
  expectEq(2, cp[0, 1]);
  expectEq(5, cp[1, 1]);

  // Swap again to verify parity tracking
  cp.swap(1, 2);

  // Col 1 should now show original col 0
  expectEq(1, cp[0, 1]);
  expectEq(4, cp[1, 1]);

  // Col 2 should now show original col 1
  expectEq(2, cp[0, 2]);
  expectEq(5, cp[1, 2]);

  // Check parity: two swaps = even parity
  expectEq(false, cp.permutation().parity());
};

"ColPermuted mutable access"_test = [] {
  InlineDense<int, 2, 3> mat{
      {1, 2, 3},
      {4, 5, 6},
  };

  ColPermuted cp{mat};

  // Modify through permuted view
  cp[0, 0] = 10;
  cp[1, 2] = 20;

  // Modifications persist within the permuted view
  expectEq(10, cp[0, 0]);
  expectEq(20, cp[1, 2]);

  // Swap columns and verify modified values move with the permutation
  cp.swap(0, 2);
  expectEq(3, cp[0, 0]);
  expectEq(20, cp[1, 0]);
  expectEq(10, cp[0, 2]);
  expectEq(4, cp[1, 2]);
};

"deduction guides"_test = [] {
  InlineDense<int, 3, 2> mat{
      {1, 2},
      {3, 4},
      {5, 6},
  };

  // Should deduce RowPermuted<InlineDense<int, 3, 2>, kDynamic>
  RowPermuted rp1{mat};
  static_assert(std::is_same_v<decltype(rp1)::ValueType, int>);

  // Should deduce with static size from Permutation
  Permutation<3> perm;
  RowPermuted rp2{mat, perm};
  static_assert(std::is_same_v<decltype(rp2)::ValueType, int>);

  // ColPermuted deduction
  ColPermuted cp1{mat};
  static_assert(std::is_same_v<decltype(cp1)::ValueType, int>);

  ColPermuted cp2{mat, Permutation<2>{}};
  static_assert(std::is_same_v<decltype(cp2)::ValueType, int>);
};

"static size permutation"_test = [] {
  InlineDense<int, 3, 2> mat{
      {1, 2},
      {3, 4},
      {5, 6},
  };

  // Use static size permutation
  RowPermuted<InlineDense<int, 3, 2>, 3> rp{mat};

  expectEq(1, rp[0, 0]);
  expectEq(2, rp[0, 1]);

  rp.swap(0, 2);

  expectEq(5, rp[0, 0]);
  expectEq(6, rp[0, 1]);
};

  return TestRegistry::result();
}
