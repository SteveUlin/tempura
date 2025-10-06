#include "symbolic3/simplify.h"

#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/matching.h"
#include "symbolic3/operators.h"
#include "symbolic3/pattern_matching.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  // Test power rules at compile-time
  {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // x^0 → 1
    constexpr auto expr1 = pow(x, Constant<0>{});
    constexpr auto result1 = power_zero.apply(expr1, ctx);
    // Should be Constant<1>{}

    // x^1 → x
    constexpr auto expr2 = pow(x, Constant<1>{});
    constexpr auto result2 = power_one.apply(expr2, ctx);
    // Should be x
  }

  // Test addition identity
  {
    constexpr Symbol y;
    constexpr auto ctx = default_context();

    // y + 0 → y
    constexpr auto expr = y + Constant<0>{};
    constexpr auto result = AdditionRuleCategories::Identity.apply(expr, ctx);
    // Should be y
  }

  // Test multiplication identity
  {
    constexpr Symbol z;
    constexpr auto ctx = default_context();

    // z * 1 → z
    constexpr auto expr1 = z * Constant<1>{};
    constexpr auto result1 =
        MultiplicationRuleCategories::Identity.apply(expr1, ctx);
    // Should be z

    // z * 0 → 0
    constexpr auto expr2 = z * Constant<0>{};
    constexpr auto result2 =
        MultiplicationRuleCategories::Identity.apply(expr2, ctx);
    // Should be Constant<0>{}
  }

  return 0;
}
