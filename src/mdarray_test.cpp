#include "mdarray.h"

#include <array>
#include <mdspan>
#include <span>
#include <vector>

#include "unit.h"

using namespace tempura;

using Dext2 = std::dextents<std::size_t, 2>;
using Dext3 = std::dextents<std::size_t, 3>;

// Compile-time round-trip: constexpr construction, multidim write (pack and
// array subscript), read-back. std::vector is constexpr in C++26, so the whole
// owner lives and dies inside one constant evaluation.
constexpr auto constexprRoundTrip() -> bool {
  MdArray<double, Dext2> a(2, 3);
  a[1, 2] = 7.0;
  a[std::array<std::size_t, 2>{0, 1}] = 4.0;
  return a[1, 2] == 7.0 && (a[std::array<std::size_t, 2>{0, 1}]) == 4.0 && a.size() == 6 &&
         a.rank() == 2;
}
static_assert(constexprRoundTrip());

auto main() -> int {
  "construction and shape"_test = [] {
    MdArray<double, Dext2> a(3, 4);
    expectEq(a.rank(), 2u);
    expectEq(a.rankDynamic(), 2u);
    expectEq(a.extent(0), 3u);
    expectEq(a.extent(1), 4u);
    expectEq(a.size(), 12u);
    expectFalse(a.empty());
    expectEq(a[0, 0], 0.0);  // fresh storage value-initializes to zero
    expectEq(a[2, 3], 0.0);
  };

  "indexing round-trips, layout_right is row-major"_test = [] {
    MdArray<double, Dext2> a(2, 3);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    expectEq(a[1, 2], 6.0);
    // Row-major: element (i,j) lands at i*cols + j in flat storage.
    expectEq(a.container()[0], 1.0);
    expectEq(a.container()[3], 4.0);
    expectEq(a.container()[5], 6.0);
  };

  "rank-3 proves it is genuinely N-dimensional"_test = [] {
    MdArray<double, Dext3> a(2, 2, 2);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j)
        for (std::size_t k = 0; k < 2; ++k) a[i, j, k] = v++;
    expectEq(a.rank(), 3u);
    expectEq(a[1, 1, 1], 8.0);
    // Row-major offset for (1,0,1) = 1*4 + 0*2 + 1 = 5.
    expectEq(a.container()[5], 6.0);
  };

  "subscript by array and span (runtime-rank-agnostic)"_test = [] {
    MdArray<double, Dext2> a(2, 3);
    a[1, 2] = 9.0;
    std::array<std::size_t, 2> idx{1, 2};
    expectEq(a[idx], 9.0);
    std::span<std::size_t, 2> sp{idx};
    expectEq(a[sp], 9.0);
    a[idx] = 11.0;  // writes through too
    expectEq(a[1, 2], 11.0);
  };

  "mixed static and dynamic extents"_test = [] {
    using Ext = std::extents<std::size_t, 3, std::dynamic_extent>;
    MdArray<double, Ext> a(4);  // only the one dynamic extent is supplied
    expectEq(a.rankDynamic(), 1u);
    expectEq(MdArray<double, Ext>::staticExtent(0), 3u);
    expectEq(a.extent(0), 3u);
    expectEq(a.extent(1), 4u);
    expectEq(a.size(), 12u);
  };

  "fully static extents allocate on default construction"_test = [] {
    MdArray<double, std::extents<std::size_t, 2, 2>> a;
    expectEq(a.size(), 4u);
    a[0, 1] = 9.0;
    expectEq(a[0, 1], 9.0);
  };

  "construct from a ready-made container"_test = [] {
    std::vector<double> v{1, 2, 3, 4, 5, 6};
    MdArray<double, Dext2> a(Dext2{2, 3}, v);
    expectEq(a[0, 0], 1.0);
    expectEq(a[1, 1], 5.0);
  };

  "mapping property observers"_test = [] {
    MdArray<double, Dext2> a(2, 3);
    expectTrue(a.isUnique());
    expectTrue(a.isExhaustive());
    expectTrue(a.isStrided());
    expectTrue(MdArray<double, Dext2>::isAlwaysUnique());
    expectTrue(MdArray<double, Dext2>::isAlwaysExhaustive());
    // layout_right strides: last dim contiguous, first dim spans a row.
    expectEq(a.stride(1), 1u);
    expectEq(a.stride(0), 3u);
  };

  "explicit toMdspan shares storage"_test = [] {
    MdArray<double, Dext2> a(2, 2);
    a[0, 0] = 1.0;
    auto s = a.toMdspan();      // explicit — no silent conversion
    expectEq((s[0, 0]), 1.0);
    s[1, 1] = 5.0;             // write through the view...
    expectEq(a[1, 1], 5.0);    // ...and see it in the owner.

    const auto& ca = a;
    auto cs = ca.toMdspan();   // const owner → mdspan<const double>
    expectEq((cs[1, 1]), 5.0);
  };

  "construct by copying elements out of an mdspan"_test = [] {
    std::array<double, 6> buf{1, 2, 3, 4, 5, 6};
    std::mdspan<double, Dext2> src(buf.data(), 2, 3);
    MdArray<double, Dext2> a(src);
    expectEq(a.size(), 6u);
    expectEq(a[1, 2], 6.0);
    buf[5] = 99.0;             // the MdArray copied, so it is unaffected
    expectEq(a[1, 2], 6.0);
  };

  "converting between layouts copies by index, not by bytes"_test = [] {
    MdArray<double, Dext2> row(2, 2);
    row[0, 0] = 1.0;
    row[0, 1] = 2.0;
    row[1, 0] = 3.0;
    row[1, 1] = 4.0;
    MdArray<double, Dext2, std::layout_left> col(row);
    // Logical elements match...
    expectEq((col[0, 1]), 2.0);
    expectEq((col[1, 0]), 3.0);
    // ...but column-major flat storage differs: (1,0) sits at offset 1.
    expectEq(col.container()[1], 3.0);
  };

  "extractContainer moves the storage out"_test = [] {
    MdArray<double, Dext2> a(2, 2);
    a[0, 0] = 5.0;
    auto c = std::move(a).extractContainer();
    expectEq(c.size(), 4u);
    expectEq(c[0], 5.0);
  };

  "layout_left is column-major"_test = [] {
    MdArray<double, Dext2, std::layout_left> a(2, 3);
    double v = 1.0;
    for (std::size_t j = 0; j < 3; ++j)
      for (std::size_t i = 0; i < 2; ++i) a[i, j] = v++;
    expectEq(a.container()[0], 1.0);
    expectEq(a.container()[1], 2.0);
    expectEq(a.container()[2], 3.0);  // == a[0, 1]
    expectEq((a[0, 1]), 3.0);
  };

  "array container gives stack storage"_test = [] {
    MdArray<double, std::extents<std::size_t, 2, 2>, std::layout_right,
            std::array<double, 4>>
        a;
    expectEq(a.size(), 4u);
    a[1, 1] = 3.0;
    expectEq(a[1, 1], 3.0);
    expectEq(a.container()[3], 3.0);
  };

  return TestRegistry::result();
}
