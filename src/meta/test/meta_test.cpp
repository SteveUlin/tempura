#include "meta/meta.h"

#include "unit.h"

using namespace tempura;

template <typename T, typename U>
constexpr bool is_same = false;

template <typename T>
constexpr bool is_same<T, T> = true;

auto main() -> int {
  "TypeId"_test = [] {
    // Check if Meta is unique for different types
    static_assert(kMeta<int> != kMeta<double>);
    static_assert(kMeta<int> != kMeta<void>);
    static_assert(kMeta<double> != kMeta<void>);
  };

  "IsSame"_test = [] {
    // Check if IsSame works
    static_assert(is_same<int, int>);
    static_assert(!is_same<int, double>);
    static_assert(!is_same<int, void>);
    static_assert(!is_same<double, void>);
  };

  "TypeOf"_test = [] {
    // Check if TypeOf works
    static_assert(is_same<TypeOf<kMeta<int>>, int>);
    static_assert(is_same<TypeOf<kMeta<double>>, double>);
    static_assert(is_same<TypeOf<kMeta<void>>, void>);
  };

  static_assert(is_same<IntegerSequence<SizeT, 0, 1, 2>, MakeIndexSequence<3>>);


  return 0;
}
