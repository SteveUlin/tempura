#include "matrix/storage/permutation.h"
#include "matrix/storage/identity.h"
#include "matrix/storage/dense.h"
#include "matrix/printer.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Default Constructor"_test = [] {
    matrix::RowPermutation<3> P;

    expectEq(P, matrix::Identity<3>{});
  };

  "Default Constructor"_test = [] {
    matrix::RowPermutation<3> P(matrix::Permutation<3>{0, 2, 1});

    const matrix::Dense<int, {3, 3}> A{
      {1, 0, 0},
      {0, 0, 1},
      {0, 1, 0}
    };
    expectEq(P, A);
  };

  "Permuted"_test = [] {
    const matrix::Dense m{
      {1, 2, 3},
      {4, 5, 6},
      {7, 8, 9}
    };

    matrix::RowPermuted p{m};

    p.swap(0, 2);
    std::cout << matrix::toString(p) << std::endl;
  };

  "Permute"_test = [] {
    matrix::Dense m{
      {1, 2, 3},
      {4, 5, 6},
      {7, 8, 9}
    };

    matrix::Permutation<3>{0, 2, 1}.permute(m.rows());
    std::cout << matrix::toString(m) << std::endl;
  };
  return 0;
}

