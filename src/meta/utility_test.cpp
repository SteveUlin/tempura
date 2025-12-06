#include "meta/utility.h"

#include "unit.h"

using namespace tempura;

// Test types for concept validation
struct NonFinalEmpty {};

struct FinalEmpty final {};

struct FinalWithData final {
  int x;
};

struct FinalNonTrivial final {
  FinalNonTrivial() {}  // non-trivial default constructor
};

auto main() -> int {
  // --- Type Traits Tests ---

  "isConst"_test = [] {
    static_assert(!isConst<int>);
    static_assert(isConst<const int>);
    static_assert(!isConst<int*>);
    static_assert(!isConst<const int*>);  // pointer to const, not const pointer
    static_assert(isConst<int* const>);   // const pointer
    static_assert(isConst<const int* const>);
  };

  "isReference"_test = [] {
    static_assert(!isReference<int>);
    static_assert(isReference<int&>);
    static_assert(isReference<int&&>);
    static_assert(isReference<const int&>);
    static_assert(!isReference<int*>);
  };

  "isEmpty"_test = [] {
    static_assert(isEmpty<FinalEmpty>);
    static_assert(isEmpty<NonFinalEmpty>);
    static_assert(!isEmpty<FinalWithData>);
    static_assert(!isEmpty<int>);
  };

  "isFinal"_test = [] {
    static_assert(isFinal<FinalEmpty>);
    static_assert(isFinal<FinalWithData>);
    static_assert(!isFinal<NonFinalEmpty>);
  };

  "isTriviallyDefaultConstructible"_test = [] {
    static_assert(isTriviallyDefaultConstructible<int>);
    static_assert(isTriviallyDefaultConstructible<FinalEmpty>);
    static_assert(!isTriviallyDefaultConstructible<FinalNonTrivial>);
  };

  "isTriviallyCopyConstructible"_test = [] {
    static_assert(isTriviallyCopyConstructible<int>);
    static_assert(isTriviallyCopyConstructible<FinalEmpty>);
    static_assert(isTriviallyCopyConstructible<FinalWithData>);
  };

  "isTriviallyMoveConstructible"_test = [] {
    static_assert(isTriviallyMoveConstructible<int>);
    static_assert(isTriviallyMoveConstructible<FinalEmpty>);
    static_assert(isTriviallyMoveConstructible<FinalWithData>);
  };

  "isTriviallyDestructible"_test = [] {
    static_assert(isTriviallyDestructible<int>);
    static_assert(isTriviallyDestructible<FinalEmpty>);
    static_assert(isTriviallyDestructible<FinalWithData>);
  };

  // --- CanonicalType Concept Tests ---

  "CanonicalType accepts value types"_test = [] {
    static_assert(CanonicalType<int>);
    static_assert(CanonicalType<FinalEmpty>);
    static_assert(CanonicalType<NonFinalEmpty>);
  };

  "CanonicalType rejects const types"_test = [] {
    static_assert(!CanonicalType<const int>);
    static_assert(!CanonicalType<const FinalEmpty>);
  };

  "CanonicalType rejects reference types"_test = [] {
    static_assert(!CanonicalType<int&>);
    static_assert(!CanonicalType<int&&>);
    static_assert(!CanonicalType<const int&>);
  };

  // --- TagType Concept Tests ---

  "TagType accepts proper tag types"_test = [] {
    static_assert(TagType<FinalEmpty>);
  };

  "TagType rejects non-final types"_test = [] {
    static_assert(!TagType<NonFinalEmpty>);
  };

  "TagType rejects types with data"_test = [] {
    static_assert(!TagType<FinalWithData>);
  };

  "TagType rejects non-trivial types"_test = [] {
    static_assert(!TagType<FinalNonTrivial>);
  };

  "TagType rejects const types"_test = [] {
    static_assert(!TagType<const FinalEmpty>);
  };

  "TagType rejects reference types"_test = [] {
    static_assert(!TagType<FinalEmpty&>);
    static_assert(!TagType<FinalEmpty&&>);
  };

  return TestRegistry::result();
}
