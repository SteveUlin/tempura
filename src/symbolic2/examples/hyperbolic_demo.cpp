// Demonstration of hyperbolic function support in symbolic2
//
// Shows:
// - Differentiation of sinh, cosh, tanh
// - Simplification of hyperbolic identities
// - Chain rule with hyperbolic functions
// - Evaluation of symbolic hyperbolic expressions

#include <cmath>
#include <iostream>

#include "symbolic2/derivative.h"
#include "symbolic2/evaluate.h"
#include "symbolic2/simplify.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  std::cout << "=== Hyperbolic Function Demonstration ===\n\n";

  // Basic hyperbolic differentiation
  std::cout << "1. Basic Derivatives (compile-time verified):\n";
  {
    Symbol x;

    auto f1 = sinh(x);
    auto df1 = simplify(diff(f1, x));
    std::cout << "  ✓ d/dx[sinh(x)] = cosh(x)\n";
    static_assert(match(df1, cosh(x)));

    auto f2 = cosh(x);
    auto df2 = simplify(diff(f2, x));
    std::cout << "  ✓ d/dx[cosh(x)] = sinh(x)\n";
    static_assert(match(df2, sinh(x)));

    auto f3 = tanh(x);
    auto df3 = diff(f3, x);
    std::cout << "  ✓ d/dx[tanh(x)] = 1/cosh²(x)\n";
    static_assert(match(df3, (1_c / pow(cosh(x), 2_c)) * 1_c));
  }

  // Hyperbolic identities
  std::cout << "\n2. Identity Simplifications:\n";
  {
    constexpr auto e1 = sinh(0_c);
    constexpr auto s1 = simplify(e1);
    std::cout << "  ✓ sinh(0) = 0\n";
    static_assert(match(s1, 0_c));

    constexpr auto e2 = cosh(0_c);
    constexpr auto s2 = simplify(e2);
    std::cout << "  ✓ cosh(0) = 1\n";
    static_assert(match(s2, 1_c));

    constexpr auto e3 = tanh(0_c);
    constexpr auto s3 = simplify(e3);
    std::cout << "  ✓ tanh(0) = 0\n";
    static_assert(match(s3, 0_c));
  }

  // Symmetry properties
  std::cout << "\n3. Symmetry Properties:\n";
  {
    Symbol x;

    auto e1 = sinh(-x);
    auto s1 = simplify(e1);
    std::cout << "  ✓ sinh(-x) = -sinh(x) (odd function)\n";
    static_assert(match(s1, -sinh(x)));

    auto e2 = cosh(-x);
    auto s2 = simplify(e2);
    std::cout << "  ✓ cosh(-x) = cosh(x) (even function)\n";
    static_assert(match(s2, cosh(x)));

    auto e3 = tanh(-x);
    auto s3 = simplify(e3);
    std::cout << "  ✓ tanh(-x) = -tanh(x) (odd function)\n";
    static_assert(match(s3, -tanh(x)));
  }

  // Chain rule
  std::cout << "\n4. Chain Rule Examples:\n";
  {
    Symbol x;

    auto f1 = sinh(x * x);
    auto df1 = diff(f1, x);
    std::cout << "  ✓ d/dx[sinh(x²)] = cosh(x²)·2x (chain rule applied)\n";
    static_assert(match(df1, cosh(x * x) * (1_c * x + x * 1_c)));

    auto f2 = cosh(2_c * x);
    auto df2 = diff(f2, x);
    std::cout << "  ✓ d/dx[cosh(2x)] = sinh(2x)·2 (chain rule applied)\n";
    static_assert(match(df2, sinh(2_c * x) * (0_c * x + 2_c * 1_c)));

    auto f3 = exp(tanh(x));
    auto df3 = diff(f3, x);
    std::cout << "  ✓ d/dx[exp(tanh(x))] = exp(tanh(x))·(1/cosh²(x))\n";
    static_assert(match(df3, exp(tanh(x)) * ((1_c / pow(cosh(x), 2_c)) * 1_c)));
  }

  // Numerical evaluation
  std::cout << "\n5. Numerical Evaluation:\n";
  {
    Symbol x;

    auto f = sinh(x);
    auto df = simplify(diff(f, x));

    double x_val = 1.0;
    auto f_result = evaluate(f, BinderPack{x = x_val});
    auto df_result = evaluate(df, BinderPack{x = x_val});

    std::cout << "  At x = " << x_val << ":\n";
    std::cout << "    sinh(x) = " << f_result
              << " (expected: " << std::sinh(x_val) << ")\n";
    std::cout << "    d/dx[sinh(x)] = cosh(x) = " << df_result
              << " (expected: " << std::cosh(x_val) << ")\n";

    expectNear<0.001>(f_result, std::sinh(x_val));
    expectNear<0.001>(df_result, std::cosh(x_val));
  }

  // Combined expression
  std::cout << "\n6. Complex Expression:\n";
  {
    Symbol x;

    // f(x) = sinh(x)·cosh(x) = (1/2)·sinh(2x)
    auto f = sinh(x) * cosh(x);
    auto df = simplify(diff(f, x));
    std::cout << "  f(x) = sinh(x)·cosh(x)\n";
    std::cout << "  f'(x) = cosh(x)·cosh(x) + sinh(x)·sinh(x) = cosh²(x) + "
                 "sinh²(x)\n";

    double x_val = 0.5;
    auto result = evaluate(df, BinderPack{x = x_val});
    // Expected: cosh²(x) + sinh²(x) = cosh(2x)
    double expected = std::cosh(2.0 * x_val);
    std::cout << "  At x = " << x_val << ": f'(x) = " << result
              << " (cosh(2x) = " << expected << ")\n";

    expectNear<0.001>(result, expected);
  }

  // Improved trigonometric symmetry
  std::cout << "\n7. Trigonometric Symmetry (also improved):\n";
  {
    Symbol x;

    auto e1 = sin(-x);
    auto s1 = simplify(e1);
    std::cout << "  ✓ sin(-x) = -sin(x) (odd function)\n";
    static_assert(match(s1, -sin(x)));

    auto e2 = cos(-x);
    auto s2 = simplify(e2);
    std::cout << "  ✓ cos(-x) = cos(x) (even function)\n";
    static_assert(match(s2, cos(x)));

    auto e3 = tan(-x);
    auto s3 = simplify(e3);
    std::cout << "  ✓ tan(-x) = -tan(x) (odd function)\n";
    static_assert(match(s3, -tan(x)));
  }

  std::cout << "\n✓ All demonstrations passed!\n";
  std::cout << "\nSummary of additions:\n";
  std::cout << "  • Hyperbolic function differentiation (sinh, cosh, tanh)\n";
  std::cout
      << "  • Hyperbolic function simplification (identities & symmetry)\n";
  std::cout << "  • Improved trigonometric simplification (symmetry rules)\n";
  std::cout << "  • All rules verified at compile-time with static_assert\n";

  return 0;
}
