#include <print>

#include "symbolic3/pretty_print.h"
#include "symbolic3/symbolic3.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  // Basic usage
  Symbol x, y, z;

  auto expr1 = x + Constant<1>{};
  constexpr auto str1 = PRETTY_PRINT(expr1, x);
  static_assert(str1 == "x + 1");
  std::print("expr1: {}\n", str1.c_str());

  // Multiple variables
  auto expr2 = x * y + z;
  constexpr auto str2 = PRETTY_PRINT(expr2, x, y, z);
  static_assert(str2 == "x * y + z");
  std::print("expr2: {}\n", str2.c_str());

  // Complex expression
  auto expr3 = pow(x, Constant<2>{}) + Constant<2>{} * x * y + pow(y, Constant<2>{});
  constexpr auto str3 = PRETTY_PRINT(expr3, x, y);
  std::print("expr3: {}\n", str3.c_str());

  // Transcendental functions
  auto expr4 = sin(x) * cos(y) + exp(z);
  constexpr auto str4 = PRETTY_PRINT(expr4, x, y, z);
  std::print("expr4: {}\n", str4.c_str());

  // Can add labels manually if needed
  auto expr5 = x * x * x;
  constexpr auto str5 = StaticString("cubic: ") + PRETTY_PRINT(expr5, x);
  static_assert(str5 == "cubic: x * x * x");
  std::print("{}\n", str5.c_str());

  // Works with descriptive variable names
  Symbol alpha, beta, gamma;
  auto expr6 = alpha + beta * gamma;
  constexpr auto str6 = PRETTY_PRINT(expr6, alpha, beta, gamma);
  static_assert(str6 == "alpha + beta * gamma");
  std::print("expr6: {}\n", str6.c_str());

  // Constants-only expression (no symbols needed)
  auto expr7 = Constant<5>{} + Constant<3>{} * Constant<2>{};
  constexpr auto str7 = PRETTY_PRINT(expr7);
  static_assert(str7 == "5 + 3 * 2");
  std::print("expr7: {}\n", str7.c_str());

  // Unicode symbols work great!
  Symbol θ, φ, ω;
  auto expr8 = sin(θ) * cos(φ) + ω;
  constexpr auto str8 = PRETTY_PRINT(expr8, θ, φ, ω);
  static_assert(str8 == "sin( θ) * cos( φ) + ω");
  std::print("expr8: {}\n", str8.c_str());

  // Unicode subscripts
  Symbol x₀, x₁;
  auto expr9 = x₁ - x₀;
  constexpr auto str9 = PRETTY_PRINT(expr9, x₀, x₁);
  static_assert(str9 == "x₁ - x₀");
  std::print("expr9: {}\n", str9.c_str());

  std::print("\nAll examples completed successfully!\n");
  return 0;
}
