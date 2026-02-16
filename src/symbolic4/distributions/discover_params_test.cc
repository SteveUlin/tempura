// Test for discoverParams functionality

#include "symbolic4/constants.h"
#include "symbolic4/distributions/discover_params.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/indexed.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "discoverParams finds scalar params"_test = [] {
    auto alpha = normal(0_c, 10_c);
    auto sigma = halfNormal(2_c);
    auto y = normal(alpha, sigma);

    auto discovered = discoverParams(y);

    // Should find 2 params: alpha and sigma
    constexpr std::size_t num_params = std::tuple_size_v<decltype(discovered)>;
    expectEq(2u, num_params);
  };

  "discoverParams with indexed observation"_test = [] {
    struct Obs {};

    auto alpha = normal(0_c, 10_c);
    auto sigma = halfNormal(2_c);
    auto y = plate(normal(alpha, sigma), Obs{});

    auto discovered = discoverParams(y);

    // Should find 2 params: alpha and sigma (y is observed, not discovered)
    constexpr std::size_t num_params = std::tuple_size_v<decltype(discovered)>;
    expectEq(2u, num_params);
  };

  "discoverParams finds mixed scalar + indexed params"_test = [] {
    struct Countries {};

    auto a = normal(-2_c, 1_c);
    auto sigma = exponential(1_c);
    auto z_b = plate(normal(0_c, 1_c), Countries{});
    auto n = data(Countries{});

    // p = sigmoid(a + sigma * z_b)
    auto p = 1_c / (1_c + exp(-(a + sigma * z_b)));
    auto k = plate(binomial(n, p), Countries{});

    auto discovered = discoverParams(k);

    // Should find 3 params: a (scalar), sigma (scalar), z_b (indexed)
    constexpr auto num = std::tuple_size_v<decltype(discovered)>;
    expectEq(3u, num);
  };

  return TestRegistry::result();
}
