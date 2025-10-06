// Tests for strategy_v2.h - deducing this strategy pattern

#include "symbolic3/strategy_v2.h"

#include <cassert>
#include <iostream>

#include "symbolic3/context_v2.h"
#include "symbolic3/core.h"
#include "symbolic3/matching.h"
#include "symbolic3/operators.h"

using namespace tempura::symbolic3;
using namespace tempura::symbolic3::v2;

// Test strategy: counts how many times it's applied
struct CountingStrategy {
  mutable int count = 0;

  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    count++;
    return expr;
  }
};

// Test strategy: returns identity
struct IdentityStrategy {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context /* ctx */) const {
    return expr;
  }
};

// Test strategy: checks if constant and marks it
struct MarkConstantStrategy {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context /* ctx */) const {
    if constexpr (is_constant<S>) {
      // In real use would transform, here just return
      return expr;
    } else {
      return expr;
    }
  }
};

int main() {
  std::cout << "Testing strategy_v2.h...\n";

  constexpr Symbol x;
  constexpr Constant<5> five;
  constexpr auto ctx = default_context();

  // ============================================================================
  // Test 1: Basic Strategy Application
  // ============================================================================
  {
    constexpr auto result = IdentityStrategy{}.apply(five, ctx);
    static_assert(std::is_same_v<decltype(result), decltype(five)>);

    std::cout << "  ✓ Basic strategy application works\n";
  }

  // ============================================================================
  // Test 2: Identity combinator
  // ============================================================================
  {
    constexpr Identity identity;
    constexpr auto result = identity.apply(five, ctx);
    static_assert(std::is_same_v<decltype(result), decltype(five)>);

    constexpr auto result2 = identity.apply(x, ctx);
    static_assert(std::is_same_v<decltype(result2), decltype(x)>);

    std::cout << "  ✓ Identity combinator works\n";
  }

  // ============================================================================
  // Test 3: Fail combinator
  // ============================================================================
  {
    constexpr Fail fail;
    constexpr auto result = fail.apply(five, ctx);
    static_assert(std::is_same_v<decltype(result), Never>);

    std::cout << "  ✓ Fail combinator works\n";
  }

  // ============================================================================
  // Test 4: Sequence combinator (operator>>)
  // ============================================================================
  {
    auto s1 = IdentityStrategy{};
    auto s2 = IdentityStrategy{};
    auto seq = Sequence{s1, s2};

    auto result = seq.apply(five, ctx);
    static_assert(std::is_same_v<decltype(result), decltype(five)>);

    std::cout << "  ✓ Sequence combinator works\n";
  }

  // ============================================================================
  // Test 5: Choice combinator (operator|)
  // ============================================================================
  {
    // First strategy succeeds
    auto s1 = IdentityStrategy{};
    auto s2 = Fail{};
    auto choice = Choice{s1, s2};

    auto result = choice.apply(five, ctx);
    // Should use first strategy (identity)
    static_assert(std::is_same_v<decltype(result), decltype(five)>);

    std::cout << "  ✓ Choice combinator (first succeeds) works\n";
  }

  // ============================================================================
  // Test 6: Try combinator
  // ============================================================================
  {
    // Try wraps a strategy to never fail
    auto strategy = IdentityStrategy{};
    auto try_strategy = Try{strategy};

    auto result = try_strategy.apply(five, ctx);
    static_assert(std::is_same_v<decltype(result), decltype(five)>);

    std::cout << "  ✓ Try combinator works\n";
  }

  // ============================================================================
  // Test 7: When combinator with predicate
  // ============================================================================
  {
    // Predicate: only apply to constants
    auto pred = [](auto expr, auto /* ctx */) {
      return is_constant<decltype(expr)>;
    };

    auto strategy = IdentityStrategy{};
    auto when_strategy = When{pred, strategy};

    // Apply to constant - should succeed
    auto result1 = when_strategy.apply(five, ctx);
    static_assert(std::is_same_v<decltype(result1), decltype(five)>);

    // Apply to symbol - predicate fails, returns original
    auto result2 = when_strategy.apply(x, ctx);
    static_assert(std::is_same_v<decltype(result2), decltype(x)>);

    std::cout << "  ✓ When combinator with predicate works\n";
  }

  // ============================================================================
  // Test 8: Repeat combinator (fixed count)
  // ============================================================================
  {
    CountingStrategy counter;
    auto repeat = Repeat<3>{counter};

    repeat.apply(five, ctx);
    assert(counter.count == 3);

    std::cout << "  ✓ Repeat<N> combinator works\n";
  }

  // ============================================================================
  // Test 9: Strategy Concept
  // ============================================================================
  {
    // Check that our strategies satisfy the Strategy concept
    static_assert(Strategy<IdentityStrategy>);
    static_assert(Strategy<Identity>);
    static_assert(Strategy<Fail>);
    static_assert(Strategy<CountingStrategy>);

    // Combinators should also be strategies
    auto seq = Sequence{Identity{}, Identity{}};
    static_assert(Strategy<decltype(seq)>);

    auto choice = Choice{Identity{}, Fail{}};
    static_assert(Strategy<decltype(choice)>);

    std::cout << "  ✓ Strategy concept works\n";
  }

  // ============================================================================
  // Test 10: Chaining with operator>> and operator|
  // ============================================================================
  {
    auto s1 = IdentityStrategy{};
    auto s2 = IdentityStrategy{};
    auto s3 = IdentityStrategy{};

    // Chain with >>
    auto pipeline = s1 >> s2 >> s3;
    auto result = pipeline.apply(five, ctx);

    std::cout << "  ✓ Operator>> chaining works\n";

    // Choice with |
    auto alternatives = s1 | s2 | s3;
    auto result2 = alternatives.apply(five, ctx);

    std::cout << "  ✓ Operator| chaining works\n";
  }

  // ============================================================================
  // Test 11: FixPoint combinator
  // ============================================================================
  {
    // Simple fixpoint: apply until no change
    auto strategy = IdentityStrategy{};
    auto fixpoint = FixPoint{strategy};

    // Should terminate immediately (identity doesn't change anything)
    auto result = fixpoint.apply(five, ctx);
    static_assert(std::is_same_v<decltype(result), decltype(five)>);

    std::cout << "  ✓ FixPoint combinator works\n";
  }

  // ============================================================================
  // Test 12: Constexpr strategy application
  // ============================================================================
  {
    constexpr Identity identity;
    constexpr Constant<42> val;
    constexpr auto result = identity.apply(val, ctx);

    static_assert(std::is_same_v<decltype(result), decltype(val)>);
    static_assert(result.value == 42);

    std::cout << "  ✓ Constexpr strategy application works\n";
  }

  // ============================================================================
  // Test 13: Context threading through combinators
  // ============================================================================
  {
    // Strategy that checks context
    struct ContextCheckStrategy {
      template <Symbolic S, typename Context>
      auto apply(S expr, Context ctx) const {
        // Verify context is passed through
        assert(ctx.domain == Domain::Real);
        return expr;
      }
    };

    auto s = ContextCheckStrategy{};
    auto seq = s >> s >> s;

    seq.apply(five, ctx);

    std::cout << "  ✓ Context threading through combinators works\n";
  }

  // ============================================================================
  // Test 14: Never type propagation
  // ============================================================================
  {
    constexpr Fail fail;
    constexpr auto result = fail.apply(five, ctx);

    static_assert(std::is_same_v<decltype(result), Never>);

    // Never should be detected by Choice
    auto choice = Choice{fail, Identity{}};
    auto result2 = choice.apply(five, ctx);
    static_assert(std::is_same_v<decltype(result2), decltype(five)>);

    std::cout << "  ✓ Never type propagation works\n";
  }

  std::cout << "\nAll strategy_v2 tests passed! ✅\n";
  return 0;
}
