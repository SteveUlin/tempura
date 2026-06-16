#include "vec.h"

#include <concepts>
#include <cstddef>

#include "matrix.h"
#include "unit.h"

using namespace tempura;

static_assert(std::same_as<decltype(InlineMatrix<double, 2, 3>{} * InlineVec<double, 3>{}),
                           Vec<double, 2>>);
static_assert(std::same_as<decltype(Matrix<double>{} * Vec<double>{}), Vec<double>>);

constexpr auto constexprVecOps() -> bool {
  InlineVec<double, 3> x;
  x[0] = 1.0; x[1] = 2.0; x[2] = 3.0;
  InlineVec<double, 3> y;
  y[0] = 4.0; y[1] = 5.0; y[2] = 6.0;
  InlineMatrix<double, 2, 3> a;  // selects rows 0 and 1 of x
  a[0, 0] = 1.0; a[1, 1] = 1.0;
  InlineVec<double, 2> z;  // stack destination — no heap, runs at compile time
  multiply(a, x, z);
  return dot(x, y) == 32.0 && z[0] == 1.0 && z[1] == 2.0;  // dot = 4+10+18
}
static_assert(constexprVecOps());

auto main() -> int {
  "construction and single-index subscript"_test = [] {
    Vec<double> x(3);
    expectEq(x.rank(), 1u);
    expectEq(x.size(), 3u);
    x[0] = 1.0; x[1] = 2.0; x[2] = 3.0;
    expectEq(x[2], 3.0);

    InlineVec<double, 3> s;
    s[1] = 9.0;
    expectEq(s[1], 9.0);
  };

  "dot product"_test = [] {
    Vec<double> x(3);
    x[0] = 1.0; x[1] = 2.0; x[2] = 3.0;
    Vec<double> y(3);
    y[0] = 4.0; y[1] = 5.0; y[2] = 6.0;
    expectEq(dot(x, y), 32.0);
    expectEq(dot(x, x), 14.0);  // ‖x‖²
  };

  "matrix-vector product, dynamic"_test = [] {
    // [1 2 3; 4 5 6] · [1; 1; 1] = [6; 15]
    Matrix<double> a(2, 3);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    Vec<double> x(3);
    x[0] = 1.0; x[1] = 1.0; x[2] = 1.0;
    auto y = a * x;
    expectEq(y.rank(), 1u);
    expectEq(y.extent(0), 2u);
    expectEq(y[0], 6.0);
    expectEq(y[1], 15.0);
  };

  "matrix-vector product, inline result type is a static Vec"_test = [] {
    InlineMatrix<double, 2, 3> a;
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    InlineVec<double, 3> x;
    x[0] = 1.0; x[1] = 1.0; x[2] = 1.0;
    auto y = multiply(a, x);
    static_assert(std::same_as<decltype(y), Vec<double, 2>>);
    expectEq(y[0], 6.0);
    expectEq(y[1], 15.0);
  };

  "destination matvec writes into a caller-owned vector"_test = [] {
    Matrix<double> a(2, 2);
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    Vec<double> x(2);
    x[0] = 1.0; x[1] = 1.0;
    Vec<double> y(2);
    auto& r = multiply(a, x, y);
    expectTrue(&r == &y);
    expectEq(y[0], 3.0);
    expectEq(y[1], 7.0);
  };

  "euclidean norm"_test = [] {
    Vec<double> x(2);
    x[0] = 3.0; x[1] = 4.0;
    expectEq(norm2(x), 5.0);  // √(9+16) = 5, exact in IEEE double
  };

  return TestRegistry::result();
}
