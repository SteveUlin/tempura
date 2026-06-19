#include "benchmark.h"

#include <chrono>
#include <cmath>
#include <print>
#include <random>
#include <vector>

#include "sqrt_approx.h"

using namespace tempura;
using namespace std::chrono_literals;

// Hand-rolled √x and 1/√x vs the standard library. Expected: hardware sqrtsd wins √x
// outright; but 1/√x has NO single double-precision hardware instruction (it's a sqrt +
// a divide), so the division-free fastInvSqrt is where the bit-hack can actually compete.

auto main() -> int {
  std::mt19937_64 gen{0xC0FFEE};
  std::uniform_real_distribution<double> dist{0.1, 1e6};
  std::vector<double> xs(4096);
  for (auto& v : xs) v = dist(gen);
  const auto n = static_cast<double>(xs.size());

  auto sweep = [&](auto f) {
    double acc = 0.0;
    for (double x : xs) acc += f(x);
    doNotOptimize(acc);
  };
  auto run = [&](auto bench, auto f) {  // bench taken by value: ops() mutates it
    return (bench.ops(xs.size()).warmup(50).minEpoch(2ms).maxRuntime(200ms).quiet() =
                [&] { sweep(f); })
        .median_wall.count() /
           n;  // ns per element
  };

  const double hw_sqrt = run("std::sqrt"_bench, [](double x) { return std::sqrt(x); });
  const double newton = run("sqrtNewton"_bench, [](double x) { return sqrtNewton(x); });
  const double hw_rsqrt = run("1/std::sqrt"_bench, [](double x) { return 1.0 / std::sqrt(x); });
  const double fast = run("fastInvSqrt"_bench, [](double x) { return fastInvSqrt(x); });

  std::println("");
  std::println("{:<14}{:>12}{:>14}", "", "ns/element", "vs hardware");
  std::println("{:<14}{:>12.3f}{:>13.2f}x", "std::sqrt", hw_sqrt, 1.0);
  std::println("{:<14}{:>12.3f}{:>13.2f}x", "sqrtNewton", newton, newton / hw_sqrt);
  std::println("{:<14}{:>12.3f}{:>14}", "1/std::sqrt", hw_rsqrt, "1.00x");
  std::println("{:<14}{:>12.3f}{:>13.2f}x", "fastInvSqrt", fast, fast / hw_rsqrt);
  std::println("");
  return 0;
}
