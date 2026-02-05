#include "symbolic4/mcmc/support.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // =========================================================================
  // Support inference
  // =========================================================================

  "HalfNormal has PositiveSupport"_test = [] {
    using C = Constant<0.0>;
    using Dist = HalfNormalDist<C>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, PositiveSupport>);
  };

  "Beta has UnitIntervalSupport"_test = [] {
    using C = Constant<0.0>;
    using Dist = BetaDist<C, C>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, UnitIntervalSupport>);
  };

  "Gamma has PositiveSupport"_test = [] {
    using C = Constant<0.0>;
    using Dist = GammaDist<C, C>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, PositiveSupport>);
  };

  "Normal has RealSupport"_test = [] {
    using C = Constant<0.0>;
    using Dist = NormalDist<C, C>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, RealSupport>);
  };

  "Exponential has PositiveSupport"_test = [] {
    using C = Constant<0.0>;
    using Dist = ExponentialDist<C>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, PositiveSupport>);
  };

  // =========================================================================
  // autoTransform inference
  // =========================================================================

  "autoTransform halfNormal → positive"_test = [] {
    auto sigma = halfNormal(2.0_c);
    auto t = autoTransform(sigma);

    static_assert(is_positive_v<decltype(t)>);
  };

  "autoTransform beta → unitInterval"_test = [] {
    auto theta = beta(2.0_c, 3.0_c);
    auto t = autoTransform(theta);

    static_assert(is_unit_interval_v<decltype(t)>);
  };

  "autoTransform gamma → positive"_test = [] {
    auto alpha = gamma(2.0_c, 1.0_c);
    auto t = autoTransform(alpha);

    static_assert(is_positive_v<decltype(t)>);
  };

  "autoTransform normal → unconstrained"_test = [] {
    auto mu = normal(0.0_c, 1.0_c);
    auto t = autoTransform(mu);

    static_assert(is_unconstrained_v<decltype(t)>);
  };

  "autoTransform preserves explicit transform"_test = [] {
    auto sigma = halfNormal(2.0_c);
    // User can override with explicit transform
    auto t = autoTransform(positive(sigma));

    static_assert(is_positive_v<decltype(t)>);
  };

  return TestRegistry::result();
}
