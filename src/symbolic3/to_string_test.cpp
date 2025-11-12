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

  // =============================================================================
  // COMPILE-TIME CUSTOM VARIABLE NAME TESTS
  // =============================================================================

  "Custom variable name - single symbol"_test = [] {
    Symbol x;
    constexpr auto ctx = makeSymbolNames(x, StaticString("alpha"));
    constexpr auto result = toString(x, ctx);

    static_assert(result == "alpha");
    static_assert(result.N == 5);
  };

  "Custom variable name - multiple symbols"_test = [] {
    Symbol x;
    Symbol y;
    constexpr auto ctx = makeSymbolNames(x, StaticString("alpha"), y, StaticString("beta"));

    constexpr auto x_result = toString(x, ctx);
    constexpr auto y_result = toString(y, ctx);

    static_assert(x_result == "alpha");
    static_assert(y_result == "beta");
  };

  "Custom variable name - symbol not in context uses default"_test = [] {
    Symbol x;
    Symbol y;
    constexpr auto ctx = makeSymbolNames(x, StaticString("alpha"));

    constexpr auto x_result = toString(x, ctx);
    static_assert(x_result == "alpha");

    auto y_result = toString(y, ctx);
    std::printf("y without custom name: %s\n", toStringRuntime(y).c_str());
  };

  "Custom variable name - simple expression"_test = [] {
    Symbol x;
    Symbol y;
    constexpr auto ctx = makeSymbolNames(x, StaticString("x"), y, StaticString("y"));
    auto expr = x + y;
    constexpr auto result = toString(expr, ctx);

    static_assert(result == "x + y");
    std::printf("x + y = %s\n", result.c_str());
  };

  "Custom variable name - expression with constants"_test = [] {
    Symbol x;
    constexpr auto ctx = makeSymbolNames(x, StaticString("x"));
    auto expr = Constant<2>{} * x + Constant<1>{};
    constexpr auto result = toString(expr, ctx);

    static_assert(result == "2 * x + 1");
    std::printf("2*x + 1 = %s\n", result.c_str());
  };

  "Custom variable name - nested expression"_test = [] {
    Symbol x;
    Symbol y;
    constexpr auto ctx = makeSymbolNames(x, StaticString("x"), y, StaticString("y"));
    auto expr = (x + y) * Constant<2>{};
    constexpr auto result = toString(expr, ctx);

    static_assert(result == "(x + y) * 2");
    std::printf("(x + y) * 2 = %s\n", result.c_str());
  };

  "Custom variable name - transcendental functions"_test = [] {
    Symbol x;
    constexpr auto ctx = makeSymbolNames(x, StaticString("theta"));
    auto expr = sin(x);
    constexpr auto result = toString(expr, ctx);

    static_assert(result == "sin( theta)");
    std::printf("sin(theta) = %s\n", result.c_str());
  };

  "Custom variable name - complex expression"_test = [] {
    Symbol x;
    Symbol y;
    constexpr auto ctx = makeSymbolNames(x, StaticString("x"), y, StaticString("y"));
    auto expr = sin(x * x) + Constant<2>{} * cos(y);
    auto result = toString(expr, ctx);

    std::printf("sin(x*x) + 2*cos(y) = %s\n", result.c_str());

    auto runtime_str = std::string(result.c_str());
    assert(runtime_str.find("x") != std::string::npos);
    assert(runtime_str.find("y") != std::string::npos);
  };

  "Custom variable name - Greek letters"_test = [] {
    Symbol alpha_sym;
    Symbol beta_sym;
    constexpr auto ctx = makeSymbolNames(alpha_sym, StaticString("α"), beta_sym, StaticString("β"));
    auto expr = alpha_sym + beta_sym;
    constexpr auto result = toString(expr, ctx);

    static_assert(result == "α + β");
    std::printf("α + β = %s\n", result.c_str());
  };

  "Custom variable name - power and division"_test = [] {
    Symbol x;
    Symbol y;
    constexpr auto ctx = makeSymbolNames(x, StaticString("x"), y, StaticString("y"));
    auto expr = pow(x, Constant<2>{}) / y;
    auto result = toString(expr, ctx);

    std::printf("x^2 / y = %s\n", result.c_str());

    auto runtime_str = std::string(result.c_str());
    assert(runtime_str.find("x") != std::string::npos);
    assert(runtime_str.find("y") != std::string::npos);
  };

  "Custom variable name - negation"_test = [] {
    Symbol x;
    constexpr auto ctx = makeSymbolNames(x, StaticString("x"));
    auto expr = -x;
    constexpr auto result = toString(expr, ctx);

    static_assert(result == "-( x)");
    std::printf("-x = %s\n", result.c_str());
  };

  "Custom variable name - empty context uses defaults"_test = [] {
    Symbol x;
    auto ctx = emptySymbolNames();
    auto result = toString(x, ctx);

    assert(std::string(result.c_str()) == toStringRuntime(x));
    std::printf("x with empty context: %s\n", result.c_str());
  };

  "StaticString equality tests"_test = [] {
    constexpr auto s1 = StaticString("hello");
    constexpr auto s2 = StaticString("hello");
    constexpr auto s3 = StaticString("world");
    constexpr auto s4 = StaticString("hel") + StaticString("lo");

    static_assert(s1 == s2);
    static_assert(!(s1 == s3));
    static_assert(s1 == s4);
    static_assert(s1.N == 5);
    static_assert(s3.N == 5);
  };

  // =============================================================================
  // IMPROVED SYNTAX TESTS - String literal comparison and UDL
  // =============================================================================

  "String literal comparison syntax"_test = [] {
    Symbol x;
    Symbol y;
    constexpr auto ctx = makeSymbolNames(x, StaticString("x"), y, StaticString("y"));

    auto expr1 = x + y;
    constexpr auto result1 = toString(expr1, ctx);
    static_assert(result1 == "x + y");

    auto expr2 = Constant<2>{} * x;
    constexpr auto result2 = toString(expr2, ctx);
    static_assert(result2 == "2 * x");

    auto expr3 = (x + y) * Constant<3>{};
    constexpr auto result3 = toString(expr3, ctx);
    static_assert(result3 == "(x + y) * 3");

    std::printf("String literal comparison syntax works!\n");
  };

  "User-defined literal _cts syntax"_test = [] {
    using namespace tempura;

    Symbol x;
    Symbol y;
    constexpr auto ctx = makeSymbolNames(x, "x"_cts, y, "y"_cts);

    auto expr1 = x + y;
    constexpr auto result1 = toString(expr1, ctx);
    static_assert(result1 == "x + y"_cts);

    auto expr2 = x * x + Constant<2>{} * x + Constant<1>{};
    constexpr auto result2 = toString(expr2, ctx);
    static_assert(result2 == "x * x + 2 * x + 1"_cts);

    std::printf("User-defined literal _cts syntax works!\n");
  };

  "Greek letters with cleaner syntax"_test = [] {
    using namespace tempura;

    Symbol alpha;
    Symbol beta;
    constexpr auto ctx = makeSymbolNames(alpha, "α"_cts, beta, "β"_cts);

    auto expr = alpha * beta + Constant<1>{};
    constexpr auto result = toString(expr, ctx);
    static_assert(result == "α * β + 1");

    std::printf("α * β + 1 = %s\n", result.c_str());
  };

  std::printf("\nAll to_string tests passed (including custom variable names)!\n");
  return 0;
}
