// UponStoppedSender - transforms stopped completion from the upstream sender

#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include "concepts.h"
#include "completion_signatures.h"

namespace tempura {

// Helper: Transform a single signature for uponStopped
// SetStoppedTag() -> SetValueTag(Result), others pass through
template <typename Sig, typename F>
struct TransformUponStoppedSig {
  using Type = CompletionSignatures<Sig>;  // Pass through non-stopped sigs
};

template <typename F>
struct TransformUponStoppedSig<SetStoppedTag(), F> {
  using ResultType = std::invoke_result_t<F>;
  using Type = CompletionSignatures<SetValueTag(ResultType)>;
};

template <typename Sig, typename F>
using TransformUponStoppedSigT = typename TransformUponStoppedSig<Sig, F>::Type;

// Helper: Transform stopped signatures by applying function F
template <typename CompletionSigs, typename F>
struct TransformUponStoppedSignaturesImpl;

template <typename... Sigs, typename F>
struct TransformUponStoppedSignaturesImpl<CompletionSignatures<Sigs...>, F> {
  using Type = MergeCompletionSignaturesT<TransformUponStoppedSigT<Sigs, F>...>;
};

template <typename CompletionSigs, typename F>
using TransformUponStoppedSignaturesT =
    typename TransformUponStoppedSignaturesImpl<CompletionSigs, F>::Type;

template <typename F, typename R>
class UponStoppedReceiver {
 public:
  UponStoppedReceiver(F* func, R* receiver) : func_(func), receiver_(receiver) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    receiver_->setValue(std::forward<Args>(args)...);
  }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    receiver_->setError(std::forward<ErrorArgs>(args)...);
  }

  void setStopped() noexcept {
    auto result = std::invoke(*func_);
    receiver_->setValue(std::move(result));
  }

 private:
  F* func_;
  R* receiver_;
};

template <typename S, typename F, typename R>
class UponStoppedOperationState {
 public:
  using InnerReceiver = UponStoppedReceiver<F, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  UponStoppedOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        inner_op_(
            std::move(sender).connect(InnerReceiver{&func_, &receiver_})) {}

  // Operation States are not copyable nor movable
  UponStoppedOperationState(const UponStoppedOperationState&) = delete;
  auto operator=(const UponStoppedOperationState&)
      -> UponStoppedOperationState& = delete;
  UponStoppedOperationState(UponStoppedOperationState&&) = delete;
  auto operator=(UponStoppedOperationState&&)
      -> UponStoppedOperationState& = delete;

  void start() noexcept { inner_op_.start(); }

 private:
  F func_;
  R receiver_;
  InnerOpState inner_op_;
};

template <typename S, typename F>
class UponStoppedSender {
 public:
  // Transform stopped signatures, pass through value/error signatures
  template <typename Env = EmptyEnv>
  using CompletionSignatures = TransformUponStoppedSignaturesT<
      GetCompletionSignaturesT<S, Env>, F>;

  UponStoppedSender(S sender, F func)
      : sender_(std::move(sender)), func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return UponStoppedOperationState<S, F, std::remove_cvref_t<R>>{
        std::move(sender_), std::move(func_), std::forward<R>(receiver)};
  }

 private:
  S sender_;
  F func_;
};

template <typename S, typename F>
UponStoppedSender(S, F)
    -> UponStoppedSender<std::remove_cvref_t<S>, std::remove_cvref_t<F>>;

// Adaptor for pipe operator - wraps a transformation function
template <typename F>
struct UponStoppedAdaptor {
  F func;

  template <typename S>
  auto operator()(S&& sender) const {
    return UponStoppedSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
auto uponStopped(S&& sender, F&& func) {
  return UponStoppedSender{std::forward<S>(sender), std::forward<F>(func)};
}

// Helper to create a UponStopped adaptor for use with operator|
template <typename F>
auto uponStopped(F&& func) {
  return UponStoppedAdaptor<std::decay_t<F>>{std::forward<F>(func)};
}

// Pipe operator for sender | adaptor syntax
template <typename S, typename F>
auto operator|(S&& sender, const UponStoppedAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

}  // namespace tempura
