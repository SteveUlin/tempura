// Completion signatures for sender/receiver model (P2300)
//
// Completion signatures describe all possible ways a sender can complete.
// Each signature is represented as a function type using tag types:
//   SetValueTag(Args...) - value completion with Args
//   SetErrorTag(Error)   - error completion with Error
//   SetStoppedTag()      - stopped completion (always empty)

#pragma once

#include <concepts>
#include <type_traits>

#include "meta/tags.h"
#include "meta/type_list_ops.h"

namespace tempura {

// Forward declaration - full definition in env.h
struct EmptyEnv;

// ============================================================================
// Completion Channel Tag Types
// ============================================================================

// Tag type for value completions - represents setValue(Args...)
struct SetValueTag {
  template <typename... Args>
  using Signature = SetValueTag(Args...);
};

// Tag type for error completions - represents setError(Error)
struct SetErrorTag {
  template <typename Error>
  using Signature = SetErrorTag(Error);
};

// Tag type for stopped completions - represents setStopped()
struct SetStoppedTag {
  using Signature = SetStoppedTag();
};

// ============================================================================
// CompletionSignatures - Container for all completion signatures
// ============================================================================

// Primary template - holds a list of completion signatures
// Each signature is a function type: Tag(Args...)
template <typename... Sigs>
struct CompletionSignatures {
  // Empty by design - this is a type-level container
  // Internally stored as TypeList for easier manipulation
  using AsList = TypeList<Sigs...>;
};

// ============================================================================
// Signature Analysis - Extract information from signatures
// ============================================================================

// Check if a type is a completion signature
template <typename T>
struct IsCompletionSignature : std::false_type {};

template <typename... Args>
struct IsCompletionSignature<SetValueTag(Args...)> : std::true_type {};

template <typename E>
struct IsCompletionSignature<SetErrorTag(E)> : std::true_type {};

template <>
struct IsCompletionSignature<SetStoppedTag()> : std::true_type {};

template <typename T>
inline constexpr bool kIsCompletionSignature = IsCompletionSignature<T>::value;

// Extract the tag from a signature
template <typename Sig>
struct SignatureTag;

template <typename... Args>
struct SignatureTag<SetValueTag(Args...)> {
  using Type = SetValueTag;
};

template <typename E>
struct SignatureTag<SetErrorTag(E)> {
  using Type = SetErrorTag;
};

template <>
struct SignatureTag<SetStoppedTag()> {
  using Type = SetStoppedTag;
};

template <typename Sig>
using SignatureTagT = typename SignatureTag<Sig>::Type;

// Extract arguments from a signature as a TypeList
template <typename Sig>
struct SignatureArgs;

template <typename... Args>
struct SignatureArgs<SetValueTag(Args...)> {
  using Type = TypeList<Args...>;
};

template <typename E>
struct SignatureArgs<SetErrorTag(E)> {
  using Type = TypeList<E>;
};

template <>
struct SignatureArgs<SetStoppedTag()> {
  using Type = TypeList<>;
};

template <typename Sig>
using SignatureArgsT = typename SignatureArgs<Sig>::Type;

// ============================================================================
// Filtering Signatures by Tag
// ============================================================================

// Predicate: does signature have a specific tag?
template <typename Tag>
struct HasTagPredicate {
  template <typename Sig>
  struct Apply : std::is_same<Tag, SignatureTagT<Sig>> {};
};

// Filter signatures by tag (returns TypeList)
template <typename Tag, typename SigList>
struct FilterByTag {
  template <typename Sig>
  using Pred = typename HasTagPredicate<Tag>::template Apply<Sig>;

  using Type = Filter_t<Pred, SigList>;
};

template <typename Tag, typename SigList>
using FilterByTagT = typename FilterByTag<Tag, SigList>::Type;

// Extract value signatures from CompletionSignatures (returns TypeList)
template <typename CompletionSigs>
struct ValueSignatures;

template <typename... Sigs>
struct ValueSignatures<CompletionSignatures<Sigs...>> {
  using Type = FilterByTagT<SetValueTag, TypeList<Sigs...>>;
};

template <typename CompletionSigs>
using ValueSignaturesT = typename ValueSignatures<CompletionSigs>::Type;

// Extract error signatures from CompletionSignatures (returns TypeList)
template <typename CompletionSigs>
struct ErrorSignatures;

template <typename... Sigs>
struct ErrorSignatures<CompletionSignatures<Sigs...>> {
  using Type = FilterByTagT<SetErrorTag, TypeList<Sigs...>>;
};

template <typename CompletionSigs>
using ErrorSignaturesT = typename ErrorSignatures<CompletionSigs>::Type;

// Check if stopped signature is present
template <typename CompletionSigs>
struct HasStoppedSignature;

template <typename... Sigs>
struct HasStoppedSignature<CompletionSignatures<Sigs...>>
    : std::disjunction<std::is_same<Sigs, SetStoppedTag()>...> {};

template <typename CompletionSigs>
inline constexpr bool kHasStoppedSignature =
    HasStoppedSignature<CompletionSigs>::value;

// ============================================================================
// Merging Completion Signatures
// ============================================================================

// Convert TypeList back to CompletionSignatures
template <typename SigList>
struct ListToCompletionSignatures;

template <typename... Sigs>
struct ListToCompletionSignatures<TypeList<Sigs...>> {
  using Type = CompletionSignatures<Sigs...>;
};

template <typename SigList>
using ListToCompletionSignaturesT =
    typename ListToCompletionSignatures<SigList>::Type;

// Merge multiple CompletionSignatures into one
template <typename... CompletionSigs>
struct MergeCompletionSignatures;

template <>
struct MergeCompletionSignatures<> {
  using Type = CompletionSignatures<>;
};

template <typename... Sigs>
struct MergeCompletionSignatures<CompletionSignatures<Sigs...>> {
  using Type = CompletionSignatures<Sigs...>;
};

template <typename... Sigs1, typename... Sigs2, typename... Rest>
struct MergeCompletionSignatures<CompletionSignatures<Sigs1...>,
                                 CompletionSignatures<Sigs2...>, Rest...> {
  using Type =
      typename MergeCompletionSignatures<CompletionSignatures<Sigs1...,
                                                              Sigs2...>,
                                         Rest...>::Type;
};

template <typename... CompletionSigs>
using MergeCompletionSignaturesT =
    typename MergeCompletionSignatures<CompletionSigs...>::Type;

// ============================================================================
// Transform Completion Signatures
// ============================================================================

// Apply a metafunction to all signatures
// MetaFn<Sig>::Type should return a CompletionSignatures
template <template <typename> class MetaFn, typename... Sigs>
struct TransformSignatures;

template <template <typename> class MetaFn>
struct TransformSignatures<MetaFn> {
  using Type = CompletionSignatures<>;
};

template <template <typename> class MetaFn, typename Sig, typename... Rest>
struct TransformSignatures<MetaFn, Sig, Rest...> {
  using Transformed = typename MetaFn<Sig>::Type;
  using RestSigs = typename TransformSignatures<MetaFn, Rest...>::Type;
  using Type = MergeCompletionSignaturesT<Transformed, RestSigs>;
};

template <template <typename> class MetaFn, typename... Sigs>
using TransformSignaturesT =
    typename TransformSignatures<MetaFn, Sigs...>::Type;

}  // namespace tempura
