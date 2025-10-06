// Example demonstrating the new advanced simplification rules
// Compile: g++ -std=c++2c -I../src -o advanced_simplify_demo
// advanced_simplify_demo.cpp Run: ./advanced_simplify_demo

#include <iostream>

#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/simplify.h"
#include "symbolic3/to_string.h"

using namespace tempura;
using namespace tempura::symbolic3;

// Convenience wrapper
template <typename T>
inline std::string to_string(T expr) {
  return toStringRuntime(expr);
}

int main() {
  std::cout << "Symbolic3: Advanced Simplification Demo\n";
  std::cout << "========================================\n\n";

  constexpr Symbol x;
  constexpr Symbol y;
  constexpr Symbol a;
  constexpr Symbol b;
  constexpr auto ctx = default_context();

  // Logarithm identities
  std::cout << "=== Logarithm Identities ===\n";
  {
    auto expr = log(x * y);
    auto simplified = full_simplify(expr, ctx);
    std::cout << "log(x*y) → " << to_string(simplified) << "\n";
    // Expected: log(x) + log(y)
  }
  {
    auto expr = log(x / y);
    auto simplified = full_simplify(expr, ctx);
    std::cout << "log(x/y) → " << to_string(simplified) << "\n";
    // Expected: log(x) - log(y)
  }
  {
    auto expr = log(pow(x, 3_c));
    auto simplified = full_simplify(expr, ctx);
    std::cout << "log(x³) → " << to_string(simplified) << "\n";
    // Expected: 3*log(x)
  }

  // Exponential identities
  std::cout << "\n=== Exponential Identities ===\n";
  {
    auto expr = exp(a + b);
    auto simplified = full_simplify(expr, ctx);
    std::cout << "exp(a+b) → " << to_string(simplified) << "\n";
    // Expected: exp(a)*exp(b)
  }
  {
    auto expr = exp(a - b);
    auto simplified = full_simplify(expr, ctx);
    std::cout << "exp(a-b) → " << to_string(simplified) << "\n";
    // Expected: exp(a)/exp(b)
  }
  {
    auto expr = exp(2_c * log(x));
    auto simplified = full_simplify(expr, ctx);
    std::cout << "exp(2*log(x)) → " << to_string(simplified) << "\n";
    // Expected: x²
  }

  // Trigonometric identities
  std::cout << "\n=== Trigonometric Identities ===\n";
  {
    auto expr = sin(2_c * x);
    auto simplified = full_simplify(expr, ctx);
    std::cout << "sin(2x) → " << to_string(simplified) << "\n";
    // Expected: 2*sin(x)*cos(x)
  }
  {
    auto expr = cos(2_c * x);
    auto simplified = full_simplify(expr, ctx);
    std::cout << "cos(2x) → " << to_string(simplified) << "\n";
    // Expected: cos²(x) - sin²(x)
  }
  {
    auto expr = tan(x);
    auto simplified = full_simplify(expr, ctx);
    std::cout << "tan(x) → " << to_string(simplified) << "\n";
    // Expected: sin(x)/cos(x)
  }

  // Pythagorean identity
  std::cout << "\n=== Pythagorean Identity ===\n";
  {
    auto expr = pow(sin(x), 2_c) + pow(cos(x), 2_c);
    auto simplified = full_simplify(expr, ctx);
    std::cout << "sin²(x) + cos²(x) → " << to_string(simplified) << "\n";
    // Expected: 1
  }

  // Complex combinations
  std::cout << "\n=== Complex Combinations ===\n";
  {
    // Inverse operations should cancel
    auto expr = exp(log(x)) + log(exp(y));
    auto simplified = full_simplify(expr, ctx);
    std::cout << "exp(log(x)) + log(exp(y)) → " << to_string(simplified)
              << "\n";
    // Expected: x + y
  }
  {
    // Pythagorean identity in multiplication
    auto expr = (pow(sin(x), 2_c) + pow(cos(x), 2_c)) * y;
    auto simplified = full_simplify(expr, ctx);
    std::cout << "(sin²(x) + cos²(x)) * y → " << to_string(simplified) << "\n";
    // Expected: y
  }
  {
    // Nested transcendental functions
    auto expr = log(exp(a) * exp(b));
    auto simplified = full_simplify(expr, ctx);
    std::cout << "log(exp(a)*exp(b)) → " << to_string(simplified) << "\n";
    // Expected: a + b (via multiple rule applications)
  }

  std::cout << "\n=== Calculus Applications ===\n";
  {
    // These rules are especially useful for symbolic differentiation
    auto f = pow(x, 3_c) * log(x);
    std::cout << "Original function: " << to_string(f) << "\n";

    // If we had differentiation (which symbolic3 has), we could:
    // auto df = diff(f, x);
    // auto df_simplified = full_simplify(df, ctx);
    // std::cout << "Derivative: " << to_string(df_simplified) << "\n";
  }

  std::cout << "\n✨ Demo complete!\n";
  std::cout << "\nNew capabilities:\n";
  std::cout << "  • Logarithm laws (product, quotient, power)\n";
  std::cout << "  • Exponential laws (sum, difference, log inverse)\n";
  std::cout << "  • Trigonometric identities (double angle, Pythagorean)\n";
  std::cout << "  • Tangent definition (tan = sin/cos)\n";
  std::cout << "  • Automatic integration with existing pipelines\n";

  return 0;
}
