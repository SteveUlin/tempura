// Tests for pattern matching with special focus on Literal handling
#include "symbolic4/constants.h"
#include "symbolic4/strategy/pattern.h"
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

  return TestRegistry::result();
}
