#include "bayes/bernoulli.h"

#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "Bernoulli prob"_test = [] {
    expectNear(0.8, Bernoulli{0.8}.prob(true));
    expectNear(0.2, Bernoulli{0.8}.prob(false));
  };

  "Bernoulli sample"_test = [] {
    std::mt19937 g{0};
    Bernoulli b{0.5};
    const int N = 10'000;
    int count = 0;
    for (int i = 0; i < N; ++i) {
      count += static_cast<int>(b.sample(g));
    }
    expectNear<.1>(0.5, static_cast<double>(count) / N);
  };

  "Bernoulli logProb"_test = [] {
    expectNear<0.1>(-0.22, Bernoulli{0.8}.logProb(true));
    expectNear<0.1>(-1.61, Bernoulli{0.8}.logProb(false));
  };

  // TODO: Re-enable when unnormalizedLogProb is implemented
  // "Bernoulli unnormalizedLogProb"_test = [] {
  //   expectNear<0.1>(-0.22, Bernoulli{0.8}.unnormalizedLogProb(true));
  //   expectNear<0.1>(-1.61, Bernoulli{0.8}.unnormalizedLogProb(false));
  // };

  "Bernoulli cdf"_test = [] {
    expectNear<0.1>(0.2, Bernoulli{0.8}.cdf(false));
    expectNear<0.1>(1.0, Bernoulli{0.8}.cdf(true));
  };
}
