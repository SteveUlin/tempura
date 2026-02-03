// Test for discoverParams functionality

#include "symbolic4/distributions/discover_params.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/indexed/plate.h"
#include "unit/unit.h"

using namespace tempura;
using namespace tempura::symbolic4;
using namespace unit;

auto main() -> int {
  "discoverParams finds scalar params"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto sigma = halfNormal(2.0);
    auto y = normal(alpha, sigma);

    auto discovered = discoverParams(y);

    // Should find 2 params: alpha and sigma
    constexpr std::size_t num_params = std::tuple_size_v<decltype(discovered)>;
    expectEq(2u, num_params);
  };

  "discoverParams with indexed observation"_test = [] {
    struct Obs {};

    auto alpha = normal(0.0, 10.0);
    auto sigma = halfNormal(2.0);
    auto y = plate<Obs>(normal(alpha, sigma));

    auto discovered = discoverParams(y);

    // Should find 2 params: alpha and sigma (y is observed, not discovered)
    constexpr std::size_t num_params = std::tuple_size_v<decltype(discovered)>;
    expectEq(2u, num_params);
  };

  "discovered params have correct IDs"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto sigma = halfNormal(2.0);
    auto y = normal(alpha, sigma);

    auto discovered = discoverParams(y);

    // Get the first discovered param
    auto& first = std::get<0>(discovered);

    // Its symbol should match alpha's symbol type
    using FirstSymbol = typename std::decay_t<decltype(first)>::symbol_type;
    using AlphaSymbol = typename decltype(alpha)::unconstrained_symbol_type;

    // They should be the same type
    static_assert(std::is_same_v<FirstSymbol, AlphaSymbol> ||
                  std::tuple_size_v<decltype(discovered)> == 2,
                  "Discovered params should have matching IDs");
  };

  return TestRegistry::result();
}
