#include "matrix3/matrix.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "DynamicDense construction"_test = [] {
    DynamicDense<double> mat(3, 4);
    expectEq(mat.rows(), 3UL);
    expectEq(mat.cols(), 4UL);

    // All elements should be zero-initialized
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 4; ++j) {
        expectEq(mat[i, j], 0.0);
      }
    }
  };

  "DynamicDense element access"_test = [] {
    DynamicDense<int> mat(2, 3);
    mat[0, 0] = 1;
    mat[0, 1] = 2;
    mat[0, 2] = 3;
    mat[1, 0] = 4;
    mat[1, 1] = 5;
    mat[1, 2] = 6;

    expectEq(mat[0, 0], 1);
    expectEq(mat[0, 1], 2);
    expectEq(mat[0, 2], 3);
    expectEq(mat[1, 0], 4);
    expectEq(mat[1, 1], 5);
    expectEq(mat[1, 2], 6);

    // Verify column-major storage: elements in same column are contiguous
    // Column 0: [1, 4], Column 1: [2, 5], Column 2: [3, 6]
    expectEq(mat.data()[0], 1);  // mat[0,0]
    expectEq(mat.data()[1], 4);  // mat[1,0]
    expectEq(mat.data()[2], 2);  // mat[0,1]
    expectEq(mat.data()[3], 5);  // mat[1,1]
    expectEq(mat.data()[4], 3);  // mat[0,2]
    expectEq(mat.data()[5], 6);  // mat[1,2]
  };

  "DynamicDense resize"_test = [] {
    DynamicDense<double> mat(2, 2);
    mat[0, 0] = 1.0;
    mat[0, 1] = 2.0;
    mat[1, 0] = 3.0;
    mat[1, 1] = 4.0;

    // Resize to larger dimensions
    mat.resize(3, 4);
    expectEq(mat.rows(), 3UL);
    expectEq(mat.cols(), 4UL);

    // After resize, data is reset to zero
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 4; ++j) {
        expectEq(mat[i, j], 0.0);
      }
    }

    // Resize to smaller dimensions
    mat.resize(1, 1);
    expectEq(mat.rows(), 1UL);
    expectEq(mat.cols(), 1UL);
  };

  "DynamicDense pushRow"_test = [] {
    DynamicDense<int> mat(2, 3);
    mat[0, 0] = 1;
    mat[0, 1] = 2;
    mat[0, 2] = 3;
    mat[1, 0] = 4;
    mat[1, 1] = 5;
    mat[1, 2] = 6;

    // Push a new row
    std::array<int, 3> new_row = {7, 8, 9};
    mat.pushRow(new_row);

    expectEq(mat.rows(), 3UL);
    expectEq(mat.cols(), 3UL);

    // Check original data preserved
    expectEq(mat[0, 0], 1);
    expectEq(mat[0, 1], 2);
    expectEq(mat[0, 2], 3);
    expectEq(mat[1, 0], 4);
    expectEq(mat[1, 1], 5);
    expectEq(mat[1, 2], 6);

    // Check new row
    expectEq(mat[2, 0], 7);
    expectEq(mat[2, 1], 8);
    expectEq(mat[2, 2], 9);
  };

  "DynamicDense reserveRows"_test = [] {
    DynamicDense<double> mat(2, 3);
    mat.reserveRows(100);

    // Dimensions unchanged
    expectEq(mat.rows(), 2UL);
    expectEq(mat.cols(), 3UL);

    // Capacity should be at least 100 * 3 = 300
    expectGE(mat.data().capacity(), 300UL);
  };

  "DynamicDense rows() and cols() accessors"_test = [] {
    DynamicDense<float> mat1(5, 10);
    expectEq(mat1.rows(), 5UL);
    expectEq(mat1.cols(), 10UL);

    DynamicDense<float> mat2(1, 1);
    expectEq(mat2.rows(), 1UL);
    expectEq(mat2.cols(), 1UL);

    DynamicDense<float> mat3(100, 50);
    expectEq(mat3.rows(), 100UL);
    expectEq(mat3.cols(), 50UL);
  };

  "DynamicDense multiple pushRow operations"_test = [] {
    DynamicDense<int> mat(0, 3);
    expectEq(mat.rows(), 0UL);
    expectEq(mat.cols(), 3UL);

    // Build matrix row by row
    mat.pushRow(std::array{1, 2, 3});
    expectEq(mat.rows(), 1UL);
    expectEq(mat[0, 0], 1);
    expectEq(mat[0, 1], 2);
    expectEq(mat[0, 2], 3);

    mat.pushRow(std::array{4, 5, 6});
    expectEq(mat.rows(), 2UL);
    expectEq(mat[1, 0], 4);
    expectEq(mat[1, 1], 5);
    expectEq(mat[1, 2], 6);

    mat.pushRow(std::array{7, 8, 9});
    expectEq(mat.rows(), 3UL);
    expectEq(mat[2, 0], 7);
    expectEq(mat[2, 1], 8);
    expectEq(mat[2, 2], 9);
  };

  return TestRegistry::result();
}
