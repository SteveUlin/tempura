
#include <format>
#include <iostream>
#include <print>
#include <thread>
#include <vector>

#include "bayes/cauchy.h"
#include "bayes/exponential.h"
#include "bayes/gamma.h"
#include "bayes/logistic.h"
#include "bayes/normal.h"
#include "plot.h"

using namespace tempura;

auto main() -> int {
  // bayes::Logistic dist{1.0, 1.0};
  // bayes::Normal dist{0.0, 1.0};
  bayes::Gamma dist{0.5, 1.0};
  std::mt19937 gen{std::random_device{}()};

  constexpr size_t kSamples = 100'000;
  std::vector<double> samples;
  samples.reserve(kSamples);
  for (size_t i = 0; i < kSamples; ++i) {
    samples.push_back(dist.sample(gen));
  }

  constexpr size_t kPointsPerFrame = 200;
  constexpr int kBins = 21;
  for (size_t i = 1; i < kSamples / kPointsPerFrame; ++i) {
    if (i > 1) {
      std::print("\033[{}F\033[0J", kBins);
    }
    auto slice = samples | std::views::take(i * kPointsPerFrame);
    std::print("{}", generateTextHistogram(slice, {.bins = kBins,
                                                   .min_x = 0.0,
                                                   .max_x = 5.0,
                                                   .max_y = 30000,
                                                   .normalize = false}));
    std::cout.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }

  return 0;
}
