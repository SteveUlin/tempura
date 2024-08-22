#include "matrix/slice.h"

#include "matrix/dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Default Constructor"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    std::cout << "HERE: " << m[0, 0] << '\n';
    auto a = matrix::Slicer<{1, 2}>::at({0, 0}, m);

    // expectEq(m.shape(), matrix::RowCol{2, 3});
    expectEq(1., a[0, 0]);
    expectEq(0., a[0, 1]);
    expectEq(0., a[0, 0]);
    std::cout << "HERE: " << a[0, 0] << '\n';
  };
  "Test Iterelements"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    for (const auto& val : matrix::IterElements(m)) {
      std::cout << val << std::endl;
    }
  };
  "Test Iterelements"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    for (const auto& row : matrix::IterRows(m)) {
      for (const auto& x : matrix::IterElements(row)) {
        std::cout << x << " ";
      }
      std::cout << std::endl;
    }
  };
  "Test Col"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    for (const auto& row : matrix::IterCols(m)) {
      for (const auto& x : matrix::IterElements(row)) {
        std::cout << x << " ";
      }
      std::cout << std::endl;
    }
  };
  return 0;
}
