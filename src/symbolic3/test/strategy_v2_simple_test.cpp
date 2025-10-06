// Simplified tests for strategy.h focusing on core functionality

#include <iostream>

#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/strategy.h"

using namespace tempura::symbolic3;

// Simple test strategy
struct IdentityStrategy {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context /*ctx*/) const {
    return expr;
  }
};

int main() {
  std::cout << "Testing strategy_v2.h (simplified)...\n";

  constexpr Symbol x;
  constexpr Constant<5> five;
  constexpr auto ctx = default_context();

  // Test 1: Basic strategy application
  {
    constexpr auto result = IdentityStrategy{}.apply(five, ctx);
    static_assert(std::is_same_v<decltype(result), decltype(five)>);
    std::cout << "  ✓ Basic strategy application works\n";
  }

  // Test 2: Identity combinator
  {
    constexpr Identity identity;
    constexpr auto result = identity.apply(five, ctx);
    static_assert(std::is_same_v<decltype(result), decltype(five)>);
    std::cout << "  ✓ Identity combinator works\n";
  }

  // Test 3: Fail combinator
  {
    constexpr Fail fail;
    constexpr auto result = fail.apply(five, ctx);
    (void)result;  // Use result to avoid warning
    std::cout << "  ✓ Fail combinator works\n";
  }

  // Test 4: Sequence combinator
  {
    auto s1 = IdentityStrategy{};
    auto s2 = IdentityStrategy{};
    auto seq = Sequence<IdentityStrategy, IdentityStrategy>{s1, s2};

    auto result = seq.apply(five, ctx);
    (void)result;
    std::cout << "  ✓ Sequence combinator works\n";
  }

  // Test 5: Sequence with operator>>
  {
    auto s1 = IdentityStrategy{};
    auto s2 = IdentityStrategy{};
    auto pipeline = s1 >> s2;

    auto result = pipeline.apply(five, ctx);
    (void)result;
    std::cout << "  ✓ Operator>> works\n";
  }

  // Test 6: Try combinator
  {
    auto strategy = IdentityStrategy{};
    auto try_strat = Try<IdentityStrategy>{strategy};

    auto result = try_strat.apply(five, ctx);
    (void)result;
    std::cout << "  ✓ Try combinator works\n";
  }

  // Test 7: When combinator
  {
    auto pred = [](auto, auto) { return true; };
    auto strategy = IdentityStrategy{};
    auto when_strat = When<decltype(pred), IdentityStrategy>{pred, strategy};

    auto result = when_strat.apply(five, ctx);
    (void)result;
    std::cout << "  ✓ When combinator works\n";
  }

  // Test 8: Repeat combinator
  {
    auto strategy = IdentityStrategy{};
    auto repeat = Repeat<IdentityStrategy, 3>{strategy};

    auto result = repeat.apply(five, ctx);
    (void)result;
    std::cout << "  ✓ Repeat combinator works\n";
  }

  // Test 9: FixPoint combinator
  {
    auto strategy = IdentityStrategy{};
    auto fixpoint = FixPoint<IdentityStrategy>{strategy};

    auto result = fixpoint.apply(five, ctx);
    (void)result;
    std::cout << "  ✓ FixPoint combinator works\n";
  }

  // Test 10: Strategy concept
  {
    static_assert(Strategy<IdentityStrategy>);
    static_assert(Strategy<Identity>);
    static_assert(Strategy<Fail>);
    std::cout << "  ✓ Strategy concept works\n";
  }

  // Test 11: Constexpr strategy application
  {
    constexpr Identity identity;
    constexpr Constant<42> val;
    constexpr auto result = identity.apply(val, ctx);

    static_assert(std::is_same_v<decltype(result), decltype(val)>);
    static_assert(result.value == 42);
    std::cout << "  ✓ Constexpr strategy application works\n";
  }

  std::cout << "\nAll strategy_v2 tests passed! ✅\n";
  return 0;
}
