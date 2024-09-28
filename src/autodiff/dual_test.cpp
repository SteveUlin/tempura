#include "dual.h"

#include "sstream"
#include "unit.h"

using namespace tempura;

template <double delta = 0.0001, typename Lhs, typename Rhs>
auto expectApproxEq(const Dual<Lhs>& lhs, const Dual<Rhs>& rhs,
                    const std::source_location location =
                        std::source_location::current()) -> bool {
  // Avoid the early stopping behavior of "and"
  bool v = expectApproxEq<delta>(lhs.value, rhs.value, location);
  bool g = expectApproxEq<delta>(lhs.gradient, rhs.gradient, location);
  return v and g;
}

constexpr auto dualEq(Dual<double> lhs, Dual<double> rhs) -> bool {
  return lhs.value == rhs.value and lhs.gradient == rhs.gradient;
}

auto f(const Dual<double>& x, const Dual<double>& y) {
  return x * x * y;
}

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
    static_assert(dualEq(Dual{4.0, 6.0}, lhs + rhs));
  };

  "inplace addtion"_test = [] {
    Dual<double> lhs{1.0, 2.0};
    Dual<double> rhs{3.0, 4.0};
    lhs += rhs;
    expectTrue(dualEq(Dual{4.0, 6.0}, lhs));
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
    expectTrue(dualEq(Dual{-2.0, -2.0}, lhs));
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
    expectTrue(dualEq(Dual{3.0, 10.0}, lhs));
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
    expectTrue(dualEq(Dual{4.0, -5.0}, lhs));
  };

  "sqrt"_test = [] {
    Dual<double> dual{4.0, 3.0};
    expectApproxEq(Dual{2.0, 0.75}, sqrt(dual));
  };

  "exp"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectTrue(dualEq(Dual{1.0, 3.0}, exp(dual)));
  };

  "log"_test = [] {
    Dual<double> dual{1.0, 2.0};
    expectApproxEq(Dual{0.0, 2.0}, log(dual));
  };

  "pow"_test = [] {
    Dual<double> dual{2.0, 3.0};
    expectApproxEq(Dual{8.0, 36.0}, pow(dual, 3.));
  };

  "pow2"_test = [] {
    Dual<double> base{2.0, 3.0};
    Dual<double> exponent{3.0, 0.0};
    expectApproxEq(Dual{8.0, 36.0}, pow(base, exponent));
  };

  "sin"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectApproxEq(Dual{0.0, 3.0}, sin(dual));
  };

  "cos"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectApproxEq(Dual{1.0, 0.0}, cos(dual));
  };

  "tan"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectApproxEq(Dual{0.0, 3.0}, tan(dual));
  };

  "asin"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectApproxEq(Dual{0.0, 3.0}, asin(dual));
  };

  "acos"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectApproxEq(Dual{1.570796, -3.0}, acos(dual));
  };

  "atan"_test = [] {
    Dual<double> dual{0.0, 3.0};
    expectApproxEq(Dual{0.0, 3.0}, atan(dual));
  };

  "evalWrt"_test = [] {
    expectApproxEq(Dual{12.0, 12.0}, evalWrt<0>(f, 2.0, 3.0));
    expectApproxEq(Dual{12.0, 4.0}, evalWrt<1>(f, 2.0, 3.0));
    std::function g = f;
    expectApproxEq(Dual{12.0, 4.0}, evalWrt<1>(g, 2.0, 3.0));
    expectApproxEq(
        Dual{12.0, 4.0},
        evalWrt<1>([](Dual<double> x, Dual<double> y) { return x * x * y; },
                   2.0, 3.0));
  };

  "jacobian"_test = [] {
    for (const auto& val : jacobian(f, 2.0, 3.0)) {
      std::cout << val << std::endl;
    }
  };
  return 0;
}
