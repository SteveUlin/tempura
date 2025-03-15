
#include <format>
#include <print>
#include <thread>
#include <vector>

#include "bayes/exponential.h"
#include "bayes/normal.h"
#include "bayes/logistic.h"
#include "bayes/cauchy.h"
#include "plot.h"

using namespace tempura;

constexpr int N = 21;

auto main() -> int {
  // bayes::Logistic dist{1.0, 1.0};
  bayes::Normal dist{0.0, 1.0};
  std::mt19937 gen{std::random_device{}()};
  std::vector<double> samples;
  samples.reserve(10'000);
  for (int64_t i = 0; i < 200'000; ++i) {
    samples.push_back(dist.ratioOfUniforms(gen));
    if (i % 1000 == 0) {
      if (i > 0) {
        // Clear the last 20 lines with an ANSI escape code.
        std::print("\033[{}F\033[0J", N);
      }
      std::print("{}", generateTextHistogram(samples, {.bins = 21,
                                                       .min_x = -5.0,
                                                       .max_x = 5.0,
                                                       .max_y = 30000,
                                                       .normalize = false}));

      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

  }

  return 0;
}
