#include "matrix2/storage/dense.h"

#include <array>
#include <vector>

#include "unit.h"

using namespace tempura;
using namespace tempura::matrix;

auto main() -> int {
  "Default Constructor"_test = [] {
    static_assert([] consteval {
      Dense<double, 2, 3> m{};
      return m.data() == std::vector{0., 0., 0., 0., 0., 0.};
    }());
  };

  "Vector Constructor"_test = [] {
    static_assert([] consteval {
      Dense<double, 2, 2> m{{0., 1.}, {2., 3.}};
      return m.data() == std::vector{0., 2., 1., 3.};
    }());
  };

  "Copy Constructor"_test = [] {
    static_assert([] consteval {
      Dense<double, 2, 2> m{{0., 1.}, {2., 3.}};
      Dense<double, 2, 2> n{m};
      return n.data() == std::vector{0., 2., 1., 3.};
    }());
  };

  "Copy Assignment"_test = [] {
    static_assert([] consteval {
      Dense<double, 2, 2> m{{0., 1.}, {2., 3.}};
      Dense<double, 2, 2> n{{4., 5.}, {6., 7.}};
      n = m;
      return n.data() == std::vector{0., 2., 1., 3.};
    }());
  };

  "Shape"_test = [] {
    static_assert([] consteval {
      Dense<double, 2, 2> m{{0., 1.}, {2., 3.}};
      return m.shape() == matrix::RowCol{.row = 2, .col = 2};
    }());
  };

  "[] operator"_test = [] {
    static_assert([] consteval {
      Dense<double, 2, 2> n{{0., 1.}, {2., 3.}};
      return n[0, 0] == 0. and n[0, 1] == 1. and n[1, 0] == 2. and n[1, 1] == 3.;
    }());
  };

  "[] row operator"_test = [] {
    static_assert([] consteval {
      Dense<double, 4, 1> m{{0.}, {1.}, {2.}, {3.}};
      return m[0] == 0. and m[1] == 1. and m[2] == 2. and m[3] == 3.;
    }());
  };

  "[] col operator"_test = [] {
    static_assert([] consteval {
      Dense<double, 1, 4> m{{0., 1., 2., 3.}};
      return m[0] == 0. and m[1] == 1. and m[2] == 2. and m[3] == 3.;
    }());
  };

  "const for loop"_test = [] {
    static_assert([] consteval {
      Dense<double, 2, 3> m{{0., 1., 2.}, {3., 4., 5.}};
      double sum = 0.;
      for (const auto& element : m) {
        sum += element;
      }
      return sum == 15.;
    }());
  };

  "mutuable for loop"_test = [] {
    static_assert([] consteval {
      Dense<double, 2, 3> m{{0., 1., 2.}, {3., 4., 5.}};
      for (auto& element : m) {
        element *= 2.;
      }
      return m.data() == std::vector{0., 6., 2., 8., 4., 10.};
    }());
  };

  return TestRegistry::result();
}
