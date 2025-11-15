// Scheduler implementations for the sender/receiver model

#pragma once

#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "concepts.h"
#include "synchronization/event_loop.h"
#include "synchronization/threadpool.h"

namespace tempura {

// ============================================================================
// InlineScheduler - executes immediately on calling thread
// ============================================================================

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

// ============================================================================
// ThreadPoolScheduler - executes on a thread pool's worker threads
// ============================================================================

// Operation state for thread pool schedule sender
template <typename R>
class ThreadPoolScheduleOperationState {
 public:
  ThreadPoolScheduleOperationState(ThreadPool& pool, R r)
      : pool_(&pool), receiver_(std::move(r)) {}

  // Operation States are not copyable nor movable
  ThreadPoolScheduleOperationState(const ThreadPoolScheduleOperationState&) =
      delete;
  auto operator=(const ThreadPoolScheduleOperationState&)
      -> ThreadPoolScheduleOperationState& = delete;
  ThreadPoolScheduleOperationState(ThreadPoolScheduleOperationState&&) =
      delete;
  auto operator=(ThreadPoolScheduleOperationState&&)
      -> ThreadPoolScheduleOperationState& = delete;

  void start() noexcept {
    // Submit work to thread pool. Operation state must remain valid until
    // lambda executes and calls setValue() (P2300 requirement).
    pool_->submit([this]() noexcept { receiver_.setValue(); });
  }

 private:
  ThreadPool* pool_;
  R receiver_;
};

// Sender returned by ThreadPoolScheduler::schedule()
class ThreadPoolScheduleSender {
 public:
  using ValueTypes = std::tuple<>;  // schedule() sends no values

  explicit ThreadPoolScheduleSender(ThreadPool& pool) : pool_(&pool) {}

  template <ReceiverOf<> R>
  auto connect(R&& receiver) && {
    return ThreadPoolScheduleOperationState<R>{*pool_,
                                               std::forward<R>(receiver)};
  }

 private:
  ThreadPool* pool_;
};

// Scheduler that executes work on a thread pool's worker threads
//
// REQUIRES: The ThreadPool must outlive the scheduler and all work scheduled
// through it. The scheduler does not take ownership of the ThreadPool.
//
// Example usage:
//   ThreadPool pool{4};  // 4 worker threads
//
//   ThreadPoolScheduler scheduler{pool};
//   auto work = scheduler.schedule() | then([] { return 42; });
//   auto result = syncWait(std::move(work));
//
class ThreadPoolScheduler {
 public:
  explicit ThreadPoolScheduler(ThreadPool& pool) : pool_(&pool) {}

  auto schedule() const { return ThreadPoolScheduleSender{*pool_}; }

 private:
  ThreadPool* pool_;
};

static_assert(Scheduler<ThreadPoolScheduler>);

// ============================================================================
// NewThreadContext - manages threads spawned on demand
// ============================================================================

// NewThreadContext manages a collection of threads that are created on demand
//
// Each call to submit() spawns a new thread to execute the task. All threads
// are joined when the context is destroyed.
//
// WARNING: This can easily exhaust system resources if used carelessly.
// Use ThreadPool for most workloads.
//
class NewThreadContext {
 public:
  using Task = std::move_only_function<void()>;

  NewThreadContext() = default;

  // Disallow copy and move (contexts are synchronization points)
  NewThreadContext(const NewThreadContext&) = delete;
  NewThreadContext(NewThreadContext&&) = delete;
  auto operator=(const NewThreadContext&) -> NewThreadContext& = delete;
  auto operator=(NewThreadContext&&) -> NewThreadContext& = delete;

  // Destructor joins all spawned threads
  ~NewThreadContext() {
    std::scoped_lock lock{mutex_};
    for (auto& thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  // Spawn a new thread to execute the task
  void submit(Task task) {
    std::scoped_lock lock{mutex_};
    threads_.emplace_back(std::move(task));
  }

 private:
  std::vector<std::thread> threads_;
  std::mutex mutex_;  // Protects threads_ vector
};

// Operation state for new thread schedule sender
template <typename R>
class NewThreadScheduleOperationState {
 public:
  NewThreadScheduleOperationState(NewThreadContext& context, R r)
      : context_(&context), receiver_(std::move(r)) {}

  // Operation States are not copyable nor movable
  NewThreadScheduleOperationState(const NewThreadScheduleOperationState&) =
      delete;
  auto operator=(const NewThreadScheduleOperationState&)
      -> NewThreadScheduleOperationState& = delete;
  NewThreadScheduleOperationState(NewThreadScheduleOperationState&&) = delete;
  auto operator=(NewThreadScheduleOperationState&&)
      -> NewThreadScheduleOperationState& = delete;

  void start() noexcept {
    // Spawn a new thread to execute the receiver
    context_->submit([this]() noexcept { receiver_.setValue(); });
  }

 private:
  NewThreadContext* context_;
  R receiver_;
};

// Sender returned by NewThreadScheduler::schedule()
class NewThreadScheduleSender {
 public:
  using ValueTypes = std::tuple<>;  // schedule() sends no values

  explicit NewThreadScheduleSender(NewThreadContext& context)
      : context_(&context) {}

  template <ReceiverOf<> R>
  auto connect(R&& receiver) && {
    return NewThreadScheduleOperationState<R>{*context_,
                                              std::forward<R>(receiver)};
  }

 private:
  NewThreadContext* context_;
};

// Scheduler that spawns a new thread for each scheduled task
//
// WARNING: This scheduler can easily exhaust system resources if used
// carelessly. Each call to schedule() spawns a new thread. Use ThreadPool
// for most workloads.
//
// REQUIRES: The NewThreadContext must outlive the scheduler and all work
// scheduled through it. The scheduler does not take ownership of the context.
//
// Example usage:
//   NewThreadContext context;
//   NewThreadScheduler scheduler{context};
//   auto work = scheduler.schedule() | then([] { return 42; });
//   auto result = syncWait(std::move(work));
//   // Threads are automatically joined when context is destroyed
//
class NewThreadScheduler {
 public:
  explicit NewThreadScheduler(NewThreadContext& context)
      : context_(&context) {}

  auto schedule() const { return NewThreadScheduleSender{*context_}; }

 private:
  NewThreadContext* context_;
};

static_assert(Scheduler<NewThreadScheduler>);

}  // namespace tempura
