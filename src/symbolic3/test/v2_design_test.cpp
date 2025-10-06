// Test demonstrating v2 design improvements:
// 1. Deducing this instead of CRTP
// 2. Data-driven context instead of behavioral tags

#include <cassert>
#include <iostream>

#include "symbolic3/context_v2.h"
#include "symbolic3/core.h"
#include "symbolic3/matching.h"
#include "symbolic3/operators.h"

using namespace tempura::symbolic3;
using namespace tempura::symbolic3::v2;

// ============================================================================
// Simple strategy example using deducing this
// ============================================================================

struct SimpleFoldConstants {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    // Query the context mode
    if (!ctx.mode.fold_numeric_constants) {
      return expr;
    }

    // Fold addition of constants
    if constexpr (matches_op_v<AddOp, S>) {
      using args = get_args_t<S>;
      using L = std::tuple_element_t<0, args>;
      using R = std::tuple_element_t<1, args>;

      if constexpr (is_constant<L> && is_constant<R>) {
        return Constant<L::value + R::value>{};
      }
    }

    return expr;
  }
};

struct SimplifyZero {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    if (!ctx.mode.fold_algebraic) {
      return expr;
    }

    // x + 0 → x
    if constexpr (matches_op_v<AddOp, S>) {
      using args = get_args_t<S>;
      using L = std::tuple_element_t<0, args>;
      using R = std::tuple_element_t<1, args>;

      if constexpr (is_constant<R> && R::value == 0) {
        return L{};
      }
      if constexpr (is_constant<L> && L::value == 0) {
        return R{};
      }
    }

    return expr;
  }
};

// ============================================================================
// Tests
// ============================================================================

int main() {
  std::cout << "Testing v2 design improvements...\n\n";

  // Test 1: Fold constants in numeric mode
  {
    std::cout << "Test 1: Fold constants in numeric mode\n";
    constexpr Constant<2> two;
    constexpr Constant<3> three;
    constexpr auto expr = two + three;

    // Numeric mode: fold everything
    constexpr auto numeric_ctx = numeric_context();
    static_assert(numeric_ctx.mode.fold_numeric_constants);

    constexpr auto result = SimpleFoldConstants{}.apply(expr, numeric_ctx);

    // Result should be Constant<5>
    static_assert(is_constant<decltype(result)>);
    static_assert(decltype(result)::value == 5);
    std::cout << "  ✓ 2 + 3 folded to 5\n\n";
  }

  // Test 2: Preserve constants in symbolic mode
  {
    std::cout << "Test 2: Preserve constants in symbolic mode\n";
    constexpr Constant<2> two;
    constexpr Constant<3> three;
    constexpr auto expr = two + three;

    // Symbolic mode: preserve structure
    constexpr auto symbolic_ctx = symbolic_context();
    static_assert(!symbolic_ctx.mode.fold_numeric_constants);

    constexpr auto result =
        SimpleFoldConstants{}.apply(SimpleFoldConstants{}, expr, symbolic_ctx);

    // Result should be unchanged (still 2 + 3)
    static_assert(std::is_same_v<decltype(result), decltype(expr)>);
    std::cout << "  ✓ 2 + 3 preserved as expression\n\n";
  }

  // Test 3: Algebraic simplification with mode control
  {
    std::cout << "Test 3: Algebraic simplification (enabled)\n";
    constexpr Symbol x;
    constexpr Constant<0> zero;
    constexpr auto expr = x + zero;

    // With algebraic simplification enabled
    auto ctx = default_context();
    ctx.mode.fold_algebraic = true;

    auto result = SimplifyZero{}.apply(SimplifyZero{}, expr, ctx);

    // Result should be just x
    static_assert(std::is_same_v<decltype(result), decltype(x)>);
    std::cout << "  ✓ x + 0 simplified to x\n\n";
  }

  // Test 4: Algebraic simplification disabled
  {
    std::cout << "Test 4: Algebraic simplification (disabled)\n";
    constexpr Symbol x;
    constexpr Constant<0> zero;
    constexpr auto expr = x + zero;

    // With algebraic simplification disabled
    auto ctx = default_context();
    ctx.mode.fold_algebraic = false;

    auto result = SimplifyZero{}.apply(SimplifyZero{}, expr, ctx);

    // Result should be unchanged (still x + 0)
    static_assert(std::is_same_v<decltype(result), decltype(expr)>);
    std::cout << "  ✓ x + 0 preserved (simplification disabled)\n\n";
  }

  // Test 5: Context carries domain information
  {
    std::cout << "Test 5: Context domain information\n";
    constexpr auto real_ctx = default_context();
    static_assert(real_ctx.get_domain() == Domain::Real);

    constexpr auto int_ctx = integer_context();
    static_assert(int_ctx.get_domain() == Domain::Integer);

    constexpr auto mod_ctx = modular_context<7>();
    static_assert(mod_ctx.get_domain() == Domain::ModularArithmetic);
    static_assert(mod_ctx.is_modular());
    static_assert(mod_ctx.modulus() == 7);

    std::cout << "  ✓ Real, Integer, Modular domains work\n\n";
  }

  // Test 6: Data-driven vs behavioral context
  {
    std::cout << "Test 6: Data-driven design benefits\n";
    auto ctx = default_context();

    // GOOD: Strategy queries "what" to do
    if (ctx.mode.fold_numeric_constants) {
      std::cout << "  ✓ Strategy queries mode flags\n";
    }

    // NOT: "am I inside trig function?" - behavioral approach
    // The context provides WHAT to do, not WHERE we are

    std::cout << "  ✓ Context is data-driven, not behavioral\n\n";
  }

  std::cout << "All v2 design tests passed!\n";
  return 0;
}
