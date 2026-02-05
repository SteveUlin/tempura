// bench_info_diff.cc — Compile-time benchmark for INFO-DOMAIN differentiation
//
// Compare build times against the type-domain diff() path via -ftime-trace.
// Both paths are included so the parse cost is equal.

#include <experimental/meta>

#include "symbolic4/constants.h"
#include "symbolic4/core.h"
#include "symbolic4/indexed/reduce_over.h"
#include "symbolic4/operators.h"
#include "symbolic4/strategy/diff.h"
#include "symbolic4/strategy/info_diff.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Tags at namespace scope
struct BDX {};
struct BDY {};
struct BDZ {};
struct BDW {};
struct BDDim {};

Symbol<BDX> bx;
Symbol<BDY> by;
Symbol<BDZ> bz;
Symbol<BDW> bw;

// Force the compiler to fully evaluate info-domain differentiation.
consteval auto infoDiffForce(std::meta::info expr, std::meta::info var) -> bool {
  auto result = diffInfo(expr, var);
  (void)result;
  return true;
}

// ============================================================================
// Level 0: Terminal differentiation (6 expressions)
// ============================================================================

static_assert(infoDiffForce(^^decltype(bx), ^^Symbol<BDX>));          // d/dx[x] = 1
static_assert(infoDiffForce(^^decltype(by), ^^Symbol<BDX>));          // d/dx[y] = 0
static_assert(infoDiffForce(^^Constant<42.0>, ^^Symbol<BDX>));        // d/dx[c] = 0
static_assert(infoDiffForce(^^Fraction<1, 2>, ^^Symbol<BDX>));        // d/dx[½] = 0
static_assert(infoDiffForce(^^decltype(pi), ^^Symbol<BDX>));          // d/dx[π] = 0
static_assert(infoDiffForce(^^decltype(e), ^^Symbol<BDX>));           // d/dx[e] = 0

// ============================================================================
// Level 1: Simple derivatives (6 expressions)
// ============================================================================

static_assert(infoDiffForce(^^decltype(bx + by), ^^Symbol<BDX>));     // sum
static_assert(infoDiffForce(^^decltype(bx * by), ^^Symbol<BDX>));     // product
static_assert(infoDiffForce(^^decltype(bx / by), ^^Symbol<BDX>));     // quotient
static_assert(infoDiffForce(^^decltype(exp(bx)), ^^Symbol<BDX>));     // exp
static_assert(infoDiffForce(^^decltype(log(bx)), ^^Symbol<BDX>));     // log
static_assert(infoDiffForce(^^decltype(sin(bx)), ^^Symbol<BDX>));     // sin

// ============================================================================
// Level 2: Chain rule (6 expressions)
// ============================================================================

static_assert(infoDiffForce(^^decltype(exp(2_c * bx)), ^^Symbol<BDX>));
static_assert(infoDiffForce(^^decltype(sin(bx * bx)), ^^Symbol<BDX>));
static_assert(infoDiffForce(^^decltype(log(bx + 1_c)), ^^Symbol<BDX>));
static_assert(infoDiffForce(^^decltype(sqrt(bx * by)), ^^Symbol<BDX>));
static_assert(infoDiffForce(^^decltype(cos(exp(bx))), ^^Symbol<BDX>));
static_assert(infoDiffForce(^^decltype(lgamma(bx)), ^^Symbol<BDX>));

// ============================================================================
// Level 3: Gradient-like expressions
// ============================================================================

// Product rule chains — simpler, fit within step budget
static_assert(infoDiffForce(^^decltype(bx * by * bz), ^^Symbol<BDX>));
static_assert(infoDiffForce(^^decltype(exp(bx) * sin(by) * log(bz)), ^^Symbol<BDX>));

// Bernoulli log-prob
static_assert(infoDiffForce(
    ^^decltype(bx * log(by) + (1_c - bx) * log(1_c - by)),
    ^^Symbol<BDY>));

// Normal log-prob
static_assert(infoDiffForce(
    ^^decltype(-pow(bx - by, 2_c) / (2_c * pow(bz, 2_c)) - log(bz)),
    ^^Symbol<BDX>));

auto main() -> int { return 0; }
