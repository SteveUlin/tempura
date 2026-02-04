// Microbenchmark for LTO vs non-LTO comparison
// Runs symbolic differentiation + evaluation many times to get measurable runtime

#include <chrono>
#include <print>
#include <ratio>

#include "symbolic4/core.h"
#include "symbolic4/constants.h"
#include "symbolic4/operators.h"
#include "symbolic4/strategy/diff.h"
#include "symbolic4/interpreter/eval.h"

using namespace tempura::symbolic4;

struct TagX {};
struct TagY {};

// Prevent the compiler from optimizing away the result
volatile double sink = 0;

auto main() -> int {
  Symbol<TagX> x;
  Symbol<TagY> y;

  // Build a moderately complex expression
  auto f = sin(x * x) + cos(x * y) - exp(x + y) + log(x * x + y * y + 1_c);

  // Differentiate twice (creates large expression types)
  auto df_dx = differentiate(f, x);
  auto d2f = differentiate(df_dx, x);

  constexpr int kIterations = 1'000'000;

  auto start = std::chrono::high_resolution_clock::now();

  double sum = 0.0;
  for (int i = 0; i < kIterations; ++i) {
    double xv = 1.0 + 0.001 * (i % 100);
    double yv = 2.0 + 0.001 * (i % 50);
    sum += evaluate(d2f, x = xv, y = yv);
  }
  sink = sum;

  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration<double, std::milli>(end - start).count();

  std::println("Iterations: {}", kIterations);
  std::println("Total time: {:.2f} ms", elapsed);
  std::println("Per iteration: {:.3f} ns", elapsed * 1e6 / kIterations);
  std::println("Sum (to prevent optimization): {:.6e}", static_cast<double>(sink));

  return 0;
}
