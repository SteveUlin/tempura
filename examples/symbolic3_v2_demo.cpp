// Simple demonstration of v2 design improvements:
// 1. Data-driven context vs behavioral tags
// 2. Showing deducing this benefits (in comments)

#include <iostream>

#include "symbolic3/context_v2.h"
#include "symbolic3/core.h"

using namespace tempura::symbolic3;
using namespace tempura::symbolic3::v2;

int main() {
  std::cout << "V2 Design Demonstration\n";
  std::cout << "========================\n\n";

  // ============================================================================
  // DEMO 1: Context carries DATA, not BEHAVIOR
  // ============================================================================

  std::cout << "1. Data-Driven Context Design\n";
  std::cout << "------------------------------\n\n";

  // Different contexts for different use cases
  {
    std::cout << "Creating contexts with different modes:\n\n";

    auto numeric_ctx = numeric_context();
    std::cout << "  numeric_context():\n";
    std::cout << "    fold_numeric_constants: "
              << numeric_ctx.mode.fold_numeric_constants << "\n";
    std::cout << "    fold_symbolic_constants: "
              << numeric_ctx.mode.fold_symbolic_constants << "\n";
    std::cout << "    preserve_special_values: "
              << numeric_ctx.mode.preserve_special_values << "\n";
    std::cout << "    → Use case: Aggressive numerical evaluation\n\n";

    auto symbolic_ctx = symbolic_context();
    std::cout << "  symbolic_context():\n";
    std::cout << "    fold_numeric_constants: "
              << symbolic_ctx.mode.fold_numeric_constants << "\n";
    std::cout << "    fold_symbolic_constants: "
              << symbolic_ctx.mode.fold_symbolic_constants << "\n";
    std::cout << "    preserve_special_values: "
              << symbolic_ctx.mode.preserve_special_values << "\n";
    std::cout << "    → Use case: Preserve mathematical structure\n\n";

    auto default_ctx = default_context();
    std::cout << "  default_context():\n";
    std::cout << "    fold_numeric_constants: "
              << default_ctx.mode.fold_numeric_constants << "\n";
    std::cout << "    fold_algebraic: " << default_ctx.mode.fold_algebraic
              << "\n";
    std::cout << "    → Use case: Balanced simplification\n\n";
  }

  // ============================================================================
  // DEMO 2: Domain information compile-time accessible
  // ============================================================================

  std::cout << "2. Domain Information\n";
  std::cout << "---------------------\n\n";

  {
    constexpr auto real_ctx = default_context();
    static_assert(real_ctx.domain == Domain::Real);
    static_assert(real_ctx.is_real());
    std::cout << "  Real domain context created\n";

    constexpr auto int_ctx = integer_context();
    static_assert(int_ctx.domain == Domain::Integer);
    static_assert(int_ctx.is_integer());
    std::cout << "  Integer domain context created\n";

    constexpr auto mod_ctx = modular_context<7>();
    static_assert(mod_ctx.domain == Domain::ModularArithmetic);
    static_assert(mod_ctx.is_modular());
    static_assert(mod_ctx.modulus() == 7);
    std::cout << "  Modular<7> domain context created\n\n";
  }

  // ============================================================================
  // DEMO 3: Key Design Improvement
  // ============================================================================

  std::cout << "3. Design Philosophy\n";
  std::cout << "--------------------\n\n";

  std::cout << "OLD WAY (v1 - behavioral tags):\n";
  std::cout << "  Context has: InsideTrigTag, ConstantFoldingEnabledTag\n";
  std::cout << "  Strategy checks: ctx.has<InsideTrigTag>()\n";
  std::cout << "  Problem: Strategy knows WHERE it is\n\n";

  std::cout << "NEW WAY (v2 - data-driven):\n";
  std::cout << "  Context has: SimplificationMode with flags\n";
  std::cout << "  Strategy checks: ctx.mode.fold_numeric_constants\n";
  std::cout << "  Benefit: Strategy knows WHAT to do\n\n";

  std::cout << "Example:\n";
  std::cout << "  // In a strategy's apply() function:\n";
  std::cout << "  if (!ctx.mode.fold_numeric_constants) {\n";
  std::cout << "    return expr;  // Don't fold\n";
  std::cout << "  }\n";
  std::cout << "  // ... folding logic ...\n\n";

  std::cout << "  The strategy doesn't need to know:\n";
  std::cout << "  - Are we inside a trig function?\n";
  std::cout << "  - Are we in symbolic mode?\n";
  std::cout << "  - What is the caller doing?\n\n";

  std::cout << "  It only needs to know:\n";
  std::cout << "  - Should I fold constants? (query the flag)\n\n";

  // ============================================================================
  // DEMO 4: Deducing This (Future Improvement)
  // ============================================================================

  std::cout << "4. Deducing This (C++23)\n";
  std::cout << "------------------------\n\n";

  std::cout << "CRTP (v1):\n";
  std::cout << "  template <typename Impl>\n";
  std::cout << "  struct Strategy {\n";
  std::cout << "    template <Symbolic S, typename Context>\n";
  std::cout << "    constexpr auto apply(S expr, Context ctx) const {\n";
  std::cout << "      return static_cast<Impl const&>(*this).apply_impl(expr, "
               "ctx);\n";
  std::cout << "    }\n";
  std::cout << "  };\n\n";

  std::cout << "Deducing this (v2):\n";
  std::cout << "  struct Strategy {\n";
  std::cout << "    template <Symbolic S, typename Context>\n";
  std::cout << "    constexpr auto apply(this auto const& self, S expr, "
               "Context ctx) {\n";
  std::cout << "      // 'self' is explicit parameter\n";
  std::cout << "      // No CRTP boilerplate\n";
  std::cout << "      // Clearer error messages\n";
  std::cout << "    }\n";
  std::cout << "  };\n\n";

  std::cout << "Benefits:\n";
  std::cout << "  - No template parameter for Impl\n";
  std::cout << "  - Explicit self parameter\n";
  std::cout << "  - Better error messages\n";
  std::cout << "  - Simpler to understand\n\n";

  std::cout << "All demonstrations complete!\n";
  return 0;
}
