#include <iostream>
#include <typeinfo>

#include "symbolic3/evaluate.h"
#include "symbolic3/pattern_matching.h"
#include "symbolic3/simplify.h"

using namespace tempura::symbolic3;

auto main() -> int {
  std::cout << "\n=== Testing foldConstants directly ===\n\n";

  auto expr = 1_c + 2_c;
  std::cout << "Expression: 1 + 2\n";
  std::cout << "  Type: " << typeid(expr).name() << "\n";
  std::cout << "  Evaluates to: " << evaluate(expr, BinderPack{}) << "\n\n";

  // Check if individual constants match 𝐜
  constexpr bool c1_matches = match(Constant<1>{}, 𝐜);
  constexpr bool c2_matches = match(Constant<2>{}, 𝐜);
  std::cout << "  Constant<1>{} matches 𝐜: " << (c1_matches ? "YES" : "NO")
            << "\n";
  std::cout << "  Constant<2>{} matches 𝐜: " << (c2_matches ? "YES" : "NO")
            << "\n\n";

  // Check the fold expression manually
  // using ExprType = decltype(expr);  // Unused but kept for debugging
  constexpr bool args_are_constants =
      []<typename Op, Symbolic... Args>(Expression<Op, Args...>) {
        return (match(Args{}, 𝐜) && ...);
      }(expr);
  std::cout << "  All args match 𝐜: " << (args_are_constants ? "YES" : "NO")
            << "\n\n";

  // Check if foldConstants can be called
  constexpr bool can_fold = requires { foldConstants(expr); };
  std::cout << "  Can call foldConstants: " << (can_fold ? "YES" : "NO")
            << "\n\n";

  return 0;
}
