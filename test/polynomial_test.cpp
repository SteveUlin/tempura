#include "polynomial.h"
#include "unit.h"

#include <print>

using namespace tempura;

auto main() -> int {
  "Polynomial Range Constructor"_test = [] {
    constexpr std::array<double, 3> coefficients{1., 2., 3.};
    constexpr Polynomial<double, 3> p{coefficients};
    static_assert(p == Polynomial<double, 3>{1., 2., 3.});
  };

  "Polynomial Evaluation"_test = [] {
    constexpr Polynomial<double, 3> p{1., 2., 3.};
    static_assert(p(2.) == 17.);
  };

  "Polynomial Coefficients"_test = [] {
    constexpr Polynomial<double, 3> p{1., 2., 3.};
    static_assert(p[0] == 1.);
    static_assert(p[1] == 2.);
    static_assert(p[2] == 3.);
  };

  "Polynomial Iterators"_test = [] {
    constexpr Polynomial<double, 3> p{1., 2., 3.};
    static_assert(*p.begin() == 1.);
    static_assert(*std::ranges::prev(p.end()) == 3.);

    static_assert(*p.cbegin() == 1.);
    static_assert(*std::ranges::prev(p.cend()) == 3.);
  };

  "Polynomial Inplace Addition"_test = [] {
    constexpr auto p = [] consteval {
      Polynomial<double, 3> p1{1., 2., 3.};
      Polynomial<double, 3> p2{3., 2., 1.};
      p1 += p2;
      return p1;
    }();

    static_assert(p == Polynomial<double, 3>{4., 4., 4.});
  };

  "Polynomial Addition"_test = [] {
    constexpr Polynomial<double, 3> p1{1., 2., 3.};
    constexpr Polynomial<double, 3> p2{3., 2., 1.};
    constexpr Polynomial<double, 3> p3 = p1 + p2;

    static_assert(p3 == Polynomial<double, 3>{4., 4., 4.});
  };

  "Polynomial Addition Different Size"_test = [] {
    constexpr Polynomial<double, 3> p1{1., 2., 3.};
    constexpr Polynomial<double, 2> p2{3., 2.};
    constexpr Polynomial<double, 3> p3 = p1 + p2;
    static_assert(p3 == Polynomial<double, 3>{4., 4., 3.});
  };

  "Polynomial Subtraction"_test = [] {
    constexpr Polynomial<double, 3> p1{1., 2., 3.};
    constexpr Polynomial<double, 3> p2{3., 2., 1.};
    constexpr Polynomial<double, 3> p3 = p1 - p2;
    static_assert(p3 == Polynomial<double, 3>{-2., 0., 2.});
  };

  "Polynomial Subtraction Different Size"_test = [] {
    constexpr Polynomial<double, 3> p1{1., 2., 3.};
    constexpr Polynomial<double, 2> p2{3., 2.};
    constexpr Polynomial<double, 3> p3 = p1 - p2;
    static_assert(p3 == Polynomial<double, 3>{-2., 0., 3.});
  };

  "Polynomial Multiplication"_test = [] {
    constexpr Polynomial<double, 3> p1{1., 2., 3.};
    constexpr Polynomial<double, 3> p2{3., 2., 1.};
    constexpr Polynomial<double, 5> p3 = p1 * p2;
    static_assert(p3 == Polynomial<double, 5>{3., 8., 14., 8., 3.});
  };

  return TestRegistry::result();
}
