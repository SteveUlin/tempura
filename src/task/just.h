// JustSender - immediately produces values

#pragma once

#include "concepts.h"

#include <concepts>
#include <tuple>
#include <utility>

namespace tempura {

// ============================================================================
// Fundamental Sender Types
// ============================================================================

// TODO: Implement Schedule sender

// Operation state for JustSender - unpacks tuple and forwards to receiver
template <typename R, typename... Args>
class JustOperationState {
 public:
  JustOperationState(std::tuple<Args...> values, R r)
      : values_(std::move(values)), receiver_(std::move(r)) {}

  // Operation States are not copyable nor movable
  JustOperationState(const JustOperationState&) = delete;
  auto operator=(const JustOperationState&) -> JustOperationState& = delete;
  JustOperationState(JustOperationState&&) = delete;
  auto operator=(JustOperationState&&) -> JustOperationState& = delete;

  void start() noexcept {
    // Unpack tuple and call variadic setValue
    std::apply(
        [this](auto&&... args) {
          std::forward<R>(receiver_).setValue(
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
  using ValueTypes = std::tuple<Args...>;

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

// Helper function to create a JustSender
template <typename... Args>
auto just(Args&&... args) {
  return JustSender{std::forward<Args>(args)...};
}

// TODO: Implement Transfer sender

}  // namespace tempura
