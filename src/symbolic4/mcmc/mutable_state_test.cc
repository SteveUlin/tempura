#include "symbolic4/mcmc/mutable_state.h"

#include <vector>

#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Symbol types for testing
namespace {
struct AlphaSym {};
struct SigmaSym {};
struct ZSym {};
}  // namespace

auto main() -> int {
  "ScalarSlot basic access"_test = [] {
    ScalarSlot<AlphaSym> slot{3.14};
    expectNear(slot[AlphaSym{}], 3.14, 1e-10);

    slot[AlphaSym{}] = 2.71;
    expectNear(slot[AlphaSym{}], 2.71, 1e-10);
  };

  "IndexedSlot basic access"_test = [] {
    IndexedSlot<ZSym> slot{std::vector<double>{1.0, 2.0, 3.0}};
    auto span = slot[ZSym{}];
    expectEq(span.size(), 3UL);
    expectNear(span[0], 1.0, 1e-10);
    expectNear(span[2], 3.0, 1e-10);

    // Mutation through span
    span[1] = 42.0;
    expectNear(slot[ZSym{}][1], 42.0, 1e-10);
  };

  "MutableState scalar+indexed"_test = [] {
    auto state = MutableState{
        ScalarSlot<AlphaSym>{1.0},
        ScalarSlot<SigmaSym>{2.0},
        IndexedSlot<ZSym>{std::vector<double>{0.1, 0.2, 0.3}}};

    // Scalar access
    expectNear(state[AlphaSym{}], 1.0, 1e-10);
    expectNear(state[SigmaSym{}], 2.0, 1e-10);

    // Indexed access
    auto z = state[ZSym{}];
    expectEq(z.size(), 3UL);
    expectNear(z[0], 0.1, 1e-10);

    // Mutation
    state[AlphaSym{}] = 99.0;
    expectNear(state[AlphaSym{}], 99.0, 1e-10);

    z[2] = 77.0;
    expectNear(state[ZSym{}][2], 77.0, 1e-10);
  };

  "MutableState size"_test = [] {
    auto state = MutableState{
        ScalarSlot<AlphaSym>{0.0},
        ScalarSlot<SigmaSym>{0.0},
        IndexedSlot<ZSym>{std::vector<double>(5, 0.0)}};

    // 1 + 1 + 5 = 7 total scalar values
    expectEq(state.size(), 7UL);
  };

  "MutableState copy semantics"_test = [] {
    auto state = MutableState{
        ScalarSlot<AlphaSym>{1.0},
        IndexedSlot<ZSym>{std::vector<double>{2.0, 3.0}}};

    auto copy = state;
    copy[AlphaSym{}] = 99.0;
    copy[ZSym{}][0] = 88.0;

    // Original unchanged
    expectNear(state[AlphaSym{}], 1.0, 1e-10);
    expectNear(state[ZSym{}][0], 2.0, 1e-10);

    // Copy has new values
    expectNear(copy[AlphaSym{}], 99.0, 1e-10);
    expectNear(copy[ZSym{}][0], 88.0, 1e-10);
  };

  "MutableState const access"_test = [] {
    const auto state = MutableState{
        ScalarSlot<AlphaSym>{5.0},
        IndexedSlot<ZSym>{std::vector<double>{1.0, 2.0}}};

    // Const scalar → const double&
    const double& val = state[AlphaSym{}];
    expectNear(val, 5.0, 1e-10);

    // Const indexed → span<const double>
    std::span<const double> z = state[ZSym{}];
    expectEq(z.size(), 2UL);
    expectNear(z[0], 1.0, 1e-10);
  };

  return TestRegistry::result();
}
