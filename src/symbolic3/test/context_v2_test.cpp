// Tests for context_v2.h - data-driven context system

#include "symbolic3/context_v2.h"

#include <cassert>
#include <iostream>
#include <numbers>

using namespace tempura::symbolic3::v2;

int main() {
  std::cout << "Testing context_v2.h...\n";

  // ============================================================================
  // Test 1: SimplificationMode structure
  // ============================================================================
  {
    SimplificationMode mode;
    mode.fold_numeric_constants = true;
    mode.fold_algebraic = false;
    mode.preserve_special_values = true;

    assert(mode.fold_numeric_constants);
    assert(!mode.fold_algebraic);
    assert(mode.preserve_special_values);

    std::cout << "  ✓ SimplificationMode flags work\n";
  }

  // ============================================================================
  // Test 2: default_context factory
  // ============================================================================
  {
    constexpr auto ctx = default_context();
    static_assert(ctx.domain == Domain::Real);
    static_assert(ctx.depth == 0);

    // Mode should enable folding by default
    assert(ctx.mode.fold_numeric_constants);
    assert(ctx.mode.fold_algebraic);
    assert(!ctx.mode.preserve_special_values);

    std::cout << "  ✓ default_context() factory works\n";
  }

  // ============================================================================
  // Test 3: numeric_context factory
  // ============================================================================
  {
    constexpr auto ctx = numeric_context();
    static_assert(ctx.domain == Domain::Real);

    // Numeric mode: aggressive folding
    static_assert(ctx.mode.fold_numeric_constants);
    static_assert(ctx.mode.fold_symbolic_constants);
    static_assert(!ctx.mode.preserve_special_values);
    static_assert(!ctx.mode.prefer_exact);

    std::cout << "  ✓ numeric_context() factory works\n";
  }

  // ============================================================================
  // Test 4: symbolic_context factory
  // ============================================================================
  {
    constexpr auto ctx = symbolic_context();
    static_assert(ctx.domain == Domain::Real);

    // Symbolic mode: preserve structure
    static_assert(!ctx.mode.fold_numeric_constants);
    static_assert(!ctx.mode.fold_symbolic_constants);
    static_assert(ctx.mode.preserve_special_values);
    static_assert(ctx.mode.preserve_exact_rationals);
    static_assert(ctx.mode.prefer_exact);

    std::cout << "  ✓ symbolic_context() factory works\n";
  }

  // ============================================================================
  // Test 5: integer_context factory
  // ============================================================================
  {
    constexpr auto ctx = integer_context();
    static_assert(ctx.domain == Domain::Integer);
    static_assert(ctx.is_integer());
    static_assert(!ctx.is_real());
    static_assert(!ctx.is_complex());

    std::cout << "  ✓ integer_context() factory works\n";
  }

  // ============================================================================
  // Test 6: modular_context factory
  // ============================================================================
  {
    constexpr auto ctx = modular_context<7>();
    static_assert(ctx.domain == Domain::ModularArithmetic);
    static_assert(ctx.is_modular());
    static_assert(ctx.modulus() == 7);

    // Test with different modulus
    constexpr auto ctx2 = modular_context<13>();
    static_assert(ctx2.modulus() == 13);

    std::cout << "  ✓ modular_context<N>() factory works\n";
  }

  // ============================================================================
  // Test 7: angle_context factory
  // ============================================================================
  {
    constexpr auto ctx = angle_context();
    static_assert(ctx.domain == Domain::Real);
    static_assert(ctx.angle_period() == 2.0 * std::numbers::pi);

    std::cout << "  ✓ angle_context() factory works\n";
  }

  // ============================================================================
  // Test 8: Domain predicates
  // ============================================================================
  {
    constexpr auto real_ctx = default_context();
    static_assert(real_ctx.is_real());
    static_assert(!real_ctx.is_complex());
    static_assert(!real_ctx.is_integer());
    static_assert(!real_ctx.is_modular());

    constexpr auto int_ctx = integer_context();
    static_assert(!int_ctx.is_real());
    static_assert(int_ctx.is_integer());

    constexpr auto mod_ctx = modular_context<5>();
    static_assert(mod_ctx.is_modular());

    std::cout << "  ✓ Domain predicates work\n";
  }

  // ============================================================================
  // Test 9: Depth tracking
  // ============================================================================
  {
    constexpr auto ctx = default_context();
    static_assert(ctx.depth == 0);

    constexpr auto ctx1 = ctx.increment_depth<1>();
    static_assert(ctx1.depth == 1);

    constexpr auto ctx2 = ctx1.increment_depth<2>();
    static_assert(ctx2.depth == 3);

    constexpr auto ctx_reset = ctx2.reset_depth();
    static_assert(ctx_reset.depth == 0);

    std::cout << "  ✓ Depth tracking works\n";
  }

  // ============================================================================
  // Test 10: Mode modification (immutable)
  // ============================================================================
  {
    auto ctx = default_context();
    assert(ctx.mode.fold_numeric_constants);

    // Create new context with modified mode
    auto new_ctx = ctx.without_constant_folding();
    assert(!new_ctx.mode.fold_numeric_constants);
    assert(!new_ctx.mode.fold_symbolic_constants);

    // Original unchanged
    assert(ctx.mode.fold_numeric_constants);

    std::cout << "  ✓ Mode modification (immutable) works\n";
  }

  // ============================================================================
  // Test 11: with_symbolic_constants
  // ============================================================================
  {
    auto ctx = numeric_context();
    assert(!ctx.mode.preserve_special_values);

    auto symbolic = ctx.with_symbolic_constants();
    assert(!symbolic.mode.fold_symbolic_constants);
    assert(symbolic.mode.preserve_special_values);

    std::cout << "  ✓ with_symbolic_constants() works\n";
  }

  // ============================================================================
  // Test 12: Custom mode via with_mode
  // ============================================================================
  {
    auto ctx = default_context();

    SimplificationMode custom_mode;
    custom_mode.fold_numeric_constants = false;
    custom_mode.fold_algebraic = true;
    custom_mode.preserve_special_values = true;

    auto custom_ctx = ctx.with_mode(custom_mode);
    assert(!custom_ctx.mode.fold_numeric_constants);
    assert(custom_ctx.mode.fold_algebraic);
    assert(custom_ctx.mode.preserve_special_values);

    std::cout << "  ✓ with_mode() custom mode works\n";
  }

  // ============================================================================
  // Test 13: Mode preservation across depth changes
  // ============================================================================
  {
    auto ctx = numeric_context();
    assert(ctx.mode.fold_symbolic_constants);

    auto deeper = ctx.increment_depth<1>();
    assert(deeper.mode.fold_symbolic_constants);  // Mode preserved

    std::cout << "  ✓ Mode preserved across depth changes\n";
  }

  // ============================================================================
  // Test 14: Compile-time evaluation
  // ============================================================================
  {
    // All context operations should be constexpr
    constexpr auto ctx1 = default_context();
    constexpr auto ctx2 = ctx1.increment_depth<1>();
    constexpr auto ctx3 = ctx2.reset_depth();

    static_assert(ctx1.depth == 0);
    static_assert(ctx2.depth == 1);
    static_assert(ctx3.depth == 0);

    std::cout << "  ✓ All operations are constexpr\n";
  }

  std::cout << "\nAll context_v2 tests passed! ✅\n";
  return 0;
}
