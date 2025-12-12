#include "matrix3/multiplication.h"
#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "basic 2x2 matrix multiplication"_test = [] {
    InlineDense mat1 = {{1, 2}, {3, 4}};
    InlineDense mat2 = {{5, 6}, {7, 8}};

    auto result = mat1 * mat2;

    // [[1,2], [3,4]] * [[5,6], [7,8]] = [[19,22], [43,50]]
    expectEq(result[0, 0], 19);
    expectEq(result[0, 1], 22);
    expectEq(result[1, 0], 43);
    expectEq(result[1, 1], 50);
  };

  "identity matrix multiplication"_test = [] {
    InlineDense mat = {{1, 2}, {3, 4}};
    Identity<int, 2, 2> id;

    auto result1 = mat * id;
    auto result2 = id * mat;

    // mat * I = mat
    expectEq(result1[0, 0], 1);
    expectEq(result1[0, 1], 2);
    expectEq(result1[1, 0], 3);
    expectEq(result1[1, 1], 4);

    // I * mat = mat
    expectEq(result2[0, 0], 1);
    expectEq(result2[0, 1], 2);
    expectEq(result2[1, 0], 3);
    expectEq(result2[1, 1], 4);
  };

  "3x3 matrix multiplication"_test = [] {
    InlineDense mat1 = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    InlineDense mat2 = {{9, 8, 7}, {6, 5, 4}, {3, 2, 1}};

    auto result = mat1 * mat2;

    // First row: [1,2,3] * [[9],[6],[3]], [1,2,3] * [[8],[5],[2]], etc.
    expectEq(result[0, 0], 30);
    expectEq(result[0, 1], 24);
    expectEq(result[0, 2], 18);
    expectEq(result[1, 0], 84);
    expectEq(result[1, 1], 69);
    expectEq(result[1, 2], 54);
    expectEq(result[2, 0], 138);
    expectEq(result[2, 1], 114);
    expectEq(result[2, 2], 90);
  };

  "rectangular matrix multiplication 2x3 * 3x2"_test = [] {
    InlineDense<int, 2, 3> mat1 = {{1, 2, 3}, {4, 5, 6}};
    InlineDense<int, 3, 2> mat2 = {{7, 8}, {9, 10}, {11, 12}};

    auto result = mat1 * mat2;

    // Result should be 2x2
    // [[1,2,3], [4,5,6]] * [[7,8], [9,10], [11,12]]
    // = [[1*7+2*9+3*11, 1*8+2*10+3*12], [4*7+5*9+6*11, 4*8+5*10+6*12]]
    expectEq(result[0, 0], 58);
    expectEq(result[0, 1], 64);
    expectEq(result[1, 0], 139);
    expectEq(result[1, 1], 154);
  };

  "rectangular matrix multiplication 3x2 * 2x3"_test = [] {
    InlineDense<int, 3, 2> mat1 = {{1, 2}, {3, 4}, {5, 6}};
    InlineDense<int, 2, 3> mat2 = {{7, 8, 9}, {10, 11, 12}};

    auto result = mat1 * mat2;

    // Result should be 3x3
    expectEq(result[0, 0], 27);
    expectEq(result[0, 1], 30);
    expectEq(result[0, 2], 33);
    expectEq(result[1, 0], 61);
    expectEq(result[1, 1], 68);
    expectEq(result[1, 2], 75);
    expectEq(result[2, 0], 95);
    expectEq(result[2, 1], 106);
    expectEq(result[2, 2], 117);
  };

  "matrix-vector multiplication 2x3 * 3x1"_test = [] {
    InlineDense<int, 2, 3> mat = {{1, 2, 3}, {4, 5, 6}};
    InlineDense<int, 3, 1> vec = {{7}, {8}, {9}};

    auto result = mat * vec;

    // Result should be 2x1 (column vector)
    // [1*7+2*8+3*9, 4*7+5*8+6*9]^T
    expectEq(result[0, 0], 50);
    expectEq(result[1, 0], 122);
  };

  "vector-matrix multiplication 1x3 * 3x2"_test = [] {
    InlineDense<int, 1, 3> vec = {{1, 2, 3}};
    InlineDense<int, 3, 2> mat = {{4, 5}, {6, 7}, {8, 9}};

    auto result = vec * mat;

    // Result should be 1x2 (row vector)
    // [1*4+2*6+3*8, 1*5+2*7+3*9]
    expectEq(result[0, 0], 40);
    expectEq(result[0, 1], 46);
  };

  "type promotion int * double"_test = [] {
    InlineDense<int, 2, 2> mat1 = {{1, 2}, {3, 4}};
    InlineDense<double, 2, 2> mat2 = {{1.5, 2.5}, {3.5, 4.5}};

    auto result = mat1 * mat2;

    // Result should be double due to type promotion
    // [[1*1.5+2*3.5, 1*2.5+2*4.5], [3*1.5+4*3.5, 3*2.5+4*4.5]]
    expectNear(result[0, 0], 8.5, 1e-10);
    expectNear(result[0, 1], 11.5, 1e-10);
    expectNear(result[1, 0], 18.5, 1e-10);
    expectNear(result[1, 1], 25.5, 1e-10);
  };

  "zero matrix multiplication"_test = [] {
    InlineDense<int, 2, 2> mat = {{1, 2}, {3, 4}};
    InlineDense<int, 2, 2> zero = {{0, 0}, {0, 0}};

    auto result1 = mat * zero;
    auto result2 = zero * mat;

    expectEq(result1[0, 0], 0);
    expectEq(result1[0, 1], 0);
    expectEq(result1[1, 0], 0);
    expectEq(result1[1, 1], 0);

    expectEq(result2[0, 0], 0);
    expectEq(result2[0, 1], 0);
    expectEq(result2[1, 0], 0);
    expectEq(result2[1, 1], 0);
  };

  "tileMultiply with explicit block_size"_test = [] {
    InlineDense mat1 = {{1, 2}, {3, 4}};
    InlineDense mat2 = {{5, 6}, {7, 8}};

    auto result = tileMultiply<8>(mat1, mat2);

    expectEq(result[0, 0], 19);
    expectEq(result[0, 1], 22);
    expectEq(result[1, 0], 43);
    expectEq(result[1, 1], 50);
  };

  "constexpr multiplication"_test = [] {
    constexpr auto result = [] consteval {
      InlineDense mat1 = {{1, 2}, {3, 4}};
      InlineDense mat2 = {{5, 6}, {7, 8}};
      return mat1 * mat2;
    }();

    static_assert(result[0, 0] == 19);
    static_assert(result[0, 1] == 22);
    static_assert(result[1, 0] == 43);
    static_assert(result[1, 1] == 50);
  };

  "constexpr identity multiplication"_test = [] {
    constexpr auto result = [] consteval {
      InlineDense mat = {{1, 2}, {3, 4}};
      Identity<int, 2, 2> id;
      return mat * id;
    }();

    static_assert(result[0, 0] == 1);
    static_assert(result[0, 1] == 2);
    static_assert(result[1, 0] == 3);
    static_assert(result[1, 1] == 4);
  };

  "scalar-matrix multiplication"_test = [] {
    InlineDense mat = {{1, 2}, {3, 4}};
    auto result = 3 * mat;

    expectEq(result[0, 0], 3);
    expectEq(result[0, 1], 6);
    expectEq(result[1, 0], 9);
    expectEq(result[1, 1], 12);
  };

  "matrix-scalar multiplication"_test = [] {
    InlineDense mat = {{1, 2}, {3, 4}};
    auto result = mat * 3;

    expectEq(result[0, 0], 3);
    expectEq(result[0, 1], 6);
    expectEq(result[1, 0], 9);
    expectEq(result[1, 1], 12);
  };

  "scalar multiplication with double"_test = [] {
    InlineDense<int, 2, 2> mat = {{1, 2}, {3, 4}};
    auto result = 2.5 * mat;

    expectNear(result[0, 0], 2.5, 1e-10);
    expectNear(result[0, 1], 5.0, 1e-10);
    expectNear(result[1, 0], 7.5, 1e-10);
    expectNear(result[1, 1], 10.0, 1e-10);
  };

  "scalar multiplication with zero"_test = [] {
    InlineDense mat = {{1, 2}, {3, 4}};
    auto result = 0 * mat;

    expectEq(result[0, 0], 0);
    expectEq(result[0, 1], 0);
    expectEq(result[1, 0], 0);
    expectEq(result[1, 1], 0);
  };

  "scalar multiplication with negative"_test = [] {
    InlineDense mat = {{1, 2}, {3, 4}};
    auto result = -2 * mat;

    expectEq(result[0, 0], -2);
    expectEq(result[0, 1], -4);
    expectEq(result[1, 0], -6);
    expectEq(result[1, 1], -8);
  };

  "scalar multiplication with rectangular matrix"_test = [] {
    InlineDense<int, 2, 3> mat = {{1, 2, 3}, {4, 5, 6}};
    auto result = 2 * mat;

    expectEq(result[0, 0], 2);
    expectEq(result[0, 1], 4);
    expectEq(result[0, 2], 6);
    expectEq(result[1, 0], 8);
    expectEq(result[1, 1], 10);
    expectEq(result[1, 2], 12);
  };

  "constexpr scalar multiplication"_test = [] {
    constexpr auto result = [] consteval {
      InlineDense mat = {{1, 2}, {3, 4}};
      return 5 * mat;
    }();

    static_assert(result[0, 0] == 5);
    static_assert(result[0, 1] == 10);
    static_assert(result[1, 0] == 15);
    static_assert(result[1, 1] == 20);
  };

  "scalar multiplication with identity"_test = [] {
    Identity<int, 3, 3> id;
    auto result = 7 * id;

    expectEq(result[0, 0], 7);
    expectEq(result[0, 1], 0);
    expectEq(result[0, 2], 0);
    expectEq(result[1, 0], 0);
    expectEq(result[1, 1], 7);
    expectEq(result[1, 2], 0);
    expectEq(result[2, 0], 0);
    expectEq(result[2, 1], 0);
    expectEq(result[2, 2], 7);
  };

  "dynamic extent matrix multiplication"_test = [] {
    // Create Dense matrices with dynamic extents (2x2)
    Extents<std::size_t, kDynamic, kDynamic> extents{2, 2};
    Dense<int, kDynamic, kDynamic> mat1{
        extents,
        typename LayoutLeft::Mapping<Extents<std::size_t, kDynamic, kDynamic>, std::size_t>{extents},
        std::vector<int>{1, 3, 2, 4}};  // [[1,2], [3,4]] in column-major
    Dense<int, kDynamic, kDynamic> mat2{
        extents,
        typename LayoutLeft::Mapping<Extents<std::size_t, kDynamic, kDynamic>, std::size_t>{extents},
        std::vector<int>{5, 7, 6, 8}};  // [[5,6], [7,8]] in column-major

    auto result = mat1 * mat2;

    // [[1,2], [3,4]] * [[5,6], [7,8]] = [[19,22], [43,50]]
    expectEq(result[0, 0], 19);
    expectEq(result[0, 1], 22);
    expectEq(result[1, 0], 43);
    expectEq(result[1, 1], 50);
  };

  "dynamic extent scalar multiplication"_test = [] {
    Extents<std::size_t, kDynamic, kDynamic> extents{2, 2};
    Dense<int, kDynamic, kDynamic> mat{
        extents,
        typename LayoutLeft::Mapping<Extents<std::size_t, kDynamic, kDynamic>, std::size_t>{extents},
        std::vector<int>{1, 3, 2, 4}};  // [[1,2], [3,4]] in column-major

    auto result = 3 * mat;

    expectEq(result[0, 0], 3);
    expectEq(result[0, 1], 6);
    expectEq(result[1, 0], 9);
    expectEq(result[1, 1], 12);
  };

  return TestRegistry::result();
}
