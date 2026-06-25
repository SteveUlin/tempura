// Static-assert tests for type_utils.h metaprogramming primitives.

#include "task/task.h"
#include "unit.h"

#include <variant>

using namespace tempura;

// UniqueTypes: deduplication

static_assert(std::same_as<typename UniqueTypes<int, double, int, float, double>::type,
              std::tuple<int, double, float>>);

static_assert(std::same_as<typename UniqueTypes<>::type, std::tuple<>>);

static_assert(std::same_as<typename UniqueTypes<int>::type, std::tuple<int>>);

static_assert(std::same_as<typename UniqueTypes<int, int, int>::type,
              std::tuple<int>>);

// TupleToVariant: non-empty tuple

static_assert(std::same_as<typename TupleToVariant<std::tuple<int, double>>::type,
              std::variant<int, double>>);

// TupleToVariant: empty tuple → variant<monostate>

static_assert(std::same_as<typename TupleToVariant<std::tuple<>>::type,
              std::variant<std::monostate>>);

// MergeUniqueErrorTypes: across senders with overlapping error types

using E1 = CompletionSignatures<SetValueTag(int), SetErrorTag(float)>;
using E2 = CompletionSignatures<SetValueTag(int), SetErrorTag(double), SetErrorTag(float)>;

struct S1 { template <typename Env = EmptyEnv> using CompletionSignatures = E1; };
struct S2 { template <typename Env = EmptyEnv> using CompletionSignatures = E2; };

// float appears in both, double only in S2 — merged variant has float and double
using Merged = typename MergeUniqueErrorTypes<S1, S2>::type;
static_assert(std::same_as<Merged, std::variant<float, double>>);

// Single sender, no errors → variant<monostate>
struct S3 {
  template <typename Env = EmptyEnv>
  using CompletionSignatures =
      tempura::CompletionSignatures<SetValueTag(int), SetStoppedTag()>;
};
static_assert(std::same_as<typename MergeUniqueErrorTypes<S3>::type,
              std::variant<std::monostate>>);

auto main() -> int {
  return TestRegistry::result();
}
