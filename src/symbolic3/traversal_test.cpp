#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  // ============================================================================
  // Traversal Strategy Tests
  // ============================================================================

  "Fold with Identity"_test = [] {
    constexpr Symbol x;
    constexpr Identity id;
    constexpr auto folded = fold(id);

    constexpr auto result = folded.apply(x, default_context());
    static_assert(match(result, x));
  };

  "Fold on expression"_test = [] {
    constexpr Symbol x;
    constexpr Constant<2> two;
    constexpr auto expr = x + two;

    constexpr Identity id;
    constexpr auto folded = fold(id);

    constexpr auto result = folded.apply(expr, default_context());
    static_assert(is_add<decltype(result)>);
  };

  "Unfold with Identity"_test = [] {
    constexpr Symbol x;
    constexpr Identity id;
    constexpr auto unfolded = unfold(id);

    constexpr auto result = unfolded.apply(x, default_context());
    static_assert(match(result, x));
  };

  "Innermost with Identity"_test = [] {
    constexpr Symbol x;
    constexpr Constant<2> two;
    constexpr auto expr = x + two;

    constexpr Identity id;
    constexpr auto inner = innermost(id);

    constexpr auto result = inner.apply(expr, default_context());
    static_assert(is_add<decltype(result)>);
  };

  "Outermost with Identity"_test = [] {
    constexpr Symbol x;
    constexpr Constant<2> two;
    constexpr auto expr = x + two;

    constexpr Identity id;
    constexpr auto outer = outermost(id);

    constexpr auto result = outer.apply(expr, default_context());
    static_assert(is_add<decltype(result)>);
  };

  "TopDown traversal"_test = [] {
    constexpr Symbol x;
    constexpr Identity id;
    constexpr auto td = topdown(id);

    constexpr auto result = td.apply(x, default_context());
    static_assert(match(result, x));
  };

  "BottomUp traversal"_test = [] {
    constexpr Symbol x;
    constexpr Identity id;
    constexpr auto bu = bottomup(id);

    constexpr auto result = bu.apply(x, default_context());
    static_assert(match(result, x));
  };

  return TestRegistry::result();
}
