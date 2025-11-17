// WhenAllSender - waits for all senders to complete and aggregates results

#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "concepts.h"
#include "env.h"
#include "stop_token.h"
#include "type_utils.h"

namespace tempura {

// ============================================================================
// WhenAll - Parallel Composition
// ============================================================================
//
// whenAll waits for all input senders to complete and aggregates their results.
//
// Semantics:
// - Returns tuple-of-tuples: tuple<Sender1::ValueTypes, Sender2::ValueTypes, ...>
// - Each sender's ValueTypes is already a tuple, so result is nested
// - Thread-safe: Can be used with concurrent schedulers (e.g. ThreadPool)
// - Error handling: First error or stop signal wins, cancels entire operation
// - Completion: Only the last child to complete triggers the receiver
//
// Example:
//   auto result = syncWait(whenAll(just(42), just("hello")));
//   // result = tuple<tuple<int>, tuple<const char*>>
//   auto [t1, t2] = *result;
//   int value = std::get<0>(t1);         // 42
//   const char* str = std::get<0>(t2);   // "hello"

// State shared across all child operation states
template <typename R, typename... Senders>
class WhenAllSharedState {
 public:
  using ValueTuple = std::tuple<typename Senders::ValueTypes...>;

  WhenAllSharedState(R receiver, std::size_t count)
      : receiver_(std::move(receiver)),
        remaining_count_(count),
        completed_(false) {}

  // Called when a child sender completes successfully
  template <std::size_t Index, typename... Args>
  void setValue(Args&&... args) noexcept {
    // Store the value at the correct index (wrap in tuple)
    std::get<Index>(values_).emplace(std::forward<Args>(args)...);

    // Decrement counter and check if we're the last to complete
    if (remaining_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      // We're last to complete. Try to claim completion (success path).
      // If an error/stop occurred, they already claimed it, so we do nothing.
      bool expected = false;
      if (completed_.compare_exchange_strong(expected, true,
                                              std::memory_order_acq_rel)) {
        // Success: all senders completed without error
        // Note: all optionals in values_ are populated since we're last
        unpackAndForward(std::index_sequence_for<Senders...>{});
      }
      // else: error or stop already called receiver, we're done
    }
  }

  // Called when a child sender completes with error
  template <typename Error>
  void setError(Error&& error) noexcept {
    // Try to claim completion immediately (first error wins)
    // This happens BEFORE decrementing to prevent race with setValue
    bool expected = false;
    if (completed_.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel)) {
      // Request stop for all other senders
      stop_source_.request_stop();

      // Forward error as variant (P2300 approach: simple variant of unique types)
      using ErrorVariant = typename MergeUniqueErrorTypes<Senders...>::type;
      receiver_.setError(ErrorVariant{std::forward<Error>(error)});
    }
    // else: another error/stop already claimed completion

    // Decrement counter (remaining children will see completed_ = true)
    remaining_count_.fetch_sub(1, std::memory_order_acq_rel);
  }

  // Called when a child sender is stopped
  void setStopped() noexcept {
    // Try to claim completion immediately (first stop wins)
    // This happens BEFORE decrementing to prevent race with setValue
    bool expected = false;
    if (completed_.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel)) {
      // Request stop for all other senders
      stop_source_.request_stop();

      receiver_.setStopped();
    }
    // else: another error/stop already claimed completion

    // Decrement counter (remaining children will see completed_ = true)
    remaining_count_.fetch_sub(1, std::memory_order_acq_rel);
  }

  // Get stop token for child operations
  [[nodiscard]] auto get_stop_token() noexcept {
    return stop_source_.get_token();
  }

 private:
  template <std::size_t... Is>
  void unpackAndForward(std::index_sequence<Is...>) noexcept {
    receiver_.setValue(std::move(*std::get<Is>(values_))...);
  }

  R receiver_;
  std::tuple<std::optional<typename Senders::ValueTypes>...> values_;
  std::atomic<std::size_t> remaining_count_;
  std::atomic<bool> completed_;      // true if receiver has been called
  InplaceStopSource stop_source_;    // Stop source for active cancellation
};

// Receiver for each individual sender in the whenAll
template <std::size_t Index, typename SharedState>
class WhenAllReceiver {
 public:
  WhenAllReceiver(SharedState* state) : state_(state) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    state_->template setValue<Index>(std::forward<Args>(args)...);
  }

  template <typename Error>
  void setError(Error&& error) noexcept {
    state_->setError(std::forward<Error>(error));
  }

  void setStopped() noexcept { state_->setStopped(); }

  // Provide environment with stop token
  [[nodiscard]] auto get_env() const noexcept {
    return EnvWithStopToken{state_->get_stop_token()};
  }

 private:
  SharedState* state_;
};

// ============================================================================
// OperationTuple - Recursive storage for non-movable operation states
// ============================================================================
//
// Uses recursive template inheritance instead of std::tuple to avoid moving
// operation states. Each level of inheritance stores one operation state,
// constructed in-place via member initializer list.
//
// Example hierarchy for 3 senders:
//   OperationTuple<0, State, S0, S1, S2>
//     : OperationTuple<1, State, S1, S2>  // base class
//       : OperationTuple<2, State, S2>    // base class
//         : OperationTuple<3, State>      // empty base case
//
// Each level constructs its operation state (op_) in-place, never moved.

// Forward declaration
template <std::size_t Index, typename SharedState, typename... Senders>
class OperationTuple;

// Base case: no more senders
template <std::size_t Index, typename SharedState>
class OperationTuple<Index, SharedState> {
 protected:
  OperationTuple(SharedState*) {}
  void start() noexcept {}  // No-op for base case
};

// Recursive case: one sender + rest
template <std::size_t Index, typename SharedState, typename First, typename... Rest>
class OperationTuple<Index, SharedState, First, Rest...>
    : public OperationTuple<Index + 1, SharedState, Rest...> {
 protected:
  using Receiver = WhenAllReceiver<Index, SharedState>;
  using OpState = decltype(std::declval<First>().connect(std::declval<Receiver>()));
  using Base = OperationTuple<Index + 1, SharedState, Rest...>;

 public:
  // Construct this level's operation state in-place, then recurse
  template <typename S, typename... Ss>
  OperationTuple(SharedState* state, S&& first, Ss&&... rest)
      : Base(state, std::forward<Ss>(rest)...)  // Recurse to base classes
      , op_(std::forward<S>(first).connect(Receiver{state})) {}  // In-place construction

  // Start this operation, then recurse
  void start() noexcept {
    op_.start();
    Base::start();  // Recurse to base classes
  }

 private:
  OpState op_;  // Constructed in-place, never moved
};

// Operation state for WhenAllSender
template <typename R, typename... Senders>
class WhenAllOperationState {
 public:
  using SharedState = WhenAllSharedState<R, Senders...>;

  template <typename... Ss>
  WhenAllOperationState(R receiver, Ss&&... senders)
      : state_(std::move(receiver), sizeof...(Senders))
      , inner_ops_(&state_, std::forward<Ss>(senders)...) {}

  // Operation States are not copyable nor movable
  WhenAllOperationState(const WhenAllOperationState&) = delete;
  auto operator=(const WhenAllOperationState&) -> WhenAllOperationState& = delete;
  WhenAllOperationState(WhenAllOperationState&&) = delete;
  auto operator=(WhenAllOperationState&&) -> WhenAllOperationState& = delete;

  void start() noexcept {
    inner_ops_.start();  // Recursively starts all operations
  }

 private:
  SharedState state_;
  OperationTuple<0, SharedState, Senders...> inner_ops_;
};

template <typename... Senders>
  requires(Sender<Senders> && ...)
class WhenAllSender {
 public:
  // The result is a tuple of all the value tuples
  using ValueTypes = std::tuple<typename Senders::ValueTypes...>;

  // Error is a variant of all unique error types from all senders (P2300 approach)
  using ErrorTypes = std::tuple<typename MergeUniqueErrorTypes<Senders...>::type>;

  template <typename... Ss>
  explicit WhenAllSender(Ss&&... senders)
      : senders_(std::forward<Ss>(senders)...) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return connectImpl(std::forward<R>(receiver),
                       std::index_sequence_for<Senders...>{});
  }

 private:
  template <typename R, std::size_t... Is>
  auto connectImpl(R&& receiver, std::index_sequence<Is...>) {
    return WhenAllOperationState<std::remove_cvref_t<R>, Senders...>{
        std::forward<R>(receiver), std::move(std::get<Is>(senders_))...};
  }

  std::tuple<Senders...> senders_;
};

template <typename... Senders>
WhenAllSender(Senders&&...)
    -> WhenAllSender<std::remove_cvref_t<Senders>...>;

// Helper function to create a WhenAllSender
template <typename... Senders>
  requires(Sender<Senders> && ...)
auto whenAll(Senders&&... senders) {
  return WhenAllSender{std::forward<Senders>(senders)...};
}

}  // namespace tempura
