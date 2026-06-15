#include "comparison.h"

#include <array>
#include <limits>

using namespace tempura;

namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
}  // namespace

// ─── isClose: relative tolerance scales with magnitude ──────────────────────
static_assert(isClose(1.0, 1.0 + 1e-10, {.rtol = 1e-9}));
static_assert(!isClose(1.0, 1.1, {.rtol = 1e-9}));
// A large magnitude widens the relative bound proportionally.
static_assert(isClose(1e6, 1e6 + 1.0, {.rtol = 1e-5}));
// Near zero a pure relative bound collapses — atol is the required floor.
static_assert(!isClose(0.0, 1e-12, {.rtol = 1e-9}));
static_assert(isClose(0.0, 1e-12, {.atol = 1e-9}));
// Same-sign infinities read as close; opposite signs and NaN never do.
static_assert(isClose(kInf, kInf, {.rtol = 1e-9}));
static_assert(!isClose(kInf, -kInf, {.rtol = 1e-9}));
static_assert(!isClose(kNan, kNan, {.atol = 1e9}));

// ─── ulpDistance: 0 for equal, 1 for adjacent representables ─────────────────
static_assert(ulpDistance(1.0, 1.0) == 0);
static_assert(ulpDistance(1.0, 1.0 + 0x1p-52) == 1);  // 2^-52 is the ULP at 1.0
static_assert(ulpDistance(1.0, 1.0 + 0x1p-51) == 2);
static_assert(ulpDistance(0.0, -0.0) == 0);  // signed zeros are exactly equal

// ─── IEEE classification ────────────────────────────────────────────────────
static_assert(isNan(kNan) && !isNan(1.0));
static_assert(isInf(kInf) && isInf(-kInf) && !isInf(1.0));
static_assert(isFinite(1.0) && !isFinite(kInf) && !isFinite(kNan));

// ─── range closeness, with size mismatch as a failure ───────────────────────
static_assert(rangesClose(std::array{1.0, 2.0, 3.0},
                          std::array{1.0, 2.0 + 1e-10, 3.0}, {.rtol = 1e-9}));
static_assert(!rangesClose(std::array{1.0, 2.0, 3.0},
                           std::array{1.0, 2.5, 3.0}, {.rtol = 1e-9}));
static_assert(!rangesClose(std::array{1.0, 2.0, 3.0}, std::array{1.0, 2.0},
                           {.rtol = 1e-9}));

// ─── existing predicates still hold (isClose now owns relative tolerance) ────
static_assert(isNear(1.0, 1.0001, 1e-3));
static_assert(rangesEqual(std::array{1, 2, 3}, std::array{1, 2, 3}));

auto main() -> int { return 0; }
