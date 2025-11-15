// Core concepts for the sender/receiver model

#pragma once

#include <concepts>
#include <system_error>
#include <type_traits>

namespace tempura {

// ============================================================================
// Core Concepts
// ============================================================================

// A type that can can take in some task output and do something with it.
//
// All three completion channels (setValue, setError, setStopped) must be
// noexcept since operation states cannot provide exception safety in tempura's
// no-exceptions environment.
template <typename R, typename... Args>
concept ReceiverOf =
    std::move_constructible<R> &&
    requires(R&& r, Args&&... args, std::error_code ec) {
      {
        std::forward<R>(r).setValue(std::forward<Args>(args)...)
      } noexcept -> std::same_as<void>;
      { std::forward<R>(r).setError(ec) } noexcept -> std::same_as<void>;
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
// All senders must expose a `ValueTypes` alias (typically std::tuple<Args...>)
// to enable compile-time type deduction of their output values.
template <typename S>
concept Sender = std::move_constructible<S> && requires {
  typename std::remove_cvref_t<S>::ValueTypes;
};

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

}  // namespace tempura
