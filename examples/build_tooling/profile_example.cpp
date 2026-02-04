// ============================================================================
// profile_example.cpp — Compile-time profiling target for -ftime-report
// ============================================================================
//
// This file is deliberately template-heavy. It exercises deep instantiation
// chains in symbolic4 to serve as a profiling target that reveals where
// compile time goes.
//
// Build with profiling enabled:
//   cmake -B build -DTEMPURA_PROFILE_COMPILE=ON
//   cmake --build build --target build_tooling_profile 2>profile.log
//
// In profile.log, look for these GCC phases:
//   - "template instantiation" — dominated by Expression<Op, Args...> nesting
//   - "overload resolution"    — operators.h tries many overloads per op
//   - "constant expression evaluation" — constexpr static_asserts in tests
//   - "constraint checking"    — concept satisfaction for Symbolic<T>
//
// ============================================================================

#include <print>

// --- Core expression algebra ---
// Each include pulls in a web of template definitions. The cost compounds
// because Expression<Op, Args...> types grow combinatorially.
#include "symbolic4/core.h"
#include "symbolic4/constants.h"
#include "symbolic4/operators.h"

// --- Strategy-based differentiation ---
// diff.h uses recursive rewriting: each differentiation rule pattern-matches
// the expression tree and rebuilds it. For nested expressions, the compiler
// must instantiate the rule for every sub-expression.
#include "symbolic4/strategy/diff.h"

// --- Evaluation ---
// eval.h uses if-constexpr dispatch over the expression type. Deep trees
// generate deeply nested if-constexpr chains that the compiler must trace.
#include "symbolic4/interpreter/eval.h"

using namespace tempura::symbolic4;

// Symbol tags at namespace scope so their identity is stable across all
// function templates and instantiation contexts.
struct TagX {};
struct TagY {};
struct TagA {};
struct TagB {};
struct TagC {};

// ============================================================================
// Section 1: Deep expression trees
// ============================================================================
// Each binary operation doubles the type depth. A chain of 8 operations
// produces Expression<Op, Expression<Op, Expression<Op, ...>>> ~8 levels deep.
// The compiler must instantiate operator overloads at every level.

auto section1() {
  std::println("--- Section 1: Deep expression trees ---");

  Symbol<TagX> x;
  Symbol<TagY> y;

  // Level 1-2: simple binary ops
  auto e1 = x + y;
  auto e2 = e1 * x;

  // Level 3-4: transcendentals (each wraps in Expression<SinOp/CosOp, ...>)
  auto e3 = sin(e2);
  auto e4 = cos(e3) + exp(x);

  // Level 5-6: more nesting
  auto e5 = e4 * e4 + log(e3 + 1_c);
  auto e6 = sqrt(e5) / (e4 + 1_c);

  // Level 7-8: the deepest chain
  auto e7 = sin(e6) * cos(e5) + exp(e4);
  auto e8 = e7 * e7 - e6 + 2_c;

  double result = evaluate(e8, x = 1.0, y = 2.0);
  std::println("  e8(1,2) = {:.6f}", result);
}

// ============================================================================
// Section 2: Differentiation chains
// ============================================================================
// Differentiating a deep expression triggers recursive strategy application.
// Each node in the expression tree gets pattern-matched and rewritten,
// producing a derivative expression that's typically 2-3x larger than the
// original (product rule duplicates sub-expressions).

auto section2() {
  std::println("--- Section 2: Differentiation ---");

  Symbol<TagX> x;
  Symbol<TagY> y;

  // A moderately complex expression
  auto f = sin(x * x) + cos(x * y) - exp(x + y);

  // First derivative w.r.t. x — triggers full strategy traversal
  auto df_dx = differentiate(f, x);

  // First derivative w.r.t. y — another full traversal
  auto df_dy = differentiate(f, y);

  // Second derivative — differentiating the already-large df_dx
  // This is the most expensive single operation: the derivative expression
  // is ~3x the size of f, so differentiating it again produces ~9x the
  // original type complexity.
  auto d2f_dxdx = differentiate(df_dx, x);

  double r1 = evaluate(df_dx, x = 1.0, y = 2.0);
  std::println("  ∂f/∂x(1,2) = {:.6f}", r1);

  double r2 = evaluate(df_dy, x = 1.0, y = 2.0);
  std::println("  ∂f/∂y(1,2) = {:.6f}", r2);

  double r3 = evaluate(d2f_dxdx, x = 1.0, y = 2.0);
  std::println("  ∂²f/∂x²(1,2) = {:.6f}", r3);
}

// ============================================================================
// Section 3: Multiple independent expressions
// ============================================================================
// Instantiating many different expression types forces the compiler to
// generate unique code for each. This tests the "breadth" of template
// instantiation, complementing the "depth" tested in sections 1-2.

auto section3() {
  std::println("--- Section 3: Breadth — many expression types ---");

  Symbol<TagA> a;
  Symbol<TagB> b;
  Symbol<TagC> c;

  auto expr1 = sin(a) + cos(b);
  auto expr2 = exp(a * b) - log(c + 1_c);
  auto expr3 = sqrt(a * a + b * b);
  auto expr4 = a / (b + c);
  auto expr5 = sin(cos(exp(a)));

  // Each differentiation produces a unique type
  auto d1 = differentiate(expr1, a);
  auto d2 = differentiate(expr2, b);
  auto d3 = differentiate(expr3, a);
  auto d4 = differentiate(expr4, c);
  auto d5 = differentiate(expr5, a);

  double sum = 0.0;
  sum += evaluate(d1, a = 1.0, b = 2.0, c = 3.0);
  sum += evaluate(d2, a = 1.0, b = 2.0, c = 3.0);
  sum += evaluate(d3, a = 1.0, b = 2.0, c = 3.0);
  sum += evaluate(d4, a = 1.0, b = 2.0, c = 3.0);
  sum += evaluate(d5, a = 1.0, b = 2.0, c = 3.0);

  std::println("  sum of derivatives at (1,2,3) = {:.6f}", sum);
}

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::println("Build tooling: compile profiling example");
  std::println("=========================================");
  std::println("");

  section1();
  std::println("");
  section2();
  std::println("");
  section3();

  std::println("");
  std::println("Done. If built with -DTEMPURA_PROFILE_COMPILE=ON,");
  std::println("check stderr for GCC phase timing.");
  return 0;
}
