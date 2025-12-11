#include "matrix3/matrix.h"
#include "matrix3/to_string.h"
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

  return TestRegistry::result();
}
