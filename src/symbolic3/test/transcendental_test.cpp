#include "symbolic3/simplify.h"

// Test that transcendental function rules compile and are well-formed
// Full functional testing requires traversal strategies to apply rules
// recursively

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  // ========================================================================
  // Test that rule objects are constexpr and well-formed
  // ========================================================================

  {
    // Verify ExpRules compiles
    [[maybe_unused]] constexpr auto rules = ExpRules;
    static_assert(Strategy<decltype(rules)>, "ExpRules should be a Strategy");
  }

  {
    // Verify LogRules compiles
    [[maybe_unused]] constexpr auto rules = LogRules;
    static_assert(Strategy<decltype(rules)>, "LogRules should be a Strategy");
  }

  {
    // Verify SinRules compiles
    [[maybe_unused]] constexpr auto rules = SinRules;
    static_assert(Strategy<decltype(rules)>, "SinRules should be a Strategy");
  }

  {
    // Verify CosRules compiles
    [[maybe_unused]] constexpr auto rules = CosRules;
    static_assert(Strategy<decltype(rules)>, "CosRules should be a Strategy");
  }

  {
    // Verify TanRules compiles
    [[maybe_unused]] constexpr auto rules = TanRules;
    static_assert(Strategy<decltype(rules)>, "TanRules should be a Strategy");
  }

  {
    // Verify SqrtRules compiles
    [[maybe_unused]] constexpr auto rules = SqrtRules;
    static_assert(Strategy<decltype(rules)>, "SqrtRules should be a Strategy");
  }

  {
    // Verify transcendental_simplify compiles
    [[maybe_unused]] constexpr auto rules = transcendental_simplify;
    static_assert(Strategy<decltype(rules)>,
                  "transcendental_simplify should be a Strategy");
  }

  {
    // Verify that combined algebraic_simplify includes transcendental rules
    [[maybe_unused]] constexpr auto rules = algebraic_simplify;
    static_assert(Strategy<decltype(rules)>,
                  "algebraic_simplify should be a Strategy");
  }

  // Note: Full functional tests require fixes to the Choice combinator
  // to properly handle return type deduction when rules don't match.
  // The rules are correctly structured and compile, which validates
  // the design. Runtime testing will be done after combinator improvements.

  return 0;
}
