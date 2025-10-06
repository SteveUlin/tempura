// Example: Using symbolic3 combinator system
// Demonstrates context-aware transformations and strategy composition

#include <print>

#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

auto main() -> int {
  std::println("=== Symbolic3 Combinator System Demo ===\n");

  // Create symbolic variables
  constexpr Symbol x;
  constexpr Symbol y;
  constexpr Symbol z;

  std::println("Created symbols: x, y, z\n");

  // ========================================================================
  // Demo 1: Identity Strategy
  // ========================================================================

  std::println("Demo 1: Identity Strategy");
  std::println("--------------------------");

  constexpr Identity id;
  constexpr auto ctx = default_context();

  constexpr auto result1 = id.apply(x, ctx);
  static_assert(std::is_same_v<decltype(result1), decltype(x)>);

  std::println("Identity(x) = x ✓");
  std::println();

  // ========================================================================
  // Demo 2: Context System
  // ========================================================================

  std::println("Demo 2: Context System");
  std::println("----------------------");

  constexpr auto ctx1 = TransformContext<>{};
  std::println("Initial context depth: {}", ctx1.depth);

  constexpr auto ctx2 = ctx1.increment_depth<1>();
  std::println("After increment: {}", ctx2.depth);

  constexpr auto ctx3 = ctx2.with(InsideTrigTag{});
  std::println("Has InsideTrigTag: {}", ctx3.has<InsideTrigTag>());

  constexpr auto ctx4 = ctx3.without(InsideTrigTag{});
  std::println("After removing tag: {}", ctx4.has<InsideTrigTag>());
  std::println();

  // ========================================================================
  // Demo 3: Strategy Composition
  // ========================================================================

  std::println("Demo 3: Strategy Composition");
  std::println("-----------------------------");

  constexpr Identity id1;
  constexpr Identity id2;
  constexpr Identity id3;

  // Sequential composition: apply in order
  constexpr auto sequential = id1 >> id2 >> id3;
  constexpr auto seq_result = sequential.apply(x, ctx);
  static_assert(std::is_same_v<decltype(seq_result), decltype(x)>);

  std::println("(id1 >> id2 >> id3)(x) = x ✓");

  // Choice composition: try first, fallback to second
  constexpr auto choice = id1 | id2;
  constexpr auto choice_result = choice.apply(y, ctx);
  static_assert(std::is_same_v<decltype(choice_result), decltype(y)>);

  std::println("(id1 | id2)(y) = y ✓");
  std::println();

  // ========================================================================
  // Demo 4: Recursion Combinators
  // ========================================================================

  std::println("Demo 4: Recursion Combinators");
  std::println("------------------------------");

  // FixPoint: apply until no change or depth limit
  constexpr FixPoint<Identity, 10> fixpoint{.strategy = id};
  constexpr auto fp_result = fixpoint.apply(z, ctx);
  static_assert(std::is_same_v<decltype(fp_result), decltype(z)>);

  std::println("FixPoint<Identity, 10>(z) = z ✓");
  std::println("(Identity reaches fixpoint immediately)");
  std::println();

  // ========================================================================
  // Demo 5: Context Propagation
  // ========================================================================

  std::println("Demo 5: Context Propagation");
  std::println("----------------------------");

  // Create a context with specific tags
  constexpr auto numeric_ctx = TransformContext<>{}
                                   .with(ConstantFoldingEnabledTag{})
                                   .with(NumericModeTag{});

  std::println("Numeric context created:");
  std::println("  - Has ConstantFoldingEnabled: {}",
               numeric_ctx.has<ConstantFoldingEnabledTag>());
  std::println("  - Has NumericMode: {}", numeric_ctx.has<NumericModeTag>());
  std::println("  - Has SymbolicMode: {}", numeric_ctx.has<SymbolicModeTag>());

  constexpr auto symbolic_ctx =
      numeric_ctx.without(NumericModeTag{}).with(SymbolicModeTag{});

  std::println("\nAfter switching to symbolic mode:");
  std::println("  - Has NumericMode: {}", symbolic_ctx.has<NumericModeTag>());
  std::println("  - Has SymbolicMode: {}", symbolic_ctx.has<SymbolicModeTag>());
  std::println();

  // ========================================================================
  // Summary
  // ========================================================================

  std::println("=== Summary ===");
  std::println("Symbolic3 successfully demonstrates:");
  std::println("  ✓ Generic strategy pattern (CRTP-based)");
  std::println("  ✓ Context system with type-safe tags");
  std::println("  ✓ Composition operators (>>, |)");
  std::println("  ✓ Recursion combinators (FixPoint, etc.)");
  std::println("  ✓ Fully constexpr evaluation");
  std::println("\nAll operations verified at compile-time!");

  return 0;
}
