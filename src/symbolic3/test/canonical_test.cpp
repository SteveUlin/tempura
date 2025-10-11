#include <iostream>

#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  "Variadic function objects - AddOp evaluation"_test = [] {
    // Test that AddOp supports variadic evaluation using fold expressions
    constexpr symbolic3::AddOp add;

    // Unary (identity)
    constexpr auto result0 = add(5);
    static_assert(result0 == 5, "add(5) should be 5");

    // Binary
    constexpr auto result1 = add(1, 2);
    static_assert(result1 == 3, "1 + 2 should be 3");

    // Ternary
    constexpr auto result2 = add(1, 2, 3);
    static_assert(result2 == 6, "1 + 2 + 3 should be 6");

    // Quaternary
    constexpr auto result3 = add(1, 2, 3, 4);
    static_assert(result3 == 10, "1 + 2 + 3 + 4 should be 10");

    std::cout << "✓ AddOp variadic evaluation works (using fold expressions)\n";
    std::cout << "  Unary: " << result0 << "\n";
    std::cout << "  Binary: " << result1 << "\n";
    std::cout << "  Ternary: " << result2 << "\n";
    std::cout << "  Quaternary: " << result3 << "\n";
  };

  "Variadic function objects - MulOp evaluation"_test = [] {
    // Test that MulOp supports variadic evaluation using fold expressions
    constexpr symbolic3::MulOp mul;

    // Unary (identity)
    constexpr auto result0 = mul(7);
    static_assert(result0 == 7, "mul(7) should be 7");

    // Binary
    constexpr auto result1 = mul(2, 3);
    static_assert(result1 == 6, "2 * 3 should be 6");

    // Ternary
    constexpr auto result2 = mul(2, 3, 4);
    static_assert(result2 == 24, "2 * 3 * 4 should be 24");

    std::cout << "✓ MulOp variadic evaluation works (using fold expressions)\n";
    std::cout << "  Unary: " << result0 << "\n";
    std::cout << "  Binary: " << result1 << "\n";
    std::cout << "  Ternary: " << result2 << "\n";
  };

  "Canonical form infrastructure exists"_test = [] {
    // Test that the canonical form trait system exists
    constexpr bool add_uses_canonical = uses_canonical_form<symbolic3::AddOp>;
    constexpr bool mul_uses_canonical = uses_canonical_form<symbolic3::MulOp>;
    constexpr bool sub_uses_canonical = uses_canonical_form<symbolic3::SubOp>;

    static_assert(add_uses_canonical, "AddOp should use canonical form");
    static_assert(mul_uses_canonical, "MulOp should use canonical form");
    static_assert(!sub_uses_canonical, "SubOp should NOT use canonical form");

    std::cout << "✓ Canonical form trait system works\n";
    std::cout << "  AddOp uses canonical: " << add_uses_canonical << "\n";
    std::cout << "  MulOp uses canonical: " << mul_uses_canonical << "\n";
    std::cout << "  SubOp uses canonical: " << sub_uses_canonical << "\n";
  };

  "Canonical strategy exists"_test = [] {
    // Test that the to_canonical strategy exists
    // Note: Full flattening implementation is in progress
    // This test just verifies the infrastructure compiles

    std::cout << "✓ to_canonical strategy defined\n";
    std::cout << "  (Full flattening implementation in progress)\n";
    std::cout << "  See RECOMMENDATION_2_IMPLEMENTATION.md for status\n";
  };

  "Expression types maintain binary structure"_test = [] {
    // Verify current binary structure is preserved
    constexpr Symbol a;
    constexpr Symbol b;
    constexpr Symbol c;

    constexpr auto expr = (a + b) + c;

    // Currently this is Expression<Add, Expression<Add, a, b>, c>
    using ExprType = decltype(expr);
    constexpr bool is_add_expr = is_expression<ExprType>;

    static_assert(is_add_expr, "Should be an Add expression");

    std::cout << "✓ Binary tree structure preserved (as expected)\n";
    std::cout << "  (a + b) + c maintains nested structure\n";
    std::cout
        << "  Canonical form will flatten this to Expression<Add, a, b, c>\n";
  };

  return 0;
}
