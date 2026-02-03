// Tests for OrderedMonotonic
#include "symbolic4/mcmc/ordered_effect.h"
#include "unit.h"

#include <array>
#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Basic cumEffect properties
  // ===========================================================================

  "OrderedMonotonic<2> cumEffect baseline and max"_test = [] {
    // K=2: 2 levels, 1-element simplex (single parameter)
    // delta[0] = 1.0 (trivial simplex)
    std::array<double, 1> delta = {1.0};

    auto cum = OrderedMonotonic<2>::cumEffect(delta);

    // cum[1] = 0 (baseline)
    // cum[2] = delta[0] = 1.0 (max effect)
    expectEq(0.0, cum[1]);
    expectEq(1.0, cum[2]);
  };

  "OrderedMonotonic<3> cumEffect sums correctly"_test = [] {
    // K=3: 3 levels, 2-element simplex
    std::array<double, 2> delta = {0.6, 0.4};

    auto cum = OrderedMonotonic<3>::cumEffect(delta);

    // cum[1] = 0 (baseline)
    // cum[2] = delta[0] = 0.6
    // cum[3] = delta[0] + delta[1] = 1.0
    expectEq(0.0, cum[1]);
    expectNear(0.6, cum[2], 1e-14);
    expectNear(1.0, cum[3], 1e-14);
  };

  "OrderedMonotonic<4> cumEffect for bangladesh kids"_test = [] {
    // K=4: 4 levels (1-4 children), 3-element simplex
    // This matches the bangladesh_contraception.cpp model
    std::array<double, 3> delta = {0.5, 0.3, 0.2};

    auto cum = OrderedMonotonic<4>::cumEffect(delta);

    // cum[1] = 0 (baseline, 1 child)
    // cum[2] = delta[0] = 0.5
    // cum[3] = delta[0] + delta[1] = 0.8
    // cum[4] = delta[0] + delta[1] + delta[2] = 1.0
    expectEq(0.0, cum[1]);
    expectNear(0.5, cum[2], 1e-14);
    expectNear(0.8, cum[3], 1e-14);
    expectNear(1.0, cum[4], 1e-14);
  };

  "OrderedMonotonic<4> cumEffect with uniform simplex"_test = [] {
    // Uniform: each increment has equal share (1/3 each for 3 deltas)
    std::array<double, 3> delta = {1.0/3.0, 1.0/3.0, 1.0/3.0};

    auto cum = OrderedMonotonic<4>::cumEffect(delta);

    expectEq(0.0, cum[1]);
    expectNear(1.0/3.0, cum[2], 1e-14);
    expectNear(2.0/3.0, cum[3], 1e-14);
    expectNear(1.0, cum[4], 1e-14);
  };

  "OrderedMonotonic cumEffect at K is always 1"_test = [] {
    // With valid simplex (sums to 1), cum[K] = 1.0
    std::array<double, 2> delta3 = {0.7, 0.3};
    auto cum3 = OrderedMonotonic<3>::cumEffect(delta3);
    expectNear(1.0, cum3[3], 1e-14);

    std::array<double, 4> delta5 = {0.1, 0.15, 0.25, 0.5};
    auto cum5 = OrderedMonotonic<5>::cumEffect(delta5);
    expectNear(1.0, cum5[5], 1e-14);
  };

  "OrderedMonotonic cumEffect at 1 is always 0"_test = [] {
    std::array<double, 2> delta3 = {0.8, 0.2};
    auto cum3 = OrderedMonotonic<3>::cumEffect(delta3);
    expectEq(0.0, cum3[1]);

    std::array<double, 5> delta6 = {0.1, 0.2, 0.15, 0.25, 0.3};
    auto cum6 = OrderedMonotonic<6>::cumEffect(delta6);
    expectEq(0.0, cum6[1]);
  };

  // ===========================================================================
  // kNumDeltas constant
  // ===========================================================================

  "OrderedMonotonic kNumDeltas is K-1"_test = [] {
    static_assert(OrderedMonotonic<2>::kNumDeltas == 1);
    static_assert(OrderedMonotonic<3>::kNumDeltas == 2);
    static_assert(OrderedMonotonic<4>::kNumDeltas == 3);
    static_assert(OrderedMonotonic<5>::kNumDeltas == 4);
    static_assert(OrderedMonotonic<10>::kNumDeltas == 9);
  };

  // ===========================================================================
  // deltaRange tests
  // ===========================================================================

  "OrderedMonotonic<4> deltaRange for k=1 (baseline)"_test = [] {
    auto [start, end] = OrderedMonotonic<4>::deltaRange(1);
    expectEq(std::size_t{0}, start);
    expectEq(std::size_t{0}, end);  // Empty range
  };

  "OrderedMonotonic<4> deltaRange for k=2"_test = [] {
    auto [start, end] = OrderedMonotonic<4>::deltaRange(2);
    expectEq(std::size_t{0}, start);
    expectEq(std::size_t{1}, end);  // delta[0] contributes
  };

  "OrderedMonotonic<4> deltaRange for k=3"_test = [] {
    auto [start, end] = OrderedMonotonic<4>::deltaRange(3);
    expectEq(std::size_t{0}, start);
    expectEq(std::size_t{2}, end);  // delta[0], delta[1] contribute
  };

  "OrderedMonotonic<4> deltaRange for k=4"_test = [] {
    auto [start, end] = OrderedMonotonic<4>::deltaRange(4);
    expectEq(std::size_t{0}, start);
    expectEq(std::size_t{3}, end);  // delta[0], delta[1], delta[2] contribute
  };

  "OrderedMonotonic<3> deltaRange covers all levels"_test = [] {
    // k=0 (undefined, but should be safe)
    {
      auto [s, e] = OrderedMonotonic<3>::deltaRange(0);
      expectEq(std::size_t{0}, s);
      expectEq(std::size_t{0}, e);
    }
    // k=1
    {
      auto [s, e] = OrderedMonotonic<3>::deltaRange(1);
      expectEq(std::size_t{0}, s);
      expectEq(std::size_t{0}, e);
    }
    // k=2
    {
      auto [s, e] = OrderedMonotonic<3>::deltaRange(2);
      expectEq(std::size_t{0}, s);
      expectEq(std::size_t{1}, e);
    }
    // k=3
    {
      auto [s, e] = OrderedMonotonic<3>::deltaRange(3);
      expectEq(std::size_t{0}, s);
      expectEq(std::size_t{2}, e);
    }
  };

  // ===========================================================================
  // Integration with actual simplex values
  // ===========================================================================

  "OrderedMonotonic with real simplex from Dirichlet"_test = [] {
    // Typical posterior: most effect in first transition (1->2 kids)
    // This matches the bangladesh_contraception.cpp model output
    std::array<double, 3> delta = {0.75, 0.15, 0.10};  // 75% in first transition

    auto cum = OrderedMonotonic<4>::cumEffect(delta);

    // Effects relative to baseline (K=1):
    // K=2: 0.75 of max effect (big jump)
    // K=3: 0.90
    // K=4: 1.0 (max)
    expectEq(0.0, cum[1]);
    expectNear(0.75, cum[2], 1e-14);
    expectNear(0.90, cum[3], 1e-14);
    expectNear(1.0, cum[4], 1e-14);
  };

  "OrderedMonotonic cumEffect is monotonically increasing"_test = [] {
    // Any valid simplex should give monotonically increasing cumulative effects
    std::array<double, 4> delta = {0.3, 0.25, 0.25, 0.2};

    auto cum = OrderedMonotonic<5>::cumEffect(delta);

    // Check monotonicity: cum[1] < cum[2] < cum[3] < cum[4] < cum[5]
    expectTrue(cum[1] < cum[2]);
    expectTrue(cum[2] < cum[3]);
    expectTrue(cum[3] < cum[4]);
    expectTrue(cum[4] <= cum[5]);  // cum[5] = 1.0
  };

  // ===========================================================================
  // Edge cases
  // ===========================================================================

  "OrderedMonotonic<2> minimal case"_test = [] {
    // Simplest case: 2 levels, 1-element "simplex" (just 1.0)
    std::array<double, 1> delta = {1.0};

    auto cum = OrderedMonotonic<2>::cumEffect(delta);

    // Only 3 elements: cum[0] unused, cum[1] = 0, cum[2] = 1
    expectEq(0.0, cum[1]);
    expectEq(1.0, cum[2]);

    // deltaRange
    auto [s1, e1] = OrderedMonotonic<2>::deltaRange(1);
    expectEq(std::size_t{0}, s1);
    expectEq(std::size_t{0}, e1);

    auto [s2, e2] = OrderedMonotonic<2>::deltaRange(2);
    expectEq(std::size_t{0}, s2);
    expectEq(std::size_t{1}, e2);
  };

  "OrderedMonotonic with extreme delta (almost all in first)"_test = [] {
    // Almost all effect in first transition
    std::array<double, 3> delta = {0.99, 0.005, 0.005};

    auto cum = OrderedMonotonic<4>::cumEffect(delta);

    expectEq(0.0, cum[1]);
    expectNear(0.99, cum[2], 1e-14);
    expectNear(0.995, cum[3], 1e-14);
    expectNear(1.0, cum[4], 1e-14);
  };

  // ===========================================================================
  // Compile-time checks
  // ===========================================================================

  "OrderedMonotonic is constexpr"_test = [] {
    constexpr std::array<double, 2> delta = {0.6, 0.4};
    constexpr auto cum = OrderedMonotonic<3>::cumEffect(delta);

    static_assert(cum[1] == 0.0);
    static_assert(cum[3] == 1.0);

    constexpr auto range = OrderedMonotonic<3>::deltaRange(2);
    static_assert(range.first == 0);
    static_assert(range.second == 1);
  };

  return TestRegistry::result();
}
