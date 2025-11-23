// Type utilities for sender/receiver operations

#pragma once

#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "completion_signatures.h"

namespace tempura {

// ============================================================================
// Type List Utilities
// ============================================================================

// Helper to remove duplicates from a type list (keeps first occurrence)
template <typename...>
struct UniqueTypes;

template <>
struct UniqueTypes<> {
  using type = std::tuple<>;
};

template <typename T, typename... Rest>
struct UniqueTypes<T, Rest...> {
 private:
  // Remove all occurrences of T from Rest
  template <typename U, typename... Ts>
  struct RemoveAll;

  template <typename U>
  struct RemoveAll<U> {
    using type = std::tuple<>;
  };

  template <typename U, typename First, typename... Ts>
  struct RemoveAll<U, First, Ts...> {
    using rest = typename RemoveAll<U, Ts...>::type;
    using type = std::conditional_t<std::is_same_v<U, First>,
                                     rest,
                                     decltype(std::tuple_cat(std::declval<std::tuple<First>>(),
                                                              std::declval<rest>()))>;
  };

  // Apply UniqueTypes to a tuple's contents
  template <typename Tuple>
  struct ApplyUnique;

  template <typename... Ts>
  struct ApplyUnique<std::tuple<Ts...>> {
    using type = typename UniqueTypes<Ts...>::type;
  };

  // Remove T from Rest, then get unique types from the result
  using rest_without_t = typename RemoveAll<T, Rest...>::type;
  using rest_unique = typename ApplyUnique<rest_without_t>::type;

 public:
  // Prepend T to the unique rest
  using type = decltype(std::tuple_cat(std::declval<std::tuple<T>>(),
                                        std::declval<rest_unique>()));
};

// Convert tuple to variant
template <typename Tuple>
struct TupleToVariant;

template <typename... Ts>
struct TupleToVariant<std::tuple<Ts...>> {
  using type = std::variant<Ts...>;
};

// Merge and deduplicate error types from all senders
template <typename... Senders>
struct MergeUniqueErrorTypes {
 private:
  // Extract error signatures from each sender's CompletionSignatures
  // Then collect all error types from those signatures
  template <typename S>
  struct ExtractErrorArgs {
    using CompletionSigs = GetCompletionSignaturesT<S, EmptyEnv>;
    using ErrorSigs = ErrorSignaturesT<CompletionSigs>;

    // Convert TypeList of SetErrorTag(T) to tuple of T
    template <typename List>
    struct SigsToArgs;

    template <typename... Sigs>
    struct SigsToArgs<TypeList<Sigs...>> {
      template <typename Sig>
      struct GetArg;

      template <typename T>
      struct GetArg<SetErrorTag(T)> {
        using Type = T;
      };

      using Type = std::tuple<typename GetArg<Sigs>::Type...>;
    };

    using Type = typename SigsToArgs<ErrorSigs>::Type;
  };

  // Concatenate all error type tuples
  using concatenated = decltype(std::tuple_cat(std::declval<typename ExtractErrorArgs<Senders>::Type>()...));

  // Extract types from concatenated tuple and deduplicate
  template <typename Tuple>
  struct UniqueFromTuple;

  template <typename... Es>
  struct UniqueFromTuple<std::tuple<Es...>> {
    using type = typename UniqueTypes<Es...>::type;
  };

 public:
  using type = typename TupleToVariant<
      typename UniqueFromTuple<concatenated>::type
  >::type;
};

}  // namespace tempura
