// LetValueSender - chains asynchronous operations by returning new senders

#pragma once

#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

#include "concepts.h"

namespace tempura {

// ============================================================================
// letValue - chains asynchronous operations by returning new senders
// ============================================================================

template <typename F, typename R>
class LetValueReceiver {
 public:
  LetValueReceiver(F* func, R* receiver) : func_(func), receiver_(receiver) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    // Apply function to get a new sender
    auto inner_sender = (*func_)(std::forward<Args>(args)...);

    // Connect and start the inner sender with our downstream receiver
    auto op = std::move(inner_sender).connect(std::move(*receiver_));
    op.start();
  }

  void setError(std::error_code ec) noexcept { receiver_->setError(ec); }

  void setStopped() noexcept { receiver_->setStopped(); }

 private:
  F* func_;
  R* receiver_;
};

template <typename S, typename F, typename R>
class LetValueOperationState {
 public:
  using InnerReceiver = LetValueReceiver<F, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  LetValueOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        inner_op_(
            std::move(sender).connect(InnerReceiver{&func_, &receiver_})) {}

  // Operation States are not copyable nor movable
  LetValueOperationState(const LetValueOperationState&) = delete;
  auto operator=(const LetValueOperationState&)
      -> LetValueOperationState& = delete;
  LetValueOperationState(LetValueOperationState&&) = delete;
  auto operator=(LetValueOperationState&&) -> LetValueOperationState& = delete;

  void start() noexcept { inner_op_.start(); }

 private:
  F func_;
  R receiver_;
  InnerOpState inner_op_;
};

template <typename S, typename F>
class LetValueSender {
 public:
  // The result type is the ValueTypes of the sender returned by F
  using InnerSender = decltype(std::apply(
      std::declval<F>(), std::declval<typename S::ValueTypes>()));
  using ValueTypes = typename InnerSender::ValueTypes;

  LetValueSender(S sender, F func)
      : sender_(std::move(sender)), func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return LetValueOperationState<S, F, std::remove_cvref_t<R>>{
        std::move(sender_), std::move(func_), std::forward<R>(receiver)};
  }

 private:
  S sender_;
  F func_;
};

template <typename S, typename F>
LetValueSender(S, F)
    -> LetValueSender<std::remove_cvref_t<S>, std::remove_cvref_t<F>>;

// Adaptor for pipe operator - wraps a transformation function
template <typename F>
struct LetValueAdaptor {
  F func;

  template <typename S>
  auto operator()(S&& sender) const {
    return LetValueSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
auto letValue(S&& sender, F&& func) {
  return LetValueSender{std::forward<S>(sender), std::forward<F>(func)};
}

// Helper to create a LetValue adaptor for use with operator|
template <typename F>
auto letValue(F&& func) {
  return LetValueAdaptor<std::decay_t<F>>{std::forward<F>(func)};
}

// Pipe operator for sender | adaptor syntax
template <typename S, typename F>
auto operator|(S&& sender, const LetValueAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

}  // namespace tempura
