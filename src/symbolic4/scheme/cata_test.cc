// Tests for symbolic4/scheme/cata.h
#include "symbolic4/scheme/cata.h"
#include "symbolic4/core.h"
#include "symbolic4/operators.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// ===========================================================================
// Test interpreter: evaluates expression with literal values only
// ===========================================================================

struct LiteralEval {
  using result_type = double;

  template <typename T>
  static constexpr auto terminal(T term) -> double {
    if constexpr (is_literal_v<T>) {
      return static_cast<double>(term.effect_.value);
    } else if constexpr (is_constant_v<T>) {
      return static_cast<double>(T::value);
    } else {
      return 0.0;  // Symbols evaluate to 0 without bindings
    }
  }

  template <typename Op, typename... Rs>
  static constexpr auto combine(Rs... children) -> double {
    return Op{}(children...);
  }
};

// ===========================================================================
// Test interpreter: counts nodes in expression tree
// ===========================================================================

struct NodeCounter {
  using result_type = int;

  template <typename T>
  static constexpr auto terminal(T) -> int {
    return 1;
  }

  template <typename Op, typename... Rs>
  static constexpr auto combine(Rs... children) -> int {
    return 1 + (children + ...);
  }
};

// ===========================================================================
// Test interpreter: with context (stateful)
// ===========================================================================

struct ContextEval {
  using result_type = double;

  struct context_type {
    double multiplier;
  };

  template <typename T>
  static constexpr auto terminal(T term, context_type& ctx) -> double {
    if constexpr (is_literal_v<T>) {
      return ctx.multiplier * static_cast<double>(term.effect_.value);
    } else {
      return 0.0;
    }
  }

  template <typename Op, typename... Rs>
  static constexpr auto combine(context_type&, Rs... children) -> double {
    return Op{}(children...);
  }
};

auto main() -> int {
  // ===========================================================================
  // fold (catamorphism) - results only
  // ===========================================================================

  "fold evaluates literal"_test = [] {
    auto expr = lit(42.0);
    double result = fold<LiteralEval>(expr);
    expectNear(42.0, result, 1e-10);
  };

  "fold evaluates addition"_test = [] {
    auto expr = lit(3.0) + lit(4.0);
    double result = fold<LiteralEval>(expr);
    expectNear(7.0, result, 1e-10);
  };

  "fold evaluates nested expression"_test = [] {
    auto expr = (lit(2.0) + lit(3.0)) * lit(4.0);
    double result = fold<LiteralEval>(expr);
    expectNear(20.0, result, 1e-10);
  };

  "fold counts nodes"_test = [] {
    auto expr = lit(1.0) + lit(2.0);  // 3 nodes: +, 1.0, 2.0
    int count = fold<NodeCounter>(expr);
    expectEq(3, count);
  };

  "fold counts deeper tree"_test = [] {
    auto expr = (lit(1.0) + lit(2.0)) * (lit(3.0) - lit(4.0));
    // 7 nodes: *, +, -, 1.0, 2.0, 3.0, 4.0
    int count = fold<NodeCounter>(expr);
    expectEq(7, count);
  };

  // ===========================================================================
  // fold with context
  // ===========================================================================

  "fold with context"_test = [] {
    auto expr = lit(5.0) + lit(3.0);
    ContextEval::context_type ctx{2.0};  // multiplier = 2

    double result = fold<ContextEval>(expr, ctx);
    // (5*2) + (3*2) = 16
    expectNear(16.0, result, 1e-10);
  };

  // ===========================================================================
  // cata with explicit ChildMode
  // ===========================================================================

  "cata ResultOnly is same as fold"_test = [] {
    auto expr = lit(10.0) - lit(3.0);
    double fold_result = fold<LiteralEval>(expr);
    double cata_result = cata<LiteralEval, ChildMode::ResultOnly>(expr);
    expectNear(fold_result, cata_result, 1e-10);
  };

  // ===========================================================================
  // Constants
  // ===========================================================================

  "fold evaluates constants"_test = [] {
    auto expr = Constant<10>{} + lit(5.0);
    double result = fold<LiteralEval>(expr);
    expectNear(15.0, result, 1e-10);
  };

  // ===========================================================================
  // Multiple operations
  // ===========================================================================

  "fold evaluates subtraction"_test = [] {
    auto expr = lit(10.0) - lit(3.0);
    double result = fold<LiteralEval>(expr);
    expectNear(7.0, result, 1e-10);
  };

  "fold evaluates multiplication"_test = [] {
    auto expr = lit(6.0) * lit(7.0);
    double result = fold<LiteralEval>(expr);
    expectNear(42.0, result, 1e-10);
  };

  "fold evaluates division"_test = [] {
    auto expr = lit(20.0) / lit(4.0);
    double result = fold<LiteralEval>(expr);
    expectNear(5.0, result, 1e-10);
  };

  // ===========================================================================
  // Complex expressions
  // ===========================================================================

  "fold evaluates complex expression"_test = [] {
    // (2 + 3) * 4 - 10 / 2 = 5 * 4 - 5 = 15
    auto expr = (lit(2.0) + lit(3.0)) * lit(4.0) - lit(10.0) / lit(2.0);
    double result = fold<LiteralEval>(expr);
    expectNear(15.0, result, 1e-10);
  };

  return TestRegistry::result();
}
