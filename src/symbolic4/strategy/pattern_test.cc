// Tests for pattern matching and rewriting with special focus on Literal handling
#include "symbolic4/constants.h"
#include "symbolic4/strategy/combinator.h"
#include "symbolic4/strategy/pattern.h"
#include "symbolic4/strategy/rule.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Alias for detail namespace to avoid ambiguity
namespace sym_detail = tempura::symbolic4::detail;

auto main() -> int {
  // =========================================================================
  // BASIC PATTERN MATCHING
  // =========================================================================

  "PatternVar matches Symbol"_test = [] {
    Symbol<struct X> x;
    static_assert(match(x_, x));
    static_assert(match(x_, Symbol<struct Y>{}));
  };

  "PatternVar matches Constant"_test = [] {
    static_assert(match(x_, Constant<42>{}));
    static_assert(match(x_, 0_c));
    static_assert(match(x_, 1_c));
  };

  "PatternVar matches Literal"_test = [] {
    auto l = lit(3.14);
    static_assert(match(x_, decltype(l){}));
  };

  "PatternVar matches Expression"_test = [] {
    Symbol<struct X> x;
    static_assert(match(x_, decltype(x + 1_c){}));
    static_assert(match(x_, decltype(sin(x)){}));
  };

  "Symbol matches same Symbol"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;
    static_assert(match(x, x));
    static_assert(!match(x, y));
  };

  "Constant matches same value"_test = [] {
    static_assert(match(Constant<5>{}, Constant<5>{}));
    static_assert(!match(Constant<5>{}, Constant<6>{}));
  };

  "Expression matches structurally"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;
    auto expr1 = x + y;
    auto expr2 = x + y;
    auto expr3 = y + x;

    static_assert(match(decltype(expr1){}, decltype(expr2){}));
    static_assert(!match(decltype(expr1){}, decltype(expr3){}));
  };

  "Pattern with PatternVar matches expression"_test = [] {
    Symbol<struct X> x;
    auto pattern = x_ + 0_c;
    auto expr = (x * 2_c) + 0_c;

    static_assert(match(decltype(pattern){}, decltype(expr){}));
  };

  // =========================================================================
  // BINDING EXTRACTION - The critical Literal tests
  // =========================================================================

  "extractBindings captures Symbol"_test = [] {
    Symbol<struct X> x;
    auto pattern = x_ + 0_c;
    auto expr = x + 0_c;

    auto bindings = extractBindings(pattern, expr);
    static_assert(!sym_detail::isBindingFailure<decltype(bindings)>());

    auto bound = get(bindings, x_);
    static_assert(isSame<decltype(bound), decltype(x)>);
  };

  "extractBindings captures Constant"_test = [] {
    auto pattern = x_ + 0_c;
    auto expr = Constant<42>{} + 0_c;

    auto bindings = extractBindings(pattern, expr);
    static_assert(!sym_detail::isBindingFailure<decltype(bindings)>());

    auto bound = get(bindings, x_);
    static_assert(is_constant_v<decltype(bound)>);
    static_assert(decltype(bound)::value == 42);
  };

  "extractBindings captures Literal with value - CRITICAL"_test = [] {
    // CRITICAL TEST: Literal runtime values must be preserved!
    auto l = lit(3.14);
    auto pattern = x_ + 0_c;
    auto expr = l + 0_c;

    auto bindings = extractBindings(pattern, expr);
    static_assert(!sym_detail::isBindingFailure<decltype(bindings)>());

    auto bound = get(bindings, x_);
    static_assert(is_literal_v<decltype(bound)>);

    // The bound value should have the same runtime value
    expectNear(bound.effect_.value, 3.14);
  };

  "extractBindings captures Expression"_test = [] {
    Symbol<struct X> x;
    auto pattern = x_ * 0_c;
    auto expr = (x + 1_c) * 0_c;

    auto bindings = extractBindings(pattern, expr);
    static_assert(!sym_detail::isBindingFailure<decltype(bindings)>());

    auto bound = get(bindings, x_);
    static_assert(is_expression_v<decltype(bound)>);
    static_assert(isSame<get_op_t<decltype(bound)>, AddOp>);
  };

  "extractBindings fails on structure mismatch"_test = [] {
    auto pattern = x_ + y_;
    auto expr = Constant<5>{} * Constant<3>{};

    auto bindings = extractBindings(pattern, expr);
    static_assert(sym_detail::isBindingFailure<decltype(bindings)>());
  };

  "extractBindings with multiple pattern vars"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;
    auto pattern = x_ + y_;
    auto expr = x + y;

    auto bindings = extractBindings(pattern, expr);
    static_assert(!sym_detail::isBindingFailure<decltype(bindings)>());

    auto bound_x = get(bindings, x_);
    auto bound_y = get(bindings, y_);
    static_assert(isSame<decltype(bound_x), decltype(x)>);
    static_assert(isSame<decltype(bound_y), decltype(y)>);
  };

  // =========================================================================
  // SUBSTITUTION - The other critical Literal test
  // =========================================================================

  "substitute replaces PatternVar with Symbol"_test = [] {
    Symbol<struct X> x;
    auto pattern = x_ + 0_c;
    auto expr = x + 0_c;

    auto bindings = extractBindings(pattern, expr);
    auto result = substitute(x_, bindings);

    static_assert(isSame<decltype(result), decltype(x)>);
  };

  "substitute replaces PatternVar with Literal - CRITICAL"_test = [] {
    // CRITICAL TEST: Literal values must survive substitution!
    auto l = lit(2.718);
    auto pattern = x_ + 0_c;
    auto expr = l + 0_c;

    auto bindings = extractBindings(pattern, expr);
    auto result = substitute(x_, bindings);

    static_assert(is_literal_v<decltype(result)>);
    expectNear(result.effect_.value, 2.718);
  };

  "substitute with Literal in complex pattern - CRITICAL"_test = [] {
    auto l = lit(42.0);
    auto pattern = x_ * 0_c;
    auto expr = l * 0_c;
    auto replacement = x_ + 1_c;

    auto bindings = extractBindings(pattern, expr);
    auto result = substitute(replacement, bindings);

    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, AddOp>);

    auto lhs = result.template arg<0>();
    static_assert(is_literal_v<decltype(lhs)>);
    expectNear(lhs.effect_.value, 42.0);
  };

  // =========================================================================
  // REWRITE RULES - Now with Strategy interface
  // =========================================================================

  "Rewrite rule applies correctly"_test = [] {
    auto l = lit(123.456);
    auto add_0_c_rule = Rewrite{x_ + 0_c, x_};

    auto expr = l + 0_c;
    auto result = add_0_c_rule.apply(expr);

    // Rule should transform to just the literal
    static_assert(is_literal_v<decltype(result)>);
    expectNear(result.effect_.value, 123.456);
  };

  "Rewrite with expression replacement preserves Literal - CRITICAL"_test = [] {
    // Rule: x * 2 -> x + x, applied to lit(5.0) * 2
    auto l = lit(5.0);
    auto double_rule = Rewrite{x_ * 2_c, x_ + x_};

    auto expr = l * 2_c;
    auto result = double_rule.apply(expr);

    // Should be lit(5.0) + lit(5.0)
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, AddOp>);

    auto lhs = result.template arg<0>();
    auto rhs = result.template arg<1>();
    static_assert(is_literal_v<decltype(lhs)>);
    static_assert(is_literal_v<decltype(rhs)>);
    expectNear(lhs.effect_.value, 5.0);
    expectNear(rhs.effect_.value, 5.0);
  };

  // =========================================================================
  // REWRITE SYSTEM - Choice chain with Id at end
  // =========================================================================

  "RewriteSystem is Choice chain"_test = [] {
    auto r1 = Rewrite{x_ + 0_c, x_};
    auto r2 = Rewrite{x_ * 1_c, x_};

    // rules() creates Choice<r1, Choice<r2, Id>>
    auto system = rules(r1, r2);

    Symbol<struct X> x;

    // Test first rule
    auto expr1 = x + 0_c;
    auto result1 = system.apply(expr1);
    static_assert(isSame<decltype(result1), decltype(x)>);

    // Test second rule
    auto expr2 = x * 1_c;
    auto result2 = system.apply(expr2);
    static_assert(isSame<decltype(result2), decltype(x)>);

    // Test neither matches -> returns unchanged (Id)
    auto expr3 = x + 1_c;
    auto result3 = system.apply(expr3);
    static_assert(isSame<decltype(result3), decltype(expr3)>);
  };

  // =========================================================================
  // TRAVERSALS WITH RULES
  // =========================================================================

  "BottomUp traversal applies rules"_test = [] {
    auto l = lit(3.0);
    auto add_0_c_rule = Rewrite{x_ + 0_c, x_};

    // (l + 0_c) should simplify to l
    auto expr = l + 0_c;
    auto result = bottomup(add_0_c_rule).apply(expr);

    static_assert(is_literal_v<decltype(result)>);
    expectNear(result.effect_.value, 3.0);
  };

  "TopDown traversal applies rules"_test = [] {
    auto l = lit(7.5);
    auto add_0_c_rule = Rewrite{x_ + 0_c, x_};

    auto expr = l + 0_c;
    auto result = topdown(add_0_c_rule).apply(expr);

    static_assert(is_literal_v<decltype(result)>);
    expectNear(result.effect_.value, 7.5);
  };

  "Nested expression simplification"_test = [] {
    auto l = lit(2.0);
    auto rule_system = rules(
        Rewrite{x_ + 0_c, x_},
        Rewrite{x_ * 1_c, x_}
    );

    // (l + 0_c) * 1_c -> l * 1_c -> l
    auto expr = (l + 0_c) * 1_c;
    auto result = bottomup(rule_system).apply(expr);

    static_assert(is_literal_v<decltype(result)>);
    expectNear(result.effect_.value, 2.0);
  };

  // =========================================================================
  // MIXING LITERALS AND SYMBOLS
  // =========================================================================

  "Pattern doesn't confuse Symbol and Literal"_test = [] {
    Symbol<struct X> x;
    auto l = lit(5.0);

    static_assert(!match(x, decltype(l){}));
    static_assert(!match(decltype(l){}, x));

    static_assert(match(x_, x));
    static_assert(match(x_, decltype(l){}));
  };

  "Literal and Symbol captured separately"_test = [] {
    Symbol<struct X> x;
    auto l = lit(10.0);

    auto pattern = x_ + y_;
    auto expr = x + l;

    auto bindings = extractBindings(pattern, expr);
    static_assert(!sym_detail::isBindingFailure<decltype(bindings)>());

    auto bound_x = get(bindings, x_);
    auto bound_y = get(bindings, y_);

    static_assert(is_atom_v<decltype(bound_x)>);
    static_assert(!is_literal_v<decltype(bound_x)>);
    static_assert(is_literal_v<decltype(bound_y)>);
    expectNear(bound_y.effect_.value, 10.0);
  };

  // =========================================================================
  // STRATEGY COMPOSITION
  // =========================================================================

  "Strategy composition with >> and |"_test = [] {
    Symbol<struct X> x;
    auto r1 = Rewrite{x_ + 0_c, x_};
    auto r2 = Rewrite{x_ * 1_c, x_};

    // Choice: r1 | r2
    auto choice = r1 | r2;

    auto expr1 = x + 0_c;
    auto result1 = choice.apply(expr1);
    static_assert(isSame<decltype(result1), decltype(x)>);

    auto expr2 = x * 1_c;
    auto result2 = choice.apply(expr2);
    static_assert(isSame<decltype(result2), decltype(x)>);
  };

  // =========================================================================
  // END-TO-END: Complete rewrite workflow
  // =========================================================================

  "Full rewrite workflow with Literal value preservation"_test = [] {
    auto l = lit(999.0);
    Symbol<struct Y> y;

    auto pattern = (x_ + y_) * 0_c;
    auto expr = (l + y) * 0_c;
    auto replacement = y_ + x_;

    auto bindings = extractBindings(pattern, expr);
    static_assert(!sym_detail::isBindingFailure<decltype(bindings)>());

    auto bound_x = get(bindings, x_);
    auto bound_y = get(bindings, y_);
    static_assert(is_literal_v<decltype(bound_x)>);
    static_assert(!is_literal_v<decltype(bound_y)>);
    expectNear(bound_x.effect_.value, 999.0);

    auto result = substitute(replacement, bindings);
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, AddOp>);

    auto result_lhs = result.template arg<0>();
    auto result_rhs = result.template arg<1>();

    static_assert(!is_literal_v<decltype(result_lhs)>);
    static_assert(is_literal_v<decltype(result_rhs)>);
    expectNear(result_rhs.effect_.value, 999.0);
  };

  // =========================================================================
  // FIX COMBINATOR - Recursive strategies via fixed point
  // =========================================================================

  "Fix defines bottomup: fix([s](auto rec) { return all(rec) >> s; })"_test = [] {
    auto l = lit(4.0);
    auto add_zero_rule = Rewrite{x_ + 0_c, x_};

    // Define bottomup using fix instead of the dedicated BottomUp type
    auto my_bottomup = fix([&](auto rec) { return all(rec) >> add_zero_rule; });

    // (l + 0) should simplify to l
    auto expr = l + 0_c;
    auto result = my_bottomup.apply(expr);

    static_assert(is_literal_v<decltype(result)>);
    expectNear(result.effect_.value, 4.0);
  };

  "Fix defines topdown: fix([s](auto rec) { return s >> all(rec); })"_test = [] {
    auto l = lit(6.0);
    auto add_zero_rule = Rewrite{x_ + 0_c, x_};

    // Define topdown using fix
    auto my_topdown = fix([&](auto rec) { return add_zero_rule >> all(rec); });

    auto expr = l + 0_c;
    auto result = my_topdown.apply(expr);

    static_assert(is_literal_v<decltype(result)>);
    expectNear(result.effect_.value, 6.0);
  };

  "Fix nested expression: bottomup on ((l + 0) * 1)"_test = [] {
    auto l = lit(8.0);
    auto rule_system = rules(Rewrite{x_ + 0_c, x_}, Rewrite{x_ * 1_c, x_});

    // bottomup via fix
    auto my_bottomup = fix([&](auto rec) { return all(rec) >> rule_system; });

    // ((l + 0) * 1) -> (l * 1) -> l
    auto expr = (l + 0_c) * 1_c;
    auto result = my_bottomup.apply(expr);

    static_assert(is_literal_v<decltype(result)>);
    expectNear(result.effect_.value, 8.0);
  };

  "Fix equivalent to BottomUp"_test = [] {
    Symbol<struct X> x;
    auto add_zero_rule = Rewrite{x_ + 0_c, x_};

    auto via_fix = fix([&](auto rec) { return all(rec) >> add_zero_rule; });
    auto via_dedicated = bottomup(add_zero_rule);

    auto expr = (x + 0_c) + 0_c;

    auto result_fix = via_fix.apply(expr);
    auto result_ded = via_dedicated.apply(expr);

    // Both should produce the same result type
    static_assert(isSame<decltype(result_fix), decltype(result_ded)>);
  };

  return TestRegistry::result();
}
