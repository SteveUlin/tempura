// Static-assert tests for completion_signatures.h metaprogramming primitives.

#include "task/task.h"
#include "unit.h"

using namespace tempura;

// Signature extraction

using S1 = CompletionSignatures<SetValueTag(int), SetValueTag(double, float),
                                SetErrorTag(std::string), SetStoppedTag()>;

static_assert(std::same_as<ValueSignaturesT<S1>,
              TypeList<SetValueTag(int), SetValueTag(double, float)>>);

static_assert(std::same_as<ErrorSignaturesT<S1>,
              TypeList<SetErrorTag(std::string)>>);

static_assert(Size_v<ValueSignaturesT<S1>> == 2);
static_assert(Size_v<ErrorSignaturesT<S1>> == 1);
static_assert(kHasStoppedSignature<S1>);

// Signatures without stopped

using S2 = CompletionSignatures<SetValueTag(int)>;
static_assert(!kHasStoppedSignature<S2>);
static_assert(Size_v<ErrorSignaturesT<S2>> == 0);

// SignatureArgsT

static_assert(std::same_as<SignatureArgsT<SetValueTag(int, bool)>,
              TypeList<int, bool>>);

static_assert(std::same_as<SignatureArgsT<SetErrorTag(float)>,
              TypeList<float>>);

static_assert(std::same_as<SignatureArgsT<SetStoppedTag()>, TypeList<>>);

// MergeCompletionSignaturesT

using M = MergeCompletionSignaturesT<
    CompletionSignatures<SetValueTag(int)>,
    CompletionSignatures<SetErrorTag(float), SetStoppedTag()>>;

static_assert(std::same_as<M,
    CompletionSignatures<SetValueTag(int), SetErrorTag(float), SetStoppedTag()>>);

// ListToTupleT

static_assert(std::same_as<ListToTupleT<TypeList<int, bool>>,
              std::tuple<int, bool>>);

static_assert(std::same_as<ListToTupleT<TypeList<>>, std::tuple<>>);

auto main() -> int {
  return TestRegistry::result();
}
