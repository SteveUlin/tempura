// OnSender - execute sender on specified scheduler, return result to original
//
// The on() algorithm implements the "there-and-back-again" pattern:
// 1. Query receiver's environment for current scheduler
// 2. Transition execution to target scheduler
// 3. Execute sender with target scheduler environment
// 4. Transition back to original scheduler
// 5. Deliver result to original receiver
//
// This is the P2300 round-trip scheduler transition pattern.

#pragma once

#include <type_traits>
#include <utility>

#include "concepts.h"
#include "env.h"

namespace tempura {

// Forward declarations
template <Scheduler Sched, Sender S, typename R>
class OnOperationState;

// Receiver wrapper that provides target scheduler environment to inner sender
template <Scheduler Sched, Sender S, typename R>
class OnReceiver {
 public:
  using OpState = OnOperationState<Sched, S, R>;

  OnReceiver(Sched* target_sched, R* receiver, OpState* op_state)
      : target_sched_(target_sched), receiver_(receiver), op_state_(op_state) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    op_state_->scheduleBackAndComplete(std::forward<Args>(args)...);
  }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    op_state_->scheduleBackAndError(std::forward<ErrorArgs>(args)...);
  }

  void setStopped() noexcept { op_state_->scheduleBackAndStop(); }

  // Provide target scheduler environment to inner sender
  auto get_env() const noexcept { return withScheduler(*target_sched_); }

 private:
  Sched* target_sched_;
  R* receiver_;
  OpState* op_state_;
};

// Operation state for on() - manages scheduler transitions and inner sender
template <Scheduler Sched, Sender S, typename R>
class OnOperationState {
 public:
  using InnerReceiver = OnReceiver<Sched, S, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  OnOperationState(Sched target_sched, S sender, auto original_sched,
                   R receiver)
      : target_sched_(target_sched),
        original_sched_(std::move(original_sched)),
        receiver_(std::move(receiver)),
        inner_receiver_(InnerReceiver{&target_sched_, &receiver_, this}),
        inner_op_(std::move(sender).connect(inner_receiver_)) {}

  // Operation States are not copyable nor movable
  OnOperationState(const OnOperationState&) = delete;
  auto operator=(const OnOperationState&) -> OnOperationState& = delete;
  OnOperationState(OnOperationState&&) = delete;
  auto operator=(OnOperationState&&) -> OnOperationState& = delete;

  void start() noexcept {
    // Schedule to target scheduler, then start inner operation
    auto schedule_sender = target_sched_.schedule();
    auto start_op = std::move(schedule_sender).connect(*this);
    start_op.start();
  }

  // Called by scheduler when we've transitioned to target scheduler
  void setValue() noexcept {
    // Now we're on target scheduler - start the inner operation
    inner_op_.start();
  }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&...) noexcept {
    // Scheduling to target failed - forward error
    receiver_.setError(
        std::make_error_code(std::errc::resource_unavailable_try_again));
  }

  void setStopped() noexcept {
    // Scheduling to target was stopped
    receiver_.setStopped();
  }

  // Schedule back to original scheduler and complete with value
  template <typename... Args>
  void scheduleBackAndComplete(Args&&... args) noexcept {
    // Store args in tuple for later
    result_.emplace(std::forward<Args>(args)...);
    has_value_ = true;

    // Schedule back to original scheduler
    auto schedule_sender = original_sched_.schedule();
    auto return_op = std::move(schedule_sender).connect(*this);
    return_op.start();
  }

  // Schedule back to original scheduler and complete with error
  template <typename... ErrorArgs>
  void scheduleBackAndError(ErrorArgs&&... args) noexcept {
    has_error_ = true;
    error_code_ = std::make_error_code(std::errc::io_error);

    // Schedule back to original scheduler
    auto schedule_sender = original_sched_.schedule();
    auto return_op = std::move(schedule_sender).connect(*this);
    return_op.start();
  }

  // Schedule back to original scheduler and complete with stopped
  void scheduleBackAndStop() noexcept {
    has_stopped_ = true;

    // Schedule back to original scheduler
    auto schedule_sender = original_sched_.schedule();
    auto return_op = std::move(schedule_sender).connect(*this);
    return_op.start();
  }

 private:
  Sched target_sched_;
  decltype(get_scheduler(get_env(std::declval<R>()))) original_sched_;
  R receiver_;
  InnerReceiver inner_receiver_;
  InnerOpState inner_op_;

  // Result storage for round-trip
  std::optional<typename S::ValueTypes> result_;
  std::error_code error_code_;
  bool has_value_ = false;
  bool has_error_ = false;
  bool has_stopped_ = false;
};

// OnSender - adaptor that runs sender on target scheduler
template <Scheduler Sched, Sender S>
class OnSender {
 public:
  using ValueTypes = typename S::ValueTypes;
  using ErrorTypes = typename S::ErrorTypes;

  OnSender(Sched sched, S sender)
      : target_sched_(sched), sender_(std::move(sender)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    // Query receiver's environment for original scheduler
    auto env = get_env(receiver);
    auto original_sched = get_scheduler(env);

    return OnOperationState<Sched, S, std::remove_cvref_t<R>>{
        target_sched_, std::move(sender_), original_sched,
        std::forward<R>(receiver)};
  }

 private:
  Sched target_sched_;
  S sender_;
};

// Deduction guide
template <Scheduler Sched, Sender S>
OnSender(Sched, S) -> OnSender<Sched, std::remove_cvref_t<S>>;

// on() - run sender on scheduler, return to original
template <Scheduler Sched, Sender S>
auto on(Sched sched, S&& sender) {
  return OnSender{sched, std::forward<S>(sender)};
}

// Adaptor for pipe operator
template <Scheduler Sched>
struct OnAdaptor {
  Sched sched;

  template <Sender S>
  auto operator()(S&& sender) const {
    return OnSender{sched, std::forward<S>(sender)};
  }
};

// Helper to create On adaptor for use with operator|
template <Scheduler Sched>
auto on(Sched sched) {
  return OnAdaptor<Sched>{sched};
}

// Pipe operator for sender | on(scheduler) syntax
template <Sender S, Scheduler Sched>
auto operator|(S&& sender, const OnAdaptor<Sched>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

}  // namespace tempura
