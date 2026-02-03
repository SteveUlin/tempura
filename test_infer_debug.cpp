#include <iostream>
#include <type_traits>
#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/infer.h"

using namespace tempura::symbolic4;

int main() {
    struct Obs {};
    
    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 5.0);
    auto sigma = halfNormal(2.0);
    auto x = data<Obs>();
    auto y = plate<Obs>(normal(alpha + beta * x, sigma));
    
    // Check what discoverParams returns
    auto discovered = discoverParams(y);
    constexpr std::size_t num_discovered = std::tuple_size_v<decltype(discovered)>;
    std::cout << "Number of discovered params: " << num_discovered << "\n";
    
    // Check if the discovered params become DiscoveredRV
    auto discovered_rvs = infer_detail::tupleTransform(
        [](const auto& p) { return infer_detail::makeDiscoveredRV(p); }, 
        discovered);
    constexpr std::size_t num_rvs = std::tuple_size_v<decltype(discovered_rvs)>;
    std::cout << "Number of discovered RVs: " << num_rvs << "\n";
    
    // Check if DiscoveredRV satisfies IsRandomVar
    if constexpr (num_rvs > 0) {
        using FirstRV = std::tuple_element_t<0, decltype(discovered_rvs)>;
        std::cout << "First RV is IsRandomVar: " << IsRandomVar<FirstRV> << "\n";
        std::cout << "First RV is IsIndexedRandomVar: " << IsIndexedRandomVar<FirstRV> << "\n";
    }
    
    return 0;
}
