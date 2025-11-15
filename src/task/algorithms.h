// Algorithms for working with senders

#pragma once

#include "receivers.h"

#include <latch>
#include <optional>
#include <type_traits>
#include <utility>

namespace tempura {

// ============================================================================
// Algorithms
// ============================================================================

// Synchronously wait for a sender to complete and return its value.
//
// This function blocks until the sender completes, then returns an optional
// containing the values produced by the sender. If the sender calls setError()
// or setStopped(), the optional will be empty.
//
// Returns: std::optional<Sender::ValueTypes>
//
// Example:
//   auto result = syncWait(just(42, "hello"));
//   if (result) {
//     auto [num, str] = *result;  // num=42, str="hello"
//   }
template <typename S>
auto syncWait(S&& sender) {
  using ValueTuple = typename std::remove_cvref_t<S>::ValueTypes;

  // Create storage for the result
  std::optional<ValueTuple> result;

  // One-shot synchronization for blocking until completion
  std::latch latch{1};

  // Create a blocking receiver
  auto receiver = []<typename... Args>(
                      std::type_identity<std::tuple<Args...>>,
                      std::optional<std::tuple<Args...>>& opt,
                      std::latch& latch) {
    return BlockingReceiver<Args...>{opt, latch};
  }(std::type_identity<ValueTuple>{}, result, latch);

  // Connect the sender to the receiver to create an operation state.
  // The receiver is moved into the operation state (P2300 pattern).
  auto op = std::forward<S>(sender).connect(std::move(receiver));

  // Start the operation
  op.start();

  // Wait for completion
  latch.wait();

  // Return the result (will be empty if error or stopped)
  return result;
}

// TODO: Implement startDetached

// TODO: Implement connect customization point

// TODO: Implement start customization point

}  // namespace tempura
