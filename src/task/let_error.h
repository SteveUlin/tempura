// LetErrorSender - chains asynchronous error recovery by returning new senders

#pragma once

#include "concepts.h"

#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tempura {

// ============================================================================
// letError - chains asynchronous error recovery by returning new senders
// ============================================================================

template <typename F, typename R>
class LetErrorReceiver {
 public:
  LetErrorReceiver(F* func, R* receiver) : func_(func), receiver_(receiver) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    receiver_->setValue(std::forward<Args>(args)...);
  }

  void setError(std::error_code ec) noexcept {
    // Apply function to get a new sender
    auto inner_sender = (*func_)(ec);

    // Connect and start the inner sender with our downstream receiver
    auto op = std::move(inner_sender).connect(std::move(*receiver_));
    op.start();
  }

  void setStopped() noexcept { receiver_->setStopped(); }

 private:
  F* func_;
  R* receiver_;
};

template <typename S, typename F, typename R>
class LetErrorOperationState {
 public:
  using InnerReceiver = LetErrorReceiver<F, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  LetErrorOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        inner_op_(
            std::move(sender).connect(InnerReceiver{&func_, &receiver_})) {}

  // Operation States are not copyable nor movable
  LetErrorOperationState(const LetErrorOperationState&) = delete;
  auto operator=(const LetErrorOperationState&)
      -> LetErrorOperationState& = delete;
  LetErrorOperationState(LetErrorOperationState&&) = delete;
  auto operator=(LetErrorOperationState&&) -> LetErrorOperationState& = delete;

  void start() noexcept { inner_op_.start(); }

 private:
  F func_;
  R receiver_;
  InnerOpState inner_op_;
};

template <typename S, typename F>
class LetErrorSender {
 public:
  // The result type is the ValueTypes of the sender returned by F
  using InnerSender = decltype(std::declval<F>()(std::declval<std::error_code>()));
  using ValueTypes = typename InnerSender::ValueTypes;

  LetErrorSender(S sender, F func)
      : sender_(std::move(sender)), func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return LetErrorOperationState<S, F, std::remove_cvref_t<R>>{
        std::move(sender_), std::move(func_), std::forward<R>(receiver)};
  }

 private:
  S sender_;
  F func_;
};

template <typename S, typename F>
LetErrorSender(S, F)
    -> LetErrorSender<std::remove_cvref_t<S>, std::remove_cvref_t<F>>;

// Adaptor for pipe operator - wraps a transformation function
template <typename F>
struct LetErrorAdaptor {
  F func;

  template <typename S>
  auto operator()(S&& sender) const {
    return LetErrorSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
auto letError(S&& sender, F&& func) {
  return LetErrorSender{std::forward<S>(sender), std::forward<F>(func)};
}

// Helper to create a LetError adaptor for use with operator|
template <typename F>
auto letError(F&& func) {
  return LetErrorAdaptor<std::decay_t<F>>{std::forward<F>(func)};
}

// Pipe operator for sender | adaptor syntax
template <typename S, typename F>
  requires requires(S&& s, const LetErrorAdaptor<F>& adaptor) {
    adaptor(std::forward<S>(s));
  }
auto operator|(S&& sender, const LetErrorAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

// TODO: Implement letStopped sender

}  // namespace tempura
