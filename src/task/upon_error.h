// UponErrorSender - transforms errors from the upstream sender

#pragma once

#include <functional>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

#include "concepts.h"
#include "completion_signatures.h"

namespace tempura {

// Helper: Transform error signatures by applying function F
template <typename CompletionSigs, typename F>
struct TransformUponErrorSignaturesImpl;

template <typename... Sigs, typename F>
struct TransformUponErrorSignaturesImpl<CompletionSignatures<Sigs...>, F> {
  // Transform one signature at a time
  template <typename Sig>
  struct TransformOne {
    using Type = CompletionSignatures<Sig>;  // Pass through non-error sigs
  };

  // Transform error signatures: SetErrorTag(Args...) -> SetValueTag(Result)
  template <typename... Args>
  struct TransformOne<SetErrorTag(Args...)> {
    using ResultType = decltype(std::invoke(std::declval<F>(), std::declval<Args>()...));
    using Type = CompletionSignatures<SetValueTag(ResultType)>;
  };

  using Type = MergeCompletionSignaturesT<typename TransformOne<Sigs>::Type...>;
};

template <typename CompletionSigs, typename F>
using TransformUponErrorSignaturesT =
    typename TransformUponErrorSignaturesImpl<CompletionSigs, F>::Type;

template <typename F, typename R>
class UponErrorReceiver {
 public:
  UponErrorReceiver(F* func, R* receiver) : func_(func), receiver_(receiver) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    receiver_->setValue(std::forward<Args>(args)...);
  }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    auto result = std::invoke(*func_, std::forward<ErrorArgs>(args)...);
    receiver_->setValue(std::move(result));
  }

  void setStopped() noexcept { receiver_->setStopped(); }

 private:
  F* func_;
  R* receiver_;
};

template <typename S, typename F, typename R>
class UponErrorOperationState {
 public:
  using InnerReceiver = UponErrorReceiver<F, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  UponErrorOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        inner_op_(
            std::move(sender).connect(InnerReceiver{&func_, &receiver_})) {}

  // Operation States are not copyable nor movable
  UponErrorOperationState(const UponErrorOperationState&) = delete;
  auto operator=(const UponErrorOperationState&)
      -> UponErrorOperationState& = delete;
  UponErrorOperationState(UponErrorOperationState&&) = delete;
  auto operator=(UponErrorOperationState&&)
      -> UponErrorOperationState& = delete;

  void start() noexcept { inner_op_.start(); }

 private:
  F func_;
  R receiver_;
  InnerOpState inner_op_;
};

template <typename S, typename F>
class UponErrorSender {
 public:
  // Transform error signatures, pass through value/stopped signatures
  template <typename Env = EmptyEnv>
  using CompletionSignatures = TransformUponErrorSignaturesT<
      GetCompletionSignaturesT<S, Env>, F>;

  UponErrorSender(S sender, F func)
      : sender_(std::move(sender)), func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return UponErrorOperationState<S, F, std::remove_cvref_t<R>>{
        std::move(sender_), std::move(func_), std::forward<R>(receiver)};
  }

 private:
  S sender_;
  F func_;
};

template <typename S, typename F>
UponErrorSender(S, F)
    -> UponErrorSender<std::remove_cvref_t<S>, std::remove_cvref_t<F>>;

// Adaptor for pipe operator - wraps a transformation function
template <typename F>
struct UponErrorAdaptor {
  F func;

  template <typename S>
  auto operator()(S&& sender) const {
    return UponErrorSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
auto uponError(S&& sender, F&& func) {
  return UponErrorSender{std::forward<S>(sender), std::forward<F>(func)};
}

// Helper to create a UponError adaptor for use with operator|
template <typename F>
auto uponError(F&& func) {
  return UponErrorAdaptor<std::decay_t<F>>{std::forward<F>(func)};
}

// Pipe operator for sender | adaptor syntax
template <typename S, typename F>
auto operator|(S&& sender, const UponErrorAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

// TODO: Implement uponStopped sender

}  // namespace tempura
