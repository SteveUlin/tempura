#include <iostream>
#include <typeinfo>

#include "symbolic3/evaluate.h"
#include "symbolic3/pattern_matching.h"
#include "symbolic3/simplify.h"

using namespace tempura::symbolic3;

// Helper to print expression type structure
template <typename T>
void print_type(const char* label) {
  std::cout << label << ": " << typeid(T).name() << "\n";
}

auto main() -> int {
  std::cout << "\n=== Manual Step-by-Step Simplification ===\n\n";

  Symbol x;
  Symbol y;

  // Start with: x + y + x (which is (x + y) + x due to left-assoc)
  auto expr = x + y + x;

  std::cout << "Initial: x + y + x\n";
  print_type<decltype(expr)>("  Type");
  std::cout << "  Evaluates to: " << evaluate(expr, BinderPack{x = 10, y = 5})
            << "\n\n";

  // Apply simplify
  {
    auto result = simplify(expr, default_context());
    std::cout << "After simplify:\n";
    print_type<decltype(result)>("  Type");
    std::cout << "  Evaluates to: "
              << evaluate(result, BinderPack{x = 10, y = 5}) << "\n\n";

    // Check if it matches expected pattern
    constexpr bool is_collected =
        match(result, x * 𝐜 + 𝐚𝐧𝐲) || match(result, x * 𝐜);
    std::cout << "  Matches 'x*c + ...': " << (is_collected ? "YES" : "NO")
              << "\n\n";
  }

  // Now test with simpler case: x + x (should become 2*x)
  {
    auto simple = x + x;
    std::cout << "Simple case: x + x\n";
    print_type<decltype(simple)>("  Before Type");

    auto result = simplify(simple, default_context());
    print_type<decltype(result)>("  After Type");
    std::cout << "  Evaluates to: " << evaluate(result, BinderPack{x = 10})
              << "\n";

    constexpr bool is_mul = match(result, x * 𝐜) || match(result, 𝐜 * x);
    std::cout << "  Is multiplication: " << (is_mul ? "YES" : "NO") << "\n\n";
  }

  // Test pattern matching directly
  {
    std::cout << "Direct pattern matching test:\n";
    // auto xx = x + x;  // Reuse 'x' from above
    constexpr bool matches_like_terms = match(x + x, x_ + x_);
    std::cout << "  (x + x) matches x_ + x_: "
              << (matches_like_terms ? "YES" : "NO") << "\n\n";
  }

  // Test if LikeTerms rule works in isolation
  {
    std::cout << "Testing LikeTerms rule directly:\n";
    constexpr auto like_terms_rule = Rewrite{x_ + x_, x_ * 2_c};
    auto result = like_terms_rule.apply(x + x, default_context());
    print_type<decltype(result)>("  Result type");
    std::cout << "  Evaluates to: " << evaluate(result, BinderPack{x = 10})
              << "\n";

    constexpr bool matches_mul = match(result, x * 𝐜) || match(result, 𝐜 * x);
    std::cout << "  Is multiplication: " << (matches_mul ? "YES" : "NO")
              << "\n\n";
  }

  return 0;
}
