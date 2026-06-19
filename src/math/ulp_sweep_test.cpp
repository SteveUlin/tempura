#include "ulp_sweep.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  constexpr std::array xs{0.1, 0.5, 1.0, 2.0, 7.3, 100.0, 1e6};

  "identical candidate and reference → 0 ULP"_test = [&] {
    auto r = sweepUlps([](double x) { return std::sqrt(x); },
                       [](double x) { return std::sqrt(x); }, xs);
    expectEq(r.max_ulps, std::uint64_t{0});
    expectFalse(r.class_mismatch);
    // and the assertion wrapper passes
    expectTrue(expectMaxUlps([](double x) { return std::sqrt(x); },
                             [](double x) { return std::sqrt(x); }, xs, 0));
  };

  "a small perturbation registers a small, bounded ULP error"_test = [&] {
    auto r = sweepUlps([](double x) { return std::sqrt(x) * (1.0 + 0x1p-50); },
                       [](double x) { return std::sqrt(x); }, xs);
    expectGT(r.max_ulps, std::uint64_t{0});
    expectLT(r.max_ulps, std::uint64_t{64});
  };

  "class mismatch is caught as a hard failure, not a huge ULP number"_test = [&] {
    // candidate returns +inf where the reference is finite (a reconstruction-bug shape)
    auto r = sweepUlps([](double x) { return x > 5.0 ? std::numeric_limits<double>::infinity() : x; },
                       [](double x) { return x; }, xs);
    expectTrue(r.class_mismatch);
    expectEq(r.max_ulps, std::numeric_limits<std::uint64_t>::max());
    expectEq(r.worst_input, 7.3);  // first point where x > 5
  };

  "matching non-finite classes are fine (both nan, both +inf)"_test = [&] {
    constexpr std::array edge{std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity()};
    auto r = sweepUlps([](double x) { return x; }, [](double x) { return x; }, edge);
    expectEq(r.max_ulps, std::uint64_t{0});  // nan==nan, inf==inf → 0, no mismatch
    expectFalse(r.class_mismatch);
  };

  return TestRegistry::result();
}
