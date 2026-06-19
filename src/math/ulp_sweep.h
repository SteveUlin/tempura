#pragma once

#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <source_location>
#include <span>

#include "comparison.h"
#include "unit.h"

namespace tempura {

// A reusable ULP-sweep oracle for elementary functions: evaluate a candidate and a trusted
// reference (usually the system libm) over a set of inputs and report the worst ULP error.
// Built on comparison.h::ulpDistance — NOT a new metric.
//
// The load-bearing design choice (and the one the naive version gets wrong): ulpDistance
// has NO nan/inf screen and returns garbage on them. So we CLASSIFY BOTH the candidate and
// the reference first. A class mismatch (e.g. the candidate returns inf where the reference
// is finite — exactly the reconstruction bug you want to catch) is a hard failure, not a
// large-but-finite ULP number; only when both are finite do we call ulpDistance.

enum class FpClass : std::uint8_t { kFinite, kNan, kPosInf, kNegInf };

inline auto classOf(double x) -> FpClass {
  if (std::isnan(x)) return FpClass::kNan;
  if (std::isinf(x)) return x > 0 ? FpClass::kPosInf : FpClass::kNegInf;
  return FpClass::kFinite;
}

struct UlpResult {
  std::uint64_t max_ulps = 0;
  double worst_input = 0;
  double got = 0;             // candidate value at the worst input
  double want = 0;            // reference value at the worst input
  bool class_mismatch = false;  // a nan/inf/finite class disagreement occurred
};

template <typename Cand, typename Ref>
auto sweepUlps(Cand&& cand, Ref&& ref, std::span<const double> xs) -> UlpResult {
  UlpResult r{};
  for (double x : xs) {
    const double got = cand(x);
    const double want = ref(x);
    const bool mismatch = classOf(got) != classOf(want);
    std::uint64_t d = 0;
    if (mismatch) {
      d = std::numeric_limits<std::uint64_t>::max();
    } else if (classOf(want) == FpClass::kFinite) {
      d = ulpDistance(got, want);
    }  // else: same non-finite class (both nan, or both same-signed inf) → 0
    if (d > r.max_ulps) {  // strict: keep the FIRST input reaching the worst error
      r.max_ulps = d;
      r.worst_input = x;
      r.got = got;
      r.want = want;
      r.class_mismatch = mismatch;
    }
  }
  return r;
}

// Assert the worst ULP error over the sweep is within max_ulps, reporting the worst input.
template <typename Cand, typename Ref>
auto expectMaxUlps(Cand&& cand, Ref&& ref, std::span<const double> xs, std::uint64_t max_ulps,
                   std::source_location loc = std::source_location::current()) -> bool {
  const auto r = sweepUlps(cand, ref, xs);
  if (r.class_mismatch || r.max_ulps > max_ulps) {
    reportFailure(loc, std::format("ULP sweep: {} ulps at x={:g} (got {:g}, want {:g}){}",
                                   r.max_ulps, r.worst_input, r.got, r.want,
                                   r.class_mismatch ? " [class mismatch]" : ""));
    return false;
  }
  return true;
}

}  // namespace tempura
