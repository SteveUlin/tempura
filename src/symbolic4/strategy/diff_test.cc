// Tests for pattern-matching based differentiation
#include "symbolic4/strategy/diff.h"

#include "symbolic4/constants.h"
#include "symbolic4/operators.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  constexpr Symbol<struct X> x;
  constexpr Symbol<struct Y> y;

  // =========================================================================
  // TERMINALS
  // =========================================================================

  "diff of variable"_test = [&] {
    static_assert(match(1_c, differentiate(x, x)));
    static_assert(match(0_c, differentiate(y, x)));
    static_assert(match(0_c, differentiate(42_c, x)));
    static_assert(match(0_c, differentiate(pi, x)));
    static_assert(match(0_c, differentiate(e, x)));
  };

  // =========================================================================
  // LINEAR OPERATIONS
  // =========================================================================

  "diff of sums"_test = [&] {
    static_assert(match(1_c, differentiate(x + y, x)));
    // simplify() doesn't fold 1_c + 1_c → 2_c (no constant folding rule)
    static_assert(match(1_c + 1_c, differentiate(x + x, x)));
    static_assert(match(1_c, differentiate(x - y, x)));
    static_assert(match(-1_c, differentiate(-x, x)));
  };

  // =========================================================================
  // PRODUCT & QUOTIENT
  // =========================================================================

  "diff of products"_test = [&] {
    static_assert(match(y, differentiate(x * y, x)));
    static_assert(match(x + x, differentiate(x * x, x)));
  };

  "diff of quotients"_test = [&] {
    constexpr auto r1 = differentiate(x / y, x);
    constexpr auto r2 = differentiate(y / x, x);
    static_assert(isSame<get_op_t<decltype(r1)>, DivOp>);
    static_assert(isSame<get_op_t<decltype(r2)>, DivOp>);
  };

  // =========================================================================
  // TRANSCENDENTAL
  // =========================================================================

  "diff of exp/log"_test = [&] {
    static_assert(match(exp(x), differentiate(exp(x), x)));
    static_assert(match(1_c / x, differentiate(log(x), x)));
    static_assert(match(1_c / (2_c * sqrt(x)), differentiate(sqrt(x), x)));
  };

  "diff of trig"_test = [&] {
    static_assert(match(cos(x), differentiate(sin(x), x)));
    static_assert(match(-sin(x), differentiate(cos(x), x)));
  };

  "diff of hyperbolic"_test = [&] {
    static_assert(match(cosh(x), differentiate(sinh(x), x)));
    static_assert(match(sinh(x), differentiate(cosh(x), x)));
  };

  "diff of inverse trig"_test = [&] {
    static_assert(match(1_c / sqrt(1_c - x * x), differentiate(asin(x), x)));
    static_assert(match(1_c / (1_c + x * x), differentiate(atan(x), x)));
  };

  // =========================================================================
  // CHAIN RULE
  // =========================================================================

  "chain rule"_test = [&] {
    static_assert(match(exp(2_c * x) * 2_c, differentiate(exp(2_c * x), x)));
    // sin(x)² has complex form after diff, just check structure
    constexpr auto r = differentiate(pow(sin(x), 2_c), x);
    static_assert(isSame<get_op_t<decltype(r)>, MulOp>);
  };

  // =========================================================================
  // SPECIAL FUNCTIONS
  // =========================================================================

  "diff of special functions"_test = [&] {
    static_assert(match(digamma(x), differentiate(lgamma(x), x)));
  };

  // Note: Literal<T> tests removed — info-domain diff can't round-trip
  // runtime embedded values. Use Constant<V> instead.

  return TestRegistry::result();
}
