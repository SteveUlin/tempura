#include "tape.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "dual.h"
#include "unit.h"
#include "var.h"
#include "weighted_dag.h"

using namespace tempura;
using namespace tempura::autodiff;

auto main() -> int {
  "single variable: d(x²)/dx = 2x"_test = [] {
    Tape<double> tape;
    auto x = variable(tape, 3.0);
    auto y = x * x;
    auto adj = tape.backward(y.id);
    expectClose((y.value), 9.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.id]), 6.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  "multiple inputs: one sweep yields the whole gradient"_test = [] {
    // f(x,z) = x·z + sin(x); ∂f/∂x = z + cos(x), ∂f/∂z = x
    Tape<double> tape;
    auto x = variable(tape, 1.3);
    auto z = variable(tape, 2.1);
    auto f = x * z + sin(x);
    auto adj = tape.backward(f.id);
    expectClose((f.value), 1.3 * 2.1 + std::sin(1.3), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.id]), 2.1 + std::cos(1.3), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[z.id]), 1.3, {.rtol = 1e-12, .atol = 1e-12});
  };

  "shared subexpression stays linear: d(x⁴)/dx = 4x³"_test = [] {
    // node.h's recursive sweep was EXPONENTIAL here (re-walks the shared w each path);
    // the flat tape accumulates both contributions in one linear pass.
    Tape<double> tape;
    auto x = variable(tape, 2.0);
    auto w = x * x;  // x², used twice
    auto q = w * w;  // (x²)² = x⁴
    auto adj = tape.backward(q.id);
    expectClose((q.value), 16.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.id]), 4 * 8.0, {.rtol = 1e-12, .atol = 1e-12});  // 4x³ = 32
  };

  "diamond DAG gradient"_test = [] {
    // a = x·z, b = x+z, c = a·b; ∂c/∂x = z·b + a, ∂c/∂z = x·b + a
    Tape<double> tape;
    auto x = variable(tape, 2.0);
    auto z = variable(tape, 3.0);
    auto a = x * z;        // 6
    auto b = x + z;        // 5
    auto c = a * b;        // 30
    auto adj = tape.backward(c.id);
    expectClose((c.value), 30.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.id]), 3.0 * 5.0 + 6.0, {.rtol = 1e-12, .atol = 1e-12});  // 21
    expectClose((adj[z.id]), 2.0 * 5.0 + 6.0, {.rtol = 1e-12, .atol = 1e-12});  // 16
  };

  "substitution sweeps match a hand-computed triangular solve"_test = [] {
    // L = [[0,0,0],[2,0,0],[3,5,0]]: forward from e₀ gathers 1 → 2 → 3+10;
    // backward from e₂ scatters the same paths, so the (0,2) entries agree.
    WeightedDag<double> dag;
    auto a = dag.addNode({});
    auto b = dag.addNode({{a, 2.0}});
    auto c = dag.addNode({{a, 3.0}, {b, 5.0}});
    auto fwd = forwardSubstitute(dag, std::vector{1.0, 0.0, 0.0});
    expectRangeEq(fwd, std::vector{1.0, 2.0, 13.0});
    auto bwd = backwardSubstitute(dag, std::vector{0.0, 0.0, 1.0});
    expectRangeEq(bwd, std::vector{13.0, 5.0, 1.0});
    expectEq(fwd[c], bwd[a]);
  };

  "forward substitution over the recording matches dual jvp"_test = [] {
    // One recording of f(x,z) = x·z + sin(x); tangent seeds (1, 0.5) at the leaves
    Tape<double> tape;
    auto x = variable(tape, 1.3);
    auto z = variable(tape, 2.1);
    auto y = x * z + sin(x);
    auto tangents = tape.forward({{x.id, 1.0}, {z.id, 0.5}});

    auto fwd = Dual<double>{1.3, 1.0} * Dual<double>{2.1, 0.5} + sin(Dual<double>{1.3, 1.0});
    expectClose((tangents[y.id]), (fwd.gradient), {.rtol = 1e-12, .atol = 1e-12});
  };

  "reverse matches forward (dual) on the same function"_test = [] {
    // ∂/∂x of f(x,z)=x·z+sin(x) at (1.3, 2.1): reverse adj[x] vs forward dual seed on x
    Tape<double> tape;
    auto x = variable(tape, 1.3);
    auto z = variable(tape, 2.1);
    auto fx = (x * z + sin(x));
    double reverse_dx = tape.backward(fx.id)[x.id];

    Dual<double> dx{1.3, 1.0};
    Dual<double> dz{2.1, 0.0};
    auto forward = dx * dz + sin(dx);
    expectClose(reverse_dx, (forward.gradient), {.rtol = 1e-12, .atol = 1e-12});
  };

  "the tape is reusable: repeated sweeps are identical (graph untouched)"_test = [] {
    Tape<double> tape;
    auto x = variable(tape, 1.5);
    auto y = exp(x) * x;  // dy/dx = eˣ(x+1)
    auto a1 = tape.backward(y.id);
    auto a2 = tape.backward(y.id);
    expectClose((a1[x.id]), std::exp(1.5) * 2.5, {.rtol = 1e-12, .atol = 1e-12});
    expectEq((a1[x.id]), (a2[x.id]));
  };

  "scalar mixed ops record the right partial"_test = [] {
    Tape<double> tape;
    auto x = variable(tape, 4.0);
    auto y = 2.0 * x + 3.0;  // dy/dx = 2
    auto adj = tape.backward(y.id);
    expectClose((y.value), 11.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.id]), 2.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  "scalars need only construct into T: int literals, c/x"_test = [] {
    Tape<double> tape;
    auto x = variable(tape, 2.0);
    auto y = 2 * x + 1 - 1.0 / x;  // dy/dx = 2 + 1/x²
    auto adj = tape.backward(y.id);
    expectClose((y.value), 4.5, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.id]), 2.25, {.rtol = 1e-12, .atol = 1e-12});
  };

  "inverse trig partials"_test = [] {
    // d asin = 1/√(1−x²), d acos = −1/√(1−x²), d atan = 1/(1+x²), at x = 0.3
    Tape<double> tape;
    auto x = variable(tape, 0.3);
    const double r = 1.0 / std::sqrt(1.0 - 0.09);
    expectClose((tape.backward(asin(x).id)[x.id]), r, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((tape.backward(acos(x).id)[x.id]), -r, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((tape.backward(atan(x).id)[x.id]), 1.0 / 1.09, {.rtol = 1e-12, .atol = 1e-12});
  };

  "pow with variable exponent"_test = [] {
    // z = xᵉ: ∂z/∂x = e·xᵉ⁻¹, ∂z/∂e = xᵉ·ln x, at (2, 3)
    Tape<double> tape;
    auto x = variable(tape, 2.0);
    auto e = variable(tape, 3.0);
    auto z = pow(x, e);
    auto adj = tape.backward(z.id);
    expectClose((z.value), 8.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.id]), 12.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[e.id]), 8.0 * std::numbers::ln2, {.rtol = 1e-12, .atol = 1e-12});
  };

  "an infinite adjoint reaches its leaf intact"_test = [] {
    // d(1/x)/dx = −1/x² → −∞ at x = 0; a zero-multiplied spare slot would NaN it
    Tape<double> tape;
    auto x = variable(tape, 0.0);
    auto y = 1.0 / x;
    auto adj = tape.backward(y.id);
    expectInf((adj[x.id]));
  };

  "fma records a·b + c with a fused value"_test = [] {
    Tape<double> tape;
    auto x = variable(tape, 2.0);
    auto z = variable(tape, 3.0);
    auto y = fma(x, z, x);  // xz + x: ∂/∂x = z + 1, ∂/∂z = x
    auto adj = tape.backward(y.id);
    expectClose((y.value), 8.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[x.id]), 4.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((adj[z.id]), 2.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  "mutable weights re-linearize one recording at a new point"_test = [] {
    // x·x records partials (3, 3); overwritten to (5, 5) — the linearization at x = 5
    Tape<double> tape;
    auto x = variable(tape, 3.0);
    auto y = x * x;
    expectClose((tape.backward(y.id)[x.id]), 6.0, {.rtol = 1e-12, .atol = 1e-12});
    for (auto& e : tape.dag.edges(y.id)) e.weight = 5.0;
    expectClose((tape.backward(y.id)[x.id]), 10.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  "clear reuses one tape across recordings"_test = [] {
    Tape<double> tape;
    {
      auto x = variable(tape, 2.0);
      auto y = x * x;
      expectClose((tape.backward(y.id)[x.id]), 4.0, {.rtol = 1e-12, .atol = 1e-12});
    }
    tape.dag.clear();
    auto x = variable(tape, 1.5);
    auto y = exp(x);
    expectClose((tape.backward(y.id)[x.id]), std::exp(1.5), {.rtol = 1e-12, .atol = 1e-12});
  };

  return TestRegistry::result();
}
