#include "meta/meta.h"
#include "unit.h"
#include "meta/symbolic.h"

using namespace tempura;

auto main() -> int {
  "IntegerSequence"_test = [] {
    static_assert(kMeta<IntegerSequence<SizeT, 0, 1, 2>> ==
                  kMeta<MakeIndexSequence<3>>);
  };

  "DeclVal"_test = [] {
    static_assert(kMeta<decltype(kDeclVal<int>() + 3)> == kMeta<int>);
  };

  "Range Concept"_test = [] {
    MinimalArray arr{1, 2, 3};
    static_assert(Range<decltype(arr)>);
  };

  "Invocable Concept"_test = [] {
    auto lambda = [](int) {};
    static_assert(Invocable<decltype(lambda), int>);
  };

  Symbol a;
  static_assert(kMeta<decltype(a + a)> == kMeta<decltype(a + a)>,
                "Expression type mismatch");
  static_assert(kMeta<decltype(a + a)> ==
                    kMeta<Expression<AddOp, getTypeId(a), getTypeId(a)>>,
                "Expression type mismatch");
  return 0;
}
