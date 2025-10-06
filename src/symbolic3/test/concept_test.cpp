// Quick test to verify algebraic_simplify satisfies Strategy concept

#include "symbolic3/simplify.h"
#include "symbolic3/strategy.h"
#include "symbolic3/traversal.h"

using namespace tempura::symbolic3;

int main() {
  // Verify algebraic_simplify is a Strategy
  static_assert(Strategy<decltype(algebraic_simplify)>,
                "algebraic_simplify must satisfy Strategy concept");

  // Try to call innermost directly
  constexpr auto test_strategy = innermost(algebraic_simplify);

  return 0;
}
