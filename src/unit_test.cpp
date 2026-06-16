#include "unit.h"

#include <array>
#include <limits>

using namespace tempura;

namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
}  // namespace

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
  ok &= expectClose(1.0, 1.0 + 1e-10, {.rtol = 1e-9});
  ok &= expectClose(0.0, 1e-12, {.atol = 1e-9});
  ok &= expectWithinUlps(1.0, 1.0 + 0x1p-52, 1);
  ok &= expectFinite(1.0);
  ok &= expectNan(kNan);
  ok &= expectInf(kInf);
  ok &= expectRangeEq(std::array{1, 2, 3}, std::array{1, 2, 3});
  ok &= expectRangeClose(std::array{1.0, 2.0}, std::array{1.0, 2.0 + 1e-10}, {.rtol = 1e-9});
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
    expectRangeEq(std::array{1, 2, 3}, std::array{1, 2, 3});
  };

  "numeric vocabulary runs at runtime"_test = [] {
    expectClose(1.0, 1.0 + 1e-10, {.rtol = 1e-9});
    expectClose(0.0, 1e-12, {.atol = 1e-9});
    expectWithinUlps(1.0, 1.0 + 0x1p-52, 1);
    expectFinite(1.0);
    expectNan(kNan);
    expectInf(kInf);
    expectRangeClose(std::array{1.0, 2.0, 3.0}, std::array{1.0, 2.0 + 1e-10, 3.0}, {.rtol = 1e-9});
  };

  "the same checks pass in constant evaluation"_test = [] { expectTrue(compileTimeChecks()); };

  return TestRegistry::result();
}
