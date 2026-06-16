#include "matrix.h"

#include <concepts>
#include <cstddef>

#include "unit.h"

using namespace tempura;

// Value forms always return a heap Matrix; the merged static extents survive for
// compile-time shape checks, but storage is never inferred onto the stack (result
// size isn't bounded by operand size). Element type is the common type.
static_assert(std::same_as<decltype(InlineMatrix<double, 2, 3>{} * InlineMatrix<double, 3, 4>{}),
                           Matrix<double, 2, 4>>);
static_assert(std::same_as<decltype(Matrix<double, 2, 3>{} * Matrix<double, 3, 4>{}),
                           Matrix<double, 2, 4>>);
static_assert(std::same_as<decltype(Matrix<double>{} * Matrix<double>{}), Matrix<double>>);
static_assert(std::same_as<decltype(InlineMatrix<double, 2, 2>{} + InlineMatrix<double, 2, 2>{}),
                           Matrix<double, 2, 2>>);
static_assert(std::same_as<decltype(InlineMatrix<double, 2, 2>{} + Matrix<double>{}),
                           Matrix<double, 2, 2>>);
static_assert(std::same_as<decltype(Matrix<double>{} + Matrix<double>{}), Matrix<double>>);

// Matrix multiply in a constant expression. The heap result is a transient
// allocation, valid in constexpr since C++20 made std::vector constexpr.
constexpr auto constexprProduct() -> bool {
  InlineMatrix<double, 2, 2> a;
  a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
  InlineMatrix<double, 2, 2> b;
  b[0, 0] = 5.0; b[0, 1] = 6.0; b[1, 0] = 7.0; b[1, 1] = 8.0;
  auto c = a * b;  // [1 2;3 4]·[5 6;7 8] = [19 22; 43 50]
  return c[0, 0] == 19.0 && c[0, 1] == 22.0 && c[1, 0] == 43.0 && c[1, 1] == 50.0;
}
static_assert(constexprProduct());

// The destination form is how you opt into stack storage: declare an InlineMatrix
// and fill it. No allocation, fully constexpr.
constexpr auto constexprDestination() -> bool {
  InlineMatrix<double, 2, 2> a;
  a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
  InlineMatrix<double, 2, 2> b;
  b[0, 0] = 5.0; b[0, 1] = 6.0; b[1, 0] = 7.0; b[1, 1] = 8.0;
  InlineMatrix<double, 2, 2> c;
  multiply(a, b, c);
  return c[0, 0] == 19.0 && c[0, 1] == 22.0 && c[1, 0] == 43.0 && c[1, 1] == 50.0;
}
static_assert(constexprDestination());

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

  "inline operands, heap result with preserved static shape"_test = [] {
    InlineMatrix<double, 2, 2> a;
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    InlineMatrix<double, 2, 2> b;
    b[0, 0] = 5.0; b[0, 1] = 6.0; b[1, 0] = 7.0; b[1, 1] = 8.0;
    auto c = a + b;
    static_assert(std::same_as<decltype(c), Matrix<double, 2, 2>>);
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

  "inline product keeps a static shape but lands on the heap"_test = [] {
    InlineMatrix<double, 2, 3> a;
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    InlineMatrix<double, 3, 2> b;
    v = 7.0;
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 2; ++j) b[i, j] = v++;
    auto c = a * b;
    static_assert(std::same_as<decltype(c), Matrix<double, 2, 2>>);
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

  "destination add fills a preallocated result and returns it"_test = [] {
    Matrix<double> a(2, 2);
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    Matrix<double> b(2, 2);
    b[0, 0] = 10.0; b[0, 1] = 20.0; b[1, 0] = 30.0; b[1, 1] = 40.0;
    Matrix<double> dst(2, 2);
    auto& r = add(a, b, dst);
    expectTrue(&r == &dst);
    expectEq(dst[0, 0], 11.0);
    expectEq(dst[1, 1], 44.0);
  };

  "destination multiply into a stack InlineMatrix is the stack opt-in"_test = [] {
    InlineMatrix<double, 2, 3> a;
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    InlineMatrix<double, 3, 2> b;
    v = 7.0;
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 2; ++j) b[i, j] = v++;
    InlineMatrix<double, 2, 2> c;  // caller owns stack storage; no heap result
    multiply(a, b, c);
    expectEq(c[0, 0], 58.0);
    expectEq(c[1, 1], 154.0);
  };

  "multiplyAdd accumulates A·B into the destination"_test = [] {
    Matrix<double> a(2, 2);
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    Matrix<double> b(2, 2);
    b[0, 0] = 5.0; b[0, 1] = 6.0; b[1, 0] = 7.0; b[1, 1] = 8.0;
    Matrix<double> dst(2, 2);
    dst[0, 0] = 1.0; dst[0, 1] = 1.0; dst[1, 0] = 1.0; dst[1, 1] = 1.0;
    multiplyAdd(a, b, dst);  // A·B = [19 22; 43 50], plus the ones already in dst
    expectEq(dst[0, 0], 20.0);
    expectEq(dst[1, 1], 51.0);
  };

  "operator+= mutates the left operand in place"_test = [] {
    Matrix<double> a(2, 2);
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    Matrix<double> b(2, 2);
    b[0, 0] = 10.0; b[0, 1] = 20.0; b[1, 0] = 30.0; b[1, 1] = 40.0;
    auto& r = (a += b);
    expectTrue(&r == &a);
    expectEq(a[0, 0], 11.0);
    expectEq(a[1, 1], 44.0);
  };

  "operator*= with a square right operand"_test = [] {
    Matrix<double> a(2, 2);
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    Matrix<double> b(2, 2);
    b[0, 0] = 5.0; b[0, 1] = 6.0; b[1, 0] = 7.0; b[1, 1] = 8.0;
    a *= b;  // [1 2;3 4]·[5 6;7 8] = [19 22; 43 50]
    expectEq(a[0, 0], 19.0);
    expectEq(a[0, 1], 22.0);
    expectEq(a[1, 0], 43.0);
    expectEq(a[1, 1], 50.0);
  };

  "operator*= on an InlineMatrix keeps stack storage"_test = [] {
    InlineMatrix<double, 2, 2> a;
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    InlineMatrix<double, 2, 2> id;  // identity
    id[0, 0] = 1.0; id[1, 1] = 1.0;
    a *= id;  // copy-back path preserves a's type; product is unchanged a
    static_assert(std::same_as<decltype(a), InlineMatrix<double, 2, 2>>);
    expectEq(a[0, 0], 1.0);
    expectEq(a[1, 1], 4.0);
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
