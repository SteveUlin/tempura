#include "symbolic4/distributions/discrete.h"

#include <array>

#include "symbolic4/interpreter/eval.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Test enum
enum class Choice { A, B, C, kCount };

auto main() -> int {
  "DiscreteSymbol creation"_test = [] {
    DiscreteSymbol<struct X, Choice> x;

    static_assert(is_discrete_symbol_v<decltype(x)>);
    static_assert(decltype(x)::num_values == 3);
  };

  "DiscreteSymbol binding"_test = [] {
    DiscreteSymbol<struct X, Choice> x;

    auto binding = (x = Choice::B);

    static_assert(is_discrete_binding_v<decltype(binding)>);
    expectEq(binding.index(), 1UL);
    expectEq(binding.value, Choice::B);
  };

  "DiscreteDist with literal probabilities"_test = [] {
    // Create distribution with makeDiscreteDist
    auto dist = makeDiscreteDist<Choice>(0.5_c, 0.3_c, 0.2_c);

    static_assert(decltype(dist)::K == 3);

    // Check we can access probability expressions
    auto p0 = dist.template probAt<0>();
    auto p1 = dist.template probAt<1>();
    auto p2 = dist.template probAt<2>();

    expectNear(evaluate(p0, BinderPack<>{}), 0.5, 1e-10);
    expectNear(evaluate(p1, BinderPack<>{}), 0.3, 1e-10);
    expectNear(evaluate(p2, BinderPack<>{}), 0.2, 1e-10);
  };

  "DiscreteDist with symbolic probabilities"_test = [] {
    Symbol<struct PA> p_a;
    Symbol<struct PB> p_b;
    Symbol<struct PC> p_c;

    auto dist = makeDiscreteDist<Choice>(p_a, p_b, p_c);

    // Get log-prob expression for symbolic discrete variable
    DiscreteSymbol<struct X, Choice> x;
    auto lp = dist.logProbFor(x);

    static_assert(is_select_v<decltype(lp)>);
  };

  "Select basic"_test = [] {
    Symbol<struct I> idx;
    auto s = select(idx, 10_c, 20_c, 30_c);

    static_assert(is_select_v<decltype(s)>);
    static_assert(decltype(s)::num_values == 3);
  };

  "discreteDist factory function"_test = [] {
    auto dist = discreteDist<Choice>(0.25_c, 0.5_c, 0.25_c);

    auto p0 = dist.template probAt<0>();
    auto p1 = dist.template probAt<1>();
    auto p2 = dist.template probAt<2>();

    expectNear(evaluate(p0, BinderPack<>{}), 0.25, 1e-10);
    expectNear(evaluate(p1, BinderPack<>{}), 0.5, 1e-10);
    expectNear(evaluate(p2, BinderPack<>{}), 0.25, 1e-10);
  };

  "makeDiscreteDistFromArray"_test = [] {
    std::array<double, 3> probs = {0.1, 0.2, 0.7};
    auto dist = makeDiscreteDistFromArray<Choice>(probs);

    static_assert(decltype(dist)::K == 3);
  };

  return TestRegistry::result();
}
