// Algorithms for working with senders

#pragma once

#include "receivers.h"

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

// TODO: Implement startDetached

// TODO: Implement connect customization point

// TODO: Implement start customization point

}  // namespace tempura
