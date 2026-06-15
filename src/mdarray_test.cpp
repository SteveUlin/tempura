#include "mdarray.h"

#include <array>
#include <mdspan>
#include <span>

#include "unit.h"

using namespace tempura;

using Dext2 = std::dextents<std::size_t, 2>;

// Compile-time round-trip: constexpr construction, multidim write, read-back.
// std::vector is constexpr in C++26, so the whole owner lives and dies inside
// one constant evaluation.
constexpr auto constexprRoundTrip() -> bool {
  MdArray<double, Dext2> a(2, 3);
  a[1, 2] = 7.0;
  return a[1, 2] == 7.0 && a.size() == 6 && a.extent(0) == 2 && a.rank() == 2;
}
static_assert(constexprRoundTrip());

auto main() -> int {
  "construction and shape"_test = [] {
    MdArray<double, Dext2> a(3, 4);
    expectEq(a.rank(), 2u);
    expectEq(a.extent(0), 3u);
    expectEq(a.extent(1), 4u);
    expectEq(a.size(), 12u);
    expectFalse(a.empty());
    // Fresh storage value-initializes to zero.
    expectEq(a[0, 0], 0.0);
    expectEq(a[2, 3], 0.0);
  };

  "indexing round-trips, layout_right is row-major"_test = [] {
    MdArray<double, Dext2> a(2, 3);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    expectEq(a[1, 2], 6.0);
    // Row-major: element (i,j) lands at i*cols + j in flat storage.
    expectEq(a.data()[0], 1.0);
    expectEq(a.data()[3], 4.0);
    expectEq(a.data()[5], 6.0);
  };

  "mixed static and dynamic extents"_test = [] {
    // Rows fixed at 3, columns chosen at runtime.
    using Ext = std::extents<std::size_t, 3, std::dynamic_extent>;
    MdArray<double, Ext> a(4);  // only the one dynamic extent is supplied
    expectEq(a.rank_dynamic(), 1u);
    expectEq(MdArray<double, Ext>::static_extent(0), 3u);
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

  "implicit conversion to mdspan shares storage"_test = [] {
    MdArray<double, Dext2> a(2, 2);
    a[0, 0] = 1.0;
    // Owner converts to a view with no explicit .view() at the call site.
    std::mdspan<double, Dext2> s = a;
    expectEq((s[0, 0]), 1.0);
    s[1, 1] = 5.0;            // write through the view...
    expectEq(a[1, 1], 5.0);   // ...and see it in the owner.

    const auto& ca = a;
    std::mdspan<const double, Dext2> cs = ca;
    expectEq((cs[1, 1]), 5.0);
  };

  "layout_left is column-major"_test = [] {
    MdArray<double, Dext2, std::layout_left> a(2, 3);
    double v = 1.0;
    for (std::size_t j = 0; j < 3; ++j)
      for (std::size_t i = 0; i < 2; ++i) a[i, j] = v++;
    // Column-major: element (i,j) lands at i + j*rows.
    expectEq(a.data()[0], 1.0);
    expectEq(a.data()[1], 2.0);
    expectEq(a.data()[2], 3.0);  // == a[0, 1]
    expectEq((a[0, 1]), 3.0);
  };

  "array container gives stack storage"_test = [] {
    // Same template, fixed-array storage — the stack variant of the heap path.
    MdArray<double, std::extents<std::size_t, 2, 2>, std::layout_right,
            std::array<double, 4>>
        a;
    expectEq(a.size(), 4u);
    a[1, 1] = 3.0;
    expectEq(a[1, 1], 3.0);
    expectEq(a.data()[3], 3.0);
  };

  return TestRegistry::result();
}
