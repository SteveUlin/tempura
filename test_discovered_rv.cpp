#include <iostream>
#include "symbolic4/infer.h"
#include "symbolic4/distributions/random_var.h"

using namespace tempura::symbolic4;

int main() {
    // Test if DiscoveredRV satisfies IsRandomVar
    using TestDist = NormalDist<Atom<void, Embedded<double>>, Atom<void, Embedded<double>>>;
    struct TestId {};
    using TestParam = DiscoveredParam<TestId, TestDist>;
    using TestRV = infer_detail::DiscoveredRV<TestParam>;
    
    std::cout << "IsRandomVar<TestRV>: " << IsRandomVar<TestRV> << "\n";
    std::cout << "IsRandomVar<RandomVar<TestDist, TestId>>: " << IsRandomVar<RandomVar<TestDist, TestId>> << "\n";
    
    return 0;
}
