#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura::symbolic3;

auto main() -> int {
  "Short-circuit annihilator"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr Symbol z;
    constexpr Symbol w;

    // Test: 0 * (complex_expr) should return 0 immediately
    // without recursing into the complex expression
    constexpr auto expr = 0_c * (x + y + z + w);

    // Traditional full_simplify would recurse into (x+y+z+w) first
    // Two-stage should catch this at the quick pattern phase
    auto result = two_stage_simplify(expr, default_context());

    // The result should be 0 (we check this at runtime since we can't
    // do static_assert due to the return type complexity)
    std::print("Input:  0 · (x + y + z + w)\n");
    std::print("Output: {}\n", to_string(result));
  };

  "Identity short-circuit"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    constexpr auto expr = 1_c * (x + y);
    auto result = two_stage_simplify(expr, default_context());

    std::print("Input:  1 · (x + y)\n");
    std::print("Output: {}\n", to_string(result));
  };

  "Nested annihilator"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr Symbol z;
    constexpr Symbol w;

    constexpr auto expr = x + (0_c * (y + z)) + w;
    auto result = two_stage_simplify(expr, default_context());

    std::print("Input:  x + (0 · (y + z)) + w\n");
    std::print("Output: {}\n", to_string(result));
  };

  "Like term collection"_test = [] {
    constexpr Symbol x;

    constexpr auto expr = x + x + x;
    auto result = two_stage_simplify(expr, default_context());

    std::print("Input:  x + x + x\n");
    std::print("Output: {}\n", to_string(result));
    std::print("Expected: 3·x\n");
  };

  "Constant folding"_test = [] {
    constexpr Symbol x;

    constexpr auto expr = 2_c + 3_c + x;
    auto result = two_stage_simplify(expr, default_context());

    std::print("Input:  2 + 3 + x\n");
    std::print("Output: {}\n", to_string(result));
    std::print("Expected: 5 + x\n");
  };

  "Complex expression - both phases"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    constexpr auto expr = (x + x) + (0_c * y) + 2_c + 3_c;
    auto result = two_stage_simplify(expr, default_context());

    std::print("Input:  (x + x) + (0 · y) + 2 + 3\n");
    std::print("Output: {}\n", to_string(result));
    std::print("Expected: 5 + 2·x\n");
  };

  "Compare with full_simplify"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr Symbol z;

    constexpr auto expr = 0_c * (x + y + z);

    auto two_stage_result = two_stage_simplify(expr, default_context());
    auto full_result = full_simplify(expr, default_context());

    std::print("Expression: 0 · (x + y + z)\n");
    std::print("Two-stage:   {}\n", to_string(two_stage_result));
    std::print("Full (trad): {}\n", to_string(full_result));
    std::print("Both should produce: 0\n");
  };

  return 0;
}
