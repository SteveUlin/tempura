#include <cassert>
#include <iostream>

#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

int main() {
  std::cout << "=== Fraction Feature Demo ===" << std::endl << std::endl;

  // ===================================================================
  // Manual Fraction Creation
  // ===================================================================
  std::cout << "1. Manual Fraction Creation:" << std::endl;

  constexpr auto half_direct = Fraction<1, 2>{};
  std::cout << "   Fraction<1, 2>: " << toStringRuntime(half_direct)
            << std::endl;

  constexpr auto half_literal = 1_frac / 2_frac;
  std::cout << "   1_frac / 2_frac: " << toStringRuntime(half_literal)
            << std::endl;

  constexpr auto third_const = third;
  std::cout << "   third constant: " << toStringRuntime(third_const)
            << std::endl;

  std::cout << std::endl;

  // ===================================================================
  // Automatic GCD Reduction
  // ===================================================================
  std::cout << "2. Automatic GCD Reduction:" << std::endl;

  constexpr auto reduced = Fraction<4, 6>{};
  std::cout << "   Fraction<4, 6> reduces to: " << toStringRuntime(reduced)
            << " (numerator=" << reduced.numerator
            << ", denominator=" << reduced.denominator << ")" << std::endl;

  constexpr auto reduced2 = Fraction<10, 15>{};
  std::cout << "   Fraction<10, 15> reduces to: " << toStringRuntime(reduced2)
            << std::endl;

  std::cout << std::endl;

  // ===================================================================
  // Fraction Arithmetic
  // ===================================================================
  std::cout << "3. Fraction Arithmetic:" << std::endl;

  constexpr auto sum = Fraction<1, 2>{} + Fraction<1, 3>{};
  std::cout << "   1/2 + 1/3 = " << toStringRuntime(sum) << std::endl;

  constexpr auto product = Fraction<2, 3>{} * Fraction<3, 4>{};
  std::cout << "   2/3 * 3/4 = " << toStringRuntime(product) << std::endl;

  constexpr auto quotient = Fraction<1, 2>{} / Fraction<1, 3>{};
  std::cout << "   (1/2) / (1/3) = " << toStringRuntime(quotient) << std::endl;

  std::cout << std::endl;

  // ===================================================================
  // Mixed Arithmetic with Constants
  // ===================================================================
  std::cout << "4. Mixed Arithmetic with Constants:" << std::endl;

  constexpr auto mixed1 = Fraction<1, 2>{} + Constant<1>{};
  std::cout << "   1/2 + 1 = " << toStringRuntime(mixed1) << std::endl;

  constexpr auto mixed2 = Constant<2>{} * Fraction<1, 3>{};
  std::cout << "   2 * 1/3 = " << toStringRuntime(mixed2) << std::endl;

  std::cout << std::endl;

  // ===================================================================
  // Evaluation to Numeric Values
  // ===================================================================
  std::cout << "5. Evaluation to Numeric Values:" << std::endl;

  constexpr auto half_val = evaluate(Fraction<1, 2>{}, BinderPack{});
  std::cout << "   1/2 as double: " << half_val << std::endl;

  constexpr auto third_val = evaluate(Fraction<1, 3>{}, BinderPack{});
  std::cout << "   1/3 as double: " << third_val << std::endl;

  std::cout << std::endl;

  // ===================================================================
  // Symbolic Expressions with Fractions
  // ===================================================================
  std::cout << "6. Symbolic Expressions with Fractions:" << std::endl;

  Symbol x, y;

  auto expr1 = x * Fraction<1, 2>{};
  std::cout << "   x * (1/2): " << toStringRuntime(expr1) << std::endl;

  auto expr2 = x * half + y * third;
  std::cout << "   x * half + y * third: " << toStringRuntime(expr2)
            << std::endl;

  auto expr3 = pow(x, Fraction<1, 2>{});
  std::cout << "   x^(1/2): " << toStringRuntime(expr3) << std::endl;

  std::cout << std::endl;

  // ===================================================================
  // Comparison and Ordering
  // ===================================================================
  std::cout << "7. Comparison and Ordering:" << std::endl;

  static_assert(Fraction<1, 3>{} < Fraction<1, 2>{});
  std::cout << "   1/3 < 1/2: true" << std::endl;

  static_assert(Fraction<2, 4>{} == Fraction<1, 2>{});
  std::cout << "   2/4 == 1/2: true (after reduction)" << std::endl;

  static_assert(lessThan(Fraction<1, 4>{}, Fraction<1, 3>{}));
  std::cout << "   lessThan(1/4, 1/3): true" << std::endl;

  std::cout << std::endl;

  // ===================================================================
  // Sign Normalization
  // ===================================================================
  std::cout << "8. Sign Normalization:" << std::endl;

  constexpr auto neg1 = Fraction<-3, 4>{};
  std::cout << "   Fraction<-3, 4>: " << toStringRuntime(neg1) << std::endl;

  constexpr auto neg2 = Fraction<3, -4>{};
  std::cout << "   Fraction<3, -4> normalizes to: " << toStringRuntime(neg2)
            << " (numerator=" << neg2.numerator << ")" << std::endl;

  constexpr auto neg3 = Fraction<-3, -4>{};
  std::cout << "   Fraction<-3, -4> normalizes to: " << toStringRuntime(neg3)
            << std::endl;

  std::cout << std::endl;

  // ===================================================================
  // Common Fraction Constants
  // ===================================================================
  std::cout << "9. Common Fraction Constants:" << std::endl;

  std::cout << "   half: " << toStringRuntime(half) << std::endl;
  std::cout << "   third: " << toStringRuntime(third) << std::endl;
  std::cout << "   quarter: " << toStringRuntime(quarter) << std::endl;
  std::cout << "   two_thirds: " << toStringRuntime(two_thirds) << std::endl;
  std::cout << "   three_quarters: " << toStringRuntime(three_quarters)
            << std::endl;

  std::cout << std::endl << "=== All tests passed! ===" << std::endl;

  return 0;
}
