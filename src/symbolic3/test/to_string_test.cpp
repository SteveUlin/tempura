#include <cassert>

#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  // Basic constant conversion tests
  "Constant integer to_string"_test = [] {
    constexpr auto zero_str = toString(Constant<0>{});
    static_assert(zero_str.N == 1);  // "0"

    constexpr auto five_str = toString(Constant<5>{});
    static_assert(five_str.N == 1);  // "5"

    constexpr auto neg_str = toString(Constant<-3>{});
    static_assert(neg_str.N == 2);  // "-3"
  };

  // Symbol conversion tests
  "Symbol to_string"_test = [] {
    Symbol x;
    Symbol y;

    auto x_str = toStringRuntime(x);
    auto y_str = toStringRuntime(y);

    assert(x_str.find("x") != std::string::npos);
    assert(y_str.find("x") != std::string::npos);

    std::printf("x: %s, y: %s\n", x_str.c_str(), y_str.c_str());
  };

  // Simple expression conversion
  "Addition expression to_string"_test = [] {
    Symbol x;
    auto expr = x + Constant<1>{};

    auto str = toStringRuntime(expr);
    std::printf("x + 1 = %s\n", str.c_str());

    // Should contain both operands
    assert(str.find("x") != std::string::npos);
    assert(str.find("1") != std::string::npos);
    assert(str.find("+") != std::string::npos);
  };

  // Multiplication expression
  "Multiplication expression to_string"_test = [] {
    Symbol x;
    auto expr = Constant<2>{} * x;

    auto str = toStringRuntime(expr);
    std::printf("2 * x = %s\n", str.c_str());

    assert(str.find("2") != std::string::npos);
    assert(str.find("x") != std::string::npos);
    assert(str.find("*") != std::string::npos);
  };

  // Nested expression
  "Nested expression to_string"_test = [] {
    Symbol x;
    auto expr = (x + Constant<1>{}) * Constant<2>{};

    auto str = toStringRuntime(expr);
    std::printf("(x + 1) * 2 = %s\n", str.c_str());

    // All components should be present
    assert(str.find("x") != std::string::npos);
    assert(str.find("1") != std::string::npos);
    assert(str.find("2") != std::string::npos);
  };

  // Transcendental functions
  "Trigonometric function to_string"_test = [] {
    Symbol x;
    auto expr = sin(x);

    auto str = toStringRuntime(expr);
    std::printf("sin(x) = %s\n", str.c_str());

    assert(str.find("sin") != std::string::npos);
    assert(str.find("x") != std::string::npos);
  };

  "Exponential function to_string"_test = [] {
    Symbol x;
    auto expr = exp(x);

    auto str = toStringRuntime(expr);
    std::printf("exp(x) = %s\n", str.c_str());

    assert(str.find("exp") != std::string::npos);
  };

  // Debugging utilities
  "debug_print basic"_test = [] {
    Symbol x;
    auto expr = x + Constant<1>{};

    std::printf("Testing debug_print:\n");
    debug_print(expr, "expr");
  };

  "debug_print_compact"_test = [] {
    Symbol x;
    auto expr = Constant<2>{} * x + Constant<1>{};

    std::printf("Testing debug_print_compact:\n");
    debug_print_compact(expr, "2*x + 1");
  };

  "debug_print_tree simple"_test = [] {
    Symbol x;
    auto expr = x + Constant<1>{};

    std::printf("Testing debug_print_tree (simple):\n");
    debug_print_tree(expr);
  };

  "debug_print_tree nested"_test = [] {
    Symbol x;
    auto expr = (x + Constant<1>{}) * Constant<2>{};

    std::printf("\nTesting debug_print_tree (nested):\n");
    debug_print_tree(expr, 0, "(x+1)*2");
  };

  "debug_print_tree complex"_test = [] {
    Symbol x;
    Symbol y;
    auto expr = sin(x * y) + cos(x);

    std::printf("\nTesting debug_print_tree (complex):\n");
    debug_print_tree(expr, 0, "sin(x*y) + cos(x)");
  };

  "debug_type_info"_test = [] {
    Symbol x;
    auto expr = x + Constant<1>{};

    auto type_str = debug_type_info(expr);
    std::printf("Type info: %s\n", type_str.c_str());

    assert(type_str.find("Expression") != std::string::npos);
    assert(type_str.find("Add") != std::string::npos);
  };

  // Negation operator (unary)
  "Negation to_string"_test = [] {
    Symbol x;
    auto expr = -x;

    auto str = toStringRuntime(expr);
    std::printf("-x = %s\n", str.c_str());

    assert(str.find("-") != std::string::npos);
    assert(str.find("x") != std::string::npos);
  };

  // Power operator
  "Power to_string"_test = [] {
    Symbol x;
    auto expr = pow(x, Constant<2>{});

    auto str = toStringRuntime(expr);
    std::printf("x^2 = %s\n", str.c_str());

    assert(str.find("x") != std::string::npos);
    assert(str.find("2") != std::string::npos);
  };

  // Complex expression for visual inspection
  "Complex expression visualization"_test = [] {
    Symbol x;
    Symbol y;
    auto expr = sin(x * x) + Constant<2>{} * cos(y) - exp(x + y);

    std::printf("\n=== Complex Expression Visualization ===\n");
    debug_print(expr, "Full expression");
    std::printf("\n");
    debug_print_compact(expr, "Compact form");
    std::printf("\n");
    debug_print_tree(expr, 0, "Tree structure");
    std::printf("========================================\n\n");
  };

  std::printf("All to_string tests passed!\n");
  return 0;
}
