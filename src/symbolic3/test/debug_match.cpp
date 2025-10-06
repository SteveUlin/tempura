// Test match function directly
#include <iostream>

#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/pattern_matching.h"

using namespace tempura::symbolic3;

int main() {
  constexpr Symbol y;
  constexpr auto expr = y + 0_c;
  constexpr auto pattern = x_ + 0_c;

  std::cout << "Testing match function...\n\n";

  // Test basic match
  constexpr bool result = match(pattern, expr);
  std::cout << "Does (x_ + 0_c) match (y + 0_c)? " << (result ? "YES" : "NO")
            << "\n";

  // Test individual components
  constexpr bool pat_var_matches_symbol = match(x_, y);
  std::cout << "Does x_ match y? " << (pat_var_matches_symbol ? "YES" : "NO")
            << "\n";

  constexpr bool const_matches = match(0_c, 0_c);
  std::cout << "Does 0_c match 0_c? " << (const_matches ? "YES" : "NO") << "\n";

  return 0;
}
