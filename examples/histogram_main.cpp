
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
  bayes::Normal dist{0.0, 1.0};
  // bayes::Gamma dist{0.5, 1.0};
  std::mt19937 gen{std::random_device{}()};

  constexpr size_t kSamples = 200'000;
  std::vector<double> samples;
  samples.reserve(kSamples);
  for (size_t i = 0; i < kSamples; ++i) {
    samples.push_back(dist.sample(gen));
  }

  constexpr size_t kPointsPerFrame = 400;
  constexpr int kBins = 21;
  std::vector<std::string> frames;
  for (size_t i = 1; i < kSamples / kPointsPerFrame; ++i) {
    auto slice = samples | std::views::take(i * kPointsPerFrame);
    frames.push_back(generateTextHistogram(slice, {.bins = kBins,
                                                   .min_x = -5.0,
                                                   .max_x = 5.0,
                                                   .max_y = 30000,
                                                   .normalize = false}));
  }

  // The next render will be 16ms (~60fps) from the previous render time. If
  // this is new time is in the past, we will keep adding 30ms increments and
  // drop frames.
  auto next_render = std::chrono::system_clock::now();
  bool first = true;
  for (size_t i = 0; i < frames.size() - 1; ++i) {
    std::this_thread::sleep_until(next_render);
    next_render += std::chrono::milliseconds(16);
    if (next_render < std::chrono::system_clock::now()) {
      continue;
    }
    if (!first) {
      std::print("\033[{}F\033[0J", kBins);
    } else {
      first = false;
    }
    std::cout << frames[i];
    std::cout.flush();
  }

  return 0;
}
