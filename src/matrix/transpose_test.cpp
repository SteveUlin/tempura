#include "transpose.h"

#include <concepts>
#include <cstddef>

#include "mdarray.h"
#include "unit.h"

using namespace tempura;

// constexpr end to end: static extents swap, indices swap, read back at compile time.
constexpr auto transposeRoundTrip() -> bool {
  InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
  auto t = transposed(m);
  return t.extent(0) == 3 && t.extent(1) == 2 && t[0, 0] == 1 && t[0, 1] == 4 && t[1, 0] == 2 &&
         t[1, 1] == 5 && t[2, 0] == 3 && t[2, 1] == 6 && decltype(t)::rank() == 2;
}
static_assert(transposeRoundTrip());

auto main() -> int {
  "extents and indices swap"_test = [] {
    InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    auto t = transposed(m);
    expectEq(t.extent(0), std::size_t{3});
    expectEq(t.extent(1), std::size_t{2});
    expectEq((t[0, 0]), 1); expectEq((t[0, 1]), 4);
    expectEq((t[1, 0]), 2); expectEq((t[1, 1]), 5);
    expectEq((t[2, 0]), 3); expectEq((t[2, 1]), 6);
  };

  "write-through aliases the source"_test = [] {
    InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    auto t = transposed(m);
    t[1, 0] = 99;
    expectEq((m[0, 1]), 99);  // t[1,0] is m[0,1]
    m[1, 2] = 77;
    expectEq((t[2, 1]), 77);  // m[1,2] is t[2,1]
  };

  "double transpose recovers the original"_test = [] {
    InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    auto tt = transposed(transposed(m));
    expectEq(tt.extent(0), std::size_t{2});
    expectEq(tt.extent(1), std::size_t{3});
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) expectEq((tt[i, j]), (m[i, j]));
  };

  "1x1 and column vector"_test = [] {
    InlineDense<int, 1, 1> one{{42}};
    auto t1 = transposed(one);
    expectEq(t1.extent(0), std::size_t{1});
    expectEq((t1[0, 0]), 42);

    InlineDense<int, 3, 1> col{{1}, {2}, {3}};
    auto tc = transposed(col);
    expectEq(tc.extent(0), std::size_t{1});
    expectEq(tc.extent(1), std::size_t{3});
    expectEq((tc[0, 0]), 1); expectEq((tc[0, 1]), 2); expectEq((tc[0, 2]), 3);
  };

  "const owner yields a read-only transpose"_test = [] {
    const InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    auto t = transposed(m);
    static_assert(std::same_as<decltype(t)::element_type, const int>);
    expectEq((t[2, 1]), 6);
  };

  "materialize copies into independent storage"_test = [] {
    InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    Dense<int, Dyn, Dyn> r = transposed(m);  // converting-from-mdspan ctor copies
    r[0, 0] = 99;
    expectEq((m[0, 0]), 1);               // source untouched
    expectEq((transposed(m)[0, 0]), 1);
    expectEq((r[0, 0]), 99);
    expectEq(r.extent(0), std::size_t{3});
    expectEq(r.extent(1), std::size_t{2});
  };

  "materialize converts element type"_test = [] {
    Dense<double, Dyn, Dyn> r = transposed(InlineDense<int, 2, 2>{{1, 2}, {3, 4}});
    expectEq(r.extent(0), std::size_t{2});
    expectEq((r[0, 1]), 3.0);  // = src[1,0] = 3
  };

  "transposed composes with multiply: AᵀA Gram matrix"_test = [] {
    InlineDense<double, 2, 3> a{{1, 2, 3}, {4, 5, 6}};
    auto g = multiply(transposed(a), a);  // 3x3, statically shaped
    static_assert(std::same_as<decltype(g), Dense<double, 3, 3>>);
    expectEq((g[0, 0]), 17.0); expectEq((g[0, 1]), 22.0); expectEq((g[0, 2]), 27.0);
    expectEq((g[1, 0]), 22.0); expectEq((g[1, 1]), 29.0); expectEq((g[1, 2]), 36.0);
    expectEq((g[2, 0]), 27.0); expectEq((g[2, 1]), 36.0); expectEq((g[2, 2]), 45.0);
  };

  "materialize a transpose into an owner (heap and stack)"_test = [] {
    InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    auto h = materialize(transposed(m));  // heap; static extents survive the round-trip
    static_assert(Owning<decltype(h)>);
    static_assert(std::same_as<decltype(h), Dense<int, 3, 2>>);
    h[0, 0] = 99;
    expectEq((m[0, 0]), 1);  // decoupled from the source
    expectEq((h[2, 1]), 6);
    auto s = materialize<Storage::Stack>(transposed(m));  // stack
    static_assert(std::same_as<decltype(s), InlineDense<int, 3, 2>>);
    expectEq((s[1, 0]), 2);
  };

  return TestRegistry::result();
}
