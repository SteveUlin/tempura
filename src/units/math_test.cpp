#include "units/math.h"

#include "units/literals.h"
#include "units/units.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::literals;

// ============================================================================
// Basic functions
// ============================================================================

auto main() -> int {
  "abs"_test = [] {
    auto pos = 5.0_m;
    auto neg = -5.0_m;

    expectEq(abs(pos).count(), 5.0);
    expectEq(abs(neg).count(), 5.0);
  };

  "floor ceil round trunc"_test = [] {
    auto val = 3.7_m;

    expectEq(floor(val).count(), 3.0);
    expectEq(ceil(val).count(), 4.0);
    expectEq(round(val).count(), 4.0);
    expectEq(trunc(val).count(), 3.0);

    auto neg = -3.7_m;
    expectEq(floor(neg).count(), -4.0);
    expectEq(ceil(neg).count(), -3.0);
    expectEq(round(neg).count(), -4.0);
    expectEq(trunc(neg).count(), -3.0);
  };

  "fmod"_test = [] {
    auto a = 10.0_m;
    auto b = 3.0_m;
    auto result = fmod(a, b);
    expectNear(result.count(), 1.0, 1e-10);
  };

  "sqrt changes dimensions"_test = [] {
    // sqrt of area gives length
    auto area = 100.0_m * 100.0_m;  // 10000 m²
    auto side = sqrt(area);

    // Value should be 100
    expectNear(side.count(), 100.0, 1e-10);

    // Dimension should be Length^(1/2) * Length^(1/2) = Length
    // (area is L², sqrt gives L)
  };

  "cbrt"_test = [] {
    auto vol = 8.0_m * 8.0_m * 8.0_m;  // 512 m³
    auto side = cbrt(vol);
    expectNear(side.count(), 8.0, 1e-10);
  };

  "pow<N>"_test = [] {
    auto length = 3.0_m;
    auto squared = pow<2>(length);
    expectNear(squared.count(), 9.0, 1e-10);

    auto cubed = pow<3>(length);
    expectNear(cubed.count(), 27.0, 1e-10);
  };

  "trigonometric functions"_test = [] {
    // sin(0) = 0
    auto zero = 0.0 * Radian{};
    expectNear(sin(zero), 0.0, 1e-10);

    // cos(0) = 1
    expectNear(cos(zero), 1.0, 1e-10);

    // sin(π/2) = 1
    auto pi_2 = 90.0 * Degree{};
    expectNear(sin(pi_2), 1.0, 1e-10);

    // cos(π/2) = 0
    expectNear(cos(pi_2), 0.0, 1e-10);

    // tan(π/4) = 1
    auto pi_4 = 45.0 * Degree{};
    expectNear(tan(pi_4), 1.0, 1e-10);
  };

  "inverse trig functions"_test = [] {
    // arcsin(0) = 0
    auto angle = arcsin(0.0);
    expectNear(angle.count(), 0.0, 1e-10);

    // arccos(1) = 0
    angle = arccos(1.0);
    expectNear(angle.count(), 0.0, 1e-10);

    // arctan(1) = π/4
    angle = arctan(1.0);
    expectNear(angle.count(), 3.14159265358979323846 / 4.0, 1e-10);
  };

  "atan2"_test = [] {
    auto y = 1.0_m;
    auto x = 1.0_m;
    auto angle = atan2(y, x);
    expectNear(angle.count(), 3.14159265358979323846 / 4.0, 1e-10);

    // atan2(1, 0) = π/2
    auto zero = 0.0_m;
    angle = atan2(y, zero);
    expectNear(angle.count(), 3.14159265358979323846 / 2.0, 1e-10);
  };

  "hypot"_test = [] {
    // 3-4-5 triangle
    auto a = 3.0_m;
    auto b = 4.0_m;
    auto c = hypot(a, b);
    expectNear(c.count(), 5.0, 1e-10);

    // 3D hypot
    auto x = 2.0_m;
    auto y = 3.0_m;
    auto z = 6.0_m;
    auto r = hypot(x, y, z);
    expectNear(r.count(), 7.0, 1e-10);
  };

  "min max clamp"_test = [] {
    auto a = 3.0_m;
    auto b = 5.0_m;

    expectEq(min(a, b).count(), 3.0);
    expectEq(max(a, b).count(), 5.0);

    auto lo = 2.0_m;
    auto hi = 4.0_m;
    expectEq(clamp(a, lo, hi).count(), 3.0);  // within range
    expectEq(clamp(b, lo, hi).count(), 4.0);  // clamped to hi
    expectEq(clamp(1.0_m, lo, hi).count(), 2.0);  // clamped to lo
  };

  "sign"_test = [] {
    expectEq(sign(5.0_m), 1);
    expectEq(sign(-5.0_m), -1);
    expectEq(sign(0.0_m), 0);
  };

  "lerp"_test = [] {
    auto a = 0.0_m;
    auto b = 10.0_m;

    expectEq(lerp(a, b, 0.0).count(), 0.0);
    expectEq(lerp(a, b, 1.0).count(), 10.0);
    expectEq(lerp(a, b, 0.5).count(), 5.0);
    expectEq(lerp(a, b, 0.25).count(), 2.5);
  };

  return TestRegistry::result();
}
