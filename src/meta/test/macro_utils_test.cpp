#include "meta/macro_utils.h"

#include <print>

#include "unit.h"

using namespace tempura;

// Test macros for FOR_EACH demonstration
#define DECLARE_INT(x) int x = __COUNTER__
#define DECLARE_DOUBLE(x) double x = 3.14
#define INCREMENT(x) ++x

int main() {
  "TEMPURA_FOR_EACH with single argument"_test = [] {
    TEMPURA_FOR_EACH(DECLARE_INT, a)
    expectEq(a, 0);
    std::print("✓ Single arg: a={}\n", a);
  };

  "TEMPURA_FOR_EACH with multiple arguments"_test = [] {
    TEMPURA_FOR_EACH(DECLARE_INT, b, c, d)

    // Each should have different values from __COUNTER__
    expectNeq(b, c);
    expectNeq(c, d);

    std::print("✓ Multiple args: b={}, c={}, d={}\n", b, c, d);
  };

  "TEMPURA_FOR_EACH with many arguments"_test = [] {
    TEMPURA_FOR_EACH(DECLARE_INT, v1, v2, v3, v4, v5, v6, v7, v8)

    expectNeq(v1, v8);
    std::print("✓ 8 args: v1={}, v8={}\n", v1, v8);
  };

  "TEMPURA_FOR_EACH with different macro"_test = [] {
    TEMPURA_FOR_EACH(DECLARE_DOUBLE, x, y, z)

    expectEq(x, 3.14);
    expectEq(y, 3.14);
    expectEq(z, 3.14);

    std::print("✓ Different macro: x={}, y={}, z={}\n", x, y, z);
  };

  "TEMPURA_FOR_EACH with operations"_test = [] {
    int e = 0, f = 0, g = 0;

    TEMPURA_FOR_EACH(INCREMENT, e, f, g)

    expectEq(e, 1);
    expectEq(f, 1);
    expectEq(g, 1);

    std::print("✓ Operations: e={}, f={}, g={}\n", e, f, g);
  };

  "TEMPURA_FOR_EACH with empty args"_test = [] {
    // Should be a no-op (thanks to __VA_OPT__)
    TEMPURA_FOR_EACH(DECLARE_INT)

    std::print("✓ Empty args works (no-op)\n");
  };

  "All FOR_EACH tests passed"_test = [] {
    std::print("✓ All macro expansion tests passed\n");
  };

  return TestRegistry::result();
}

#undef DECLARE_INT
#undef DECLARE_DOUBLE
#undef INCREMENT
