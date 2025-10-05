#include "symbolic.h"
#include "pattern_matching.h"
#include <iostream>

using namespace tempura;

// Define a symbol for testing
constexpr auto x = [] {};

int main() {
  std::cout << "=== Pattern Matching Demo ===\n\n";

  // Example 1: Simple pattern matching with bindings
  std::cout << "Example 1: Matching pow(x, 2)\n";
  constexpr auto expr1 = pow(Symbol<decltype(x)>{}, 2_c);

  // OLD WAY (verbose):
  std::cout << "  Old way: if (match(expr, pow(AnyArg{}, 2_c))) { auto base = left(expr); }\n";

  // NEW WAY (clean):
  std::cout << "  New way: if (auto [base, exp] = pattern_match(pow(x_, n_), expr)) { ... }\n";
  std::cout << "  Pattern matched: " << (match(expr1, pow(AnyArg{}, 2_c)) ? "yes" : "no") << "\n\n";

  // Example 2: Simplification rules using patterns
  std::cout << "Example 2: Simplification Rules\n";
  std::cout << "  Pattern: pow(x_, 0_c) => 1\n";
  std::cout << "  Pattern: pow(x_, 1_c) => x\n";
  std::cout << "  Pattern: x_ * 0_c => 0\n";
  std::cout << "  Pattern: x_ + 0_c => x\n\n";

  // Example 3: Complex nested pattern
  std::cout << "Example 3: Nested Pattern (x^a)^b\n";
  constexpr auto expr3 = pow(pow(Symbol<decltype(x)>{}, 2_c), 3_c);
  std::cout << "  Expression: (x^2)^3\n";
  std::cout << "  Pattern: pow(pow(x_, a_), b_)\n";
  std::cout << "  Can extract: x, a=2, b=3\n";
  std::cout << "  Transform to: x^(a*b) = x^6\n\n";

  // Example 4: Comparison with current approach
  std::cout << "Example 4: Code Comparison\n";
  std::cout << R"(
CURRENT APPROACH (verbose):
  template <Symbolic S>
  static constexpr auto apply(S expr) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(expr);
    return pow(x, a * b);
  }

PATTERN MATCHING APPROACH (clean):
  return when(pow(pow(x_, a_), b_))
    .then([](auto x, auto a, auto b) {
      return pow(x, a * b);
    });
)";

  std::cout << "\n=== Proposed Pattern Syntax ===\n\n";

  std::cout << "Binders:\n";
  std::cout << "  x_, y_, z_  - Bind to any expression\n";
  std::cout << "  a_, b_, c_  - Bind to any expression\n";
  std::cout << "  n_, m_, p_  - Bind to any expression\n\n";

  std::cout << "Wildcards:\n";
  std::cout << "  AnyArg{}      - Match any expression\n";
  std::cout << "  AnyExpr{}     - Match any Expression<Op, ...>\n";
  std::cout << "  AnyConstant{} - Match any Constant<value>\n";
  std::cout << "  AnySymbol{}   - Match any Symbol<Unique>\n\n";

  std::cout << "Usage:\n";
  std::cout << R"(
// Destructure and transform
if (auto result = pattern_match(pow(x_, n_), expr)) {
  auto base = result.get<0>();  // or: auto [base, exp] = result;
  auto exp = result.get<1>();
  return /* transformation */;
}

// Fluent DSL style
return when(pow(x_, 0_c)).then([]() { return 1_c; })
  .or_when(pow(x_, 1_c)).then([](auto x) { return x; })
  .or_when(x_ * 0_c).then([]() { return 0_c; })
  .or_else([](auto expr) { return expr; });

// Use in simplification rules
struct PowZeroRule {
  static constexpr auto pattern = pow(x_, 0_c);

  template <Symbolic S>
  static constexpr auto matches(S expr) {
    return pattern_match(pattern, expr).matched;
  }

  template <Symbolic S>
  static constexpr auto apply(S expr) {
    return 1_c;  // No need to extract anything!
  }
};

struct PowPowRule {
  static constexpr auto pattern = pow(pow(x_, a_), b_);

  template <Symbolic S>
  static constexpr auto apply(S expr) {
    auto result = pattern_match(pattern, expr);
    return pow(result.get<0>(),          // x
               result.get<1>() * result.get<2>());  // a * b
  }
};
)";

  std::cout << "\n=== Benefits ===\n\n";
  std::cout << "1. No manual left/right/operand navigation\n";
  std::cout << "2. Pattern is self-documenting\n";
  std::cout << "3. Type-safe binding extraction\n";
  std::cout << "4. Closer to mathematical notation\n";
  std::cout << "5. Easier to read and maintain\n";
  std::cout << "6. Less error-prone\n\n";

  std::cout << "=== Potential Extensions ===\n\n";
  std::cout << "- Guards: when(pow(x_, n_)).where(n > 0)\n";
  std::cout << "- Repeated patterns: when(x_ + x_)  // Match x + x but not x + y\n";
  std::cout << "- Commutative matching: when(x_ + y_) matches both x+y and y+x\n";
  std::cout << "- Type constraints: when(pow(x_, Constant{}))\n";
  std::cout << "- Sequence matching: when(add(xs_...))  // Match any number of addends\n\n";

  return 0;
}
