// bench_info_simplify.cc — Compile-time benchmark for info-domain simplification

#include <experimental/meta>

#include "symbolic4/constants.h"
#include "symbolic4/core.h"
#include "symbolic4/operators.h"
#include "symbolic4/strategy/info_simplify.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Tags at namespace scope — prefixed to avoid collision with operators.h names
struct BTagA {};
struct BTagB {};
struct BTagC {};
struct BTagD {};

Symbol<BTagA> sa;
Symbol<BTagB> sb;
Symbol<BTagC> sc;
Symbol<BTagD> sd;

// Force the compiler to fully evaluate info-domain simplification.
// Returns a trivially-true value so we can use it in static_assert.
consteval auto infoForce(std::meta::info expr) -> bool {
  auto result = infoSimplify(expr);
  (void)result;
  return true;
}

// ============================================================================
// Level 0: Single rule applications (6 expressions)
// ============================================================================

static_assert(infoForce(^^decltype(sa + 0_c)));                // x + 0 → x
static_assert(infoForce(^^decltype(0_c + sa)));                // 0 + x → x
static_assert(infoForce(^^decltype(sa * 1_c)));                // x * 1 → x
static_assert(infoForce(^^decltype(1_c * sa)));                // 1 * x → x
static_assert(infoForce(^^decltype(sa * 0_c)));                // x * 0 → 0
static_assert(infoForce(^^decltype(sa - 0_c)));                // x - 0 → x

// ============================================================================
// Level 1: Two-rule chains (6 expressions)
// ============================================================================

static_assert(infoForce(^^decltype((sa + 0_c) * 1_c)));       // (x+0)*1 → x
static_assert(infoForce(^^decltype((0_c + sa) / 1_c)));       // (0+x)/1 → x
static_assert(infoForce(^^decltype((sa * 1_c) + 0_c)));       // (x*1)+0 → x
static_assert(infoForce(^^decltype((sa - 0_c) * 1_c)));       // (x-0)*1 → x
static_assert(infoForce(^^decltype(0_c * (sa + sb))));         // 0*(x+y) → 0
static_assert(infoForce(^^decltype((sa - sa) + sb)));          // (x-x)+y → y

// ============================================================================
// Level 2: Three-rule chains (6 expressions)
// ============================================================================

static_assert(infoForce(^^decltype(((sa + 0_c) * 1_c) - 0_c)));
static_assert(infoForce(^^decltype(((sa * 0_c) + sb) * 1_c)));
static_assert(infoForce(^^decltype(((sa + 0_c) + (sb * 0_c)))));
static_assert(infoForce(^^decltype(((sa * 1_c) + 0_c) / 1_c)));
static_assert(infoForce(^^decltype(-(-sa) + 0_c)));
static_assert(infoForce(^^decltype(exp(log(sa)) * 1_c)));

// ============================================================================
// Level 3: Gradient-like expressions (6 expressions)
// ============================================================================

static_assert(infoForce(^^decltype(sa * 1_c + 1_c * sa)));
static_assert(infoForce(^^decltype(sa * 0_c + 1_c * sb)));
static_assert(infoForce(^^decltype((sa * 1_c + 1_c * sa) + (sb * 0_c + 0_c * sb))));
static_assert(infoForce(^^decltype((1_c + 0_c) * sc + (sa + sb) * 0_c)));
static_assert(infoForce(^^decltype((1_c * sb - sa * 0_c) / (sb * sb))));
static_assert(infoForce(^^decltype(exp(sa) * 1_c)));

// ============================================================================
// Level 4: Deep nesting / multi-branch (6 expressions)
// ============================================================================

static_assert(infoForce(^^decltype(((sa + 0_c) * 1_c + 0_c) / exp(log(sb)))));
static_assert(infoForce(^^decltype((sa * 1_c + sb * 0_c) * (sc * 1_c + sd * 0_c))));
static_assert(infoForce(^^decltype(exp(log(sa)) + exp(log(sb)))));
static_assert(infoForce(^^decltype(-(-exp(log(sa))) * 1_c + 0_c)));
static_assert(infoForce(^^decltype(((sa - sa) + sb) * ((sc + 0_c) / 1_c))));
static_assert(infoForce(^^decltype(((sa + 0_c) * (sb + 0_c) + (sc * 0_c + sd * 0_c)) / 1_c)));

auto main() -> int { return 0; }
