#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>

namespace tempura {

// TimerQueue provides time-based task scheduling
//
// Tasks can be scheduled to execute at a specific time point or after a
// specific duration. Multiple threads can concurrently schedule tasks, which
// are then executed serially by a single worker thread at their scheduled
// times.
//
// The TimerQueue does not own the worker thread; instead, it provides a run()
// method that the worker thread calls to process tasks.
//
// Shutdown semantics:
// - stop() prevents new tasks from being scheduled
// - run() drains all pending tasks (executing them at their scheduled times)
// - Tasks scheduled before stop() completes are guaranteed to execute
//
// Example usage:
//   TimerQueue queue;
//   std::thread worker([&queue] { queue.run(); });
//
//   queue.scheduleAfter(std::chrono::seconds(1), [] {
//     std::cout << "Executed after 1 second\n";
//   });
//
//   queue.stop();
//   worker.join();
//
class TimerQueue {
 public:
  using Task = std::move_only_function<void()>;
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  TimerQueue() = default;

  // Disallow copy and move (timer queues are synchronization points)
  TimerQueue(const TimerQueue&) = delete;
  TimerQueue(TimerQueue&&) = delete;
  auto operator=(const TimerQueue&) -> TimerQueue& = delete;
  auto operator=(TimerQueue&&) -> TimerQueue& = delete;

  // Signal the timer queue to stop
  // Prevents new tasks from being scheduled and signals the worker to exit
  // run() will drain all previously-scheduled tasks before returning
  void stop() {
    std::scoped_lock lock{mutex_};
    stopped_ = true;
    cond_var_.notify_one();
  }

 private:
  // Forward declarations for friend functions and classes
  template <typename Rep, typename Period>
  friend auto scheduleAfter(TimerQueue& queue,
                            std::chrono::duration<Rep, Period> delay);
  friend auto scheduleAt(TimerQueue& queue, TimePoint when);
  template <typename R>
  friend class TimerScheduleOperationState;

  // Schedule a task to execute at a specific time point
  // Returns false if the queue has been stopped (rejects the task)
  auto scheduleAt(TimePoint when, Task task) -> bool {
    std::scoped_lock lock{mutex_};
    if (stopped_) {
      return false;  // Reject new tasks after stop
    }
    queue_.push({when, std::move(task)});
    cond_var_.notify_one();
    return true;
  }

  // Schedule a task to execute after a specific duration from now
  // Returns false if the queue has been stopped (rejects the task)
  template <typename Rep, typename Period>
  auto scheduleAfter(std::chrono::duration<Rep, Period> delay, Task task)
      -> bool {
    return scheduleAt(Clock::now() + delay, std::move(task));
  }

 public:
  // Run the timer queue (blocking call for worker thread)
  // Processes all tasks from the queue at their scheduled times until stop()
  // is called. Drains all pending tasks before returning.
  // PRECONDITION:
  //   - Must not be called concurrently from multiple threads
  void run() {
    while (true) {
      Task task;
      {
        std::unique_lock lock{mutex_};

        // Wait until we have a task or are stopped
        cond_var_.wait(lock, [this] { return stopped_ || !queue_.empty(); });

        // Exit only when stopped AND queue is fully drained
        if (stopped_ && queue_.empty()) {
          break;
        }

        // If queue is empty but not stopped, continue waiting
        if (queue_.empty()) {
          continue;
        }

        // Get the next scheduled task
        auto when = queue_.top().when;
        auto now = Clock::now();

        // If it's not time yet, wait until the scheduled time or until
        // a new task arrives (which might have an earlier time)
        if (when > now) {
          cond_var_.wait_until(lock, when,
                               [this, when] { return stopped_ || (!queue_.empty() && queue_.top().when < when); });

          // After waiting, check conditions again
          if (stopped_ && queue_.empty()) {
            break;
          }
          if (queue_.empty()) {
            continue;
          }

          // Check if we were woken up by a new earlier task
          if (queue_.top().when != when) {
            continue;  // Re-evaluate with the new top element
          }

          // Check if it's time yet
          now = Clock::now();
          if (queue_.top().when > now) {
            continue;  // Still not time, wait again
          }
        }

        // Time to execute the task
        // Note: top() returns const&, but we need to move the task out.
        // This is safe because we immediately pop() the element.
        task = std::move(const_cast<Entry&>(queue_.top()).task);
        queue_.pop();
      }

      task();
    }
  }

 private:
  struct Entry {
    TimePoint when;
    Task task;

    // Priority queue orders by largest first, so we reverse the comparison
    // to get earliest time first
    auto operator<(const Entry& other) const -> bool {
      return when > other.when;
    }
  };

  std::priority_queue<Entry> queue_;
  std::mutex mutex_;
  std::condition_variable cond_var_;
  bool stopped_{false};
};

}  // namespace tempura
