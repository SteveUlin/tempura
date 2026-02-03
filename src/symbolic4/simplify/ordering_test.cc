#include "symbolic4/simplify/ordering.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Define symbols at file scope for constexpr usage
inline constexpr Symbol<struct X> x{};
inline constexpr Symbol<struct Y> y{};
inline constexpr Symbol<struct Z> z{};

auto main() -> int {
  "constants order by value"_test = [] {
    static_assert(compare(Constant<1>{}, Constant<2>{}) == Ordering::Less);
    static_assert(compare(Constant<5>{}, Constant<3>{}) == Ordering::Greater);
    static_assert(compare(Constant<7>{}, Constant<7>{}) == Ordering::Equal);
    static_assert(compare(Constant<-1>{}, Constant<1>{}) == Ordering::Less);
  };

  "fractions order by value"_test = [] {
    static_assert(compare(Fraction<1, 2>{}, Fraction<3, 4>{}) == Ordering::Less);
    static_assert(compare(Fraction<2, 3>{}, Fraction<1, 3>{}) == Ordering::Greater);
    static_assert(compare(Fraction<1, 2>{}, Fraction<2, 4>{}) == Ordering::Equal);
    static_assert(compare(Fraction<-1, 2>{}, Fraction<1, 2>{}) == Ordering::Less);
  };

  "symbols order consistently"_test = [] {
    // Same symbol compares equal
    static_assert(compare(x, x) == Ordering::Equal);
    static_assert(compare(y, y) == Ordering::Equal);

    // Different symbols have consistent ordering (antisymmetric)
    constexpr auto xy = compare(x, y);
    constexpr auto yx = compare(y, x);
    static_assert((xy == Ordering::Less && yx == Ordering::Greater) ||
                  (xy == Ordering::Greater && yx == Ordering::Less) ||
                  (xy == Ordering::Equal && yx == Ordering::Equal));
  };

  "cross-category ordering"_test = [] {
    constexpr auto expr = x + y;

    // Expression < Atom
    static_assert(compare(expr, x) == Ordering::Less);
    static_assert(compare(x, expr) == Ordering::Greater);

    // Atom < Fraction
    static_assert(compare(x, Fraction<1, 2>{}) == Ordering::Less);
    static_assert(compare(Fraction<1, 2>{}, x) == Ordering::Greater);

    // Fraction < Constant
    static_assert(compare(Fraction<1, 2>{}, Constant<1>{}) == Ordering::Less);
    static_assert(compare(Constant<1>{}, Fraction<1, 2>{}) == Ordering::Greater);

    // Transitive: Expression < Constant
    static_assert(compare(expr, Constant<1>{}) == Ordering::Less);
  };

  "expressions order by operator first"_test = [] {
    constexpr auto add_expr = x + y;
    constexpr auto mul_expr = x * y;

    // AddOp < MulOp (by our precedence ordering)
    static_assert(compare(add_expr, mul_expr) == Ordering::Less);
    static_assert(compare(mul_expr, add_expr) == Ordering::Greater);
  };

  "same operator compares args lexicographically"_test = [] {
    // x + y vs x + y should be equal
    static_assert(compare(x + y, x + y) == Ordering::Equal);

    // x + y vs x + z: compare second args (y vs z)
    constexpr auto xy_xz = compare(x + y, x + z);
    constexpr auto y_z = compare(y, z);
    static_assert(xy_xz == y_z);
  };

  "nested expressions compare recursively"_test = [] {
    constexpr auto y_vs_z = compare(y, z);
    constexpr auto inner_cmp = compare(x + y, x + z);
    static_assert(inner_cmp == y_vs_z);
  };

  "lessThan and greaterThan predicates"_test = [] {
    static_assert(lessThan(Constant<1>{}, Constant<2>{}));
    static_assert(!lessThan(Constant<2>{}, Constant<1>{}));
    static_assert(!lessThan(Constant<1>{}, Constant<1>{}));

    static_assert(greaterThan(Constant<2>{}, Constant<1>{}));
    static_assert(!greaterThan(Constant<1>{}, Constant<2>{}));
    static_assert(!greaterThan(Constant<1>{}, Constant<1>{}));
  };

  "SymbolicComparator for sorting"_test = [] {
    constexpr SymbolicComparator cmp;
    static_assert(cmp(Constant<1>{}, Constant<2>{}));
    static_assert(!cmp(Constant<2>{}, Constant<1>{}));
  };

  return TestRegistry::result();
}
