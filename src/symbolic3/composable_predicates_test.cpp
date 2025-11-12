// Test composable predicate system
#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/pattern_matching.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;
using namespace tempura::symbolic3::predicates;

int main() {
  // ==========================================================================
  // Basic Predicate Building Blocks
  // ==========================================================================

  "is_constant predicate"_test = [] {
    constexpr Symbol y;
    constexpr auto pattern = x_;

    // Test with constant
    {
      constexpr auto expr = 5_c;
      constexpr auto bindings = extractBindings(pattern, expr);
      constexpr auto pred = var_is_constant(x_);
      static_assert(pred(bindings), "Should detect constant");
    }

    // Test with symbol
    {
      constexpr auto expr = y;
      constexpr auto bindings = extractBindings(pattern, expr);
      constexpr auto pred = var_is_constant(x_);
      static_assert(!pred(bindings), "Should not match symbol");
    }
  };

  "is_symbol predicate"_test = [] {
    constexpr Symbol y;
    constexpr auto pattern = x_;

    // Test with symbol
    {
      constexpr auto expr = y;
      constexpr auto bindings = extractBindings(pattern, expr);
      constexpr auto pred = var_is_symbol(x_);
      static_assert(pred(bindings), "Should detect symbol");
    }

    // Test with constant
    {
      constexpr auto expr = 5_c;
      constexpr auto bindings = extractBindings(pattern, expr);
      constexpr auto pred = var_is_symbol(x_);
      static_assert(!pred(bindings), "Should not match constant");
    }
  };

  "is_expression predicate"_test = [] {
    constexpr Symbol a;
    constexpr auto pattern = x_;

    // Test with expression
    {
      constexpr auto expr = a + 1_c;
      constexpr auto bindings = extractBindings(pattern, expr);
      constexpr auto pred = var_is_expression(x_);
      static_assert(pred(bindings), "Should detect expression");
    }

    // Test with constant
    {
      constexpr auto expr = 5_c;
      constexpr auto bindings = extractBindings(pattern, expr);
      constexpr auto pred = var_is_expression(x_);
      static_assert(!pred(bindings), "Should not match constant");
    }
  };

  // ==========================================================================
  // Comparison Predicates
  // ==========================================================================

  "var_less_than predicate"_test = [] {
    constexpr auto pattern = x_ + y_;
    constexpr auto expr = 2_c + 5_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    constexpr auto pred = var_less_than(x_, y_);
    static_assert(pred(bindings), "2 < 5 should be true");
  };

  "var_greater_than predicate"_test = [] {
    constexpr auto pattern = x_ + y_;
    constexpr auto expr = 5_c + 2_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    constexpr auto pred = var_greater_than(x_, y_);
    static_assert(pred(bindings), "5 > 2 should be true");
  };

  "var_equal_to predicate"_test = [] {
    constexpr auto pattern = x_ + y_;
    constexpr auto expr = 5_c + 5_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    constexpr auto pred = var_equal_to(x_, y_);
    static_assert(pred(bindings), "5 == 5 should be true");
  };

  "var_not_equal_to predicate"_test = [] {
    constexpr auto pattern = x_ + y_;
    constexpr auto expr = 5_c + 3_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    constexpr auto pred = var_not_equal_to(x_, y_);
    static_assert(pred(bindings), "5 != 3 should be true");
  };

  // ==========================================================================
  // Logical Operators: AND
  // ==========================================================================

  "AND operator (&&)"_test = [] {
    constexpr auto pattern = x_ + y_;
    constexpr auto expr = 2_c + 5_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    // Both conditions true
    constexpr auto pred1 = var_is_constant(x_) && var_is_constant(y_);
    static_assert(pred1(bindings), "Both x and y are constants");

    // First false
    constexpr auto pred2 = var_is_symbol(x_) && var_is_constant(y_);
    static_assert(!pred2(bindings), "x is not a symbol");

    // Second false
    constexpr auto pred3 = var_is_constant(x_) && var_is_symbol(y_);
    static_assert(!pred3(bindings), "y is not a symbol");

    // Both false
    constexpr auto pred4 = var_is_symbol(x_) && var_is_symbol(y_);
    static_assert(!pred4(bindings), "Neither x nor y is a symbol");
  };

  // ==========================================================================
  // Logical Operators: OR
  // ==========================================================================

  "OR operator (||)"_test = [] {
    constexpr Symbol a;
    constexpr auto pattern = x_ + y_;
    constexpr auto expr = a + 5_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    // Both true
    constexpr auto pred1 = var_is_symbol(x_) || var_is_constant(y_);
    static_assert(pred1(bindings), "x is symbol OR y is constant");

    // First true, second false
    constexpr auto pred2 = var_is_symbol(x_) || var_is_symbol(y_);
    static_assert(pred2(bindings), "x is symbol");

    // First false, second true
    constexpr auto pred3 = var_is_constant(x_) || var_is_constant(y_);
    static_assert(pred3(bindings), "y is constant");

    // Both false
    constexpr auto pred4 = var_is_expression(x_) || var_is_expression(y_);
    static_assert(!pred4(bindings), "Neither is expression");
  };

  // ==========================================================================
  // Logical Operators: NOT
  // ==========================================================================

  "NOT operator (!)"_test = [] {
    constexpr Symbol a;
    constexpr auto pattern = x_;

    // Negate true condition
    {
      constexpr auto expr = a;
      constexpr auto bindings = extractBindings(pattern, expr);
      constexpr auto pred = !var_is_constant(x_);
      static_assert(pred(bindings), "x is not a constant");
    }

    // Negate false condition
    {
      constexpr auto expr = 5_c;
      constexpr auto bindings = extractBindings(pattern, expr);
      constexpr auto pred = !var_is_symbol(x_);
      static_assert(pred(bindings), "x is not a symbol");
    }
  };

  // ==========================================================================
  // Complex Compositions
  // ==========================================================================

  "Complex predicate composition"_test = [] {
    constexpr Symbol a, b;
    constexpr auto pattern = x_ + y_;
    constexpr auto expr = a + 5_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    // (x is symbol) AND ((y is constant) OR (y is expression))
    constexpr auto pred =
        var_is_symbol(x_) && (var_is_constant(y_) || var_is_expression(y_));
    static_assert(pred(bindings), "Complex condition should hold");
  };

  "Chained AND operators"_test = [] {
    constexpr auto pattern = x_ + y_ + z_;
    constexpr auto expr = 1_c + 2_c + 3_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    // x, y, and z are all constants
    constexpr auto pred =
        var_is_constant(x_) && var_is_constant(y_) && var_is_constant(z_);
    static_assert(pred(bindings), "All three are constants");
  };

  "Chained OR operators"_test = [] {
    constexpr Symbol a;
    constexpr auto pattern = x_;
    constexpr auto expr = a;
    constexpr auto bindings = extractBindings(pattern, expr);

    // x is constant OR symbol OR expression
    constexpr auto pred =
        var_is_constant(x_) || var_is_symbol(x_) || var_is_expression(x_);
    static_assert(pred(bindings), "x is at least one of these");
  };

  "De Morgan's Laws"_test = [] {
    constexpr Symbol a;
    constexpr auto pattern = x_ + y_;
    constexpr auto expr = a + 5_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    // !(A && B) == !A || !B
    constexpr auto pred1 = !(var_is_constant(x_) && var_is_constant(y_));
    constexpr auto pred2 = !var_is_constant(x_) || !var_is_constant(y_);
    static_assert(pred1(bindings) == pred2(bindings), "De Morgan's law 1");

    // !(A || B) == !A && !B
    constexpr auto pred3 = !(var_is_symbol(x_) || var_is_symbol(y_));
    constexpr auto pred4 = !var_is_symbol(x_) && !var_is_symbol(y_);
    static_assert(pred3(bindings) == pred4(bindings), "De Morgan's law 2");
  };

  // ==========================================================================
  // Predicates in Rewrite Rules
  // ==========================================================================

  "Rewrite with simple predicate"_test = [] {
    // Swap if second is smaller: x + y → y + x when y < x
    constexpr auto rule = Rewrite{x_ + y_, y_ + x_, var_less_than(y_, x_)};

    // Should swap
    {
      constexpr auto expr = 5_c + 2_c;
      constexpr auto result = rule.apply(expr, default_context());
      static_assert(match(result, 2_c + 5_c), "Should swap to canonical order");
    }

    // Should not swap
    {
      constexpr auto expr = 2_c + 5_c;
      constexpr auto result = rule.apply(expr, default_context());
      static_assert(match(result, 2_c + 5_c), "Already in canonical order");
    }
  };

  "Rewrite with composed predicate"_test = [] {
    // Remove zero only if other operand is not zero: 0 + x → x when x != 0
    constexpr auto rule = Rewrite{
        0_c + x_, x_,
        var_is_constant(x_) &&
            !var_equal_to(x_, PatternVar{})  // Not matching self
    };

    constexpr auto expr = 0_c + 5_c;
    constexpr auto result = rule.apply(expr, default_context());
    static_assert(match(result, 5_c), "Should remove zero");
  };

  "Rewrite with type checking predicate"_test = [] {
    // Double constants only: x → 2*x when x is constant
    constexpr auto rule = Rewrite{x_, x_ * 2_c, var_is_constant(x_)};

    // Should double constant
    {
      constexpr auto expr = 3_c;
      constexpr auto result = rule.apply(expr, default_context());
      static_assert(match(result, 3_c * 2_c), "Should double constant");
    }

    // Should not double symbol
    {
      constexpr Symbol a;
      constexpr auto expr = a;
      constexpr auto result = rule.apply(expr, default_context());
      static_assert(match(result, a), "Should not double symbol");
    }
  };

  "Rewrite with multi-condition predicate"_test = [] {
    // Commutative swap with multiple conditions:
    // x + y → y + x when (x is constant) AND (y is symbol) AND (x > 0)
    constexpr auto rule = Rewrite{x_ + y_, y_ + x_,
                                  var_is_constant(x_) && var_is_symbol(y_) &&
                                      var_greater_than_literal(x_, 0_c)};

    constexpr Symbol a;

    // Should swap (all conditions met)
    {
      constexpr auto expr = 5_c + a;
      constexpr auto result = rule.apply(expr, default_context());
      static_assert(match(result, a + 5_c), "Should swap");
    }

    // Should not swap (x is not > 0)
    {
      constexpr auto expr = Constant<-1>{} + a;
      constexpr auto result = rule.apply(expr, default_context());
      static_assert(match(result, Constant<-1>{} + a), "Should not swap");
    }
  };

  // ==========================================================================
  // Short-Circuit Evaluation
  // ==========================================================================

  "AND short-circuits on false"_test = [] {
    constexpr auto pattern = x_;
    constexpr auto expr = 5_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    // First condition false, second shouldn't be evaluated
    // (In practice, both are evaluated at compile-time, but logically
    // the second's result doesn't matter)
    constexpr auto pred = var_is_symbol(x_) && var_is_expression(x_);
    static_assert(!pred(bindings), "Should be false");
  };

  "OR short-circuits on true"_test = [] {
    constexpr auto pattern = x_;
    constexpr auto expr = 5_c;
    constexpr auto bindings = extractBindings(pattern, expr);

    // First condition true, second's result doesn't matter
    constexpr auto pred = var_is_constant(x_) || var_is_expression(x_);
    static_assert(pred(bindings), "Should be true");
  };

  return 0;
}
