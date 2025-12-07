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

#include "completion_signatures.h"
#include "concepts.h"
#include "env.h"
#include "meta/manual_lifetime.h"

namespace tempura {

// Helper: Extract value tuple from sender (reuse from let_value.h logic)
template <typename S, typename Env = EmptyEnv>
struct GetOnValueTuple {
  using CompletionSigs = GetCompletionSignaturesT<S, Env>;
  using ValueSigs = ValueSignaturesT<CompletionSigs>;

  static_assert(Size_v<ValueSigs> > 0,
                "Sender must have at least one value signature for on()");

  using FirstValueSig = Get_t<0, ValueSigs>;
  using ValueArgs = SignatureArgsT<FirstValueSig>;
  using Type = ListToTupleT<ValueArgs>;
};

template <typename S, typename Env = EmptyEnv>
using GetOnValueTupleT = typename GetOnValueTuple<S, Env>::Type;

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

// Forward declaration for schedule receiver wrapper
template <Scheduler Sched, Sender S, typename R>
struct ScheduleReceiver;

// Operation state for on() - manages scheduler transitions and inner sender
template <Scheduler Sched, Sender S, typename R>
class OnOperationState {
 public:
  using InnerReceiver = OnReceiver<Sched, S, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));
  using TargetScheduleOpState = decltype(std::declval<Sched>()
                                             .schedule()
                                             .connect(std::declval<ScheduleReceiver<Sched, S, R>>()));
  using OriginalScheduleOpState =
      decltype(std::declval<decltype(get_scheduler(get_env(std::declval<R>())))>()
                   .schedule()
                   .connect(std::declval<ScheduleReceiver<Sched, S, R>>()));

  OnOperationState(Sched target_sched, S sender, auto original_sched,
                   R receiver)
      : target_sched_(target_sched),
        original_sched_(std::move(original_sched)),
        receiver_(std::move(receiver)),
        inner_receiver_(InnerReceiver{&target_sched_, &receiver_, this}),
        inner_op_(std::move(sender).connect(std::move(inner_receiver_))) {}

  // Operation States are not copyable nor movable
  OnOperationState(const OnOperationState&) = delete;
  auto operator=(const OnOperationState&) -> OnOperationState& = delete;
  OnOperationState(OnOperationState&&) = delete;
  auto operator=(OnOperationState&&) -> OnOperationState& = delete;

  void start() noexcept {
    // Schedule to target scheduler, then start inner operation
    target_schedule_op_.constructWith([this]() {
      return target_sched_.schedule().connect(ScheduleReceiver<Sched, S, R>{this});
    });
    target_schedule_op_.get().start();
  }

  // Called by scheduler when we've transitioned to target scheduler
  void setValue() noexcept {
    if (!scheduled_to_target_) {
      // First setValue: we've reached target scheduler
      scheduled_to_target_ = true;
      target_schedule_op_.destruct();
      // Now we're on target scheduler - start the inner operation
      inner_op_.start();
    } else {
      // Second setValue: we've returned to original scheduler
      original_schedule_op_.destruct();
      // Deliver result to final receiver
      if (has_value_) {
        std::apply([this](auto&&... args) { receiver_.setValue(std::forward<decltype(args)>(args)...); },
                   std::move(*result_));
      } else if (has_error_) {
        receiver_.setError(error_code_);
      } else if (has_stopped_) {
        receiver_.setStopped();
      }
    }
  }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&...) noexcept {
    if (!scheduled_to_target_) {
      // Scheduling to target failed - forward error
      target_schedule_op_.destruct();
      receiver_.setError(
          std::make_error_code(std::errc::resource_unavailable_try_again));
    } else {
      // Return scheduling failed
      original_schedule_op_.destruct();
      receiver_.setError(error_code_);
    }
  }

  void setStopped() noexcept {
    if (!scheduled_to_target_) {
      // Scheduling to target was stopped
      target_schedule_op_.destruct();
      receiver_.setStopped();
    } else {
      // Return scheduling was stopped
      original_schedule_op_.destruct();
      receiver_.setStopped();
    }
  }

  // Schedule back to original scheduler and complete with value
  template <typename... Args>
  void scheduleBackAndComplete(Args&&... args) noexcept {
    // Store args in tuple for later
    result_.emplace(std::forward<Args>(args)...);
    has_value_ = true;

    // Schedule back to original scheduler
    original_schedule_op_.constructWith([this]() {
      return original_sched_.schedule().connect(ScheduleReceiver<Sched, S, R>{this});
    });
    original_schedule_op_.get().start();
  }

  // Schedule back to original scheduler and complete with error
  template <typename... ErrorArgs>
  void scheduleBackAndError(ErrorArgs&&... args) noexcept {
    has_error_ = true;
    error_code_ = std::make_error_code(std::errc::io_error);

    // Schedule back to original scheduler
    original_schedule_op_.constructWith([this]() {
      return original_sched_.schedule().connect(ScheduleReceiver<Sched, S, R>{this});
    });
    original_schedule_op_.get().start();
  }

  // Schedule back to original scheduler and complete with stopped
  void scheduleBackAndStop() noexcept {
    has_stopped_ = true;

    // Schedule back to original scheduler
    original_schedule_op_.constructWith([this]() {
      return original_sched_.schedule().connect(ScheduleReceiver<Sched, S, R>{this});
    });
    original_schedule_op_.get().start();
  }

 private:
  Sched target_sched_;
  decltype(get_scheduler(get_env(std::declval<R>()))) original_sched_;
  R receiver_;
  InnerReceiver inner_receiver_;
  InnerOpState inner_op_;

  // Schedule operation states stored as members to avoid UAF
  ManualLifetime<TargetScheduleOpState> target_schedule_op_;
  ManualLifetime<OriginalScheduleOpState> original_schedule_op_;

  // State tracking
  bool scheduled_to_target_ = false;

  // Result storage for round-trip
  std::optional<GetOnValueTupleT<S>> result_;
  std::error_code error_code_;
  bool has_value_ = false;
  bool has_error_ = false;
  bool has_stopped_ = false;
};

// Receiver wrapper for schedule operations - forwards to OnOperationState
template <Scheduler Sched, Sender S, typename R>
struct ScheduleReceiver {
  OnOperationState<Sched, S, R>* op_state_;

  void setValue() noexcept { op_state_->setValue(); }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    op_state_->setError(std::forward<ErrorArgs>(args)...);
  }

  void setStopped() noexcept { op_state_->setStopped(); }

  auto get_env() const noexcept { return EmptyEnv{}; }
};

// OnSender - adaptor that runs sender on target scheduler
template <Scheduler Sched, Sender S>
class OnSender {
 public:
  // Completion signatures are the same as the inner sender's
  template <typename Env = EmptyEnv>
  using CompletionSignatures = GetCompletionSignaturesT<S, Env>;

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
