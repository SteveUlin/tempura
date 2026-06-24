#include "slices.h"

#include <concepts>
#include <cstddef>

#include "mdarray.h"
#include "unit.h"
#include "vec.h"

using namespace tempura;

// constexpr end to end: iterate by row and by column at compile time.
constexpr auto rowColSums() -> bool {
  InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
  int rsum = 0;
  for (auto r : rows(m))
    for (std::size_t j = 0; j < r.extent(0); ++j) rsum += r[j];
  int csum = 0;
  for (auto c : cols(m))
    for (std::size_t i = 0; i < c.extent(0); ++i) csum += c[i];
  return rsum == 21 && csum == 21;  // same elements, two traversals
}
static_assert(rowColSums());

auto main() -> int {
  "rows and cols yield rank-1 slices with the right values"_test = [] {
    InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    auto r = rows(m);
    auto c = cols(m);
    static_assert(decltype(r[0])::rank() == 1);
    static_assert(VecView<decltype(r[0])>);
    expectEq((r[1][2]), 6);     // row 1 = {4,5,6}
    expectEq((c[0][1]), 4);     // col 0 = {1,4}
    expectEq((c[2][0]), 3);     // col 2 = {3,6}
    expectEq(r[0].extent(0), std::size_t{3});  // a row spans the columns
    expectEq(c[0].extent(0), std::size_t{2});  // a column spans the rows
  };

  "a column slice is strided (stride = #cols), a row is contiguous"_test = [] {
    Dense<int, Dyn, Dyn> m(dims(2, 3));  // row-major
    expectEq(cols(m)[0].stride(0), std::size_t{3});  // step a column → skip a whole row
    expectEq(rows(m)[0].stride(0), std::size_t{1});  // a row walks unit stride
  };

  "slices write through to the source"_test = [] {
    InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    rows(m)[0][1] = 99;
    expectEq((m[0, 1]), 99);
    cols(m)[2][1] = 77;        // col 2, row 1 = m[1,2]
    expectEq((m[1, 2]), 77);
  };

  "a const matrix yields read-only slices"_test = [] {
    const InlineDense<int, 2, 3> m{{1, 2, 3}, {4, 5, 6}};
    auto r = rows(m);
    static_assert(std::same_as<decltype(r[0])::element_type, const int>);
    expectEq((r[1][0]), 4);
  };

  "slices compose with the vector kernels (matmul by dot of row·col)"_test = [] {
    InlineDense<double, 2, 3> a{{1, 2, 3}, {4, 5, 6}};
    InlineDense<double, 3, 2> b{{7, 8}, {9, 10}, {11, 12}};
    Dense<double, Dyn, Dyn> c(dims(2, 2));
    auto ra = rows(a);
    auto cb = cols(b);
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j) c[i, j] = dot(ra[i], cb[j]);
    // verify against the direct kernel — same result, different formulation
    auto direct = multiply(a, b);
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j)
        expectClose((c[i, j]), (direct[i, j]), {.rtol = 1e-12, .atol = 1e-12});
  };

  return TestRegistry::result();
}
