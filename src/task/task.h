// Async Task Execution System - Sender/Receiver Model
//
// This file provides a compile-time task expression system based on the P2300
// sender/receiver model. Senders are types (not runtime objects) that describe
// asynchronous work without executing it.

#pragma once

#include <concepts>
#include <optional>
#include <print>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tempura {

// ============================================================================
// Core Concepts
// ============================================================================

// A type that can can take in some task output and do something with it.
//
// All three completion channels (set_value, set_error, set_stopped) must be
// noexcept since operation states cannot provide exception safety in tempura's
// no-exceptions environment.
template <typename R, typename... Args>
concept ReceiverOf =
    std::move_constructible<R> &&
    requires(R&& r, Args&&... args, std::error_code ec) {
      {
        std::forward<R>(r).set_value(std::forward<Args>(args)...)
      } noexcept -> std::same_as<void>;
      { std::forward<R>(r).set_error(ec) } noexcept -> std::same_as<void>;
      { std::forward<R>(r).set_stopped() } noexcept -> std::same_as<void>;
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

// Sender lazily describe a unit of asynchronous work.
template <typename S, typename R>
concept SenderTo =
    std::move_constructible<S> && ReceiverOf<R> && requires(S&& s, R&& r) {
      { std::forward<S>(s).connect(std::forward<R>(r)) } -> OperationState;
    };

// ============================================================================
// Completion Channels
// ============================================================================

// Print an argument when a value is received.
//
// This is useful for debugging purposes.
template <typename T>
class PrintReceiver {
 public:
  void set_value(const T& value) noexcept {
    std::println("Received value: {}", value);
  }

  void set_error(std::error_code ec) noexcept {
    std::println("Error occurred: {}", ec.message());
  }

  void set_stopped() noexcept { std::println("Operation was stopped."); }
};
static_assert(ReceiverOf<PrintReceiver<int>, int>);

// A simple receiver that stores values in an optional tuple.
//
// If a stop or error is received, the optional is reset.
//
// Design note: This receiver takes a reference to external storage (the
// optional) and stores it as a pointer. This pattern is necessary because
// receivers must be move-constructible (they're moved into operation states).
// If we stored a reference member and then moved the receiver, the reference
// would dangle. By using a pointer, moves are shallow copies that preserve
// the connection to the external storage. Use this pattern for receivers that
// need to write to caller-owned storage.
template <typename... Args>
class ValueReceiver {
 public:
  explicit ValueReceiver(std::optional<std::tuple<Args...>>& opt)
      : opt_(&opt) {}

  ValueReceiver(const ValueReceiver&) = delete;
  auto operator=(const ValueReceiver&) -> ValueReceiver& = delete;

  ValueReceiver(ValueReceiver&&) = default;
  auto operator=(ValueReceiver&&) -> ValueReceiver& = default;

  void set_value(Args&&... args) noexcept {
    opt_->emplace(std::forward<Args>(args)...);
  }

  void set_error(std::error_code ec) noexcept { opt_->reset(); }

  void set_stopped() noexcept { opt_->reset(); }

 private:
  // Pointer (not reference) allows safe moves while preserving external storage
  std::optional<std::tuple<Args...>>* opt_;
};
static_assert(ReceiverOf<ValueReceiver<int>, int>);

// ============================================================================
// Fundamental Sender Types
// ============================================================================

// TODO: Implement Schedule sender

// Operation state for JustSender - unpacks tuple and forwards to receiver
template <typename R, typename... Args>
class JustOperationState {
 public:
  JustOperationState(std::tuple<Args...> values, R r) noexcept
      : values_(std::move(values)), receiver_(std::move(r)) {}

  // Operation States are not copyable nor movable
  JustOperationState(const JustOperationState&) = delete;
  auto operator=(const JustOperationState&) -> JustOperationState& = delete;
  JustOperationState(JustOperationState&&) = delete;
  auto operator=(JustOperationState&&) -> JustOperationState& = delete;

  void start() noexcept {
    // Unpack tuple and call variadic set_value
    std::apply(
        [this](auto&&... args) {
          std::forward<R>(receiver_).set_value(
              std::forward<decltype(args)>(args)...);
        },
        std::move(values_));
  }

 private:
  std::tuple<Args...> values_;
  R receiver_;
};

template <typename... Args>
  requires(std::move_constructible<Args> && ...)
class JustSender {
 public:
  using value_types = std::tuple<Args...>;

  JustSender(Args&&... args) : values_{std::forward<Args>(args)...} {}

  template <ReceiverOf<Args...> R>
  auto connect(R&& receiver) && {
    return JustOperationState<R, Args...>{std::move(values_),
                                          std::forward<R>(receiver)};
  }

 private:
  std::tuple<Args...> values_;
};

template <typename... Args>
JustSender(Args&&...) -> JustSender<std::decay_t<Args>...>;

// TODO: Implement Transfer sender

// ============================================================================
// Composition Senders
// ============================================================================

// TODO: Implement Then sender

// TODO: Implement UponError sender

// TODO: Implement UponStopped sender

// TODO: Implement Let sender

// ============================================================================
// Parallel Composition
// ============================================================================

// TODO: Implement WhenAll sender

// TODO: Implement WhenAny sender

// ============================================================================
// Scheduler Implementations
// ============================================================================

// TODO: Implement InlineScheduler

// TODO: Implement ThreadPoolScheduler

// TODO: Implement SingleThreadScheduler

// ============================================================================
// Stop Token Support
// ============================================================================

// TODO: Implement StopToken

// TODO: Implement StopSource

// ============================================================================
// Algorithms
// ============================================================================

// Synchronously wait for a sender to complete and return its value.
//
// This function blocks until the sender completes, then returns an optional
// containing the values produced by the sender. If the sender calls set_error()
// or set_stopped(), the optional will be empty.
//
// Template constraints:
// - S must be a sender type with a `value_types` member type alias
// - The sender must be connectable to a ValueReceiver
//
// Returns: std::optional<std::tuple<Args...>> where Args... are the value types
//
// Example:
//   auto result = syncWait(JustSender{42, "hello"});
//   if (result) {
//     auto [num, str] = *result;  // num=42, str="hello"
//   }
template <typename S>
auto syncWait(S&& sender) {
  // Extract the value types from the sender (e.g., std::tuple<int,
  // std::string>)
  using ValueTuple = typename std::remove_cvref_t<S>::value_types;

  // Create storage for the result
  std::optional<ValueTuple> result;

  // Create a receiver that will store values into our optional
  // Use type_identity to unpack tuple type into parameter pack
  auto receiver = []<typename... Args>(
                      std::type_identity<std::tuple<Args...>>,
                      std::optional<std::tuple<Args...>>& opt) {
    return ValueReceiver<Args...>{opt};
  }(std::type_identity<ValueTuple>{}, result);

  // Connect the sender to the receiver to create an operation state
  auto op = std::forward<S>(sender).connect(std::move(receiver));

  // Start the operation (this is synchronous for JustSender)
  op.start();

  // Return the result (will be empty if error or stopped)
  return result;
}

// TODO: Implement start_detached

// TODO: Implement connect customization point

// TODO: Implement start customization point
}  // namespace tempura
