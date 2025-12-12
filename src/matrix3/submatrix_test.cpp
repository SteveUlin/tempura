#include "matrix3/submatrix.h"

#include "matrix3/matrix.h"
#include "matrix3/transpose.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "basic submatrix access"_test = [] {
    Dense<int, 3, 4> m{{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};

    // Extract 2x2 submatrix starting at [1,1]
    auto sub = Submatrix{m, 1, 1, 2, 2};

    expectEq(2uz, sub.rows());
    expectEq(2uz, sub.cols());
    expectEq(6, sub[0, 0]);
    expectEq(7, sub[0, 1]);
    expectEq(10, sub[1, 0]);
    expectEq(11, sub[1, 1]);
  };

  "submatrix extents"_test = [] {
    Dense<double, 5, 7> m;
    auto sub = Submatrix{m, 1, 2, 3, 4};

    expectEq(3uz, sub.rows());
    expectEq(4uz, sub.cols());
    expectEq(1uz, sub.rowOffset());
    expectEq(2uz, sub.colOffset());
  };

  "mutable submatrix access"_test = [] {
    Dense<int, 3, 3> m{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

    // Extract center element as 1x1 submatrix
    auto sub = Submatrix{m, 1, 1, 1, 1};
    expectEq(5, sub[0, 0]);

    // Modify through submatrix
    sub[0, 0] = 99;
    expectEq(99, sub[0, 0]);
    expectEq(99, m[1, 1]);  // Change reflected in parent

    // Modify larger region
    auto sub2 = Submatrix{m, 0, 0, 2, 2};
    sub2[0, 0] = 10;
    sub2[0, 1] = 20;
    sub2[1, 0] = 30;
    sub2[1, 1] = 40;

    expectEq(10, m[0, 0]);
    expectEq(20, m[0, 1]);
    expectEq(30, m[1, 0]);
    expectEq(40, m[1, 1]);
  };

  "corner cases"_test = [] {
    Dense<int, 4, 5> m{{1, 2, 3, 4, 5},
                       {6, 7, 8, 9, 10},
                       {11, 12, 13, 14, 15},
                       {16, 17, 18, 19, 20}};

    // First row
    auto first_row = Submatrix{m, 0, 0, 1, 5};
    expectEq(1uz, first_row.rows());
    expectEq(5uz, first_row.cols());
    expectEq(1, first_row[0, 0]);
    expectEq(5, first_row[0, 4]);

    // Last column
    auto last_col = Submatrix{m, 0, 4, 4, 1};
    expectEq(4uz, last_col.rows());
    expectEq(1uz, last_col.cols());
    expectEq(5, last_col[0, 0]);
    expectEq(10, last_col[1, 0]);
    expectEq(15, last_col[2, 0]);
    expectEq(20, last_col[3, 0]);

    // Bottom-right corner
    auto corner = Submatrix{m, 3, 4, 1, 1};
    expectEq(20, corner[0, 0]);
  };

  "nested submatrix"_test = [] {
    Dense<int, 5, 5> m{{1, 2, 3, 4, 5},
                       {6, 7, 8, 9, 10},
                       {11, 12, 13, 14, 15},
                       {16, 17, 18, 19, 20},
                       {21, 22, 23, 24, 25}};

    // First level: 3x3 submatrix starting at [1,1]
    auto sub1 = Submatrix{m, 1, 1, 3, 3};
    expectEq(7, sub1[0, 0]);
    expectEq(9, sub1[0, 2]);
    expectEq(17, sub1[2, 0]);
    expectEq(19, sub1[2, 2]);

    // Second level: 2x2 submatrix of sub1 starting at [1,1]
    auto sub2 = Submatrix{sub1, 1, 1, 2, 2};
    expectEq(2uz, sub2.rows());
    expectEq(2uz, sub2.cols());

    // Offsets should compose: sub1 starts at [1,1], sub2 starts at [1,1] within sub1
    // So sub2 should access m starting at [2,2]
    expectEq(2uz, sub2.rowOffset());
    expectEq(2uz, sub2.colOffset());
    expectEq(13, sub2[0, 0]);  // m[2,2]
    expectEq(14, sub2[0, 1]);  // m[2,3]
    expectEq(18, sub2[1, 0]);  // m[3,2]
    expectEq(19, sub2[1, 1]);  // m[3,3]

    // Modifications through nested submatrix
    sub2[0, 0] = 100;
    expectEq(100, m[2, 2]);
    expectEq(100, sub1[1, 1]);
    expectEq(100, sub2[0, 0]);
  };

  "factory function"_test = [] {
    Dense<int, 3, 3> m{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

    auto sub = makeSubmatrix(m, 0, 1, 2, 2);
    expectEq(2uz, sub.rows());
    expectEq(2uz, sub.cols());
    expectEq(2, sub[0, 0]);
    expectEq(3, sub[0, 1]);
    expectEq(5, sub[1, 0]);
    expectEq(6, sub[1, 1]);
  };

  "submatrix of transpose"_test = [] {
    Dense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    Transpose t{m};

    // t is 3x2 (transpose of 2x3)
    expectEq(3uz, t.rows());
    expectEq(2uz, t.cols());

    // Extract 2x2 submatrix from transpose
    auto sub = Submatrix{t, 0, 0, 2, 2};
    expectEq(2uz, sub.rows());
    expectEq(2uz, sub.cols());

    // t[i,j] = m[j,i]
    expectEq(1, t[0, 0]);  // m[0,0]
    expectEq(4, t[0, 1]);  // m[1,0]
    expectEq(2, t[1, 0]);  // m[0,1]
    expectEq(5, t[1, 1]);  // m[1,1]

    expectEq(1, sub[0, 0]);
    expectEq(4, sub[0, 1]);
    expectEq(2, sub[1, 0]);
    expectEq(5, sub[1, 1]);
  };

  "single element submatrix"_test = [] {
    Dense<int, 3, 3> m{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

    auto single = Submatrix{m, 1, 1, 1, 1};
    expectEq(1uz, single.rows());
    expectEq(1uz, single.cols());
    expectEq(5, single[0, 0]);

    single[0, 0] = 42;
    expectEq(42, m[1, 1]);
  };

  "full matrix submatrix"_test = [] {
    Dense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};

    // Submatrix covering entire matrix
    auto full = Submatrix{m, 0, 0, 2, 3};
    expectEq(2uz, full.rows());
    expectEq(3uz, full.cols());

    // Should have same values as original
    for (std::size_t i = 0; i < 2; ++i) {
      for (std::size_t j = 0; j < 3; ++j) {
        expectEq(m[i, j], full[i, j]);
      }
    }
  };

  "const matrix with submatrix"_test = [] {
    const Dense<int, 3, 4> m{{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
    auto sub = Submatrix{m, 1, 1, 2, 2};  // Should work with const matrix

    expectEq(2uz, sub.rows());
    expectEq(2uz, sub.cols());
    expectEq(6, sub[0, 0]);
    expectEq(7, sub[0, 1]);
    expectEq(10, sub[1, 0]);
    expectEq(11, sub[1, 1]);
    // sub[0, 0] = 99;  // Should NOT compile (const)
  };

  "const submatrix view"_test = [] {
    Dense<int, 3, 3> m{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    const auto sub = Submatrix{m, 0, 0, 2, 2};  // Const view of non-const matrix

    // Can read through const view
    expectEq(1, sub[0, 0]);
    expectEq(2, sub[0, 1]);
    expectEq(4, sub[1, 0]);
    expectEq(5, sub[1, 1]);

    // sub[0, 0] = 99;  // Should NOT compile (const view)

    // But can modify original matrix
    m[0, 0] = 77;
    expectEq(77, sub[0, 0]);  // Change visible through const view
  };

  "mutable submatrix of const matrix"_test = [] {
    const Dense<int, 3, 3> m{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    Submatrix sub{m, 1, 1, 2, 2};  // Non-const view, but matrix is const

    // Can read
    expectEq(5, sub[0, 0]);
    expectEq(6, sub[0, 1]);
    expectEq(8, sub[1, 0]);
    expectEq(9, sub[1, 1]);

    // sub[0, 0] = 99;  // Should NOT compile (underlying matrix is const)
  };

  "const nested submatrix"_test = [] {
    const Dense<int, 4, 4> m{
        {1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};

    // Extract const submatrix from const matrix
    auto sub1 = Submatrix{m, 0, 0, 3, 3};
    expectEq(1, sub1[0, 0]);
    expectEq(11, sub1[2, 2]);

    // Nested const submatrix
    auto sub2 = Submatrix{sub1, 1, 1, 2, 2};
    expectEq(6, sub2[0, 0]);
    expectEq(7, sub2[0, 1]);
    expectEq(10, sub2[1, 0]);
    expectEq(11, sub2[1, 1]);

    // sub2[0, 0] = 99;  // Should NOT compile (const)
  };

  return TestRegistry::result();
}
