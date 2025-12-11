#include "matrix3/complex.h"

#include <complex>

#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "complex_default_constructor"_test = [] {
    constexpr Complex<double> c;
    expectEq(1.0, c[0, 0]);  // real
    expectEq(0.0, c[0, 1]);  // -imag = 0
    expectEq(0.0, c[1, 0]);  // imag = 0
    expectEq(1.0, c[1, 1]);  // real
  };

  "complex_parameterized_constructor"_test = [] {
    constexpr Complex<double> c{3.0, 4.0};  // 3+4i
    expectEq(3.0, c[0, 0]);   // real
    expectEq(-4.0, c[0, 1]);  // -imag
    expectEq(4.0, c[1, 0]);   // imag
    expectEq(3.0, c[1, 1]);   // real
  };

  "complex_from_std_complex"_test = [] {
    std::complex<double> z{2.0, -3.0};  // 2-3i
    Complex<double> c{z};
    expectEq(2.0, c[0, 0]);   // real
    expectEq(3.0, c[0, 1]);   // -(-3) = 3
    expectEq(-3.0, c[1, 0]);  // imag = -3
    expectEq(2.0, c[1, 1]);   // real
  };

  "complex_matrix_representation"_test = [] {
    constexpr Complex<double> c{5.0, 12.0};  // 5+12i
    // Matrix: ⎡ 5  -12 ⎤
    //         ⎣ 12   5 ⎦
    expectEq(5.0, c[0, 0]);
    expectEq(-12.0, c[0, 1]);
    expectEq(12.0, c[1, 0]);
    expectEq(5.0, c[1, 1]);
  };

  "complex_extents"_test = [] {
    constexpr Complex<int> c;
    static_assert(Complex<int>::rows() == 2);
    static_assert(Complex<int>::cols() == 2);
    static_assert(c.rows() == 2);
    static_assert(c.cols() == 2);
  };

  "complex_data_accessor"_test = [] {
    constexpr Complex<double> c{7.0, 24.0};
    constexpr auto z = c.data();
    static_assert(z.real() == 7.0);
    static_assert(z.imag() == 24.0);
  };

  "complex_equality"_test = [] {
    constexpr Complex<double> c1{1.0, 2.0};
    constexpr Complex<double> c2{1.0, 2.0};
    constexpr Complex<double> c3{1.0, 3.0};
    static_assert(c1 == c2);
    static_assert(!(c1 == c3));
  };

  "complex_constexpr_types"_test = [] {
    // int
    constexpr Complex<int> ci{3, 4};
    static_assert(ci[0, 0] == 3);
    static_assert(ci[0, 1] == -4);
    static_assert(ci[1, 0] == 4);
    static_assert(ci[1, 1] == 3);

    // float
    constexpr Complex<float> cf{1.5f, 2.5f};
    static_assert(cf[0, 0] == 1.5f);
    static_assert(cf[0, 1] == -2.5f);
    static_assert(cf[1, 0] == 2.5f);
    static_assert(cf[1, 1] == 1.5f);

    // double
    constexpr Complex<double> cd{6.0, 8.0};
    static_assert(cd[0, 0] == 6.0);
    static_assert(cd[0, 1] == -8.0);
    static_assert(cd[1, 0] == 8.0);
    static_assert(cd[1, 1] == 6.0);
  };

  "complex_zero"_test = [] {
    constexpr Complex<double> c{0.0, 0.0};
    static_assert(c[0, 0] == 0.0);
    static_assert(c[0, 1] == 0.0);
    static_assert(c[1, 0] == 0.0);
    static_assert(c[1, 1] == 0.0);
  };

  "complex_pure_imaginary"_test = [] {
    constexpr Complex<double> c{0.0, 1.0};  // i
    // Matrix: ⎡ 0 -1 ⎤
    //         ⎣ 1  0 ⎦
    static_assert(c[0, 0] == 0.0);
    static_assert(c[0, 1] == -1.0);
    static_assert(c[1, 0] == 1.0);
    static_assert(c[1, 1] == 0.0);
  };

  return TestRegistry::result();
}
