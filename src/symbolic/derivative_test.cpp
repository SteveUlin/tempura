#include "symbolic/derivative.h"

#include <iostream>

#include "symbolic/operators.h"
#include "symbolic/simplify.h"
#include "symbolic/symbolic.h"
#include "symbolic/to_string.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Diff: 1"_test = [] {
    TEMPURA_SYMBOLS(x);
    auto expr = 1_c;
    static_assert(diff(expr, x) == 0_c);
  };

  "Diff: y"_test = [] {
    TEMPURA_SYMBOLS(x, y);
    auto expr = y;
    static_assert(diff(expr, x) == 0_c);
  };

  "Diff: x"_test = [] {
    TEMPURA_SYMBOLS(x);
    auto expr = x;
    static_assert(diff(expr, x) == 1_c);
  };

  "Diff: x + 1"_test = [] {
    TEMPURA_SYMBOLS(x);
    auto expr = x + 1_c;
    static_assert(diff(expr, x) == 1_c + 0_c);
  };

  "Diff: x - 1"_test = [] {
    TEMPURA_SYMBOLS(x);
    auto expr = x - 1_c;
    static_assert(diff(expr, x) == 1_c - 0_c);
  };

  "Diff: -x"_test = [] {
    TEMPURA_SYMBOLS(x);
    auto expr = -x;
     static_assert(diff(expr, x) == -1_c);
  };

  "Diff: x * x"_test = [] {
    TEMPURA_SYMBOLS(x);
    auto expr = x * x;
    static_assert(diff(expr, x) == 1_c * x + x * 1_c);
  };

  "Diff: x / x"_test = [] {
    TEMPURA_SYMBOLS(x);
    auto expr = x / x;
    static_assert(diff(expr, x) == (1_c * x - x * 1_c) / pow(x, 2_c));
  };

  "Diff: x^2"_test = [] {
    TEMPURA_SYMBOLS(x);
    auto expr = pow(x, 2_c);
    static_assert(diff(expr, x) == 2_c * pow(x, 2_c - 1_c) * 1_c);
  };

  "Diff: sin(x)"_test = [] {
    TEMPURA_SYMBOLS(x);
    auto expr = sin(x);
    static_assert(diff(expr, x) == cos(x) * 1_c);
  };

  "Diff: cos(x)"_test = [] {
    TEMPURA_SYMBOLS(x);
    auto expr = cos(x);
    static_assert(diff(expr, x) == -sin(x) * 1_c);
  };

  return TestRegistry::result();
};
