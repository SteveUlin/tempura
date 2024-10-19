#include <print>
#include <random>

#include "bayes/normal.h"

using namespace tempura;
using namespace tempura::bayes;

auto main(int argc, char* argv[]) -> int {
  if (argc != 2) {
    std::println("Usage: sample_normal <number of samples>");
    return 1;
  }

  int n = std::stoi(argv[1]);
  if (n <= 0) {
    std::println("Number of samples must be positive");
    return 1;
  }

  auto normal = bayes::Normal(0., 1.);
  std::mt19937_64 g{};
  while (n-- > 0) {
    std::println("{}", normal.sample(g));
  }
  return 0;
}
