#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  "Basic symbol and constant"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr Constant<5> five;

    // Different symbols have different types
    static_assert(!std::is_same_v<decltype(x), decltype(y)>);

    // Same constant values have same type
    constexpr Constant<5> also_five;
    static_assert(std::is_same_v<decltype(five), decltype(also_five)>);
  };

  "Context depth tracking"_test = [] {
    constexpr auto ctx1 = TransformContext<>{};
    static_assert(ctx1.depth == 0);

    constexpr auto ctx2 = ctx1.increment_depth<1>();
    static_assert(ctx2.depth == 1);

    constexpr auto ctx3 = ctx2.increment_depth<5>();
    static_assert(ctx3.depth == 6);
  };

  // Note: Tag management (with/without/has) not yet implemented in
  // TransformContext These tests are TODO for when context tagging is needed

  "Identity strategy preserves input"_test = [] {
    constexpr Symbol x;
    constexpr Identity id;
    constexpr auto ctx = default_context();

    constexpr auto result = id.apply(x, ctx);
    static_assert(std::is_same_v<decltype(result), decltype(x)>);
  };

  "Sequential composition"_test = [] {
    constexpr Symbol x;
    constexpr Identity id1;
    constexpr Identity id2;

    constexpr auto composed = id1 >> id2;
    constexpr auto result = composed.apply(x, default_context());

    static_assert(std::is_same_v<decltype(result), decltype(x)>);
  };

  "FixPoint terminates on no change"_test = [] {
    constexpr Symbol x;
    constexpr Identity id;
    constexpr FixPoint<Identity, 5> fixpoint{.strategy = id};

    constexpr auto result = fixpoint.apply(x, default_context());
    static_assert(std::is_same_v<decltype(result), decltype(x)>);
  };

  return TestRegistry::result();
}
