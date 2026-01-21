// Tests for compressed.h - zero-overhead tuple storage
#include "symbolic4/compressed.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "Pair has minimal overhead for empty types"_test = [] {
    struct Empty {};
    Pair<int, Empty> p{42, {}};
    static_assert(sizeof(p) == sizeof(int));
    expectEq(p.first, 42);
  };

  "Pair with two non-empty types"_test = [] {
    Pair<int, double> p{42, 3.14};
    expectEq(p.first, 42);
    expectNear(p.second, 3.14);
  };

  "CompressedTuple stores values correctly"_test = [] {
    CompressedTuple<int, double, char> t{1, 2.5, 'a'};
    expectEq(get<0>(t), 1);
    expectNear(get<1>(t), 2.5);
    expectEq(get<2>(t), 'a');
  };

  "CompressedTuple has correct size"_test = [] {
    static_assert(CompressedTuple<int, double>::size == 2);
    static_assert(CompressedTuple<int>::size == 1);
    static_assert(CompressedTuple<>::size == 0);
  };

  "empty CompressedTuple works"_test = [] {
    CompressedTuple<> empty;
    static_assert(sizeof(empty) == 1);  // Empty struct has size 1
    (void)empty;
  };

  "CompressedTuple with empty types compresses"_test = [] {
    struct Empty1 {};
    struct Empty2 {};
    CompressedTuple<int, Empty1, Empty2> t{42, {}, {}};
    // Should only be size of int due to [[no_unique_address]]
    static_assert(sizeof(t) == sizeof(int));
    expectEq(get<0>(t), 42);
  };

  return TestRegistry::result();
}
