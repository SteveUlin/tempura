// Tests for static_assert_demo.h — verifies the "good" paths compile and work.
// The "ERROR" paths are tested by uncommenting them manually and inspecting
// the compiler output.

#include "examples/build_tooling/static_assert_demo.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::build_tooling;

auto main() -> int {
  "pattern1: dependent_false unit labels"_test = [] {
    expectEq(pattern1::unitLabel<pattern1::Meters>()[0], 'm');
    expectEq(pattern1::unitLabel<pattern1::Seconds>()[0], 's');
    expectEq(pattern1::unitLabel<pattern1::Kilograms>()[0], 'k');
  };

  "pattern2: concept-gated compute"_test = [] {
    expectEq(compute(pattern2::Literal{3.0}), 6.0);
    expectEq(compute(pattern2::Add{1.0, 2.0}), 6.0);
    expectEq(compute(pattern2::Literal{0.0}), 0.0);
    expectEq(compute(pattern2::Add{-1.0, 1.0}), 0.0);
  };

  "pattern3: symbol table lookup"_test = [] {
    constexpr pattern3::Table t{};
    expectEq(t.get<pattern3::Alpha>(), 42.0);
    expectEq(t.get<pattern3::Beta>(), 42.0);
  };

  "pattern4: fixed array bounds check"_test = [] {
    constexpr pattern4::FixedArray<3> a{.data_ = {1.0, 2.0, 3.0}};
    expectEq(a.get<0>(), 1.0);
    expectEq(a.get<1>(), 2.0);
    expectEq(a.get<2>(), 3.0);
  };

  "pattern4: static message builder"_test = [] {
    constexpr auto msg = pattern4::concat(
        pattern4::StaticMessage{"hello "}, pattern4::StaticMessage{"world"});
    expectEq(msg.size(), std::size_t{11});
    expectEq(msg.data()[0], 'h');
    expectEq(msg.data()[6], 'w');
  };

  return TestRegistry::result();
}
