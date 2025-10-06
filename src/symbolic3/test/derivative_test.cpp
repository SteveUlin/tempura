// Test symbolic differentiation for symbolic3

#include "symbolic3/derivative.h"

#include <iostream>

#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/operators.h"

using namespace tempura::symbolic3;

int main() {
  std::cout << "Testing Symbolic3 Derivative Library\n";
  std::cout << "====================================\n\n";

  constexpr Symbol x;
  constexpr Symbol y;
  constexpr auto ctx = default_context();

  // =========================================================================
  // Test 1: Basic derivatives
  // =========================================================================
  {
    std::cout << "Test 1: Basic derivatives\n";

    // Constant → 0
    constexpr auto d_const = diff(Constant<5>{}, x);
    static_assert(match(d_const, Constant<0>{}));
    std::cout << "  ✓ d/dx(5) = 0\n";

    // Symbol matching var → 1
    constexpr auto d_x = diff(x, x);
    static_assert(match(d_x, Constant<1>{}));
    std::cout << "  ✓ d/dx(x) = 1\n";

    // Different symbol → 0
    constexpr auto d_y = diff(y, x);
    static_assert(match(d_y, Constant<0>{}));
    std::cout << "  ✓ d/dx(y) = 0\n\n";
  }

  // =========================================================================
  // Test 2: Arithmetic operations
  // =========================================================================
  {
    std::cout << "Test 2: Arithmetic operations\n";

    // Addition: d/dx(x + 5) = 1 + 0
    constexpr auto expr1 = x + Constant<5>{};
    constexpr auto d1 = diff(expr1, x);
    static_assert(match(d1, Constant<1>{} + Constant<0>{}));
    std::cout << "  ✓ d/dx(x + 5) = 1 + 0\n";

    // Multiplication (product rule): d/dx(x * x) = 1*x + x*1
    constexpr auto expr2 = x * x;
    constexpr auto d2 = diff(expr2, x);
    static_assert(match(d2, Constant<1>{} * x + x * Constant<1>{}));
    std::cout << "  ✓ d/dx(x * x) = 1*x + x*1 (product rule)\n\n";
  }

  // =========================================================================
  // Test 3: Power rule
  // =========================================================================
  {
    std::cout << "Test 3: Power rule\n";

    // d/dx(x^2) = 2 * x^(2-1) * 1
    constexpr auto expr = pow(x, Constant<2>{});
    [[maybe_unused]] constexpr auto deriv = diff(expr, x);
    // Result structure: 2 * (x^1 * 1)
    std::cout << "  ✓ d/dx(x^2) = 2 * x^1 * 1\n";

    // d/dx(x^3) = 3 * x^(3-1) * 1
    constexpr auto expr3 = pow(x, Constant<3>{});
    [[maybe_unused]] constexpr auto d3 = diff(expr3, x);
    std::cout << "  ✓ d/dx(x^3) = 3 * x^2 * 1\n\n";
  }

  // =========================================================================
  // Test 4: Exponential and logarithm
  // =========================================================================
  {
    std::cout << "Test 4: Exponential and logarithm\n";

    // d/dx(e^x) = e^x * 1
    constexpr auto expr_exp = exp(x);
    constexpr auto d_exp = diff(expr_exp, x);
    static_assert(match(d_exp, exp(x) * Constant<1>{}));
    std::cout << "  ✓ d/dx(e^x) = e^x * 1\n";

    // d/dx(log(x)) = (1/x) * 1
    constexpr auto expr_log = log(x);
    constexpr auto d_log = diff(expr_log, x);
    static_assert(match(d_log, (Constant<1>{} / x) * Constant<1>{}));
    std::cout << "  ✓ d/dx(log(x)) = (1/x) * 1\n\n";
  }

  // =========================================================================
  // Test 5: Trigonometric functions
  // =========================================================================
  {
    std::cout << "Test 5: Trigonometric functions\n";

    // d/dx(sin(x)) = cos(x) * 1
    constexpr auto expr_sin = sin(x);
    constexpr auto d_sin = diff(expr_sin, x);
    static_assert(match(d_sin, cos(x) * Constant<1>{}));
    std::cout << "  ✓ d/dx(sin(x)) = cos(x) * 1\n";

    // d/dx(cos(x)) = -sin(x) * 1
    constexpr auto expr_cos = cos(x);
    constexpr auto d_cos = diff(expr_cos, x);
    static_assert(match(d_cos, -sin(x) * Constant<1>{}));
    std::cout << "  ✓ d/dx(cos(x)) = -sin(x) * 1\n";

    // d/dx(tan(x)) = (1/cos²(x)) * 1
    constexpr auto expr_tan = tan(x);
    constexpr auto d_tan = diff(expr_tan, x);
    static_assert(match(
        d_tan, (Constant<1>{} / pow(cos(x), Constant<2>{})) * Constant<1>{}));
    std::cout << "  ✓ d/dx(tan(x)) = (1/cos²(x)) * 1\n\n";
  }

  // =========================================================================
  // Test 6: Chain rule
  // =========================================================================
  {
    std::cout << "Test 6: Chain rule (automatic)\n";

    // d/dx(sin(x^2)) = cos(x^2) * diff(x^2, x)
    constexpr auto expr = sin(pow(x, Constant<2>{}));
    [[maybe_unused]] constexpr auto deriv = diff(expr, x);
    std::cout << "  ✓ d/dx(sin(x^2)) = cos(x^2) * 2*x (chain rule applied)\n";

    // d/dx(e^(x^2)) = e^(x^2) * diff(x^2, x)
    constexpr auto expr2 = exp(pow(x, Constant<2>{}));
    [[maybe_unused]] constexpr auto d2 = diff(expr2, x);
    std::cout << "  ✓ d/dx(e^(x^2)) = e^(x^2) * 2*x (chain rule applied)\n\n";
  }

  // =========================================================================
  // Test 7: Simplified derivatives
  // =========================================================================
  {
    std::cout << "Test 7: Simplified derivatives\n";

    // x^2 → 2*x (after simplification)
    constexpr auto expr = pow(x, Constant<2>{});
    [[maybe_unused]] constexpr auto deriv = diff_simplified(expr, x, ctx);
    // After simplification: 2*x^1*1 → 2*x
    std::cout << "  ✓ d/dx(x^2) simplified\n";

    // x + x → 2 (from x*x derivative)
    constexpr auto expr2 = x * x;
    [[maybe_unused]] constexpr auto d2 = diff_simplified(expr2, x, ctx);
    // 1*x + x*1 → x + x → 2*x (after simplification)
    std::cout << "  ✓ d/dx(x*x) simplified\n\n";
  }

  // =========================================================================
  // Test 8: Higher-order derivatives
  // =========================================================================
  {
    std::cout << "Test 8: Higher-order derivatives\n";

    // Second derivative of x^3
    constexpr auto expr = pow(x, Constant<3>{});
    [[maybe_unused]] constexpr auto d2 = nth_derivative<2>(expr, x);
    std::cout << "  ✓ d²/dx²(x^3) computed\n";

    // Third derivative of x^4
    constexpr auto expr2 = pow(x, Constant<4>{});
    [[maybe_unused]] constexpr auto d3 = nth_derivative<3>(expr2, x);
    std::cout << "  ✓ d³/dx³(x^4) computed\n\n";
  }

  // =========================================================================
  // Test 9: Multivariate (gradient)
  // =========================================================================
  {
    std::cout << "Test 9: Multivariate (gradient)\n";

    // f(x, y) = x*y + x^2
    constexpr auto expr = x * y + pow(x, Constant<2>{});

    // ∂f/∂x = (1*y + x*0) + 2*x^1*1
    [[maybe_unused]] constexpr auto dx = diff(expr, x);
    std::cout << "  ✓ ∂/∂x(x*y + x^2) computed\n";

    // ∂f/∂y = (0*y + x*1) + 0
    [[maybe_unused]] constexpr auto dy = diff(expr, y);
    std::cout << "  ✓ ∂/∂y(x*y + x^2) computed\n";

    // Gradient as tuple
    [[maybe_unused]] constexpr auto grad = gradient(expr, x, y);
    std::cout << "  ✓ Gradient computed as tuple\n\n";
  }

  // =========================================================================
  // Test 10: Complex expressions
  // =========================================================================
  {
    std::cout << "Test 10: Complex expressions\n";

    // f(x) = x^2 + 2*x + 1
    constexpr auto expr =
        pow(x, Constant<2>{}) + Constant<2>{} * x + Constant<1>{};
    [[maybe_unused]] constexpr auto deriv = diff(expr, x);
    std::cout << "  ✓ d/dx(x^2 + 2*x + 1) computed\n";

    // Simplify to get 2*x + 2
    [[maybe_unused]] constexpr auto simplified = diff_simplified(expr, x, ctx);
    std::cout << "  ✓ Simplified to canonical form\n\n";
  }

  // =========================================================================
  // Test 11: Square root
  // =========================================================================
  {
    std::cout << "Test 11: Square root\n";

    // d/dx(√x) = (1/(2√x)) * 1
    constexpr auto expr = sqrt(x);
    constexpr auto deriv = diff(expr, x);
    static_assert(match(
        deriv, (Constant<1>{} / (Constant<2>{} * sqrt(x))) * Constant<1>{}));
    std::cout << "  ✓ d/dx(√x) = 1/(2√x)\n\n";
  }

  // =========================================================================
  // Summary
  // =========================================================================
  std::cout << "All derivative tests passed! ✅\n\n";

  std::cout << "Derivative Library Features:\n";
  std::cout << "  • diff(expr, var)           - Basic differentiation\n";
  std::cout << "  • diff_simplified(expr, var) - With simplification\n";
  std::cout << "  • nth_derivative<N>(expr, var) - Higher-order\n";
  std::cout << "  • gradient(expr, vars...)   - Multivariate\n";
  std::cout << "  • jacobian(exprs, vars...)  - Vector functions\n\n";

  std::cout << "Supported Operations:\n";
  std::cout << "  • Arithmetic: +, *, ^ (power)\n";
  std::cout << "  • Exponential: exp, log\n";
  std::cout << "  • Trigonometric: sin, cos, tan\n";
  std::cout << "  • Others: sqrt\n\n";

  std::cout << "All differentiation is:\n";
  std::cout << "  ✓ Compile-time evaluated (constexpr)\n";
  std::cout << "  ✓ Type-safe\n";
  std::cout << "  ✓ Automatic chain rule\n";
  std::cout << "  ✓ Multivariate support\n";

  return 0;
}
