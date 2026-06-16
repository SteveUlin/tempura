#include "matrix.h"

#include <concepts>
#include <cstddef>

#include "unit.h"

using namespace tempura;

// Output type composes the two operands along independent axes: extents merge
// (static dim wins, checked), storage follows the operands (stack only if BOTH
// are stack-backed), element type is the common type.
static_assert(std::same_as<decltype(InlineMatrix<double, 2, 2>{} + InlineMatrix<double, 2, 2>{}),
                           InlineMatrix<double, 2, 2>>);
static_assert(std::same_as<decltype(Matrix<double>{} + Matrix<double>{}), Matrix<double>>);
static_assert(std::same_as<decltype(InlineMatrix<double, 2, 3>{} * InlineMatrix<double, 3, 4>{}),
                           InlineMatrix<double, 2, 4>>);
static_assert(std::same_as<decltype(Matrix<double>{} * Matrix<double>{}), Matrix<double>>);
// A static shape on the heap stays on the heap — shape is checked, storage isn't
// silently moved to the stack. This is the large-matrix case.
static_assert(std::same_as<decltype(Matrix<double, 2, 3>{} * Matrix<double, 3, 4>{}),
                           Matrix<double, 2, 4>>);
// Mixing a stack operand with a heap one recovers the static shape but stays on
// the heap (any heap operand → heap result).
static_assert(std::same_as<decltype(InlineMatrix<double, 2, 2>{} + Matrix<double>{}),
                           Matrix<double, 2, 2>>);

// Matrix multiply runs in a constant expression on stack storage — no heap.
constexpr auto constexprProduct() -> bool {
  InlineMatrix<double, 2, 2> a;
  a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
  InlineMatrix<double, 2, 2> b;
  b[0, 0] = 5.0; b[0, 1] = 6.0; b[1, 0] = 7.0; b[1, 1] = 8.0;
  auto c = a * b;  // [1 2;3 4]·[5 6;7 8] = [19 22; 43 50]
  return c[0, 0] == 19.0 && c[0, 1] == 22.0 && c[1, 0] == 43.0 && c[1, 1] == 50.0;
}
static_assert(constexprProduct());

auto main() -> int {
  "dynamic addition"_test = [] {
    Matrix<double> a(2, 2);
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    Matrix<double> b(2, 2);
    b[0, 0] = 10.0; b[0, 1] = 20.0; b[1, 0] = 30.0; b[1, 1] = 40.0;
    auto c = a + b;
    expectEq(c.extent(0), 2u);
    expectEq(c.extent(1), 2u);
    expectEq(c[0, 0], 11.0);
    expectEq(c[1, 1], 44.0);
  };

  "inline addition stays on the stack"_test = [] {
    InlineMatrix<double, 2, 2> a;
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    InlineMatrix<double, 2, 2> b;
    b[0, 0] = 5.0; b[0, 1] = 6.0; b[1, 0] = 7.0; b[1, 1] = 8.0;
    auto c = a + b;
    expectEq(c[0, 0], 6.0);
    expectEq(c[1, 1], 12.0);
  };

  "static-shape heap matrix: shape known, storage on the heap"_test = [] {
    Matrix<double, 2, 3> a;  // static extents, std::vector storage
    static_assert(decltype(a)::staticExtent(0) == 2);
    static_assert(decltype(a)::staticExtent(1) == 3);
    static_assert(decltype(a)::rankDynamic() == 0);
    expectEq(a.size(), 6u);
    a[1, 2] = 7.0;
    expectEq(a[1, 2], 7.0);

    Matrix<double, 2, 3> b;
    b[1, 2] = 5.0;
    auto c = a + b;  // result keeps the static shape, stays on the heap
    static_assert(std::same_as<decltype(c), Matrix<double, 2, 3>>);
    expectEq(c[1, 2], 12.0);
  };

  "dynamic product against a hand-computed result"_test = [] {
    // [1 2 3; 4 5 6] · [7 8; 9 10; 11 12] = [58 64; 139 154]
    Matrix<double> a(2, 3);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    Matrix<double> b(3, 2);
    v = 7.0;
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 2; ++j) b[i, j] = v++;
    auto c = a * b;
    expectEq(c.extent(0), 2u);
    expectEq(c.extent(1), 2u);
    expectEq(c[0, 0], 58.0);
    expectEq(c[0, 1], 64.0);
    expectEq(c[1, 0], 139.0);
    expectEq(c[1, 1], 154.0);
  };

  "inline product is statically sized"_test = [] {
    InlineMatrix<double, 2, 3> a;
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    InlineMatrix<double, 3, 2> b;
    v = 7.0;
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 2; ++j) b[i, j] = v++;
    auto c = a * b;
    static_assert(std::same_as<decltype(c), InlineMatrix<double, 2, 2>>);
    expectEq(c[0, 0], 58.0);
    expectEq(c[1, 1], 154.0);
  };

  "named functions and operators agree"_test = [] {
    Matrix<double> a(2, 2);
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    Matrix<double> b(2, 2);
    b[0, 0] = 5.0; b[0, 1] = 6.0; b[1, 0] = 7.0; b[1, 1] = 8.0;
    expectRangeEq((a + b).container(), add(a, b).container());
    expectRangeEq((a * b).container(), multiply(a, b).container());
  };

  "identity leaves the operand unchanged"_test = [] {
    Matrix<double> a(2, 2);
    a[0, 0] = 3.0; a[0, 1] = 5.0; a[1, 0] = 7.0; a[1, 1] = 9.0;
    Matrix<double> id(2, 2);
    id[0, 0] = 1.0; id[1, 1] = 1.0;
    auto c = a * id;
    expectEq(c[0, 0], 3.0);
    expectEq(c[0, 1], 5.0);
    expectEq(c[1, 0], 7.0);
    expectEq(c[1, 1], 9.0);
  };

  return TestRegistry::result();
}
