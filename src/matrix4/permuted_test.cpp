#include "permuted.h"

#include <cstddef>

#include "matrix.h"
#include "permutation.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "permutedRows is a lazy gather view: view[i] = m[order[i]]"_test = [] {
    Matrix<double> m(3, 2);
    for (std::size_t i = 0; i < 3; ++i) {
      m[i, 0] = static_cast<double>(i);
      m[i, 1] = static_cast<double>(10 + i);
    }
    Permutation<3> p{1, 2, 0};  // view row0<-m1, row1<-m2, row2<-m0
    auto pv = permutedRows(m, p);
    expectEq(pv[0, 0], 1.0); expectEq(pv[0, 1], 11.0);
    expectEq(pv[1, 0], 2.0); expectEq(pv[1, 1], 12.0);
    expectEq(pv[2, 0], 0.0); expectEq(pv[2, 1], 10.0);
    // It really is a view onto m: a later write to m shows through.
    m[1, 0] = 99.0;
    expectEq(pv[0, 0], 99.0);
  };

  "permutedRows materializes into contiguous storage"_test = [] {
    Matrix<double> m(3, 1);
    m[0, 0] = 10; m[1, 0] = 20; m[2, 0] = 30;
    Permutation<3> p{2, 0, 1};  // row0<-m2, row1<-m0, row2<-m1
    Matrix<double> r = permutedRows(m, p);
    expectEq(r[0, 0], 30.0);
    expectEq(r[1, 0], 10.0);
    expectEq(r[2, 0], 20.0);
  };

  "permuted view composes with multiply (zero-copy row reorder)"_test = [] {
    Matrix<double> a(2, 3);
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = static_cast<double>(i * 3 + j);
    Matrix<double> b(3, 2);
    for (std::size_t i = 0; i < 3; ++i) {
      b[i, 0] = static_cast<double>(i);
      b[i, 1] = static_cast<double>(10 + i);
    }
    Permutation<3> p{1, 2, 0};
    const Matrix<double> bperm = permutedRows(b, p);  // materialized reference
    const auto ref = multiply(a, bperm);
    const auto got = multiply(a, permutedRows(b, p));  // lazy view straight in
    expectEq(got.extent(0), ref.extent(0));
    expectEq(got.extent(1), ref.extent(1));
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j) expectEq(got[i, j], ref[i, j]);
  };

  "eager permuteRows agrees with the lazy permutedRows view"_test = [] {
    Matrix<double> m(4, 2);
    for (std::size_t i = 0; i < 4; ++i) {
      m[i, 0] = static_cast<double>(i);
      m[i, 1] = static_cast<double>(100 + i);
    }
    const Permutation<4> p{2, 0, 3, 1};
    Matrix<double> eager = m;        // copy, then gather in place
    p.permuteRows(eager);
    const auto lazy = permutedRows(m, p);  // same gather, as a view
    for (std::size_t i = 0; i < 4; ++i)
      for (std::size_t j = 0; j < 2; ++j) expectEq(eager[i, j], lazy[i, j]);
  };

  return TestRegistry::result();
}
