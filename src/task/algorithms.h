// Algorithms for working with senders

#pragma once

#include <latch>
#include <optional>
#include <type_traits>
#include <utility>

#include "receivers.h"

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

  std::optional<ValueTuple> result;

  std::latch latch{1};

  auto receiver = []<typename... Args>(std::type_identity<std::tuple<Args...>>,
                                       std::optional<std::tuple<Args...>>& opt,
                                       std::latch& latch) {
    return BlockingReceiver<Args...>{opt, latch};
  }(std::type_identity<ValueTuple>{}, result, latch);

  // Validate sender-receiver compatibility for better error messages
  using ReceiverType = decltype(receiver);
  static_assert(
      SenderTo<std::remove_cvref_t<S>, ReceiverType>,
      "The sender cannot be connected to the blocking receiver. "
      "This typically means the sender's value types don't match what syncWait expects.");

  auto op = std::forward<S>(sender).connect(std::move(receiver));

  op.start();

  latch.wait();

  return result;
}

// TODO: Implement startDetached

// TODO: Implement connect customization point

// TODO: Implement start customization point

}  // namespace tempura
