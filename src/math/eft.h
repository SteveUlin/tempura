#pragma once

#include <cmath>

namespace tempura {

// Error-free transforms and double-word arithmetic.
//
// The rounding error of a single + or × on doubles is ITSELF a representable double
// (Dekker/Knuth). Capturing it lets you carry ~2× the working precision (≈106 bits) as an
// unevaluated hi+lo pair. Reach for it when the *accumulated rounding* of plain double
// dominates the answer: catastrophic cancellation, long summations, or evaluating a
// polynomial near a multiple root (condition number → 1/u). It is not free — a few× the
// flops per op — so it stays an opt-in escape-hatch, chosen where the ULP oracle shows
// raw double bleeding digits.
//
// Conversions are EXPLICIT, always. The library never silently promotes a double to a
// DoubleWord or collapses one back: twoSum/twoProd take you in, toDouble takes you out.
// No implicit operator double, no converting constructor — crossing the boundary is a
// visible call.

// hi + lo with |lo| ≤ ½ulp(hi). An aggregate on purpose: no implicit conversion in either
// direction (constructing one still needs the type name; extracting a double needs toDouble).
struct [[nodiscard]] DoubleWord {
  double hi{};
  double lo{};
};

// Collapse to the nearest double — the only way out.
constexpr auto toDouble(DoubleWord x) -> double { return x.hi + x.lo; }

// a + b = hi + lo, EXACTLY (Knuth, branch-free; no ordering precondition).
constexpr auto twoSum(double a, double b) -> DoubleWord {
  const double s = a + b;
  const double bv = s - a;
  return {s, (a - (s - bv)) + (b - bv)};
}

// a + b = hi + lo, EXACTLY, given |a| ≥ |b| (one add cheaper than twoSum).
constexpr auto fastTwoSum(double a, double b) -> DoubleWord {
  const double s = a + b;
  return {s, b - (s - a)};
}

// a · b = hi + lo, EXACTLY — one FMA recovers the product's rounding error.
constexpr auto twoProduct(double a, double b) -> DoubleWord {
  const double p = a * b;
  return {p, std::fma(a, b, -p)};
}

// Mixed-precision steps (Muller, "Handbook of Floating-Point Arithmetic"): the double stays
// a double — it is consumed by the algorithm, never converted to a DoubleWord first.
constexpr auto operator+(DoubleWord x, double y) -> DoubleWord {
  const DoubleWord s = twoSum(x.hi, y);
  return fastTwoSum(s.hi, x.lo + s.lo);
}
constexpr auto operator*(DoubleWord x, double y) -> DoubleWord {
  const DoubleWord p = twoProduct(x.hi, y);
  return fastTwoSum(p.hi, std::fma(x.lo, y, p.lo));
}
constexpr auto operator+(double x, DoubleWord y) -> DoubleWord { return y + x; }
constexpr auto operator*(double x, DoubleWord y) -> DoubleWord { return y * x; }

// Fused multiply-add for a double-word accumulator: a·b + c, keeping ~2× precision.
constexpr auto fma(DoubleWord a, DoubleWord b, double c) -> DoubleWord {
  const DoubleWord p = twoProduct(a.hi, b.hi);
  const double e = std::fma(a.hi, b.lo, std::fma(a.lo, b.hi, p.lo));  // cross terms + product tail
  return fastTwoSum(p.hi, e) + c;  // (a·b as a double-word) + c
}

// Full double-word addition (Joldes–Muller–Popescu Alg. 6): the reason DoubleWord is a
// number and not just a pair — it composes with itself.
constexpr auto operator+(DoubleWord x, DoubleWord y) -> DoubleWord {
  const DoubleWord s = twoSum(x.hi, y.hi);
  const DoubleWord t = twoSum(x.lo, y.lo);
  const DoubleWord v = fastTwoSum(s.hi, s.lo + t.hi);
  return fastTwoSum(v.hi, t.lo + v.lo);
}

}  // namespace tempura
