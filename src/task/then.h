// ThenSender - transforms values from the upstream sender

#pragma once

#include <functional>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

#include "concepts.h"

namespace tempura {

// ============================================================================
// Composition Senders
// ============================================================================

template <typename F, typename R>
class ThenReceiver {
 public:
  ThenReceiver(F* func, R* receiver) : func_(func), receiver_(receiver) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    auto result = std::invoke(*func_, std::forward<Args>(args)...);
    receiver_->setValue(std::move(result));
  }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    receiver_->setError(std::forward<ErrorArgs>(args)...);
  }

  void setStopped() noexcept { receiver_->setStopped(); }

 private:
  F* func_;
  R* receiver_;
};

template <typename S, typename F, typename R>
class ThenOperationState {
 public:
  using InnerReceiver = ThenReceiver<F, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  ThenOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        inner_op_(
            std::move(sender).connect(InnerReceiver{&func_, &receiver_})) {}

  // Operation States are not copyable nor movable
  ThenOperationState(const ThenOperationState&) = delete;
  auto operator=(const ThenOperationState&) -> ThenOperationState& = delete;
  ThenOperationState(ThenOperationState&&) = delete;
  auto operator=(ThenOperationState&&) -> ThenOperationState& = delete;

  void start() noexcept { inner_op_.start(); }

 private:
  F func_;
  R receiver_;
  InnerOpState inner_op_;
};

template <typename S, typename F>
class ThenSender {
 public:
  using ValueTypes = std::tuple<decltype(std::apply(
      std::declval<F>(), std::declval<typename S::ValueTypes>()))>;
  using ErrorTypes = typename S::ErrorTypes;  // Propagate error types

  // Validate that the function can be called with the sender's value types
  static_assert(
      requires { std::apply(std::declval<F>(), std::declval<typename S::ValueTypes>()); },
      "The function passed to then must be callable with the sender's value types. "
      "Check that your lambda/function signature matches the values produced by the sender.");

  ThenSender(S sender, F func)
      : sender_(std::move(sender)), func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return ThenOperationState<S, F, std::remove_cvref_t<R>>{
        std::move(sender_), std::move(func_), std::forward<R>(receiver)};
  }

 private:
  S sender_;
  F func_;
};

template <typename S, typename F>
ThenSender(S, F) -> ThenSender<std::remove_cvref_t<S>, std::remove_cvref_t<F>>;

// Adaptor for pipe operator - wraps a transformation function
template <typename F>
struct ThenAdaptor {
  F func;

  template <typename S>
  auto operator()(S&& sender) const {
    return ThenSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
auto then(S&& sender, F&& func) {
  return ThenSender{std::forward<S>(sender), std::forward<F>(func)};
}

// Helper to create a Then adaptor for use with operator|
template <typename F>
auto then(F&& func) {
  return ThenAdaptor<std::decay_t<F>>{std::forward<F>(func)};
}

// Pipe operator for sender | adaptor syntax
template <typename S, typename F>
auto operator|(S&& sender, const ThenAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

}  // namespace tempura
