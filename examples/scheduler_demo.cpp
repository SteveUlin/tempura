// Demonstration of how schedulers control execution threads

#include "task/task.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <optional>
#include <print>
#include <thread>

using namespace tempura;

// Helper to get thread ID as a string
auto getThreadId() -> std::string {
  auto id = std::this_thread::get_id();
  std::ostringstream oss;
  oss << id;
  return oss.str();
}

auto main() -> int {
  std::println("=== Scheduler Demo ===\n");
  std::println("Main thread ID: {}", getThreadId());

  // =========================================================================
  // Example 1: InlineScheduler - everything runs on the calling thread
  // =========================================================================
  std::println("\n--- InlineScheduler ---");
  {
    InlineScheduler sched;

    auto sender = sched.schedule() | then([&] {
      std::println("  Work executing on thread: {}", getThreadId());
      return 42;
    });

    auto result = syncWait(std::move(sender));
    std::println("  Result: {}", std::get<0>(*result));
  }

  // =========================================================================
  // Example 2: SingleThreadScheduler - work runs on background thread
  // =========================================================================
  std::println("\n--- SingleThreadScheduler ---");
  {
    SingleThreadScheduler sched;

    std::optional<std::tuple<int>> result;
    std::mutex result_mutex;
    std::condition_variable result_cv;
    bool done = false;

    auto sender = sched.schedule() | then([&] {
      std::println("  Work executing on thread: {}", getThreadId());
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return 99;
    });

    // Create a custom receiver that handles the async result
    struct AsyncReceiver {
      std::optional<std::tuple<int>>* result_;
      std::mutex* mutex_;
      std::condition_variable* cv_;
      bool* done_;

      void setValue(int value) noexcept {
        std::println("  Result received on thread: {}", getThreadId());
        {
          std::lock_guard lock(*mutex_);
          result_->emplace(value);
          *done_ = true;
        }
        cv_->notify_one();
      }

      void setError(std::error_code ec) noexcept {
        std::println("  Error: {}", ec.message());
        {
          std::lock_guard lock(*mutex_);
          *done_ = true;
        }
        cv_->notify_one();
      }

      void setStopped() noexcept {
        std::println("  Stopped");
        {
          std::lock_guard lock(*mutex_);
          *done_ = true;
        }
        cv_->notify_one();
      }
    };

    auto op = std::move(sender).connect(
        AsyncReceiver{&result, &result_mutex, &result_cv, &done});

    op.start();

    // Wait for result
    {
      std::unique_lock lock(result_mutex);
      result_cv.wait(lock, [&] { return done; });
    }

    std::println("  Back on main thread: {}", getThreadId());
    if (result) {
      std::println("  Result: {}", std::get<0>(*result));
    }
  }

  // =========================================================================
  // Example 3: Multiple schedulers showing thread switches
  // =========================================================================
  std::println("\n--- Chaining Work Across Threads ---");
  {
    InlineScheduler inline_sched;
    SingleThreadScheduler thread_sched;

    std::optional<std::tuple<std::string>> result;
    std::mutex result_mutex;
    std::condition_variable result_cv;
    bool done = false;

    auto sender =
        // Start on main thread
        inline_sched.schedule()
        | then([&] {
            std::println("  Step 1 on thread: {}", getThreadId());
            return 10;
          })
        // Switch to background thread
        | letValue([&](int x) {
            return thread_sched.schedule() | then([&, x] {
              std::println("  Step 2 on thread: {} (value={})", getThreadId(), x);
              std::this_thread::sleep_for(std::chrono::milliseconds(50));
              return x * 2;
            });
          })
        // Still on background thread
        | then([&](int x) {
            std::println("  Step 3 on thread: {} (value={})", getThreadId(), x);
            return std::format("Result: {}", x);
          });

    struct AsyncStringReceiver {
      std::optional<std::tuple<std::string>>* result_;
      std::mutex* mutex_;
      std::condition_variable* cv_;
      bool* done_;

      void setValue(std::string value) noexcept {
        std::println("  Final callback on thread: {}", getThreadId());
        {
          std::lock_guard lock(*mutex_);
          result_->emplace(std::move(value));
          *done_ = true;
        }
        cv_->notify_one();
      }

      void setError(std::error_code) noexcept {
        {
          std::lock_guard lock(*mutex_);
          *done_ = true;
        }
        cv_->notify_one();
      }

      void setStopped() noexcept {
        {
          std::lock_guard lock(*mutex_);
          *done_ = true;
        }
        cv_->notify_one();
      }
    };

    auto op = std::move(sender).connect(
        AsyncStringReceiver{&result, &result_mutex, &result_cv, &done});

    op.start();

    {
      std::unique_lock lock(result_mutex);
      result_cv.wait(lock, [&] { return done; });
    }

    std::println("  Back on main thread: {}", getThreadId());
    if (result) {
      std::println("  Final result: {}", std::get<0>(*result));
    }
  }

  std::println("\n=== Demo Complete ===");
  return 0;
}
