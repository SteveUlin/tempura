// Tests for scheme/fold.h - catamorphism (tree folding)
#include "symbolic4/core.h"
#include "symbolic4/operators.h"
#include "symbolic4/scheme/fold.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Test interpreter: counts all nodes in the expression tree
struct CountNodes {
  using result_type = int;
  struct context_type {};

  template <typename T>
  static constexpr auto terminal(T, context_type&) -> int {
    return 1;
  }

  template <typename Op, typename... Rs>
  static constexpr auto combine(context_type&, Op, Rs... child_counts) -> int {
    if constexpr (sizeof...(Rs) == 0) {
      return 1;  // Nullary op
    } else {
      return 1 + (child_counts + ...);
    }
  }
};

// Test interpreter: sums all constant values
struct SumConstants {
  using result_type = int;
  struct context_type {};

  template <typename T>
  static constexpr auto terminal(T, context_type&) -> int {
    if constexpr (is_constant_v<T>) {
      return static_cast<int>(T::value);
    } else {
      return 0;
    }
  }

  template <typename Op, typename... Rs>
  static constexpr auto combine(context_type&, Op, Rs... child_sums) -> int {
    if constexpr (sizeof...(Rs) == 0) {
      return 0;
    } else {
      return (child_sums + ...);
    }
  }
};

// Test interpreter: computes tree depth
struct Depth {
  using result_type = int;
  struct context_type {};

  template <typename T>
  static constexpr auto terminal(T, context_type&) -> int {
    return 1;
  }

  template <typename Op, typename... Rs>
  static constexpr auto combine(context_type&, Op, Rs... depths) -> int {
    if constexpr (sizeof...(Rs) == 0) {
      return 1;
    } else if constexpr (sizeof...(Rs) == 1) {
      return 1 + (depths + ...);
    } else {
      int max_depth = 0;
      ((max_depth = (depths > max_depth ? depths : max_depth)), ...);
      return 1 + max_depth;
    }
  }
};

auto main() -> int {
  Symbol<struct X> x;

  "fold terminal"_test = [&] {
    CountNodes::context_type ctx;
    expectEq(fold<CountNodes>(x, ctx), 1);
  };

  "fold binary expression"_test = [&] {
    auto expr = x + Constant<5>{};
    CountNodes::context_type ctx;
    expectEq(fold<CountNodes>(expr, ctx), 3);  // root + 2 children
  };

  "fold nested expression"_test = [&] {
    auto nested = (x + x) * x;
    CountNodes::context_type ctx;
    expectEq(fold<CountNodes>(nested, ctx), 5);
  };

  "fold nullary ops (pi, e)"_test = [] {
    CountNodes::context_type ctx;
    expectEq(fold<CountNodes>(pi, ctx), 1);
    expectEq(fold<CountNodes>(e, ctx), 1);
  };

  "fold unary ops"_test = [&] {
    CountNodes::context_type ctx1;
    expectEq(fold<CountNodes>(-x, ctx1), 2);  // neg + x

    CountNodes::context_type ctx2;
    expectEq(fold<CountNodes>(sin(x), ctx2), 2);  // sin + x
  };

  "fold sums constants"_test = [&] {
    SumConstants::context_type ctx1;
    expectEq(fold<SumConstants>(x, ctx1), 0);  // No constants

    SumConstants::context_type ctx2;
    expectEq(fold<SumConstants>(Constant<3>{}, ctx2), 3);

    SumConstants::context_type ctx3;
    expectEq(fold<SumConstants>(Constant<3>{} + Constant<7>{}, ctx3), 10);

    // Mixed: x + 3 + 7 = 10 (x contributes 0)
    SumConstants::context_type ctx4;
    expectEq(fold<SumConstants>(x + Constant<3>{} + Constant<7>{}, ctx4), 10);
  };

  "fold computes depth"_test = [&] {
    Depth::context_type ctx1;
    expectEq(fold<Depth>(x, ctx1), 1);

    Depth::context_type ctx2;
    expectEq(fold<Depth>(x + one, ctx2), 2);

    Depth::context_type ctx3;
    expectEq(fold<Depth>((x + x) * x, ctx3), 3);
  };

  return TestRegistry::result();
}
