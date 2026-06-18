#include "vec.h"

#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>

#include "mdarray.h"
#include "unit.h"

using namespace tempura;

static_assert(std::same_as<decltype(InlineDense<double, 2, 3>{} * InlineVec<double, 3>{}),
                           Vec<double, 2>>);
static_assert(std::same_as<decltype(Dense<double, dyn, dyn>{} * Vec<double, dyn>{}), Vec<double, dyn>>);

constexpr auto constexprVecOps() -> bool {
  InlineVec<double, 3> x;
  x[0] = 1.0; x[1] = 2.0; x[2] = 3.0;
  InlineVec<double, 3> y;
  y[0] = 4.0; y[1] = 5.0; y[2] = 6.0;
  InlineDense<double, 2, 3> a;  // selects rows 0 and 1 of x
  a[0, 0] = 1.0; a[1, 1] = 1.0;
  InlineVec<double, 2> z;  // stack destination — no heap, runs at compile time
  multiply(a, x, z);
  return dot(x, y) == 32.0 && z[0] == 1.0 && z[1] == 2.0;  // dot = 4+10+18
}
static_assert(constexprVecOps());

auto main() -> int {
  "construction and single-index subscript"_test = [] {
    Vec<double, dyn> x(3);
    expectEq(x.rank(), 1u);
    expectEq(x.size(), 3u);
    x[0] = 1.0; x[1] = 2.0; x[2] = 3.0;
    expectEq(x[2], 3.0);

    InlineVec<double, 3> s;
    s[1] = 9.0;
    expectEq(s[1], 9.0);
  };

  "dot product"_test = [] {
    Vec<double, dyn> x(3);
    x[0] = 1.0; x[1] = 2.0; x[2] = 3.0;
    Vec<double, dyn> y(3);
    y[0] = 4.0; y[1] = 5.0; y[2] = 6.0;
    expectEq(dot(x, y), 32.0);
    expectEq(dot(x, x), 14.0);  // ‖x‖²
  };

  "matrix-vector product, dynamic"_test = [] {
    // [1 2 3; 4 5 6] · [1; 1; 1] = [6; 15]
    Dense<double, dyn, dyn> a(2, 3);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    Vec<double, dyn> x(3);
    x[0] = 1.0; x[1] = 1.0; x[2] = 1.0;
    auto y = a * x;
    expectEq(y.rank(), 1u);
    expectEq(y.extent(0), 2u);
    expectEq(y[0], 6.0);
    expectEq(y[1], 15.0);
  };

  "matrix-vector product, inline result type is a static Vec"_test = [] {
    InlineDense<double, 2, 3> a;
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 3; ++j) a[i, j] = v++;
    InlineVec<double, 3> x;
    x[0] = 1.0; x[1] = 1.0; x[2] = 1.0;
    auto y = multiply(a, x);
    static_assert(std::same_as<decltype(y), Vec<double, 2>>);
    expectEq(y[0], 6.0);
    expectEq(y[1], 15.0);
  };

  "destination matvec writes into a caller-owned vector"_test = [] {
    Dense<double, dyn, dyn> a(2, 2);
    a[0, 0] = 1.0; a[0, 1] = 2.0; a[1, 0] = 3.0; a[1, 1] = 4.0;
    Vec<double, dyn> x(2);
    x[0] = 1.0; x[1] = 1.0;
    Vec<double, dyn> y(2);
    auto& r = multiply(a, x, y);
    expectTrue(&r == &y);
    expectEq(y[0], 3.0);
    expectEq(y[1], 7.0);
  };

  "euclidean norm"_test = [] {
    Vec<double, dyn> x(2);
    x[0] = 3.0; x[1] = 4.0;
    expectEq(norm2(x), 5.0);  // √(9+16) = 5, exact in IEEE double
  };

  "vector value-init: static checks count, dynamic infers length"_test = [] {
    InlineVec<double, 3> v{1.0, 2.0, 3.0};  // static, compile-time count
    expectEq(v[0], 1.0);
    expectEq(v[2], 3.0);
    MdArray w{4.0, 5.0};  // CTAD → InlineVec
    static_assert(std::same_as<decltype(w), InlineVec<double, 2>>);
    expectEq(w[1], 5.0);
    Vec<double, dyn> d{1.0, 2.0, 3.0};  // dynamic, length inferred
    expectEq(d.size(), 3u);
    expectEq(d[2], 3.0);
    Vec<double, dyn> z(3);  // parens = zero vector of length 3
    expectEq(z.size(), 3u);
    expectEq(z[0], 0.0);
  };

  "norm2 is overflow/underflow safe (scaled)"_test = [] {
    Vec<double, 2> big{1e200, 1e200};  // naive Σxᵢ² would overflow to ∞
    expectClose(norm2(big), std::sqrt(2.0) * 1e200, {.rtol = 1e-12});
    Vec<double, 2> small{1e-200, 1e-200};  // naive Σxᵢ² would underflow to 0
    expectClose(norm2(small), std::sqrt(2.0) * 1e-200, {.rtol = 1e-12});
  };

  "norm2 propagates NaN (sticky scale)"_test = [] {
    Vec<double, 2> v{0.0, std::numeric_limits<double>::quiet_NaN()};
    expectNan(norm2(v));  // the zero early-out must not swallow the NaN
  };

  "dot returns the operand precision, not the wide accumulator"_test = [] {
    InlineVec<float, 3> x{1.0F, 2.0F, 3.0F};
    InlineVec<float, 3> y{4.0F, 5.0F, 6.0F};
    static_assert(std::same_as<decltype(dot(x, y)), float>);  // Accumulator is internal-only
    expectEq(dot(x, y), 32.0F);                               // 4 + 10 + 18
  };

  "norm2 of a vector with ±inf is +inf; NaN still dominates"_test = [] {
    Vec<double, 2> v{std::numeric_limits<double>::infinity(), 1.0};
    expectInf(norm2(v));
    Vec<double, 2> w{std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::quiet_NaN()};
    expectNan(norm2(w));
  };

  "matvec multiplyAdd: y += A·x"_test = [] {
    Dense<double, 2, 2> a{{1.0, 2.0}, {3.0, 4.0}};
    Vec<double, 2> x{1.0, 1.0};
    Vec<double, 2> y{10.0, 20.0};
    multiplyAdd(a, x, y);  // A·x = {3, 7}; y → {13, 27}
    expectEq(y[0], 13.0);
    expectEq(y[1], 27.0);
  };

  return TestRegistry::result();
}
