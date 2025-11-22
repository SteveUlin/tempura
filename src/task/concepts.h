// Core concepts for the sender/receiver model

#pragma once

#include <concepts>
#include <system_error>
#include <type_traits>

#include "completion_signatures.h"

namespace tempura {

// ============================================================================
// Core Concepts
// ============================================================================

// A type that can can take in some task output and do something with it.
//
// All three completion channels (setValue, setError, setStopped) must be
// noexcept since operation states cannot provide exception safety in tempura's
// no-exceptions environment.
//
// Note: ReceiverOf checks only the value types. Error types are checked
// separately via the setError method which accepts variadic arguments.
template <typename R, typename... Args>
concept ReceiverOf =
    std::move_constructible<R> &&
    requires(R&& r, Args&&... args) {
      {
        std::forward<R>(r).setValue(std::forward<Args>(args)...)
      } noexcept -> std::same_as<void>;
      { std::forward<R>(r).setStopped() } noexcept -> std::same_as<void>;
    };

// Schedulers create senders that represent the scheduling of work.
//
// Note: Equality comparison is not required in this simplified Phase 1
// implementation. P2300 requires it for scheduler optimization, but we defer
// that complexity to Phase 4.
template <typename S>
concept Scheduler =
    std::copy_constructible<std::remove_cvref_t<S>> && requires(const S& s) {
      { s.schedule() };  // Returns a sender
    };

// OperationState represents a stateful asynchronous operation that can be
// started.
template <typename O>
concept OperationState = requires(O& o) { o.start(); };

// Sender lazily describes a unit of asynchronous work.
//
// P2300-compliant senders must provide CompletionSignatures describing all
// possible completion channels. For backward compatibility, we also accept
// senders with ValueTypes/ErrorTypes aliases (legacy interface).
template <typename S, typename Env = EmptyEnv>
concept Sender = std::move_constructible<S> &&
                 (requires {
                   typename std::remove_cvref_t<S>::template CompletionSignatures<
                       Env>;
                 } ||
                  requires {
                    typename std::remove_cvref_t<S>::ValueTypes;
                    typename std::remove_cvref_t<S>::ErrorTypes;
                  });

// SenderTo checks that a sender can be connected to a specific receiver.
//
// Note: We don't check ReceiverOf here because we'd need to know the sender's
// value types. The connect() requirement provides sufficient validation - if
// the receiver can't accept the sender's values, connect() will fail to
// compile.
template <typename S, typename R>
concept SenderTo = Sender<S> && std::move_constructible<R> &&
                   requires(S&& s, R&& r) {
                     {
                       std::forward<S>(s).connect(std::forward<R>(r))
                     } -> OperationState;
                   };

// ============================================================================
// Helpers for Extracting Completion Signatures
// ============================================================================

// Get completion signatures from a sender (handles both old and new styles)
template <typename S, typename Env = EmptyEnv>
struct GetCompletionSignatures {
  // Try new-style CompletionSignatures first
  using Type = typename std::remove_cvref_t<S>::template CompletionSignatures<
      Env>;
};

// Fallback for legacy senders with ValueTypes/ErrorTypes
template <typename S>
  requires requires {
    typename std::remove_cvref_t<S>::ValueTypes;
    typename std::remove_cvref_t<S>::ErrorTypes;
  } && (!requires { typename std::remove_cvref_t<S>::template CompletionSignatures<EmptyEnv>; })
struct GetCompletionSignatures<S, EmptyEnv> {
  using ValueTypes = typename std::remove_cvref_t<S>::ValueTypes;
  using ErrorTypes = typename std::remove_cvref_t<S>::ErrorTypes;

  // Convert ValueTypes (tuple<Args...>) to SetValueTag(Args...)
  template <typename Tuple>
  struct TupleToValueSig;

  template <typename... Args>
  struct TupleToValueSig<std::tuple<Args...>> {
    using Type = SetValueTag(Args...);
  };

  // Convert ErrorTypes (tuple<Errors...>) to multiple SetErrorTag sigs
  template <typename Tuple>
  struct TupleToErrorSigs;

  template <typename... Errors>
  struct TupleToErrorSigs<std::tuple<Errors...>> {
    using Type = CompletionSignatures<SetErrorTag(Errors)...>;
  };

  using ValueSig = typename TupleToValueSig<ValueTypes>::Type;
  using ErrorSigs = typename TupleToErrorSigs<ErrorTypes>::Type;

  // Merge value signature, error signatures, and stopped
  using Type = MergeCompletionSignaturesT<
      CompletionSignatures<ValueSig, SetStoppedTag()>,
      ErrorSigs>;
};

template <typename S, typename Env = EmptyEnv>
using GetCompletionSignaturesT =
    typename GetCompletionSignatures<S, Env>::Type;

}  // namespace tempura
