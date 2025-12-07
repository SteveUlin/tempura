// Repeat algorithms - repeat sender execution based on conditions
//
// Provides three algorithms:
// - repeatEffect(sender): Repeat forever until error/stopped
// - repeatEffectUntil(sender, predicate): Repeat until predicate returns true
// - repeatN(sender, count): Repeat exactly N times
//
// Requirements:
// - Source sender must be copyable (reconnected each iteration)
// - Source sender must be an effect (produces void)

#pragma once

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include "concepts.h"
#include "meta/manual_lifetime.h"

namespace tempura {

// ============================================================================
// repeatEffectUntil - repeat until predicate returns true
// ============================================================================

template <typename S, typename P, typename R>
class RepeatEffectUntilOperationState;

template <typename S, typename P, typename R>
class RepeatEffectUntilReceiver {
 public:
  using OpState = RepeatEffectUntilOperationState<S, P, R>;

  explicit RepeatEffectUntilReceiver(OpState* op) : op_(op) {}

  // Accept and discard any values - repeat algorithms treat the sender as an effect
  template <typename... Args>
  void setValue(Args&&...) noexcept { op_->onValueComplete(); }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    op_->forwardError(std::forward<ErrorArgs>(args)...);
  }

  void setStopped() noexcept { op_->forwardStopped(); }

 private:
  OpState* op_;
};

template <typename S, typename P, typename R>
class RepeatEffectUntilOperationState {
 public:
  using InnerReceiver = RepeatEffectUntilReceiver<S, P, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  RepeatEffectUntilOperationState(S source, P predicate, R receiver)
      : source_(std::move(source)),
        predicate_(std::move(predicate)),
        receiver_(std::move(receiver)),
        constructed_(false) {}

  ~RepeatEffectUntilOperationState() {
    if (constructed_) {
      inner_op_.destruct();
    }
  }

  // Non-copyable, non-movable
  RepeatEffectUntilOperationState(const RepeatEffectUntilOperationState&) =
      delete;
  auto operator=(const RepeatEffectUntilOperationState&)
      -> RepeatEffectUntilOperationState& = delete;
  RepeatEffectUntilOperationState(RepeatEffectUntilOperationState&&) = delete;
  auto operator=(RepeatEffectUntilOperationState&&)
      -> RepeatEffectUntilOperationState& = delete;

  void start() noexcept {
    // Copy the source sender - repeat needs to connect multiple times
    inner_op_.constructWith(
        [this] { return S(source_).connect(InnerReceiver{this}); });
    constructed_ = true;
    inner_op_->start();
  }

  void onValueComplete() noexcept {
    // Destroy previous operation
    inner_op_.destruct();
    constructed_ = false;

    // Check if we should stop
    if (std::invoke(predicate_)) {
      std::move(receiver_).setValue();
      return;
    }

    // Copy and reconstruct - repeat needs fresh sender each iteration
    inner_op_.constructWith(
        [this] { return S(source_).connect(InnerReceiver{this}); });
    constructed_ = true;
    inner_op_->start();
  }

  template <typename... ErrorArgs>
  void forwardError(ErrorArgs&&... args) noexcept {
    std::move(receiver_).setError(std::forward<ErrorArgs>(args)...);
  }

  void forwardStopped() noexcept { std::move(receiver_).setStopped(); }

 private:
  S source_;
  P predicate_;
  R receiver_;
  ManualLifetime<InnerOpState> inner_op_;
  bool constructed_;
};

template <typename S, typename P>
class RepeatEffectUntilSender {
 public:
  // Completion signatures: void on success, pass through errors/stopped
  template <typename Env = EmptyEnv>
  using CompletionSignatures = MergeCompletionSignaturesT<
      tempura::CompletionSignatures<SetValueTag(), SetStoppedTag()>,
      ListToCompletionSignaturesT<
          ErrorSignaturesT<GetCompletionSignaturesT<S, Env>>>>;

  RepeatEffectUntilSender(S source, P predicate)
      : source_(std::move(source)), predicate_(std::move(predicate)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return RepeatEffectUntilOperationState<S, P, std::remove_cvref_t<R>>{
        std::move(source_), std::move(predicate_), std::forward<R>(receiver)};
  }

 private:
  S source_;
  P predicate_;
};

template <typename S, typename P>
RepeatEffectUntilSender(S, P)
    -> RepeatEffectUntilSender<std::remove_cvref_t<S>, std::remove_cvref_t<P>>;

// Free function
template <typename S, typename P>
auto repeatEffectUntil(S&& source, P&& predicate) {
  return RepeatEffectUntilSender{std::forward<S>(source),
                                  std::forward<P>(predicate)};
}

// Adaptor for pipe syntax
template <typename P>
struct RepeatEffectUntilAdaptor {
  P predicate;

  template <typename S>
  auto operator()(S&& sender) const {
    return RepeatEffectUntilSender{std::forward<S>(sender), predicate};
  }
};

template <typename P>
auto repeatEffectUntil(P&& predicate) {
  return RepeatEffectUntilAdaptor<std::decay_t<P>>{std::forward<P>(predicate)};
}

template <typename S, typename P>
auto operator|(S&& sender, const RepeatEffectUntilAdaptor<P>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

// ============================================================================
// repeatEffect - repeat forever until error/stopped
// ============================================================================

template <typename S>
auto repeatEffect(S&& source) {
  return repeatEffectUntil(std::forward<S>(source), [] { return false; });
}

struct RepeatEffectAdaptor {
  template <typename S>
  auto operator()(S&& sender) const {
    return repeatEffect(std::forward<S>(sender));
  }
};

inline auto repeatEffect() { return RepeatEffectAdaptor{}; }

template <typename S>
auto operator|(S&& sender, RepeatEffectAdaptor) {
  return repeatEffect(std::forward<S>(sender));
}

// ============================================================================
// repeatN - repeat exactly N times
// ============================================================================

template <typename S, typename R>
class RepeatNOperationState;

template <typename S, typename R>
class RepeatNReceiver {
 public:
  using OpState = RepeatNOperationState<S, R>;

  explicit RepeatNReceiver(OpState* op) : op_(op) {}

  // Accept and discard any values - repeat algorithms treat the sender as an effect
  template <typename... Args>
  void setValue(Args&&...) noexcept { op_->onValueComplete(); }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    op_->forwardError(std::forward<ErrorArgs>(args)...);
  }

  void setStopped() noexcept { op_->forwardStopped(); }

 private:
  OpState* op_;
};

template <typename S, typename R>
class RepeatNOperationState {
 public:
  using InnerReceiver = RepeatNReceiver<S, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  RepeatNOperationState(S source, std::size_t count, R receiver)
      : source_(std::move(source)),
        remaining_(count),
        receiver_(std::move(receiver)),
        constructed_(false) {}

  ~RepeatNOperationState() {
    if (constructed_) {
      inner_op_.destruct();
    }
  }

  // Non-copyable, non-movable
  RepeatNOperationState(const RepeatNOperationState&) = delete;
  auto operator=(const RepeatNOperationState&)
      -> RepeatNOperationState& = delete;
  RepeatNOperationState(RepeatNOperationState&&) = delete;
  auto operator=(RepeatNOperationState&&) -> RepeatNOperationState& = delete;

  void start() noexcept {
    // If count is 0, complete immediately
    if (remaining_ == 0) {
      std::move(receiver_).setValue();
      return;
    }

    // Copy the source sender - repeat needs to connect multiple times
    inner_op_.constructWith(
        [this] { return S(source_).connect(InnerReceiver{this}); });
    constructed_ = true;
    inner_op_->start();
  }

  void onValueComplete() noexcept {
    // Destroy previous operation
    inner_op_.destruct();
    constructed_ = false;

    --remaining_;

    // Check if we're done
    if (remaining_ == 0) {
      std::move(receiver_).setValue();
      return;
    }

    // Copy and reconstruct - repeat needs fresh sender each iteration
    inner_op_.constructWith(
        [this] { return S(source_).connect(InnerReceiver{this}); });
    constructed_ = true;
    inner_op_->start();
  }

  template <typename... ErrorArgs>
  void forwardError(ErrorArgs&&... args) noexcept {
    std::move(receiver_).setError(std::forward<ErrorArgs>(args)...);
  }

  void forwardStopped() noexcept { std::move(receiver_).setStopped(); }

 private:
  S source_;
  std::size_t remaining_;
  R receiver_;
  ManualLifetime<InnerOpState> inner_op_;
  bool constructed_;
};

template <typename S>
class RepeatNSender {
 public:
  // Completion signatures: void on success, pass through errors/stopped
  template <typename Env = EmptyEnv>
  using CompletionSignatures = MergeCompletionSignaturesT<
      tempura::CompletionSignatures<SetValueTag(), SetStoppedTag()>,
      ListToCompletionSignaturesT<
          ErrorSignaturesT<GetCompletionSignaturesT<S, Env>>>>;

  RepeatNSender(S source, std::size_t count)
      : source_(std::move(source)), count_(count) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return RepeatNOperationState<S, std::remove_cvref_t<R>>{
        std::move(source_), count_, std::forward<R>(receiver)};
  }

 private:
  S source_;
  std::size_t count_;
};

template <typename S>
RepeatNSender(S, std::size_t) -> RepeatNSender<std::remove_cvref_t<S>>;

// Free function
template <typename S>
auto repeatN(S&& source, std::size_t count) {
  return RepeatNSender{std::forward<S>(source), count};
}

// Adaptor for pipe syntax
struct RepeatNAdaptor {
  std::size_t count;

  template <typename S>
  auto operator()(S&& sender) const {
    return RepeatNSender{std::forward<S>(sender), count};
  }
};

inline auto repeatN(std::size_t count) { return RepeatNAdaptor{count}; }

template <typename S>
auto operator|(S&& sender, RepeatNAdaptor adaptor) {
  return repeatN(std::forward<S>(sender), adaptor.count);
}

}  // namespace tempura
