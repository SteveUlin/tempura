#include "matrix3/matrix.h"
#include "matrix3/submatrix.h"
#include "matrix3/to_string.h"
#include "matrix3/transpose.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "Integer matrix formatting"_test = [] {
    Dense mat{{1, 2, 3}, {4, 5, 6}};
    auto str = toString(mat);
    // Check for brackets and content
    expectTrue(str.find("⎡") != std::string::npos);
    expectTrue(str.find("⎣") != std::string::npos);
    expectTrue(str.find("1") != std::string::npos);
    expectTrue(str.find("6") != std::string::npos);
  };

  "Floating point matrix formatting"_test = [] {
    Dense mat{{1.5, 2.7}, {3.1, 4.9}};
    auto str = toString(mat);
    // Float formatting uses .4f precision
    expectTrue(str.find("1.5000") != std::string::npos);
    expectTrue(str.find("2.7000") != std::string::npos);
  };

  "Single row matrix"_test = [] {
    Dense mat{{1, 2, 3, 4}};
    auto str = toString(mat);
    // Single row uses square brackets
    expectTrue(str.find("[") != std::string::npos);
    expectTrue(str.find("]") != std::string::npos);
    // Should not have multi-row brackets
    expectTrue(str.find("⎡") == std::string::npos);
  };

  "Single column matrix"_test = [] {
    Dense<int, 3, 1> mat{};
    mat[0, 0] = 1;
    mat[1, 0] = 2;
    mat[2, 0] = 3;
    auto str = toString(mat);
    // Multi-row uses Unicode brackets
    expectTrue(str.find("⎡") != std::string::npos);
    expectTrue(str.find("⎢") != std::string::npos);
    expectTrue(str.find("⎣") != std::string::npos);
  };

  "Column alignment with varying widths"_test = [] {
    Dense mat{{1, 100}, {2000, 3}};
    auto str = toString(mat);
    // All numbers should be present
    expectTrue(str.find("1") != std::string::npos);
    expectTrue(str.find("100") != std::string::npos);
    expectTrue(str.find("2000") != std::string::npos);
    expectTrue(str.find("3") != std::string::npos);
  };

  "2x2 matrix"_test = [] {
    InlineDense mat{{10, 20}, {30, 40}};
    auto str = toString(mat);
    expectTrue(str.find("10") != std::string::npos);
    expectTrue(str.find("20") != std::string::npos);
    expectTrue(str.find("30") != std::string::npos);
    expectTrue(str.find("40") != std::string::npos);
    expectTrue(str.find("⎡") != std::string::npos);
    expectTrue(str.find("⎣") != std::string::npos);
  };

  "3x3 matrix with middle rows"_test = [] {
    Dense mat{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    auto str = toString(mat);
    // Should have top, middle, and bottom row brackets
    expectTrue(str.find("⎡") != std::string::npos);
    expectTrue(str.find("⎢") != std::string::npos);
    expectTrue(str.find("⎣") != std::string::npos);
  };

  "Float precision test"_test = [] {
    Dense mat{{0.123456789, 1.987654321}};
    auto str = toString(mat);
    // Should round to 4 decimal places
    expectTrue(str.find("0.1235") != std::string::npos);
    expectTrue(str.find("1.9877") != std::string::npos);
  };

  "Negative numbers"_test = [] {
    Dense mat{{-1, 2}, {3, -4}};
    auto str = toString(mat);
    expectTrue(str.find("-1") != std::string::npos);
    expectTrue(str.find("-4") != std::string::npos);
  };

  "Transpose view formatting"_test = [] {
    Dense mat{{1, 2, 3}, {4, 5, 6}};
    auto transposed = Transpose{mat};
    auto str = toString(transposed);
    // Transposed should be 3x2 instead of 2x3
    // First column of transposed = first row of original: [1, 4]
    expectTrue(str.find("1") != std::string::npos);
    expectTrue(str.find("4") != std::string::npos);
    expectTrue(str.find("6") != std::string::npos);
    expectTrue(str.find("⎡") != std::string::npos);
    expectTrue(str.find("⎣") != std::string::npos);
  };

  "Submatrix view formatting"_test = [] {
    Dense mat{{20, 21, 22, 23}, {30, 31, 32, 33}, {40, 41, 42, 43}};
    auto submat = Submatrix{mat, 1, 1, 2, 2};  // Extract [[31,32],[41,42]]
    auto str = toString(submat);
    // Should show the 2x2 submatrix
    expectTrue(str.find("31") != std::string::npos);
    expectTrue(str.find("32") != std::string::npos);
    expectTrue(str.find("41") != std::string::npos);
    expectTrue(str.find("42") != std::string::npos);
    // Should not show elements outside the submatrix
    expectTrue(str.find("20") == std::string::npos);
    expectTrue(str.find("43") == std::string::npos);
  };

  "Exact output: 2x2 integer matrix"_test = [] {
    Dense mat{{10, 20}, {30, 40}};
    auto str = toString(mat);
    expectEq(str, "⎡ 10 20 ⎤\n⎣ 30 40 ⎦");
  };

  "Exact output: 2-row matrix (edge case)"_test = [] {
    // 2-row matrices are important: middle row loop doesn't execute
    Dense mat{{1, 2, 3}, {4, 5, 6}};
    auto str = toString(mat);
    expectEq(str, "⎡ 1 2 3 ⎤\n⎣ 4 5 6 ⎦");
  };

  "Exact output: single row matrix"_test = [] {
    Dense mat{{100, 200, 300}};
    auto str = toString(mat);
    expectEq(str, "[ 100 200 300 ]");
  };

  "Column alignment verification"_test = [] {
    Dense mat{{1, 100}, {2000, 3}};
    auto str = toString(mat);
    // All numbers should align: "   1" and "2000" both width 4, "100" and "  3" both width 3
    expectEq(str, "⎡    1 100 ⎤\n⎣ 2000   3 ⎦");
  };

  "Column alignment with floats"_test = [] {
    Dense mat{{1.5, 100.25}, {2000.3, 3.1}};
    auto str = toString(mat);
    // Floats use .4f: "   1.5000" and "2000.3000" width 9, "100.2500" and "  3.1000" width 8
    expectEq(str, "⎡    1.5000 100.2500 ⎤\n⎣ 2000.3000   3.1000 ⎦");
  };

  "Exact output: 3x3 with middle rows"_test = [] {
    Dense mat{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    auto str = toString(mat);
    expectEq(str, "⎡ 1 2 3 ⎤\n⎢ 4 5 6 ⎥\n⎣ 7 8 9 ⎦");
  };

  "Exact output: negative numbers with alignment"_test = [] {
    Dense mat{{-1, 100}, {-200, 3}};
    auto str = toString(mat);
    // "-1" and "-200" both right-aligned in width 4 column
    expectEq(str, "⎡   -1 100 ⎤\n⎣ -200   3 ⎦");
  };

  return TestRegistry::result();
}
