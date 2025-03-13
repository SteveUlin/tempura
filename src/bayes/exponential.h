#pragma once

#include <random>

namespace tempura::bayes {

// Exponential distribution
//
// p(x) = β exp(-β x)
//
// The exponential distribution is the probability distribution of the time
// between events in a Poisson point process, i.e., a process in which events
// occur continuously and independently at a constant average rate.
//
// For example:
//  - The time until an earthquake occurs
//  - The time until a radioactive particle decays
//  - The time until a customer arrives at a store
template <typename T = double>
class Exponential {
public:
  constexpr Exponential(T β = 1.0) : β_{β} {}

  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> T {
    // p(y) = p(x) * |dx/dy|
    //
    // let y(x) = - log(x) / β
    //     p(x) = 1.0
    //
    //   => x(y) = exp(-β y)
    //   => p(y)dy = p(x) * |dx/dy| = β exp(-β y)dy

    constexpr T scale = 1.0 / static_cast<T>(Generator::max());
    using std::log;
    return -log(static_cast<T>(g()) * scale) / β_;
  }
private:
  T β_;
};

}
