#include "to_string.h"

#include <string>

#include "mdarray.h"
#include "transpose.h"
#include "unit.h"
#include "vec.h"

using namespace tempura;

auto main() -> int {
  "2x2 box"_test = [] {
    InlineDense<int, 2, 2> m{{10, 20}, {30, 40}};
    expectEq(toString(m), std::string("⎡ 10 20 ⎤\n⎣ 30 40 ⎦"));
  };

  "single row renders as ASCII"_test = [] {
    InlineDense<int, 1, 3> m{{100, 200, 300}};
    expectEq(toString(m), std::string("[ 100 200 300 ]"));
  };

  "1x1 matrix"_test = [] {
    InlineDense<int, 1, 1> m{{5}};
    expectEq(toString(m), std::string("[ 5 ]"));
  };

  "integer columns right-align"_test = [] {
    InlineDense<int, 2, 2> m{{1, 100}, {2000, 3}};
    expectEq(toString(m), std::string("⎡    1 100 ⎤\n⎣ 2000   3 ⎦"));
  };

  "negative integers right-align"_test = [] {
    InlineDense<int, 2, 2> m{{-1, 100}, {-200, 3}};
    expectEq(toString(m), std::string("⎡   -1 100 ⎤\n⎣ -200   3 ⎦"));
  };

  "floats print at 4 decimals and align"_test = [] {
    InlineDense<double, 2, 2> m{{1.5, 100.25}, {2000.3, 3.1}};
    expectEq(toString(m), std::string("⎡    1.5000 100.2500 ⎤\n⎣ 2000.3000   3.1000 ⎦"));
  };

  "3x3 exercises the middle-row branch"_test = [] {
    InlineDense<int, 3, 3> m{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    expectEq(toString(m), std::string("⎡ 1 2 3 ⎤\n⎢ 4 5 6 ⎥\n⎣ 7 8 9 ⎦"));
  };

  "vector overload"_test = [] {
    InlineVec<int, 3> v{1, 2, 3};
    expectEq(toString(v), std::string("[ 1 2 3 ]"));
  };

  "renders a transposed view (proves it consumes any MatrixView)"_test = [] {
    InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    expectEq(toString(transposed(m)), std::string("⎡ 1 4 ⎤\n⎢ 2 5 ⎥\n⎣ 3 6 ⎦"));
  };

  "empty matrix (0 rows) renders without OOB"_test = [] {
    Dense<int, dyn, dyn> m(0, 3);
    expectEq(toString(m), std::string("[ ]"));
  };

  "float precision rounds to 4 decimals"_test = [] {
    InlineVec<double, 1> v{0.123456789};
    expectTrue(toString(v).find("0.1235") != std::string::npos);
  };

  return TestRegistry::result();
}
