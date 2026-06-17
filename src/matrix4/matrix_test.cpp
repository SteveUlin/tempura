#include "matrix.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <mdspan>
#include <utility>
#include <vector>

#include "unit.h"

using namespace tempura;

using Dext2 = std::dextents<std::size_t, 2>;
using Dext3 = std::dextents<std::size_t, 3>;

// The Dense owner is constexpr-usable: build, index two ways, query shape.
constexpr auto denseRoundTrip() -> bool {
  Dense<double, Dext2> a(2, 3);
  a[1, 2] = 7.0;
  a[std::array<std::size_t, 2>{0, 1}] = 4.0;
  return a[1, 2] == 7.0 && (a[std::array<std::size_t, 2>{0, 1}]) == 4.0 && a.size() == 6 &&
         a.rank() == 2;
}
static_assert(denseRoundTrip());

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

  "kernels operate on plain mdspans over foreign storage"_test = [] {
    std::array<double, 6> abuf{1, 2, 3, 4, 5, 6};       // 2x3, not an MdArray
    std::array<double, 6> bbuf{7, 8, 9, 10, 11, 12};    // 3x2
    std::array<double, 4> cbuf{};                        // 2x2 destination
    std::mdspan a{abuf.data(), 2, 3};
    std::mdspan b{bbuf.data(), 3, 2};
    std::mdspan c{cbuf.data(), 2, 2};
    multiply(a, b, c);  // no owner allocated; writes through to cbuf
    expectEq(cbuf[0], 58.0);
    expectEq(cbuf[3], 154.0);

    add(a, a, a);  // in-place element-wise on a foreign buffer
    expectEq(abuf[0], 2.0);
    expectEq(abuf[5], 12.0);
  };

  "destination multiply into a sub-block of a larger matrix"_test = [] {
    Matrix<double> big(3, 3);  // fill the top-left 2x2 only
    Matrix<double> a(2, 2);
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    Matrix<double> id(2, 2);
    id[0, 0] = 1.0; id[1, 1] = 1.0;
    auto block = std::submdspan(big.toMdspan(), std::pair{0, 2}, std::pair{0, 2});
    multiply(a, id, block);  // writes the product into big's corner, no copy
    expectEq(big[0, 0], 1.0);
    expectEq(big[1, 1], 4.0);
    expectEq(big[2, 2], 0.0);  // untouched
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

  // ─── the Dense owner directly (folded from the former mdarray_test) ───

  "Dense: construction, shape, zeros"_test = [] {
    Dense<double, Dext2> a(3, 4);
    expectEq(a.rank(), 2u);
    expectEq(a.rankDynamic(), 2u);
    expectEq(a.extent(0), 3u);
    expectEq(a.extent(1), 4u);
    expectEq(a.size(), 12u);
    expectFalse(a.empty());
    expectEq(a[0, 0], 0.0);
    expectEq(a[2, 3], 0.0);
  };

  "Dense: row-major (layout_right) flat layout"_test = [] {
    Dense<double, Dext2> a(2, 3);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    expectEq(a[1, 2], 6.0);
    expectEq(a.container()[0], 1.0);
    expectEq(a.container()[3], 4.0);  // (1,0) -> row 1 starts at offset 3
    expectEq(a.container()[5], 6.0);
  };

  "Dense: genuinely N-dimensional (rank 3)"_test = [] {
    Dense<double, Dext3> a(2, 2, 2);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j)
        for (std::size_t k = 0; k < 2; ++k) a[i, j, k] = v++;
    expectEq(a.rank(), 3u);
    expectEq(a[1, 1, 1], 8.0);
    expectEq(a.container()[5], 6.0);  // (1,0,1) -> 1*4 + 0*2 + 1 = 5
  };

  "Dense: subscript by index array"_test = [] {
    Dense<double, Dext2> a(2, 3);
    a[1, 2] = 9.0;
    std::array<std::size_t, 2> idx{1, 2};
    expectEq(a[idx], 9.0);
    a[idx] = 11.0;
    expectEq(a[1, 2], 11.0);
  };

  "Dense: mixed static and dynamic extents (supply only the dynamic dim)"_test = [] {
    using Ext = std::extents<std::size_t, 3, std::dynamic_extent>;
    Dense<double, Ext> a(4);
    expectEq(a.rankDynamic(), 1u);
    expectEq(Dense<double, Ext>::staticExtent(0), 3u);
    expectEq(a.extent(0), 3u);
    expectEq(a.extent(1), 4u);
    expectEq(a.size(), 12u);
  };

  "Dense: fully static extents allocate on default construction"_test = [] {
    Dense<double, std::extents<std::size_t, 2, 2>> a;
    expectEq(a.size(), 4u);
    a[0, 1] = 9.0;
    expectEq(a[0, 1], 9.0);
  };

  "Dense: construct from a ready-made container"_test = [] {
    std::vector<double> v{1, 2, 3, 4, 5, 6};
    Dense<double, Dext2> a(Dext2{2, 3}, v);
    expectEq(a[0, 0], 1.0);
    expectEq(a[1, 1], 5.0);
  };

  "Dense: toMdspan shares storage"_test = [] {
    Dense<double, Dext2> a(2, 2);
    a[0, 0] = 1.0;
    auto s = a.toMdspan();
    expectEq((s[0, 0]), 1.0);
    s[1, 1] = 5.0;
    expectEq(a[1, 1], 5.0);  // write through the view shows in the owner
    const auto& ca = a;
    auto cs = ca.toMdspan();
    expectEq((cs[1, 1]), 5.0);
  };

  "Dense: materialize by copying out of an mdspan"_test = [] {
    std::array<double, 6> buf{1, 2, 3, 4, 5, 6};
    std::mdspan<double, Dext2> src(buf.data(), 2, 3);
    Dense<double, Dext2> a(src);
    expectEq(a.size(), 6u);
    expectEq(a[1, 2], 6.0);
    buf[5] = 99.0;  // a copied, not aliased
    expectEq(a[1, 2], 6.0);
  };

  "Dense: array container gives stack storage"_test = [] {
    InlineMatrix<double, 2, 2> a;  // Dense over std::array
    expectEq(a.size(), 4u);
    a[1, 1] = 3.0;
    expectEq(a[1, 1], 3.0);
    expectEq(a.container()[3], 3.0);
  };

  "Dense: row-literal construction (CTAD infers an InlineMatrix)"_test = [] {
    Dense m{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};  // 2x3 deduced
    static_assert(std::same_as<decltype(m), InlineMatrix<double, 2, 3>>);
    expectEq(m[0, 0], 1.0);
    expectEq(m[0, 2], 3.0);
    expectEq(m[1, 0], 4.0);
    expectEq(m[1, 2], 6.0);

    // Explicit type, int literals widen to the matrix's element type.
    InlineMatrix<double, 2, 2> n{{1, 2}, {3, 4}};
    expectEq(n[0, 1], 2.0);
    expectEq(n[1, 0], 3.0);
  };

  return TestRegistry::result();
}
