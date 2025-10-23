#include "symbolic3/pretty_print.h"

#include <print>

#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  // Test PRETTY_PRINT macro
  "PRETTY_PRINT - single variable"_test = [] {
    Symbol x;
    auto expr = x + Constant<1>{};

    constexpr auto str = PRETTY_PRINT(expr, x);
    static_assert(str == "x + 1");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Single variable pretty print\n");
  };

  "PRETTY_PRINT - multiple variables"_test = [] {
    Symbol x, y;
    auto expr = x * y;

    constexpr auto str = PRETTY_PRINT(expr, x, y);
    static_assert(str == "x * y");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Multiple variables pretty print\n");
  };

  "PRETTY_PRINT - complex expression"_test = [] {
    Symbol x, y, z;
    auto expr = x * x + Constant<2>{} * y + z;

    constexpr auto str = PRETTY_PRINT(expr, x, y, z);
    static_assert(str == "x * x + 2 * y + z");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Complex expression pretty print\n");
  };

  "PRETTY_PRINT - nested expression"_test = [] {
    Symbol x, y;
    auto expr = sin(x) + cos(y);

    constexpr auto str = PRETTY_PRINT(expr, x, y);
    // The exact string depends on operator_display.h formatting

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Nested expression pretty print\n");
  };

  "PRETTY_PRINT - variable order independence"_test = [] {
    Symbol x, y, z;
    auto expr = z + y + x;

    // Order of variables in macro doesn't matter
    constexpr auto str1 = PRETTY_PRINT(expr, x, y, z);
    constexpr auto str2 = PRETTY_PRINT(expr, z, y, x);
    constexpr auto str3 = PRETTY_PRINT(expr, y, x, z);

    static_assert(str1 == str2);
    static_assert(str2 == str3);

    std::print("✓ Variable order independence\n");
  };

  "PRETTY_PRINT - Greek letters"_test = [] {
    Symbol alpha, beta;
    auto expr = alpha + beta;

    // Variable names in code become the printed names
    constexpr auto str = PRETTY_PRINT(expr, alpha, beta);
    static_assert(str == "alpha + beta");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Greek letter variable names\n");
  };

  "PRETTY_PRINT - single character names"_test = [] {
    Symbol a, b, c;
    auto expr = a * b + c;

    constexpr auto str = PRETTY_PRINT(expr, a, b, c);
    static_assert(str == "a * b + c");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Single character names\n");
  };

  "PRETTY_PRINT - reused symbol"_test = [] {
    Symbol x;
    auto expr = x * x * x;

    // Only need to list each unique symbol once
    constexpr auto str = PRETTY_PRINT(expr, x);
    static_assert(str == "x * x * x");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Reused symbol\n");
  };

  "PRETTY_PRINT - power expression"_test = [] {
    Symbol x;
    auto expr = pow(x, Constant<3>{});

    constexpr auto str = PRETTY_PRINT(expr, x);

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Power expression\n");
  };

  "PRETTY_PRINT - division"_test = [] {
    Symbol x, y;
    auto expr = x / y;

    constexpr auto str = PRETTY_PRINT(expr, x, y);
    static_assert(str == "x / y");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Division expression\n");
  };

  "PRETTY_PRINT - no symbols"_test = [] {
    auto expr = Constant<5>{} + Constant<3>{};

    // Empty variable list for constant-only expressions
    constexpr auto str = PRETTY_PRINT(expr);
    static_assert(str == "5 + 3");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ No symbols (constants only)\n");
  };

  // Unicode symbol tests
  "PRETTY_PRINT - Unicode Greek letters"_test = [] {
    Symbol α, β, γ;
    auto expr = α * β + γ;

    constexpr auto str = PRETTY_PRINT(expr, α, β, γ);
    static_assert(str == "α * β + γ");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Unicode Greek letters\n");
  };

  "PRETTY_PRINT - Unicode trigonometry"_test = [] {
    Symbol θ, φ;
    auto expr = sin(θ) + cos(φ);

    constexpr auto str = PRETTY_PRINT(expr, θ, φ);
    static_assert(str == "sin( θ) + cos( φ)");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Unicode trigonometry\n");
  };

  "PRETTY_PRINT - Unicode mixed"_test = [] {
    Symbol Δx, Δy;
    auto expr = pow(Δx, Constant<2>{}) + pow(Δy, Constant<2>{});

    constexpr auto str = PRETTY_PRINT(expr, Δx, Δy);
    static_assert(str == "Δx ^ 2 + Δy ^ 2");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Unicode delta notation\n");
  };

  "PRETTY_PRINT - Unicode subscripts"_test = [] {
    Symbol x₀, x₁, x₂;
    auto expr = x₀ + x₁ + x₂;

    constexpr auto str = PRETTY_PRINT(expr, x₀, x₁, x₂);
    static_assert(str == "x₀ + x₁ + x₂");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Unicode subscripts\n");
  };

  "PRETTY_PRINT - Unicode complex example"_test = [] {
    Symbol λ, μ, σ, ω;
    auto expr = λ * exp(μ * ω) + σ;

    constexpr auto str = PRETTY_PRINT(expr, λ, μ, σ, ω);
    static_assert(str == "λ * exp( μ * ω) + σ");

    std::print("Result: {}\n", str.c_str());
    std::print("✓ Unicode complex mathematical expression\n");
  };

  std::print("\nAll pretty_print tests passed!\n");
  return TestRegistry::result();
}
