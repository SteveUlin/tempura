#include "symbolic/simplify.h"

#include <iostream>

#include "symbolic/operators.h"
#include "symbolic/symbolic.h"
#include "symbolic/to_string.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Normalize -a"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = -a;
    static_assert(normalize(expr) == Constant<-1>{} * a);
  };

  "Normalize a - b"_test = [] {
    TEMPURA_SYMBOLS(a, b);
    auto expr = a - b;
    static_assert(normalize(expr) == a + Constant<-1>{} * b);
  };

  "Normalize 1 / a"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = 1_c / a;
    static_assert(normalize(expr) == 1_c * pow(a, Constant<-1>{}));
  };

  "Simplify 0 + a"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = 0_c + a;
    static_assert(simplify(expr) == a);
  };

  "Simplify a + 0"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = a + 0_c;
    static_assert(simplify(expr) == a);
  };

  "Simplify -a + a"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = (-1_c) * a + a;
    static_assert(simplify(expr) == 0_c);
  };

  "Simplify a + a"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = a + a;
    static_assert(simplify(expr) == 2_c * a);
  };

  "Simplify a + b + a"_test = [] {
    TEMPURA_SYMBOLS(a, b);
    auto expr = a + b + a;
    static_assert(simplify(expr) == 2_c * a + b);
  };

  "Simplify a * 0"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = a * 0_c;
    static_assert(simplify(expr) == 0_c);
  };

  "Simplify 0 * a"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = 0_c * a;
    static_assert(simplify(expr) == 0_c);
  };

  "Simplfy a * 1"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = a * 1_c;
    static_assert(simplify(expr) == a);
  };

  "Simlify 1 * a"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = 1_c * a;
    static_assert(simplify(expr) == a);
  };

  "Simplify a * a"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = a * a;
    static_assert(simplify(expr) == pow(a, 2_c));
  };

  "Simplify a * b * a"_test = [] {
    TEMPURA_SYMBOLS(a, b);
    auto expr = a * b * a;
    static_assert(simplify(expr) == pow(a, 2_c) * b);
  };

  "Simplify a * (b + c)"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);
    auto expr = a * (b + c);
    static_assert(simplify(expr) == (a * b) + (a * c));
  };

  "Simplify (a + b) * c"_test = [] {
    TEMPURA_SYMBOLS(a, b, c);
    auto expr = (a + b) * c;
    static_assert(simplify(expr) == (a + b) * c);
  };

  "Simplify (a + b) * (c + d)"_test = [] {
    TEMPURA_SYMBOLS(a, b, c, d);
    auto expr = (a + b) * (c + d);
    static_assert(simplify(expr) == (a + b) * c + (a + b) * d);
  };

  "Simplify a / a"_test = [] {
    TEMPURA_SYMBOLS(a);
    auto expr = a / a;
    std::cout << simplify(normalize(expr)) << std::endl;
  };

  "Simplify sin(0)"_test = [] {
    auto expr = sin(0_c);
    // Unfortunately sin is not constexpr yet T.T
    static_assert(simplify(expr) == 0_c);
  };

  "Simplify cos(0)"_test = [] {
    auto expr = cos(0_c);
    // Unfortunately cos is not constexpr yet T.T
    static_assert(simplify(expr) == 1_c);
  };

  "Sim[lify tan(0)"_test = [] {
    auto expr = tan(0_c);
    // Unfortunately tan is not constexpr yet T.T
    static_assert(simplify(expr) == 0_c);
  };

  return TestRegistry::result();
}
