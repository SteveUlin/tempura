#include "tape.h"

#include <cmath>
#include <vector>

#include "dual.h"
#include "unit.h"
#include "var.h"

using namespace tempura;
using namespace tempura::autodiff;

auto main() -> int {
  "single variable: d(x²)/dx = 2x"_test = [] {
    Tape<double> tape;
    auto x = variable(tape, 3.0);
    auto y = x * x;
    auto adj = tape.backward(y.idx);
    expectClose((y.value), 9.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.idx]), 6.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  "multiple inputs: one sweep yields the whole gradient"_test = [] {
    // f(x,z) = x·z + sin(x); ∂f/∂x = z + cos(x), ∂f/∂z = x
    Tape<double> tape;
    auto x = variable(tape, 1.3);
    auto z = variable(tape, 2.1);
    auto f = x * z + sin(x);
    auto adj = tape.backward(f.idx);
    expectClose((f.value), 1.3 * 2.1 + std::sin(1.3), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.idx]), 2.1 + std::cos(1.3), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[z.idx]), 1.3, {.rtol = 1e-12, .atol = 1e-12});
  };

  "shared subexpression stays linear: d(x⁴)/dx = 4x³"_test = [] {
    // node.h's recursive sweep was EXPONENTIAL here (re-walks the shared w each path);
    // the flat tape accumulates both contributions in one linear pass.
    Tape<double> tape;
    auto x = variable(tape, 2.0);
    auto w = x * x;  // x², used twice
    auto q = w * w;  // (x²)² = x⁴
    auto adj = tape.backward(q.idx);
    expectClose((q.value), 16.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.idx]), 4 * 8.0, {.rtol = 1e-12, .atol = 1e-12});  // 4x³ = 32
  };

  "diamond DAG gradient"_test = [] {
    // a = x·z, b = x+z, c = a·b; ∂c/∂x = z·b + a, ∂c/∂z = x·b + a
    Tape<double> tape;
    auto x = variable(tape, 2.0);
    auto z = variable(tape, 3.0);
    auto a = x * z;        // 6
    auto b = x + z;        // 5
    auto c = a * b;        // 30
    auto adj = tape.backward(c.idx);
    expectClose((c.value), 30.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.idx]), 3.0 * 5.0 + 6.0, {.rtol = 1e-12, .atol = 1e-12});  // 21
    expectClose((adj[z.idx]), 2.0 * 5.0 + 6.0, {.rtol = 1e-12, .atol = 1e-12});  // 16
  };

  "reverse matches forward (dual) on the same function"_test = [] {
    // ∂/∂x of f(x,z)=x·z+sin(x) at (1.3, 2.1): reverse adj[x] vs forward dual seed on x
    Tape<double> tape;
    auto x = variable(tape, 1.3);
    auto z = variable(tape, 2.1);
    auto fx = (x * z + sin(x));
    double reverse_dx = tape.backward(fx.idx)[x.idx];

    Dual<double> dx{1.3, 1.0};
    Dual<double> dz{2.1, 0.0};
    auto forward = dx * dz + sin(dx);
    expectClose(reverse_dx, (forward.gradient), {.rtol = 1e-12, .atol = 1e-12});
  };

  "the tape is reusable: repeated sweeps are identical (graph untouched)"_test = [] {
    Tape<double> tape;
    auto x = variable(tape, 1.5);
    auto y = exp(x) * x;  // dy/dx = eˣ(x+1)
    auto a1 = tape.backward(y.idx);
    auto a2 = tape.backward(y.idx);
    expectClose((a1[x.idx]), std::exp(1.5) * 2.5, {.rtol = 1e-12, .atol = 1e-12});
    expectEq((a1[x.idx]), (a2[x.idx]));
  };

  "scalar mixed ops record the right partial"_test = [] {
    Tape<double> tape;
    auto x = variable(tape, 4.0);
    auto y = 2.0 * x + 3.0;  // dy/dx = 2
    auto adj = tape.backward(y.idx);
    expectClose((y.value), 11.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.idx]), 2.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  return TestRegistry::result();
}
