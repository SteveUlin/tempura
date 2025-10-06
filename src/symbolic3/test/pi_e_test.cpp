#include <cmath>
#include <iostream>

#include "symbolic3/evaluate.h"
#include "symbolic3/simplify.h"

using namespace tempura::symbolic3;

auto main() -> int {
  std::cout << "\n=== π and e Constants Test ===\n\n";

  // Test 1: Pi constant
  {
    constexpr auto expr = π;
    std::cout << "Test 1: π constant\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto val = evaluate(expr, BinderPack{});
    std::cout << "π evaluates to: " << val << "\n";
    std::cout << "Expected: ~3.14159265358979323846\n";
    if (std::abs(val - 3.14159265358979323846) < 1e-10) {
      std::cout << "✅ PASS\n";
    } else {
      std::cout << "❌ FAIL\n";
      return 1;
    }
    std::cout << std::endl;
  }

  // Test 2: e constant
  {
    constexpr auto expr = e;
    std::cout << "Test 2: e constant\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto val = evaluate(expr, BinderPack{});
    std::cout << "e evaluates to: " << val << "\n";
    std::cout << "Expected: ~2.71828182845904523536\n";
    if (std::abs(val - 2.71828182845904523536) < 1e-10) {
      std::cout << "✅ PASS\n";
    } else {
      std::cout << "❌ FAIL\n";
      return 1;
    }
    std::cout << std::endl;
  }

  // Test 3: Pi in expressions (2π)
  {
    auto expr = π * Constant<2>{};
    std::cout << "Test 3: π * 2\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto val = evaluate(expr, BinderPack{});
    std::cout << "π * 2 evaluates to: " << val << "\n";
    std::cout << "Expected: ~6.28318530717958647692\n";
    if (std::abs(val - 6.28318530717958647692) < 1e-10) {
      std::cout << "✅ PASS\n";
    } else {
      std::cout << "❌ FAIL\n";
      return 1;
    }
    std::cout << std::endl;
  }

  // Test 4: e^2
  {
    auto expr = pow(e, Constant<2>{});
    std::cout << "Test 4: e^2\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto val = evaluate(expr, BinderPack{});
    std::cout << "e^2 evaluates to: " << val << "\n";
    std::cout << "Expected: ~7.38905609893065022723\n";
    if (std::abs(val - 7.38905609893065022723) < 1e-10) {
      std::cout << "✅ PASS\n";
    } else {
      std::cout << "❌ FAIL\n";
      return 1;
    }
    std::cout << std::endl;
  }

  // Test 5: sin(π/2)
  {
    auto expr = sin(π / Constant<2>{});
    std::cout << "Test 5: sin(π/2)\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto val = evaluate(expr, BinderPack{});
    std::cout << "sin(π/2) evaluates to: " << val << "\n";
    std::cout << "Expected: 1.0\n";
    if (std::abs(val - 1.0) < 1e-10) {
      std::cout << "✅ PASS\n";
    } else {
      std::cout << "❌ FAIL\n";
      return 1;
    }
    std::cout << std::endl;
  }

  // Test 6: e^(iπ) + 1 = 0 (using real parts only for now)
  {
    // For now just test e^π since we don't have complex numbers
    auto expr = pow(e, π);
    std::cout << "Test 6: e^π\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto val = evaluate(expr, BinderPack{});
    std::cout << "e^π evaluates to: " << val << "\n";
    std::cout << "Expected: ~23.1406926327792690057\n";
    if (std::abs(val - 23.1406926327792690057) < 1e-9) {
      std::cout << "✅ PASS\n";
    } else {
      std::cout << "❌ FAIL\n";
      return 1;
    }
    std::cout << std::endl;
  }

  std::cout << "\n✅ All π and e tests passed!\n\n";
  return 0;
}
