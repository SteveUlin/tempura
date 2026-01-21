// Tests for scheme/para.h - paramorphism (fold with original children)
#include "symbolic4/core.h"
#include "symbolic4/operators.h"
#include "symbolic4/scheme/para.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Test interpreter: doubles all terminal constant values
// Demonstrates para gives access to both original child and its result
struct DoubleConstants {
  using result_type = int;
  struct context_type {};

  template <typename T>
  static constexpr auto terminal(T, context_type&) -> int {
    if constexpr (is_constant_v<T>) {
      return static_cast<int>(T::value) * 2;
    } else {
      return 0;
    }
  }

  // Para provides Pair{original, result} for each child
  template <typename Op, typename... Ps>
  static constexpr auto combine(context_type&, Op, Ps... pairs) -> int {
    if constexpr (sizeof...(Ps) == 0) {
      return 0;
    } else {
      return (pairs.second + ...);  // Sum the results
    }
  }
};

// Test interpreter: counts nodes where child is a constant
// Uses the original child (pairs.first) to check type
struct CountConstantChildren {
  using result_type = int;
  struct context_type {};

  template <typename T>
  static constexpr auto terminal(T, context_type&) -> int {
    return 0;  // Terminals have no children
  }

  template <typename Op, typename... Ps>
  static constexpr auto combine(context_type&, Op, Ps... pairs) -> int {
    if constexpr (sizeof...(Ps) == 0) {
      return 0;
    } else {
      int count = 0;
      auto check = [&](auto pair) {
        if constexpr (is_constant_v<decltype(pair.first)>) {
          count++;
        }
        count += pair.second;  // Add children's counts
      };
      (check(pairs), ...);
      return count;
    }
  }
};

auto main() -> int {
  "para doubles constants"_test = [] {
    auto expr = Constant<3>{} + Constant<7>{};
    DoubleConstants::context_type ctx;
    auto result = para<DoubleConstants>(expr, ctx);
    expectEq(result, 20);  // 3*2 + 7*2 = 6 + 14 = 20
  };

  "para with mixed expression"_test = [] {
    Symbol<struct X> x;
    auto expr = x + Constant<5>{};
    DoubleConstants::context_type ctx;
    auto result = para<DoubleConstants>(expr, ctx);
    expectEq(result, 10);  // x contributes 0, 5*2 = 10
  };

  "para nested expression"_test = [] {
    auto expr = (Constant<2>{} + Constant<3>{}) * Constant<4>{};
    DoubleConstants::context_type ctx;
    auto result = para<DoubleConstants>(expr, ctx);
    expectEq(result, 18);  // 2*2 + 3*2 + 4*2 = 4 + 6 + 8 = 18
  };

  "para counts constant children"_test = [] {
    Symbol<struct X> x;

    // x + 5: one constant child at top level
    CountConstantChildren::context_type ctx1;
    expectEq(para<CountConstantChildren>(x + Constant<5>{}, ctx1), 1);

    // 3 + 5: two constant children at top level
    CountConstantChildren::context_type ctx2;
    expectEq(para<CountConstantChildren>(Constant<3>{} + Constant<5>{}, ctx2), 2);

    // (x + 5) + 3: one constant at top, one in nested
    CountConstantChildren::context_type ctx3;
    expectEq(para<CountConstantChildren>((x + Constant<5>{}) + Constant<3>{}, ctx3), 2);
  };

  "para terminal returns terminal result"_test = [] {
    DoubleConstants::context_type ctx;
    expectEq(para<DoubleConstants>(Constant<7>{}, ctx), 14);
  };

  return TestRegistry::result();
}
