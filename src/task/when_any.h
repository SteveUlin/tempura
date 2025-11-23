// WhenAnySender - waits for first sender to complete and returns its result

#pragma once

#include <atomic>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "completion_signatures.h"
#include "concepts.h"
#include "env.h"
#include "stop_token.h"
#include "type_utils.h"

namespace tempura {

// ============================================================================
// Value Type Merging
// ============================================================================

// Merge all value types into a variant (deduplicated)
// Each sender's value tuple is extracted from completion signatures
template <typename... Senders>
struct MergeValueTypes {
 private:
  // Collect all value tuples
  using all_values = std::tuple<ExtractValueTupleT<Senders>...>;

  // Deduplicate value types
  template <typename Tuple>
  struct UniqueFromTuple;

  template <typename... Ts>
  struct UniqueFromTuple<std::tuple<Ts...>> {
    using type = typename UniqueTypes<Ts...>::type;
  };

 public:
  using type = typename TupleToVariant<
      typename UniqueFromTuple<all_values>::type
  >::type;
};

// ============================================================================
// WhenAny - Race Composition
// ============================================================================
//
// whenAny waits for the first input sender to complete and returns its result.
//
// Semantics:
// - Returns variant of value tuples: variant<Sender1::ValueTypes, Sender2::ValueTypes, ...>
// - Each sender's ValueTypes is a tuple, variant selects which sender completed first
// - Thread-safe: Can be used with concurrent schedulers (e.g. ThreadPool)
// - Error handling: First error or stop signal wins
// - Completion: First child to complete triggers the receiver, rest ignored
//
// Example:
//   auto result = syncWait(whenAny(just(42), just("hello")));
//   // result = optional<tuple<variant<tuple<int>, tuple<const char*>>>>
//   if (result) {
//     auto [var] = *result;
//     if (std::holds_alternative<std::tuple<int>>(var)) {
//       int value = std::get<0>(std::get<std::tuple<int>>(var));  // 42
//     }
//   }

// State shared across all child operation states
template <typename R, typename... Senders>
class WhenAnySharedState {
 public:
  WhenAnySharedState(R receiver)
      : receiver_(std::move(receiver)),
        completed_(false) {}

  // Called when a child sender completes successfully
  template <std::size_t Index, typename... Args>
  void setValue(Args&&... args) noexcept {
    // Try to claim completion (first value wins)
    bool expected = false;
    if (completed_.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel)) {
      // We're first! Request stop for all other senders
      stop_source_.request_stop();

      // Forward the value as a variant
      using ValueVariant = typename MergeValueTypes<Senders...>::type;
      using ThisSender = std::tuple_element_t<Index, std::tuple<Senders...>>;
      using ThisValueTuple = ExtractValueTupleT<ThisSender>;

      receiver_.setValue(ValueVariant{std::in_place_type<ThisValueTuple>,
                                      std::forward<Args>(args)...});
    }
    // else: another sender already completed, ignore this result
  }

  // Called when a child sender completes with error
  template <typename Error>
  void setError(Error&& error) noexcept {
    // Try to claim completion (first error wins)
    bool expected = false;
    if (completed_.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel)) {
      // Request stop for all other senders
      stop_source_.request_stop();

      // Forward error as variant
      using ErrorVariant = typename MergeUniqueErrorTypes<Senders...>::type;
      receiver_.setError(ErrorVariant{std::forward<Error>(error)});
    }
    // else: another sender already completed, ignore this error
  }

  // Called when a child sender is stopped
  void setStopped() noexcept {
    // Try to claim completion (first stop wins)
    bool expected = false;
    if (completed_.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel)) {
      // Request stop for all other senders
      stop_source_.request_stop();

      receiver_.setStopped();
    }
    // else: another sender already completed, ignore this stop
  }

  // Get stop token for child operations
  [[nodiscard]] auto get_stop_token() noexcept {
    return stop_source_.get_token();
  }

 private:
  R receiver_;
  std::atomic<bool> completed_;      // true if receiver has been called
  InplaceStopSource stop_source_;    // Stop source for active cancellation
};

// Receiver for each individual sender in the whenAny
template <std::size_t Index, typename SharedState>
class WhenAnyReceiver {
 public:
  WhenAnyReceiver(SharedState* state) : state_(state) {}

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
    return withStopToken(state_->get_stop_token());
  }

 private:
  SharedState* state_;
};

// ============================================================================
// OperationTuple - Recursive storage for non-movable operation states
// ============================================================================

// Forward declaration
template <std::size_t Index, typename SharedState, typename... Senders>
class WhenAnyOperationTuple;

// Base case: no more senders
template <std::size_t Index, typename SharedState>
class WhenAnyOperationTuple<Index, SharedState> {
 protected:
  WhenAnyOperationTuple(SharedState*) {}
  void start() noexcept {}  // No-op for base case
};

// Recursive case: one sender + rest
template <std::size_t Index, typename SharedState, typename First, typename... Rest>
class WhenAnyOperationTuple<Index, SharedState, First, Rest...>
    : public WhenAnyOperationTuple<Index + 1, SharedState, Rest...> {
 protected:
  using Receiver = WhenAnyReceiver<Index, SharedState>;
  using OpState = decltype(std::declval<First>().connect(std::declval<Receiver>()));
  using Base = WhenAnyOperationTuple<Index + 1, SharedState, Rest...>;

 public:
  // Construct this level's operation state in-place, then recurse
  template <typename S, typename... Ss>
  WhenAnyOperationTuple(SharedState* state, S&& first, Ss&&... rest)
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

// Operation state for WhenAnySender
template <typename R, typename... Senders>
class WhenAnyOperationState {
 public:
  using SharedState = WhenAnySharedState<R, Senders...>;

  template <typename... Ss>
  WhenAnyOperationState(R receiver, Ss&&... senders)
      : state_(std::move(receiver))
      , inner_ops_(&state_, std::forward<Ss>(senders)...) {}

  // Operation States are not copyable nor movable
  WhenAnyOperationState(const WhenAnyOperationState&) = delete;
  auto operator=(const WhenAnyOperationState&) -> WhenAnyOperationState& = delete;
  WhenAnyOperationState(WhenAnyOperationState&&) = delete;
  auto operator=(WhenAnyOperationState&&) -> WhenAnyOperationState& = delete;

  void start() noexcept {
    inner_ops_.start();  // Recursively starts all operations
  }

 private:
  SharedState state_;
  WhenAnyOperationTuple<0, SharedState, Senders...> inner_ops_;
};

template <typename... Senders>
  requires(Sender<Senders> && ...)
class WhenAnySender {
 public:
  // Completion signatures: SetValueTag(variant<tuple<Args1...>, tuple<Args2...>, ...>)
  // Error signatures: collect all error sigs from all senders
  // Stopped: SetStoppedTag()
  template <typename Env = EmptyEnv>
  struct CompletionSignaturesHelper {
    // Value signature: SetValueTag(variant<ExtractValueTupleT<Sender1>, ...>)
    using ValueVariant = typename MergeValueTypes<Senders...>::type;
    using ValueSig = SetValueTag(ValueVariant);

    // Error signatures: collect all error sigs from all senders
    using AllErrorSigs = Concat_t<
        ErrorSignaturesT<GetCompletionSignaturesT<Senders, Env>>...>;

    // Merge into final completion signatures
    using Type = MergeCompletionSignaturesT<
        tempura::CompletionSignatures<ValueSig, SetStoppedTag()>,
        ListToCompletionSignaturesT<AllErrorSigs>>;
  };

  template <typename Env = EmptyEnv>
  using CompletionSignatures = typename CompletionSignaturesHelper<Env>::Type;

  template <typename... Ss>
  explicit WhenAnySender(Ss&&... senders)
      : senders_(std::forward<Ss>(senders)...) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return WhenAnyOperationState<std::remove_cvref_t<R>, Senders...>{
          std::forward<R>(receiver), std::move(std::get<Is>(senders_))...};
    }(std::index_sequence_for<Senders...>{});
  }

 private:

  std::tuple<Senders...> senders_;
};

template <typename... Senders>
WhenAnySender(Senders&&...)
    -> WhenAnySender<std::remove_cvref_t<Senders>...>;

// Helper function to create a WhenAnySender
template <typename... Senders>
  requires(Sender<Senders> && ...)
auto whenAny(Senders&&... senders) {
  return WhenAnySender{std::forward<Senders>(senders)...};
}

}  // namespace tempura
