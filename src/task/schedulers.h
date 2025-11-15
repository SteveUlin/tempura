// Scheduler implementations for the sender/receiver model

#pragma once

#include <utility>

#include "concepts.h"
#include "synchronization/event_loop.h"

namespace tempura {

// ============================================================================
// InlineScheduler - executes immediately on calling thread
// ============================================================================

class InlineScheduler;

// Operation state for inline schedule sender
template <typename R>
class InlineScheduleOperationState {
 public:
  explicit InlineScheduleOperationState(R r) : receiver_(std::move(r)) {}

  // Operation States are not copyable nor movable
  InlineScheduleOperationState(const InlineScheduleOperationState&) = delete;
  auto operator=(const InlineScheduleOperationState&)
      -> InlineScheduleOperationState& = delete;
  InlineScheduleOperationState(InlineScheduleOperationState&&) = delete;
  auto operator=(InlineScheduleOperationState&&)
      -> InlineScheduleOperationState& = delete;

  void start() noexcept { std::move(receiver_).setValue(); }

 private:
  R receiver_;
};

// Sender returned by InlineScheduler::schedule()
class InlineScheduleSender {
 public:
  using ValueTypes = std::tuple<>;  // schedule() sends no values

  template <ReceiverOf<> R>
  auto connect(R&& receiver) && {
    return InlineScheduleOperationState<R>{std::forward<R>(receiver)};
  }
};

// Scheduler that executes work immediately on the calling thread
class InlineScheduler {
 public:
  auto schedule() const { return InlineScheduleSender{}; }
};

static_assert(Scheduler<InlineScheduler>);

// ============================================================================
// EventLoopScheduler - executes on EventLoop's worker thread
// ============================================================================

class EventLoopScheduler;

// Operation state for event loop schedule sender
template <typename R>
class EventLoopScheduleOperationState {
 public:
  EventLoopScheduleOperationState(EventLoop& loop, R r)
      : loop_(&loop), receiver_(std::move(r)) {}

  // Operation States are not copyable nor movable
  EventLoopScheduleOperationState(const EventLoopScheduleOperationState&) =
      delete;
  auto operator=(const EventLoopScheduleOperationState&)
      -> EventLoopScheduleOperationState& = delete;
  EventLoopScheduleOperationState(EventLoopScheduleOperationState&&) = delete;
  auto operator=(EventLoopScheduleOperationState&&)
      -> EventLoopScheduleOperationState& = delete;

  void start() noexcept {
    // Post work to event loop. Operation state must remain valid until
    // lambda executes and calls setValue() (P2300 requirement).
    loop_->post([this]() noexcept { receiver_.setValue(); });
  }

 private:
  EventLoop* loop_;
  R receiver_;
};

// Sender returned by EventLoopScheduler::schedule()
class EventLoopScheduleSender {
 public:
  using ValueTypes = std::tuple<>;  // schedule() sends no values

  explicit EventLoopScheduleSender(EventLoop& loop) : loop_(&loop) {}

  template <ReceiverOf<> R>
  auto connect(R&& receiver) && {
    return EventLoopScheduleOperationState<R>{*loop_,
                                              std::forward<R>(receiver)};
  }

 private:
  EventLoop* loop_;
};

// Scheduler that executes work on an EventLoop's worker thread
//
// REQUIRES: The EventLoop must outlive the scheduler and all work scheduled
// through it. The scheduler does not take ownership of the EventLoop.
//
// Example usage:
//   EventLoop loop;
//   std::thread worker([&loop] { loop.run(); });
//
//   EventLoopScheduler scheduler{loop};
//   auto work = scheduler.schedule() | then([] { return 42; });
//   auto result = syncWait(std::move(work));
//
//   loop.stop();
//   worker.join();
//
class EventLoopScheduler {
 public:
  explicit EventLoopScheduler(EventLoop& loop) : loop_(&loop) {}

  auto schedule() const { return EventLoopScheduleSender{*loop_}; }

 private:
  EventLoop* loop_;
};

static_assert(Scheduler<EventLoopScheduler>);

}  // namespace tempura
