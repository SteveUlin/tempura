#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  // ============================================================================
  // Core Types Tests
  // ============================================================================

  "Symbol creation"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    static_assert(!match(x, y), "Different symbols should not match");
    static_assert(match(x, x), "Same symbol should match itself");
  };

  "Constant creation"_test = [] {
    constexpr Constant<5> five;
    constexpr Constant<5> also_five;
    constexpr Constant<3> three;

    static_assert(match(five, also_five), "Same constant values should match");
    static_assert(!match(five, three), "Different constants should not match");
    static_assert(five.value == 5);
  };

  "Expression creation"_test = [] {
    constexpr Symbol x;
    constexpr Constant<2> two;

    constexpr auto expr = x + two;

    static_assert(is_expression<decltype(expr)>);
    static_assert(matches_op_v<AddOp, decltype(expr)>);
  };

  // ============================================================================
  // Context Tests
  // ============================================================================

  "Context tag addition"_test = [] {
    constexpr auto ctx1 = TransformContext<>{};
    static_assert(!ctx1.has<InsideTrigTag>());

    constexpr auto ctx2 = ctx1.with(InsideTrigTag{});
    static_assert(ctx2.has<InsideTrigTag>());
  };

  "Context tag removal"_test = [] {
    constexpr auto ctx1 = TransformContext<>{}.with(InsideTrigTag{});
    static_assert(ctx1.has<InsideTrigTag>());

    constexpr auto ctx2 = ctx1.without(InsideTrigTag{});
    static_assert(!ctx2.has<InsideTrigTag>());
  };

  "Context depth tracking"_test = [] {
    constexpr auto ctx1 = TransformContext<>{};
    static_assert(ctx1.depth == 0);

    constexpr auto ctx2 = ctx1.increment_depth<1>();
    static_assert(ctx2.depth == 1);

    constexpr auto ctx3 = ctx2.increment_depth<5>();
    static_assert(ctx3.depth == 6);
  };

  "Context convenience methods"_test = [] {
    constexpr auto ctx = TransformContext<>{}
                             .enable_constant_folding()
                             .enter_trig()
                             .symbolic_mode();

    static_assert(ctx.has<ConstantFoldingEnabledTag>());
    static_assert(ctx.has<InsideTrigTag>());
    static_assert(ctx.has<SymbolicModeTag>());
    static_assert(!ctx.has<NumericModeTag>());
  };

  // ============================================================================
  // Strategy Tests
  // ============================================================================

  "Identity strategy"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();
    constexpr Identity id;

    constexpr auto result = id.apply(x, ctx);
    static_assert(match(result, x));
  };

  "Identity preserves expressions"_test = [] {
    constexpr Symbol x;
    constexpr Constant<2> two;
    constexpr auto expr = x + two;
    constexpr auto ctx = default_context();

    constexpr Identity id;
    constexpr auto result = id.apply(expr, ctx);

    static_assert(match(result, expr));
  };

  // ============================================================================
  // Composition Tests
  // ============================================================================

  "Sequential composition"_test = [] {
    constexpr Symbol x;
    constexpr Identity id1;
    constexpr Identity id2;

    constexpr auto composed = id1 >> id2;
    constexpr auto result = composed.apply(x, default_context());

    static_assert(match(result, x));
  };

  "Choice composition - first succeeds"_test = [] {
    constexpr Symbol x;
    constexpr Identity id1;
    constexpr Fail fail;

    constexpr auto choice = id1 | fail;
    constexpr auto result = choice.apply(x, default_context());

    static_assert(match(result, x));
  };

  "Choice composition - second succeeds"_test = [] {
    constexpr Symbol x;
    constexpr Fail fail;
    constexpr Identity id;

    constexpr auto choice = fail | id;
    constexpr auto result = choice.apply(x, default_context());

    static_assert(match(result, x));
  };

  // ============================================================================
  // Conditional Tests
  // ============================================================================

  "When predicate true"_test = [] {
    constexpr Symbol x;
    constexpr Identity id;

    auto always_true = [](auto, auto) { return true; };
    constexpr auto conditional = when(always_true, id);

    constexpr auto result = conditional.apply(x, default_context());
    static_assert(match(result, x));
  };

  "When predicate false"_test = [] {
    constexpr Symbol x;
    constexpr Fail fail;

    auto always_false = [](auto, auto) { return false; };
    constexpr auto conditional = when(always_false, fail);

    constexpr auto result = conditional.apply(x, default_context());
    static_assert(match(result, x));  // Should remain unchanged
  };

  "When with tag predicate"_test = [] {
    constexpr Symbol x;
    constexpr Identity id;

    constexpr has_tag_pred<InsideTrigTag> pred;
    constexpr auto conditional = when(pred, id);

    constexpr auto ctx_without = default_context();
    constexpr auto result1 = conditional.apply(x, ctx_without);
    static_assert(match(result1, x));

    constexpr auto ctx_with = default_context().with(InsideTrigTag{});
    constexpr auto result2 = conditional.apply(x, ctx_with);
    static_assert(match(result2, x));
  };

  // ============================================================================
  // Recursion Tests
  // ============================================================================

  "FixPoint with Identity"_test = [] {
    constexpr Symbol x;
    constexpr Identity id;
    constexpr FixPoint<Identity, 5> fixpoint{.strategy = id};

    constexpr auto result = fixpoint.apply(x, default_context());
    static_assert(match(result, x));
  };

  "FixPoint respects depth limit"_test = [] {
    constexpr Symbol x;

    // Create a context at depth 10
    constexpr auto ctx = TransformContext<10>{};

    constexpr Identity id;
    constexpr FixPoint<Identity, 5> fixpoint{.strategy = id};

    // Should terminate immediately due to depth
    constexpr auto result = fixpoint.apply(x, ctx);
    static_assert(match(result, x));
  };

  "Repeat exactly N times"_test = [] {
    constexpr Symbol x;
    constexpr Identity id;

    constexpr Repeat<Identity, 0> repeat0{.strategy = id};
    constexpr auto result0 = repeat0.apply(x, default_context());
    static_assert(match(result0, x));

    constexpr Repeat<Identity, 3> repeat3{.strategy = id};
    constexpr auto result3 = repeat3.apply(x, default_context());
    static_assert(match(result3, x));
  };

  // ============================================================================
  // Operator Tests
  // ============================================================================

  "Addition operator"_test = [] {
    constexpr Symbol x;
    constexpr Constant<5> five;

    constexpr auto expr = x + five;

    static_assert(is_expression<decltype(expr)>);
    static_assert(is_add<decltype(expr)>);
  };

  "Multiplication operator"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    constexpr auto expr = x * y;

    static_assert(is_expression<decltype(expr)>);
    static_assert(is_mul<decltype(expr)>);
  };

  "Trig functions"_test = [] {
    constexpr Symbol x;

    constexpr auto sin_expr = sin(x);
    constexpr auto cos_expr = cos(x);
    constexpr auto tan_expr = tan(x);

    static_assert(is_trig_function<decltype(sin_expr)>);
    static_assert(is_trig_function<decltype(cos_expr)>);
    static_assert(is_trig_function<decltype(tan_expr)>);
  };

  // ============================================================================
  // Pattern Matching Tests
  // ============================================================================

  "AnyArg matches anything"_test = [] {
    constexpr Symbol x;
    constexpr Constant<5> five;

    static_assert(match(AnyArg{}, x));
    static_assert(match(AnyArg{}, five));
    static_assert(match(x, AnyArg{}));
  };

  "AnyConstant matches only constants"_test = [] {
    constexpr Symbol x;
    constexpr Constant<5> five;

    static_assert(match(AnyConstant{}, five));
    static_assert(!match(AnyConstant{}, x));
  };

  "AnySymbol matches only symbols"_test = [] {
    constexpr Symbol x;
    constexpr Constant<5> five;

    static_assert(match(AnySymbol{}, x));
    static_assert(!match(AnySymbol{}, five));
  };

  "Never matches nothing"_test = [] {
    constexpr Symbol x;
    constexpr Constant<5> five;

    static_assert(!match(Never{}, x));
    static_assert(!match(Never{}, five));
    static_assert(!match(x, Never{}));
  };

  return TestRegistry::result();
}
