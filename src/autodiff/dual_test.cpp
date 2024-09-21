#include "dual.h"

#include "sstream"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "constructor test"_test = [] {
    constexpr Dual<double> dual{1.0, 2.0};
    expectEq(1.0, dual.value);
    expectEq(2.0, dual.gradient);
  };

  "string test"_test = [] {
    constexpr Dual<double> dual{1.0, 2.0};
    std::stringstream ss;
    ss << dual;
    expectEq("1 + 2ε", ss.str());
  };

  "constexpr comparisions"_test = [] {
    constexpr Dual<double> lhs{1.0, 2.0};
    constexpr Dual<double> rhs{3.0, 4.0};
    static_assert(lhs != rhs);
    static_assert(lhs == lhs);
    static_assert(lhs < rhs);
    static_assert(lhs <= rhs);
    static_assert(rhs > lhs);
    static_assert(rhs >= lhs);
  };

  "constexpr addtion"_test = [] {
    constexpr Dual<double> lhs{1.0, 2.0};
    constexpr Dual<double> rhs{3.0, 4.0};
    static_assert(Dual{4.0, 6.0} == lhs + rhs);
  };

  "inplace addtion"_test = [] {
    Dual<double> lhs{1.0, 2.0};
    Dual<double> rhs{3.0, 4.0};
    lhs += rhs;
    expectEq(Dual{4.0, 6.0}, lhs);
  };

  "plus"_test = [] {
    constexpr Dual<double> dual{1.0, 2.0};
    static_assert(Dual{1.0, 2.0} == +dual);
  };

  "constexpr subtraction"_test = [] {
    constexpr Dual<double> lhs{1.0, 2.0};
    constexpr Dual<double> rhs{3.0, 4.0};
    static_assert(Dual{-2.0, 6.0} == lhs - rhs);
  };

  "inplace subtraction"_test = [] {
    Dual<double> lhs{1.0, 2.0};
    Dual<double> rhs{3.0, 4.0};
    lhs -= rhs;
    expectEq(Dual{-2.0, -2.0}, lhs);
  };

  "minus"_test = [] {
    constexpr Dual<double> dual{1.0, 2.0};
    static_assert(Dual{-1.0, -2.0} == -dual);
  };

  "constexpr multiplication"_test = [] {
    constexpr Dual<double> lhs{1.0, 2.0};
    constexpr Dual<double> rhs{3.0, 4.0};
    static_assert(Dual{3.0, 10.0} == lhs * rhs);
  };

  "inplace multiplication"_test = [] {
    Dual<double> lhs{1.0, 2.0};
    Dual<double> rhs{3.0, 4.0};
    lhs *= rhs;
    expectEq(Dual{3.0, 10.0}, lhs);
  };

  "constexpr division"_test = [] {
    constexpr Dual<double> lhs{4.0, 3.0};
    constexpr Dual<double> rhs{1.0, 2.0};
    static_assert(Dual{4.0, -5.0} == lhs / rhs);
  };

  "inplace division"_test = [] {
    Dual<double> lhs{4.0, 3.0};
    Dual<double> rhs{1.0, 2.0};
    lhs /= rhs;
    expectEq(Dual{4.0, -5.0}, lhs);
  };

  "sqrt"_test = [] {
    Dual<double> dual{4.0, 3.0};
    expectEq(Dual{2.0, 0.375}, sqrt(dual));
  };

  "exp"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectEq(Dual{1.0, 3.0}, exp(dual));
  };

  "log"_test = [] {
    Dual<double> dual{1.0, 2.0};
    expectEq(Dual{0.0, 0.5}, log(dual));
  };

  "pow"_test = [] {
    Dual<double> dual{2.0, 3.0};
    expectEq(Dual{8.0, 12.0}, pow(dual, 3.));
  };

  "pow2"_test = [] {
    Dual<double> base{2.0, 3.0};
    Dual<double> exponent{3.0, 0.0};
    expectEq(Dual{8.0, 12.0}, pow(base, exponent));
  };

  "sin"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectEq(Dual{0.0, 3.0}, sin(dual));
  };

  "cos"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectEq(Dual{1.0, 0.0}, cos(dual));
  };

  "tan"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectEq(Dual{0.0, 9.0}, tan(dual));
  };

  "asin"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectEq(Dual{0.0, 0.6}, asin(dual));
  };

  "acos"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectEq(Dual{1.5707963267948966, -0.6}, acos(dual));
  };

  "atan"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectEq(Dual{0.0, 0.1111111111111111}, atan(dual));
  };

  return 0;
}
