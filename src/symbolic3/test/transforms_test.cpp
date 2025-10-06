#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  // ============================================================================
  // Constant Folding Tests
  // ============================================================================

  "Fold constants - addition"_test = [] {
    constexpr Constant<2> two;
    constexpr Constant<3> three;
    constexpr auto expr = two + three;

    constexpr FoldConstants folder;
    constexpr auto ctx = default_context();

    constexpr auto result = folder.apply(expr, ctx);

    static_assert(is_constant<decltype(result)>);
    static_assert(decltype(result)::value == 5);
  };

  "Fold constants - multiplication"_test = [] {
    constexpr Constant<2> two;
    constexpr Constant<3> three;
    constexpr auto expr = two * three;

    constexpr FoldConstants folder;
    constexpr auto ctx = default_context();

    constexpr auto result = folder.apply(expr, ctx);

    static_assert(is_constant<decltype(result)>);
    static_assert(decltype(result)::value == 6);
  };

  "Fold constants - disabled by context"_test = [] {
    constexpr Constant<2> two;
    constexpr Constant<3> three;
    constexpr auto expr = two + three;

    constexpr FoldConstants folder;
    constexpr auto ctx = symbolic_context();  // No constant folding

    constexpr auto result = folder.apply(expr, ctx);

    // Should remain as expression
    static_assert(is_add<decltype(result)>);
  };

  // ============================================================================
  // Algebraic Rules Tests
  // ============================================================================

  "Add zero identity - right"_test = [] {
    constexpr Symbol x;
    constexpr Constant<0> zero;
    constexpr auto expr = x + zero;

    constexpr ApplyAlgebraicRules rules;
    constexpr auto result = rules.apply(expr, default_context());

    static_assert(match(result, x));
  };

  "Add zero identity - left"_test = [] {
    constexpr Symbol x;
    constexpr Constant<0> zero;
    constexpr auto expr = zero + x;

    constexpr ApplyAlgebraicRules rules;
    constexpr auto result = rules.apply(expr, default_context());

    static_assert(match(result, x));
  };

  "Multiply by zero - right"_test = [] {
    constexpr Symbol x;
    constexpr Constant<0> zero;
    constexpr auto expr = x * zero;

    constexpr ApplyAlgebraicRules rules;
    constexpr auto result = rules.apply(expr, default_context());

    static_assert(is_constant<decltype(result)>);
    static_assert(decltype(result)::value == 0);
  };

  "Multiply by zero - left"_test = [] {
    constexpr Symbol x;
    constexpr Constant<0> zero;
    constexpr auto expr = zero * x;

    constexpr ApplyAlgebraicRules rules;
    constexpr auto result = rules.apply(expr, default_context());

    static_assert(is_constant<decltype(result)>);
    static_assert(decltype(result)::value == 0);
  };

  "Multiply by one - right"_test = [] {
    constexpr Symbol x;
    constexpr Constant<1> one;
    constexpr auto expr = x * one;

    constexpr ApplyAlgebraicRules rules;
    constexpr auto result = rules.apply(expr, default_context());

    static_assert(match(result, x));
  };

  "Multiply by one - left"_test = [] {
    constexpr Symbol x;
    constexpr Constant<1> one;
    constexpr auto expr = one * x;

    constexpr ApplyAlgebraicRules rules;
    constexpr auto result = rules.apply(expr, default_context());

    static_assert(match(result, x));
  };

  // ============================================================================
  // Normalization Tests
  // ============================================================================

  "Double negation elimination"_test = [] {
    constexpr Symbol x;
    constexpr auto neg_x = -x;
    constexpr auto expr = -neg_x;

    constexpr NormalizeNegation normalizer;
    constexpr auto result = normalizer.apply(expr, default_context());

    static_assert(match(result, x));
  };

  // ============================================================================
  // Pipeline Tests
  // ============================================================================

  "Algebraic simplify pipeline"_test = [] {
    constexpr Symbol x;
    constexpr Constant<1> one;
    constexpr auto expr = x * one;  // Should simplify to x

    constexpr auto result = algebraic_simplify.apply(expr, default_context());

    static_assert(match(result, x));
  };

  "Combined simplification"_test = [] {
    constexpr Symbol x;
    constexpr Constant<0> zero;
    constexpr Constant<5> five;

    // (x + 0) + 5 should simplify
    constexpr auto expr = (x + zero) + five;

    constexpr auto result = full_simplify.apply(expr, default_context());

    // Result should be x + 5 after simplification
    static_assert(is_add<decltype(result)>);
  };

  // ============================================================================
  // Context-Aware Tests
  // ============================================================================

  "TrigAware disables folding inside trig"_test = [] {
    constexpr Constant<2> two;
    constexpr Constant<3> three;
    constexpr auto sum = two + three;
    constexpr auto trig_expr = sin(sum);

    constexpr auto strategy = trig_aware_simplify;
    constexpr auto ctx = default_context();

    // When we apply trig-aware strategy, it should detect we're in a trig
    // and preserve the symbolic form
    constexpr auto result = strategy.apply(trig_expr, ctx);

    static_assert(is_trig_function<decltype(result)>);
  };

  return TestRegistry::result();
}
