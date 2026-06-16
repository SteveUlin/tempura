#include "unit.h"

#include <array>

using namespace tempura;

// The same expect* vocabulary runs in constant evaluation.
constexpr auto compileTimeChecks() -> bool {
  bool ok = true;
  ok &= expectEq(2 + 2, 4);
  ok &= expectNeq(1, 2);
  ok &= expectTrue(1 < 2);
  ok &= expectFalse(2 < 1);
  ok &= expectLT(1, 2);
  ok &= expectLE(2, 2);
  ok &= expectGT(3, 2);
  ok &= expectGE(3, 3);
  ok &= expectNear(1.0, 1.0 + 1e-6, 1e-3);
  ok &= expectRangeEq(std::array{1, 2, 3}, std::array{1, 2, 3});
  ok &= expectRangeNear(std::array{1.0, 2.0}, std::array{1.0 + 1e-6, 2.0}, 1e-3);
  return ok;
}
static_assert(compileTimeChecks());

auto main() -> int {
  "expect vocabulary runs at runtime"_test = [] {
    expectEq(2 + 2, 4);
    expectNeq(1, 2);
    expectTrue(true);
    expectFalse(false);
    expectLT(1, 2);
    expectLE(2, 2);
    expectGT(3, 2);
    expectGE(3, 3);
    expectNear(1.0, 1.0 + 1e-6, 1e-3);
    expectNear(1.0, 1.0 + 1e-6);  // default delta
    expectRangeEq(std::array{1, 2, 3}, std::array{1, 2, 3});
    expectRangeNear(std::array{1.0, 2.0}, std::array{1.0 + 1e-6, 2.0}, 1e-3);
  };

  "the same checks pass in constant evaluation"_test = [] { expectTrue(compileTimeChecks()); };

  return TestRegistry::result();
}
