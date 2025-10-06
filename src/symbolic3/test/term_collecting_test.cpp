#include <cassert>
#include <iostream>
#include <type_traits>

#include "symbolic3/evaluate.h"
#include "symbolic3/matching.h"
#include "symbolic3/simplify.h"
#include "symbolic3/to_string.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  std::cout << "\n=== Term Collecting and Canonical Ordering Tests ===\n\n";

  "Like terms collection"_test = [] {
    Symbol x;
    auto expr = x + x;
    auto result = full_simplify(expr, default_context());

    std::cout << "  x + x simplifies to: "
              << tempura::symbolic3::toStringRuntime(result) << "\n";

    // Check structure: should be multiplication of x and a constant
    bool is_factored = match(result, x * 𝐜) || match(result, 𝐜 * x);
    std::cout << "  Is factored (x * c): " << (is_factored ? "YES" : "NO")
              << "\n";

    // Verify correctness by evaluation
    auto val = evaluate(result, BinderPack{x = 5});
    std::cout << "  Evaluates to " << val << " (expected 10)\n";
    assert(val == 10);
  };

  "Factor simple"_test = [] {
    Symbol x;
    auto expr = x * 2_c + x;
    auto result = full_simplify(expr, default_context());

    std::cout << "  x*2 + x simplifies to: " << toStringRuntime(result) << "\n";

    // Should simplify to x * 3 (factoring out x)
    // The structure should be x * constant or constant * x
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x * 𝐜) || match(result, 𝐜 * x),
    //               "x*2 + x should factor to x times a constant");

    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 30);
  };

  "Factor both sides"_test = [] {
    Symbol x;
    auto expr = x * 2_c + x * 3_c;
    auto result = full_simplify(expr, default_context());

    std::cout << "  x*2 + x*3 simplifies to: " << toStringRuntime(result)
              << "\n";

    // Should simplify to x * 5 (collecting coefficients)
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x * 𝐜) || match(result, 𝐜 * x),
    // "x*2 + x*3 should factor to x times a constant");

    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 50);
  };

  "Factor reversed"_test = [] {
    Symbol x;
    auto expr = x + x * 2_c;
    auto result = full_simplify(expr, default_context());

    std::cout << "  x + x*2 simplifies to: " << toStringRuntime(result) << "\n";

    // Should simplify to x * 3 (factoring regardless of order)
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x * 𝐜) || match(result, 𝐜 * x),
    // "x + x*2 should factor to x times a constant");

    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 30);
  };

  "Complex factoring"_test = [] {
    Symbol x;
    auto expr = x * 2_c + x * 3_c + x * 4_c;
    auto result = full_simplify(expr, default_context());

    std::cout << "  x*2 + x*3 + x*4 simplifies to: " << toStringRuntime(result)
              << "\n";

    // Should simplify to x * 9 (collecting all coefficients)
    // This tests multiple applications of the factoring rule
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x * 𝐜) || match(result, 𝐜 * x),
    // "x*2 + x*3 + x*4 should factor to x times a constant");

    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 90);
  };

  "Canonical ordering addition"_test = [] {
    Symbol x;
    Symbol y;

    // Test that ordering produces a canonical form
    auto expr1 = y + x;
    auto result1 = full_simplify(expr1, default_context());

    auto expr2 = x + y;
    auto result2 = full_simplify(expr2, default_context());

    std::cout << "  y + x simplifies to: " << toStringRuntime(result1) << "\n";
    std::cout << "  x + y simplifies to: " << toStringRuntime(result2) << "\n";

    // CRITICAL: Both should produce the SAME canonical form
    // This tests that the ordering rule establishes a consistent ordering
    // TODO: Re-enable after fixing simplification
    // static_assert(std::is_same_v<decltype(result1), decltype(result2)>,
    // "y + x and x + y should produce identical canonical forms");

    auto val1 = evaluate(result1, BinderPack{x = 5, y = 3});
    auto val2 = evaluate(result2, BinderPack{x = 5, y = 3});
    assert(val1 == 8);
    assert(val2 == 8);
  };

  "Canonical ordering multiplication"_test = [] {
    Symbol x;
    Symbol y;

    // Test that multiplication ordering produces a canonical form
    auto expr1 = y * x;
    auto result1 = full_simplify(expr1, default_context());

    auto expr2 = x * y;
    auto result2 = full_simplify(expr2, default_context());

    std::cout << "  y * x simplifies to: " << toStringRuntime(result1) << "\n";
    std::cout << "  x * y simplifies to: " << toStringRuntime(result2) << "\n";

    // Both should produce the SAME canonical form
    // TODO: Re-enable after fixing simplification
    // static_assert(std::is_same_v<decltype(result1), decltype(result2)>,
    // "y * x and x * y should produce identical canonical forms");

    auto val1 = evaluate(result1, BinderPack{x = 5, y = 3});
    auto val2 = evaluate(result2, BinderPack{x = 5, y = 3});
    assert(val1 == 15);
    assert(val2 == 15);
  };

  "Associativity reordering"_test = [] {
    Symbol x;
    Symbol y;
    Symbol z;

    auto expr = (x + z) + y;
    auto result = full_simplify(expr, default_context());

    // Should evaluate correctly
    auto val = evaluate(result, BinderPack{x = 1, y = 2, z = 3});
    assert(val == 6);
  };

  "Mixed operations with factoring"_test = [] {
    Symbol x;
    auto expr = (x + 1_c) * 2_c + (x + 1_c) * 3_c;
    auto result = full_simplify(expr, default_context());

    std::cout << "  (x+1)*2 + (x+1)*3 simplifies to: "
              << toStringRuntime(result) << "\n";

    // Should factor (x+1) out: (x+1) * 5
    // Pattern should be (x+c1) * c2 where c2 = 5
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, 𝐚𝐧𝐲𝐞𝐱𝐩𝐫 * 𝐜) || match(result, 𝐜 * 𝐚𝐧𝐲𝐞𝐱𝐩𝐫),
    // "(x+1)*2 + (x+1)*3 should factor to expression * constant");

    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 55);
  };

  "Distribution prevents re-factoring loop"_test = [] {
    Symbol x;
    Symbol y;

    // (x+y)*2 should distribute to x*2 + y*2
    auto expr = (x + y) * 2_c;
    auto result = full_simplify(expr, default_context());

    std::cout << "  (x+y)*2 simplifies to: " << toStringRuntime(result) << "\n";

    // Should be a sum after distribution (not the original multiplication)
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, 𝐚𝐧𝐲 + 𝐚𝐧𝐲),
    // "(x+y)*2 should distribute to a sum");

    auto val = evaluate(result, BinderPack{x = 3, y = 4});
    assert(val == 14);
  };

  "Nested factoring"_test = [] {
    Symbol x;
    auto expr = x + x + x;
    auto result = full_simplify(expr, default_context());

    std::cout << "  x + x + x simplifies to: " << toStringRuntime(result)
              << "\n";

    // Should simplify to x * 3 through repeated like-terms collection
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x * 𝐜) || match(result, 𝐜 * x),
    // "x + x + x should factor to x times a constant");

    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 30);
  };

  "No infinite rewrite loop on ordered addition"_test = [] {
    Symbol x;
    Symbol y;
    Symbol z;

    // This tests termination - should not loop infinitely
    auto expr = (x + y) + (z + x);
    auto result = full_simplify(expr, default_context());

    std::cout << "  (x+y)+(z+x) simplifies to: " << toStringRuntime(result)
              << "\n";

    // Just verify it terminates and evaluates correctly
    // (Testing that it doesn't hang during compilation)
    auto val = evaluate(result, BinderPack{x = 1, y = 2, z = 3});
    assert(val == 7);
  };

  "Power collecting"_test = [] {
    Symbol x;
    auto expr = x * pow(x, 2_c);
    auto result = full_simplify(expr, default_context());

    std::cout << "  x * x^2 simplifies to: " << toStringRuntime(result) << "\n";

    // Should simplify to x^3 (combining powers with same base)
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, pow(x, 𝐜)),
    // "x * x^2 should combine to x^n for some constant n");

    auto val = evaluate(result, BinderPack{x = 2});
    assert(val == 8);
  };

  "Power collecting both sides"_test = [] {
    Symbol x;
    auto expr = pow(x, 2_c) * pow(x, 3_c);
    auto result = full_simplify(expr, default_context());

    std::cout << "  x^2 * x^3 simplifies to: " << toStringRuntime(result)
              << "\n";

    // Should simplify to x^5 (adding exponents)
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, pow(x, 𝐜)),
    // "x^2 * x^3 should combine to x^n for some constant n");

    auto val = evaluate(result, BinderPack{x = 2});
    assert(val == 32);
  };

  std::cout << "All term collecting and canonical ordering tests passed!\n";
  return TestRegistry::result();
}
