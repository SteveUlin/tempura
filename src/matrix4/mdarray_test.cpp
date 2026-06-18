#include "mdarray.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <mdspan>
#include <span>
#include <utility>
#include <vector>

#include "unit.h"

using namespace tempura;

using Dext2 = std::dextents<std::size_t, 2>;
using Dext3 = std::dextents<std::size_t, 3>;
using Stat22 = std::extents<std::size_t, 2, 2>;
using Stat23 = std::extents<std::size_t, 2, 3>;

// The owner is constexpr-usable end to end: build, index two ways, hand out a view,
// write through it, read back. Exercised at compile time via static_assert.
constexpr auto roundTrip() -> bool {
  MdArray<double, Dext2> a(2, 3);
  a[1, 2] = 7.0;
  a[std::array<std::size_t, 2>{0, 1}] = 4.0;
  auto s = a.toMdspan();
  s[1, 0] = 9.0;  // write through the view
  return a[1, 2] == 7.0 && (a[std::array<std::size_t, 2>{0, 1}]) == 4.0 && a[1, 0] == 9.0 &&
         a.size() == 6 && a.rank() == 2 && a.extent(0) == 2 && a.extent(1) == 3;
}
static_assert(roundTrip());

// Fully-static, array-backed owner is fully constexpr (no heap), and toMdspan over
// it round-trips in a constant expression.
constexpr auto inlineRoundTrip() -> bool {
  InlineDense<double, 2, 2> a;
  a[0, 0] = 1.0; a[1, 1] = 4.0;
  return a.size() == 4 && a.container()[0] == 1.0 && a.container()[3] == 4.0 &&
         a.rankDynamic() == 0;
}
static_assert(inlineRoundTrip());

auto main() -> int {
  "shape ctor: zeros, shape queries"_test = [] {
    MdArray<double, Dext2> a(3, 4);
    expectEq(a.rank(), 2u);
    expectEq(a.rankDynamic(), 2u);
    expectEq(a.extent(0), 3u);
    expectEq(a.extent(1), 4u);
    expectEq(a.size(), 12u);
    expectEq(a.containerSize(), 12u);
    expectFalse(a.empty());
    expectEq(a[0, 0], 0.0);
    expectEq(a[2, 3], 0.0);
  };

  "row-major (layout_right) flat layout"_test = [] {
    MdArray<double, Dext2> a(2, 3);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    expectEq(a[1, 2], 6.0);
    expectEq(a.container()[0], 1.0);
    expectEq(a.container()[3], 4.0);  // (1,0) → row 1 starts at offset 3
    expectEq(a.stride(0), 3u);        // row stride
    expectEq(a.stride(1), 1u);        // column stride
  };

  "fill-value ctor"_test = [] {
    MdArray<double, Dext2> a(Dext2{2, 2}, 5.0);
    expectEq(a[0, 0], 5.0);
    expectEq(a[1, 1], 5.0);
    InlineDense<double, 2, 2> b(Stat22{}, 7.0);  // std::array fill path
    expectEq(b[0, 1], 7.0);
    expectEq(b[1, 0], 7.0);
  };

  "rank-3 (genuinely N-dimensional)"_test = [] {
    MdArray<double, Dext3> a(2, 2, 2);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j)
        for (std::size_t k = 0; k < 2; ++k) a[i, j, k] = v++;
    expectEq(a.rank(), 3u);
    expectEq(a[1, 1, 1], 8.0);
    expectEq(a.container()[5], 6.0);  // (1,0,1) → 1*4 + 0*2 + 1 = 5
  };

  "subscript by array and by span"_test = [] {
    MdArray<double, Dext2> a(2, 3);
    a[1, 2] = 9.0;
    std::array<std::size_t, 2> idx{1, 2};
    expectEq(a[idx], 9.0);
    std::span<std::size_t, 2> sp{idx};
    expectEq(a[sp], 9.0);
    a[idx] = 11.0;
    expectEq(a[1, 2], 11.0);
  };

  "mixed static and dynamic extents (supply only the dynamic dim)"_test = [] {
    using Ext = std::extents<std::size_t, 3, std::dynamic_extent>;
    MdArray<double, Ext> a(4);
    expectEq(a.rankDynamic(), 1u);
    expectEq(MdArray<double, Ext>::staticExtent(0), 3u);
    expectEq(a.extent(0), 3u);
    expectEq(a.extent(1), 4u);
    expectEq(a.size(), 12u);
  };

  "fully static extents allocate on default construction"_test = [] {
    MdArray<double, Stat22> a;  // paper forbids this ctor; tempura sizes it
    expectEq(a.size(), 4u);
    a[0, 1] = 9.0;
    expectEq(a[0, 1], 9.0);
  };

  "adopt a ready-made container (copy and move)"_test = [] {
    std::vector<double> v{1, 2, 3, 4, 5, 6};
    MdArray<double, Dext2> a(Dext2{2, 3}, v);  // copy
    expectEq(a[0, 0], 1.0);
    expectEq(a[1, 1], 5.0);
    expectEq(v.size(), 6u);  // unchanged — copied
    MdArray<double, Dext2> b(Dext2{2, 3}, std::move(v));  // move
    expectEq(b[1, 2], 6.0);
  };

  "extractContainer hands off ownership"_test = [] {
    MdArray<double, Dext2> a(Dext2{2, 2}, 3.0);
    std::vector<double> c = std::move(a).extractContainer();
    expectEq(c.size(), 4u);
    expectEq(c[0], 3.0);
  };

  "toMdspan: member, const and non-const share storage"_test = [] {
    MdArray<double, Dext2> a(2, 2);
    a[0, 0] = 1.0;
    auto s = a.toMdspan();
    static_assert(std::same_as<decltype(s)::element_type, double>);
    expectEq((s[0, 0]), 1.0);
    s[1, 1] = 5.0;
    expectEq(a[1, 1], 5.0);  // write through the view shows in the owner
    const auto& ca = a;
    auto cs = ca.toMdspan();
    static_assert(std::same_as<decltype(cs)::element_type, const double>);
    expectEq((cs[1, 1]), 5.0);
  };

  "materialize by copying out of an mdspan"_test = [] {
    std::array<double, 6> buf{1, 2, 3, 4, 5, 6};
    std::mdspan<double, Dext2> src(buf.data(), 2, 3);
    MdArray<double, Dext2> a(src);
    expectEq(a.size(), 6u);
    expectEq(a[1, 2], 6.0);
    buf[5] = 99.0;            // a copied, not aliased
    expectEq(a[1, 2], 6.0);
  };

  "array container gives stack storage"_test = [] {
    InlineDense<double, 2, 2> a;  // MdArray over std::array
    expectEq(a.size(), 4u);
    expectTrue(decltype(a)::isAlwaysExhaustive());
    a[1, 1] = 3.0;
    expectEq(a[1, 1], 3.0);
    expectEq(a.container()[3], 3.0);
  };

  "row-literal construction (CTAD infers an array-backed MdArray)"_test = [] {
    MdArray m{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};  // 2x3 deduced
    static_assert(std::same_as<decltype(m), InlineDense<double, 2, 3>>);
    expectEq(m[0, 0], 1.0);
    expectEq(m[0, 2], 3.0);
    expectEq(m[1, 0], 4.0);
    expectEq(m[1, 2], 6.0);
  };

  "swap exchanges contents"_test = [] {
    MdArray<double, Dext2> a(Dext2{2, 2}, 1.0);
    MdArray<double, Dext2> b(Dext2{2, 2}, 2.0);
    swap(a, b);
    expectEq(a[0, 0], 2.0);
    expectEq(b[0, 0], 1.0);
  };

  "copy and move are value semantics (independent storage)"_test = [] {
    MdArray<double, Dext2> a(2, 2);
    a[0, 0] = 1.0;
    auto b = a;       // copy
    b[0, 0] = 9.0;
    expectEq(a[0, 0], 1.0);  // a untouched — deep copy
    expectEq(b[0, 0], 9.0);
    auto c = std::move(a);   // move
    expectEq(c[0, 0], 1.0);
  };

  "Dense / InlineDense are the rank-2 owners (heap and stack)"_test = [] {
    static_assert(std::same_as<Dense<double, dyn, dyn>,
                               MdArray<double, Dext2, std::layout_right, std::vector<double>>>);
    Dense<double, dyn, dyn> m(2, 2);  // fully dynamic, heap
    m[1, 1] = 7.0;
    expectEq(m[1, 1], 7.0);
    Dense<double, 2, 3> stat;  // static shape, heap
    static_assert(decltype(stat)::rankDynamic() == 0);
    expectEq(stat.size(), 6u);
    InlineDense<double, 2, 2> stack;  // static shape, stack (std::array)
    static_assert(decltype(stack)::rankDynamic() == 0);
    stack[0, 1] = 5.0;
    expectEq(stack.container().size(), 4u);
    expectEq(stack[0, 1], 5.0);
  };

  return TestRegistry::result();
}
