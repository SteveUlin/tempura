#include "matrix3/addition.h"
#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "in-place addition with +="_test = [] {
    InlineDense mat1 = {{1, 2}, {3, 4}};
    InlineDense mat2 = {{5, 6}, {7, 8}};

    mat1 += mat2;

    expectEq(mat1[0, 0], 6);
    expectEq(mat1[0, 1], 8);
    expectEq(mat1[1, 0], 10);
    expectEq(mat1[1, 1], 12);
  };

  "in-place subtraction with -="_test = [] {
    InlineDense mat1 = {{10, 20}, {30, 40}};
    InlineDense mat2 = {{1, 2}, {3, 4}};

    mat1 -= mat2;

    expectEq(mat1[0, 0], 9);
    expectEq(mat1[0, 1], 18);
    expectEq(mat1[1, 0], 27);
    expectEq(mat1[1, 1], 36);
  };

  "explicit output type with add<Out>"_test = [] {
    InlineDense<int, 2, 2> mat1 = {{1, 2}, {3, 4}};
    InlineDense<int, 2, 2> mat2 = {{5, 6}, {7, 8}};

    auto result = add<InlineDense<int, 2, 2>>(mat1, mat2);

    expectEq(result[0, 0], 6);
    expectEq(result[0, 1], 8);
    expectEq(result[1, 0], 10);
    expectEq(result[1, 1], 12);
  };

  "explicit output type with subtract<Out>"_test = [] {
    InlineDense<int, 2, 2> mat1 = {{10, 20}, {30, 40}};
    InlineDense<int, 2, 2> mat2 = {{1, 2}, {3, 4}};

    auto result = subtract<InlineDense<int, 2, 2>>(mat1, mat2);

    expectEq(result[0, 0], 9);
    expectEq(result[0, 1], 18);
    expectEq(result[1, 0], 27);
    expectEq(result[1, 1], 36);
  };

  "auto-inference addition with +"_test = [] {
    InlineDense mat1 = {{1, 2}, {3, 4}};
    InlineDense mat2 = {{5, 6}, {7, 8}};

    auto result = mat1 + mat2;

    expectEq(result[0, 0], 6);
    expectEq(result[0, 1], 8);
    expectEq(result[1, 0], 10);
    expectEq(result[1, 1], 12);
  };

  "auto-inference subtraction with -"_test = [] {
    InlineDense mat1 = {{10, 20}, {30, 40}};
    InlineDense mat2 = {{1, 2}, {3, 4}};

    auto result = mat1 - mat2;

    expectEq(result[0, 0], 9);
    expectEq(result[0, 1], 18);
    expectEq(result[1, 0], 27);
    expectEq(result[1, 1], 36);
  };

  "type promotion int + double"_test = [] {
    InlineDense<int, 2, 2> mat1 = {{1, 2}, {3, 4}};
    InlineDense<double, 2, 2> mat2 = {{1.5, 2.5}, {3.5, 4.5}};

    auto result = mat1 + mat2;

    // Result should be double due to type promotion
    expectNear(result[0, 0], 2.5, 1e-10);
    expectNear(result[0, 1], 4.5, 1e-10);
    expectNear(result[1, 0], 6.5, 1e-10);
    expectNear(result[1, 1], 8.5, 1e-10);
  };

  "type promotion int - double"_test = [] {
    InlineDense<int, 2, 2> mat1 = {{10, 20}, {30, 40}};
    InlineDense<double, 2, 2> mat2 = {{1.5, 2.5}, {3.5, 4.5}};

    auto result = mat1 - mat2;

    // Result should be double due to type promotion
    expectNear(result[0, 0], 8.5, 1e-10);
    expectNear(result[0, 1], 17.5, 1e-10);
    expectNear(result[1, 0], 26.5, 1e-10);
    expectNear(result[1, 1], 35.5, 1e-10);
  };

  "3x3 matrix addition"_test = [] {
    InlineDense mat1 = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    InlineDense mat2 = {{9, 8, 7}, {6, 5, 4}, {3, 2, 1}};

    auto result = mat1 + mat2;

    expectEq(result[0, 0], 10);
    expectEq(result[0, 1], 10);
    expectEq(result[0, 2], 10);
    expectEq(result[1, 0], 10);
    expectEq(result[1, 1], 10);
    expectEq(result[1, 2], 10);
    expectEq(result[2, 0], 10);
    expectEq(result[2, 1], 10);
    expectEq(result[2, 2], 10);
  };

  "1x3 row vector addition"_test = [] {
    InlineDense<int, 1, 3> mat1 = {{1, 2, 3}};
    InlineDense<int, 1, 3> mat2 = {{4, 5, 6}};

    auto result = mat1 + mat2;

    expectEq(result[0, 0], 5);
    expectEq(result[0, 1], 7);
    expectEq(result[0, 2], 9);
  };

  "3x1 column vector subtraction"_test = [] {
    InlineDense<int, 3, 1> mat1 = {{10}, {20}, {30}};
    InlineDense<int, 3, 1> mat2 = {{1}, {2}, {3}};

    auto result = mat1 - mat2;

    expectEq(result[0, 0], 9);
    expectEq(result[1, 0], 18);
    expectEq(result[2, 0], 27);
  };

  "constexpr addition"_test = [] {
    constexpr auto result = [] consteval {
      InlineDense mat1 = {{1, 2}, {3, 4}};
      InlineDense mat2 = {{5, 6}, {7, 8}};
      return mat1 + mat2;
    }();

    static_assert(result[0, 0] == 6);
    static_assert(result[0, 1] == 8);
    static_assert(result[1, 0] == 10);
    static_assert(result[1, 1] == 12);
  };

  "constexpr subtraction"_test = [] {
    constexpr auto result = [] consteval {
      InlineDense mat1 = {{10, 20}, {30, 40}};
      InlineDense mat2 = {{1, 2}, {3, 4}};
      return mat1 - mat2;
    }();

    static_assert(result[0, 0] == 9);
    static_assert(result[0, 1] == 18);
    static_assert(result[1, 0] == 27);
    static_assert(result[1, 1] == 36);
  };

  "constexpr in-place addition"_test = [] {
    constexpr auto result = [] consteval {
      InlineDense mat1 = {{1, 2}, {3, 4}};
      InlineDense mat2 = {{5, 6}, {7, 8}};
      mat1 += mat2;
      return mat1;
    }();

    static_assert(result[0, 0] == 6);
    static_assert(result[0, 1] == 8);
    static_assert(result[1, 0] == 10);
    static_assert(result[1, 1] == 12);
  };

  "dynamic extent addition"_test = [] {
    // Create Dense matrices with dynamic extents
    Extents<std::size_t, kDynamic, kDynamic> extents{2, 2};
    Dense<int, kDynamic, kDynamic> mat1{
        extents,
        typename LayoutLeft::Mapping<Extents<std::size_t, kDynamic, kDynamic>, std::size_t>{extents},
        std::vector<int>{1, 3, 2, 4}};
    Dense<int, kDynamic, kDynamic> mat2{
        extents,
        typename LayoutLeft::Mapping<Extents<std::size_t, kDynamic, kDynamic>, std::size_t>{extents},
        std::vector<int>{5, 7, 6, 8}};

    auto result = mat1 + mat2;

    expectEq(result[0, 0], 6);
    expectEq(result[0, 1], 8);
    expectEq(result[1, 0], 10);
    expectEq(result[1, 1], 12);
  };

  "dynamic extent subtraction"_test = [] {
    Extents<std::size_t, kDynamic, kDynamic> extents{2, 2};
    Dense<int, kDynamic, kDynamic> mat1{
        extents,
        typename LayoutLeft::Mapping<Extents<std::size_t, kDynamic, kDynamic>, std::size_t>{extents},
        std::vector<int>{10, 30, 20, 40}};
    Dense<int, kDynamic, kDynamic> mat2{
        extents,
        typename LayoutLeft::Mapping<Extents<std::size_t, kDynamic, kDynamic>, std::size_t>{extents},
        std::vector<int>{1, 3, 2, 4}};

    auto result = mat1 - mat2;

    expectEq(result[0, 0], 9);
    expectEq(result[0, 1], 18);
    expectEq(result[1, 0], 27);
    expectEq(result[1, 1], 36);
  };

  return TestRegistry::result();
}
