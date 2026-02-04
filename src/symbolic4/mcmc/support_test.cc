#include "symbolic4/mcmc/support.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // =========================================================================
  // Support inference
  // =========================================================================

  "HalfNormal has PositiveSupport"_test = [] {
    using Lit = Literal<double>;
    using Dist = HalfNormalDist<Lit>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, PositiveSupport>);
  };

  "Beta has UnitIntervalSupport"_test = [] {
    using Lit = Literal<double>;
    using Dist = BetaDist<Lit, Lit>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, UnitIntervalSupport>);
  };

  "Gamma has PositiveSupport"_test = [] {
    using Lit = Literal<double>;
    using Dist = GammaDist<Lit, Lit>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, PositiveSupport>);
  };

  "Normal has RealSupport"_test = [] {
    using Lit = Literal<double>;
    using Dist = NormalDist<Lit, Lit>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, RealSupport>);
  };

  "Exponential has PositiveSupport"_test = [] {
    using Lit = Literal<double>;
    using Dist = ExponentialDist<Lit>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, PositiveSupport>);
  };

  // =========================================================================
  // autoTransform inference
  // =========================================================================

  "autoTransform halfNormal → positive"_test = [] {
    auto sigma = halfNormal(2.0);
    auto t = autoTransform(sigma);

    static_assert(is_positive_v<decltype(t)>);
  };

  "autoTransform beta → unitInterval"_test = [] {
    auto theta = beta(2.0, 3.0);
    auto t = autoTransform(theta);

    static_assert(is_unit_interval_v<decltype(t)>);
  };

  "autoTransform gamma → positive"_test = [] {
    auto alpha = gamma(2.0, 1.0);
    auto t = autoTransform(alpha);

    static_assert(is_positive_v<decltype(t)>);
  };

  "autoTransform normal → unconstrained"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto t = autoTransform(mu);

    static_assert(is_unconstrained_v<decltype(t)>);
  };

  "autoTransform preserves explicit transform"_test = [] {
    auto sigma = halfNormal(2.0);
    // User can override with explicit transform
    auto t = autoTransform(positive(sigma));

    static_assert(is_positive_v<decltype(t)>);
  };

  return TestRegistry::result();
}
