#include "matrix3/block.h"

#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "block_two_blocks"_test = [] {
    // [2x3] + [2x2] = [2x5]
    InlineDense<double, 2, 3> left{
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
    };
    InlineDense<double, 2, 2> right{
        {7.0, 8.0},
        {9.0, 10.0},
    };

    BlockRow<InlineDense<double, 2, 3>, InlineDense<double, 2, 2>> block{left,
                                                                          right};

    // Verify extents
    static_assert(decltype(block)::kRows == 2);
    static_assert(decltype(block)::kCols == 5);
    static_assert(decltype(block)::rows() == 2);
    static_assert(decltype(block)::cols() == 5);

    // Access left block
    expectEq(1.0, block[0, 0]);
    expectEq(2.0, block[0, 1]);
    expectEq(3.0, block[0, 2]);
    expectEq(4.0, block[1, 0]);
    expectEq(5.0, block[1, 1]);
    expectEq(6.0, block[1, 2]);

    // Access right block
    expectEq(7.0, block[0, 3]);
    expectEq(8.0, block[0, 4]);
    expectEq(9.0, block[1, 3]);
    expectEq(10.0, block[1, 4]);
  };

  "block_three_blocks"_test = [] {
    // [2x2] + [2x3] + [2x1] = [2x6]
    InlineDense<int, 2, 2> a{
        {1, 2},
        {3, 4},
    };
    InlineDense<int, 2, 3> b{
        {5, 6, 7},
        {8, 9, 10},
    };
    InlineDense<int, 2, 1> c{
        {11},
        {12},
    };

    BlockRow<InlineDense<int, 2, 2>, InlineDense<int, 2, 3>,
             InlineDense<int, 2, 1>>
        block{a, b, c};

    static_assert(decltype(block)::kRows == 2);
    static_assert(decltype(block)::kCols == 6);

    // First block [0-1]
    expectEq(1, block[0, 0]);
    expectEq(2, block[0, 1]);
    expectEq(3, block[1, 0]);
    expectEq(4, block[1, 1]);

    // Second block [2-4]
    expectEq(5, block[0, 2]);
    expectEq(6, block[0, 3]);
    expectEq(7, block[0, 4]);
    expectEq(8, block[1, 2]);
    expectEq(9, block[1, 3]);
    expectEq(10, block[1, 4]);

    // Third block [5]
    expectEq(11, block[0, 5]);
    expectEq(12, block[1, 5]);
  };

  "block_deduction_guide"_test = [] {
    InlineDense<double, 3, 2> left{
        {1.0, 2.0},
        {3.0, 4.0},
        {5.0, 6.0},
    };
    InlineDense<double, 3, 1> right{
        {7.0},
        {8.0},
        {9.0},
    };

    BlockRow block{left, right};

    static_assert(decltype(block)::kRows == 3);
    static_assert(decltype(block)::kCols == 3);

    expectEq(1.0, block[0, 0]);
    expectEq(2.0, block[0, 1]);
    expectEq(7.0, block[0, 2]);
  };

  "block_const_access"_test = [] {
    const InlineDense<int, 2, 2> a{
        {1, 2},
        {3, 4},
    };
    const InlineDense<int, 2, 2> b{
        {5, 6},
        {7, 8},
    };

    const BlockRow block{a, b};

    expectEq(1, block[0, 0]);
    expectEq(2, block[0, 1]);
    expectEq(5, block[0, 2]);
    expectEq(6, block[0, 3]);
  };

  "block_mutable_access"_test = [] {
    InlineDense<double, 2, 2> left{
        {1.0, 2.0},
        {3.0, 4.0},
    };
    InlineDense<double, 2, 3> right{
        {5.0, 6.0, 7.0},
        {8.0, 9.0, 10.0},
    };

    BlockRow block{left, right};

    // Modify elements in left block
    block[0, 0] = 100.0;
    block[1, 1] = 200.0;

    // Modify elements in right block
    block[0, 2] = 300.0;
    block[1, 4] = 400.0;

    // Verify modifications to BlockRow's internal storage
    expectEq(100.0, block[0, 0]);
    expectEq(200.0, block[1, 1]);
    expectEq(300.0, block[0, 2]);
    expectEq(400.0, block[1, 4]);

    // Verify unmodified elements still accessible
    expectEq(2.0, block[0, 1]);
    expectEq(6.0, block[0, 3]);
  };

  "block_boundary_access"_test = [] {
    // Test access at block boundaries
    InlineDense<int, 3, 2> a{
        {1, 2},
        {3, 4},
        {5, 6},
    };
    InlineDense<int, 3, 2> b{
        {7, 8},
        {9, 10},
        {11, 12},
    };
    InlineDense<int, 3, 2> c{
        {13, 14},
        {15, 16},
        {17, 18},
    };

    BlockRow block{a, b, c};

    static_assert(decltype(block)::kCols == 6);

    // Last column of first block
    expectEq(2, block[0, 1]);
    // First column of second block
    expectEq(7, block[0, 2]);
    // Last column of second block
    expectEq(8, block[0, 3]);
    // First column of third block
    expectEq(13, block[0, 4]);
    // Last column of third block
    expectEq(14, block[0, 5]);
  };

  "block_single_block"_test = [] {
    // Edge case: BlockRow with a single block
    InlineDense<float, 2, 3> mat{
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f},
    };

    BlockRow<InlineDense<float, 2, 3>> block{mat};

    static_assert(decltype(block)::kRows == 2);
    static_assert(decltype(block)::kCols == 3);

    expectEq(1.0f, block[0, 0]);
    expectEq(3.0f, block[0, 2]);
    expectEq(6.0f, block[1, 2]);
  };

  "block_value_types"_test = [] {
    // Test with int
    InlineDense<int, 1, 2> int_a{{1, 2}};
    InlineDense<int, 1, 1> int_b{{3}};
    BlockRow int_block{int_a, int_b};
    expectEq(1, int_block[0, 0]);
    expectEq(3, int_block[0, 2]);

    // Test with float
    InlineDense<float, 1, 2> float_a{{1.5f, 2.5f}};
    InlineDense<float, 1, 1> float_b{{3.5f}};
    BlockRow float_block{float_a, float_b};
    expectEq(1.5f, float_block[0, 0]);
    expectEq(3.5f, float_block[0, 2]);

    // Test with double
    InlineDense<double, 1, 2> double_a{{1.25, 2.75}};
    InlineDense<double, 1, 1> double_b{{3.125}};
    BlockRow double_block{double_a, double_b};
    expectEq(1.25, double_block[0, 0]);
    expectEq(3.125, double_block[0, 2]);
  };

  "block_constexpr"_test = [] {
    constexpr auto test = []() {
      InlineDense<int, 2, 2> a{
          {1, 2},
          {3, 4},
      };
      InlineDense<int, 2, 1> b{
          {5},
          {6},
      };
      BlockRow block{a, b};
      return block[0, 0] + block[0, 1] + block[0, 2] + block[1, 0] +
             block[1, 1] + block[1, 2];
    };

    static_assert(test() == 1 + 2 + 5 + 3 + 4 + 6);  // 21
  };

  "block_large_composition"_test = [] {
    // Test with many blocks
    InlineDense<int, 2, 1> a{{1}, {2}};
    InlineDense<int, 2, 1> b{{3}, {4}};
    InlineDense<int, 2, 1> c{{5}, {6}};
    InlineDense<int, 2, 1> d{{7}, {8}};
    InlineDense<int, 2, 1> e{{9}, {10}};

    BlockRow block{a, b, c, d, e};

    static_assert(decltype(block)::kRows == 2);
    static_assert(decltype(block)::kCols == 5);

    expectEq(1, block[0, 0]);
    expectEq(3, block[0, 1]);
    expectEq(5, block[0, 2]);
    expectEq(7, block[0, 3]);
    expectEq(9, block[0, 4]);
    expectEq(2, block[1, 0]);
    expectEq(10, block[1, 4]);
  };

  // ===== BlockCol Tests =====

  "blockcol_two_blocks"_test = [] {
    // [2x3] / [1x3] = [3x3]
    InlineDense<double, 2, 3> top{
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
    };
    InlineDense<double, 1, 3> bottom{
        {7.0, 8.0, 9.0},
    };

    BlockCol<InlineDense<double, 2, 3>, InlineDense<double, 1, 3>> block{top,
                                                                          bottom};

    // Verify extents
    static_assert(decltype(block)::kRows == 3);
    static_assert(decltype(block)::kCols == 3);
    static_assert(decltype(block)::rows() == 3);
    static_assert(decltype(block)::cols() == 3);

    // Access top block
    expectEq(1.0, block[0, 0]);
    expectEq(2.0, block[0, 1]);
    expectEq(3.0, block[0, 2]);
    expectEq(4.0, block[1, 0]);
    expectEq(5.0, block[1, 1]);
    expectEq(6.0, block[1, 2]);

    // Access bottom block
    expectEq(7.0, block[2, 0]);
    expectEq(8.0, block[2, 1]);
    expectEq(9.0, block[2, 2]);
  };

  "blockcol_three_blocks"_test = [] {
    // [2x2] / [1x2] / [2x2] = [5x2]
    InlineDense<int, 2, 2> a{
        {1, 2},
        {3, 4},
    };
    InlineDense<int, 1, 2> b{
        {5, 6},
    };
    InlineDense<int, 2, 2> c{
        {7, 8},
        {9, 10},
    };

    BlockCol<InlineDense<int, 2, 2>, InlineDense<int, 1, 2>,
             InlineDense<int, 2, 2>>
        block{a, b, c};

    static_assert(decltype(block)::kRows == 5);
    static_assert(decltype(block)::kCols == 2);

    // First block [rows 0-1]
    expectEq(1, block[0, 0]);
    expectEq(2, block[0, 1]);
    expectEq(3, block[1, 0]);
    expectEq(4, block[1, 1]);

    // Second block [row 2]
    expectEq(5, block[2, 0]);
    expectEq(6, block[2, 1]);

    // Third block [rows 3-4]
    expectEq(7, block[3, 0]);
    expectEq(8, block[3, 1]);
    expectEq(9, block[4, 0]);
    expectEq(10, block[4, 1]);
  };

  "blockcol_deduction_guide"_test = [] {
    InlineDense<double, 2, 3> top{
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
    };
    InlineDense<double, 1, 3> bottom{
        {7.0, 8.0, 9.0},
    };

    BlockCol block{top, bottom};

    static_assert(decltype(block)::kRows == 3);
    static_assert(decltype(block)::kCols == 3);

    expectEq(1.0, block[0, 0]);
    expectEq(6.0, block[1, 2]);
    expectEq(7.0, block[2, 0]);
  };

  "blockcol_const_access"_test = [] {
    const InlineDense<int, 2, 2> a{
        {1, 2},
        {3, 4},
    };
    const InlineDense<int, 2, 2> b{
        {5, 6},
        {7, 8},
    };

    const BlockCol block{a, b};

    expectEq(1, block[0, 0]);
    expectEq(2, block[0, 1]);
    expectEq(5, block[2, 0]);
    expectEq(6, block[2, 1]);
  };

  "blockcol_mutable_access"_test = [] {
    InlineDense<double, 2, 2> top{
        {1.0, 2.0},
        {3.0, 4.0},
    };
    InlineDense<double, 1, 2> bottom{
        {5.0, 6.0},
    };

    BlockCol block{top, bottom};

    // Modify elements in top block
    block[0, 0] = 100.0;
    block[1, 1] = 200.0;

    // Modify elements in bottom block
    block[2, 0] = 300.0;
    block[2, 1] = 400.0;

    // Verify modifications
    expectEq(100.0, block[0, 0]);
    expectEq(200.0, block[1, 1]);
    expectEq(300.0, block[2, 0]);
    expectEq(400.0, block[2, 1]);

    // Verify unmodified elements
    expectEq(2.0, block[0, 1]);
    expectEq(3.0, block[1, 0]);
  };

  "blockcol_boundary_access"_test = [] {
    // Test access at block boundaries
    InlineDense<int, 2, 3> a{
        {1, 2, 3},
        {4, 5, 6},
    };
    InlineDense<int, 2, 3> b{
        {7, 8, 9},
        {10, 11, 12},
    };
    InlineDense<int, 2, 3> c{
        {13, 14, 15},
        {16, 17, 18},
    };

    BlockCol block{a, b, c};

    static_assert(decltype(block)::kRows == 6);

    // Last row of first block
    expectEq(4, block[1, 0]);
    // First row of second block
    expectEq(7, block[2, 0]);
    // Last row of second block
    expectEq(10, block[3, 0]);
    // First row of third block
    expectEq(13, block[4, 0]);
    // Last row of third block
    expectEq(16, block[5, 0]);
  };

  "blockcol_single_block"_test = [] {
    // Edge case: BlockCol with a single block
    InlineDense<float, 2, 3> mat{
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f},
    };

    BlockCol<InlineDense<float, 2, 3>> block{mat};

    static_assert(decltype(block)::kRows == 2);
    static_assert(decltype(block)::kCols == 3);

    expectEq(1.0f, block[0, 0]);
    expectEq(3.0f, block[0, 2]);
    expectEq(6.0f, block[1, 2]);
  };

  "blockcol_value_types"_test = [] {
    // Test with int
    InlineDense<int, 2, 1> int_a{{1}, {2}};
    InlineDense<int, 1, 1> int_b{{3}};
    BlockCol int_block{int_a, int_b};
    expectEq(1, int_block[0, 0]);
    expectEq(3, int_block[2, 0]);

    // Test with float
    InlineDense<float, 2, 1> float_a{{1.5f}, {2.5f}};
    InlineDense<float, 1, 1> float_b{{3.5f}};
    BlockCol float_block{float_a, float_b};
    expectEq(1.5f, float_block[0, 0]);
    expectEq(3.5f, float_block[2, 0]);

    // Test with double
    InlineDense<double, 2, 1> double_a{{1.25}, {2.75}};
    InlineDense<double, 1, 1> double_b{{3.125}};
    BlockCol double_block{double_a, double_b};
    expectEq(1.25, double_block[0, 0]);
    expectEq(3.125, double_block[2, 0]);
  };

  "blockcol_constexpr"_test = [] {
    constexpr auto test = []() {
      InlineDense<int, 2, 2> a{
          {1, 2},
          {3, 4},
      };
      InlineDense<int, 1, 2> b{
          {5, 6},
      };
      BlockCol block{a, b};
      return block[0, 0] + block[0, 1] + block[1, 0] + block[1, 1] +
             block[2, 0] + block[2, 1];
    };

    static_assert(test() == 1 + 2 + 3 + 4 + 5 + 6);  // 21
  };

  "blockcol_large_composition"_test = [] {
    // Test with many blocks
    InlineDense<int, 1, 2> a{{1, 2}};
    InlineDense<int, 1, 2> b{{3, 4}};
    InlineDense<int, 1, 2> c{{5, 6}};
    InlineDense<int, 1, 2> d{{7, 8}};
    InlineDense<int, 1, 2> e{{9, 10}};

    BlockCol block{a, b, c, d, e};

    static_assert(decltype(block)::kRows == 5);
    static_assert(decltype(block)::kCols == 2);

    expectEq(1, block[0, 0]);
    expectEq(2, block[0, 1]);
    expectEq(3, block[1, 0]);
    expectEq(4, block[1, 1]);
    expectEq(5, block[2, 0]);
    expectEq(6, block[2, 1]);
    expectEq(7, block[3, 0]);
    expectEq(8, block[3, 1]);
    expectEq(9, block[4, 0]);
    expectEq(10, block[4, 1]);
  };

  return TestRegistry::result();
}
