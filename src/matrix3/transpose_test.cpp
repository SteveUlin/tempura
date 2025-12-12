#include "matrix3/transpose.h"

#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "index swapping"_test = [] {
  InlineDense<int, 2, 3> m{
      {1, 2, 3},
      {4, 5, 6}};
  Transpose t{m};

  // Original: 2x3 matrix
  // [1 2 3]
  // [4 5 6]
  //
  // Transpose: 3x2 matrix
  // [1 4]
  // [2 5]
  // [3 6]

  expectEq(1, t[0, 0]);
  expectEq(4, t[0, 1]);
  expectEq(2, t[1, 0]);
  expectEq(5, t[1, 1]);
  expectEq(3, t[2, 0]);
  expectEq(6, t[2, 1]);
};

  "extent swapping"_test = [] {
  InlineDense<int, 2, 3> m{
      {1, 2, 3},
      {4, 5, 6}};
  Transpose t{m};

  expectEq(2uz, m.extent().extent(0));  // Original: 2 rows
  expectEq(3uz, m.extent().extent(1));  // Original: 3 cols

  expectEq(3uz, t.rows());  // Transpose: 3 rows (= original cols)
  expectEq(2uz, t.cols());  // Transpose: 2 cols (= original rows)
};

  "double transpose identity"_test = [] {
    InlineDense<int, 2, 3> m{
        {1, 2, 3},
        {4, 5, 6}};
    Transpose<InlineDense<int, 2, 3>> t1{m};
    // Explicitly specify type to avoid CTAD issues
    Transpose<Transpose<InlineDense<int, 2, 3>>> t2{t1};

    // t2 should have same extents and values as m
    expectEq(2uz, t2.rows());
    expectEq(3uz, t2.cols());

    for (std::size_t i = 0; i < 2; ++i) {
      for (std::size_t j = 0; j < 3; ++j) {
        expectEq(m[i, j], t2[i, j]);
      }
    }
  };

  "mutable access"_test = [] {
    // Transpose stores by reference, so modifications propagate
    InlineDense<int, 2, 3> m{
        {1, 2, 3},
        {4, 5, 6}};
    Transpose t{m};

    // Modify through transpose view (propagates to original)
    t[1, 0] = 99;  // Modifies position [0,1] in original

    expectEq(99, m[0, 1]);   // Original modified
    expectEq(99, t[1, 0]);   // Transpose reflects change

    // Modify original (visible through transpose)
    m[1, 2] = 77;

    expectEq(77, t[2, 1]);  // Transpose reflects change
  };

  "square matrix"_test = [] {
  InlineDense<int, 3, 3> m{
      {1, 2, 3},
      {4, 5, 6},
      {7, 8, 9}};
  Transpose t{m};

  expectEq(3uz, t.rows());
  expectEq(3uz, t.cols());

  // Diagonal elements unchanged
  expectEq(1, t[0, 0]);
  expectEq(5, t[1, 1]);
  expectEq(9, t[2, 2]);

  // Off-diagonal elements swapped
  expectEq(4, t[0, 1]);
  expectEq(2, t[1, 0]);
  expectEq(7, t[0, 2]);
  expectEq(3, t[2, 0]);
  expectEq(8, t[1, 2]);
  expectEq(6, t[2, 1]);
};

  "1x1 matrix"_test = [] {
  InlineDense<int, 1, 1> m{{42}};
  Transpose t{m};

  expectEq(1uz, t.rows());
  expectEq(1uz, t.cols());
  expectEq(42, t[0, 0]);
};

  "column vector"_test = [] {
  InlineDense<int, 3, 1> col{
      {1},
      {2},
      {3}};
  Transpose row{col};

  expectEq(1uz, row.rows());  // Row vector: 1 row
  expectEq(3uz, row.cols());  // Row vector: 3 cols

  expectEq(1, row[0, 0]);
  expectEq(2, row[0, 1]);
  expectEq(3, row[0, 2]);
};

  "row vector"_test = [] {
  InlineDense<int, 1, 4> row{{1, 2, 3, 4}};
  Transpose col{row};

  expectEq(4uz, col.rows());  // Column vector: 4 rows
  expectEq(1uz, col.cols());  // Column vector: 1 col

  expectEq(1, col[0, 0]);
  expectEq(2, col[1, 0]);
  expectEq(3, col[2, 0]);
  expectEq(4, col[3, 0]);
};

  // Compile-time verification (using static for constant address)
  {
    static constexpr InlineDense<int, 2, 3> m{
        {1, 2, 3},
        {4, 5, 6}};
    static constexpr Transpose t{m};
    static_assert(t.rows() == 3);
    static_assert(t.cols() == 2);
  }

  "deduction guide"_test = [] {
  InlineDense<int, 2, 3> m{
      {1, 2, 3},
      {4, 5, 6}};

  // Should deduce Transpose<InlineDense<int, 2, 3>>
  Transpose t{m};

  expectEq(3uz, t.rows());
  expectEq(2uz, t.cols());
  expectEq(4, t[0, 1]);
};

  "floating point"_test = [] {
    InlineDense<double, 2, 2> m{
        {1.5, 2.5},
        {3.5, 4.5}};
    Transpose t{m};

    expectEq(1.5, t[0, 0]);
    expectEq(3.5, t[0, 1]);
    expectEq(2.5, t[1, 0]);
    expectEq(4.5, t[1, 1]);
  };

  "const matrix with transpose"_test = [] {
    const InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    auto t = Transpose{m};  // Should work with const matrix

    expectEq(3uz, t.rows());
    expectEq(2uz, t.cols());
    expectEq(1, t[0, 0]);
    expectEq(4, t[0, 1]);
    expectEq(2, t[1, 0]);
    expectEq(5, t[1, 1]);
    expectEq(3, t[2, 0]);
    expectEq(6, t[2, 1]);
    // t[0, 0] = 99;  // Should NOT compile (const)
  };

  "const transpose view"_test = [] {
    InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    const auto t = Transpose{m};  // Const view of non-const matrix

    // Can read through const view
    expectEq(1, t[0, 0]);
    expectEq(4, t[0, 1]);
    expectEq(2, t[1, 0]);

    // t[0, 0] = 99;  // Should NOT compile (const view)

    // But can modify original matrix
    m[0, 1] = 77;
    expectEq(77, t[1, 0]);  // Change visible through const view
  };

  "mutable transpose of const matrix"_test = [] {
    const InlineDense<int, 2, 2> m{{1, 2}, {3, 4}};
    Transpose t{m};  // Non-const view, but matrix is const

    // Can read
    expectEq(1, t[0, 0]);
    expectEq(3, t[0, 1]);
    expectEq(2, t[1, 0]);
    expectEq(4, t[1, 1]);

    // t[0, 0] = 99;  // Should NOT compile (underlying matrix is const)
  };

  return TestRegistry::result();
}
