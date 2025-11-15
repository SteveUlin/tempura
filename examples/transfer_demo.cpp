// Demonstration of transfer() - getting data back to the main thread

#include "task/task.h"

#include <chrono>
#include <iostream>
#include <print>
#include <thread>

using namespace tempura;

auto getThreadId() -> std::string {
  auto id = std::this_thread::get_id();
  std::ostringstream oss;
  oss << id;
  return oss.str();
}

auto main() -> int {
  std::println("=== Transfer Demo ===\n");
  std::println("Main thread ID: {}", getThreadId());

  // =========================================================================
  // Example 1: Simple transfer - background work, result on main thread
  // =========================================================================
  std::println("\n--- Example 1: Simple Transfer ---");
  {
    SingleThreadScheduler background;
    InlineScheduler main_thread;

    auto work_on_background = background.schedule() | then([&] {
      std::println("  Computing on thread: {}", getThreadId());
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return 42 * 42;
    });

    // Transfer result back to main thread
    auto sender = transfer(std::move(work_on_background), main_thread) |
                  then([&](int result) {
                    std::println("  Processing result on thread: {}", getThreadId());
                    return result;
                  });

    auto result = syncWait(std::move(sender));
    std::println("  Final result (main thread): {}", std::get<0>(*result));
  }

  // =========================================================================
  // Example 2: Multiple transfers - ping-pong between threads
  // =========================================================================
  std::println("\n--- Example 2: Thread Ping-Pong ---");
  {
    SingleThreadScheduler worker1;
    SingleThreadScheduler worker2;
    InlineScheduler main_thread;

    auto step1 = just(10) | then([&](int x) {
      std::println("  Main thread: {} -> starting work", x);
      return x;
    });

    // Transfer to worker1
    auto step2 = transfer(std::move(step1), worker1) | then([&](int x) {
      std::println("  Worker1 thread: {} -> doubling", x);
      return x * 2;
    });

    // Transfer to worker2
    auto step3 = transfer(std::move(step2), worker2) | then([&](int x) {
      std::println("  Worker2 thread: {} -> adding 5", x);
      return x + 5;
    });

    // Transfer back to main
    auto sender = transfer(std::move(step3), main_thread) | then([&](int x) {
      std::println("  Main thread: {} -> done!", x);
      return x;
    });

    auto result = syncWait(std::move(sender));
    std::println("  Final: {}", std::get<0>(*result));
  }

  // =========================================================================
  // Example 3: Real-world pattern - parallel work then aggregate on main
  // =========================================================================
  std::println("\n--- Example 3: Parallel Computation ---");
  {
    SingleThreadScheduler worker1;
    SingleThreadScheduler worker2;
    InlineScheduler main_thread;

    // Simulate two independent computations
    auto task1 = worker1.schedule() | then([&] {
      std::println("  Task 1 computing on thread: {}", getThreadId());
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      return 100;
    });

    auto task2 = worker2.schedule() | then([&] {
      std::println("  Task 2 computing on thread: {}", getThreadId());
      std::this_thread::sleep_for(std::chrono::milliseconds(75));
      return 200;
    });

    // For now, we'll run them sequentially (whenAll not implemented yet)
    // But demonstrate the transfer pattern
    auto task1_on_main = transfer(std::move(task1), main_thread) | then([&](int result1) {
      std::println("  Got task1 result on main: {}", result1);
      return result1;
    });

    auto combined = std::move(task1_on_main) | letValue([&, task2 = std::move(task2)](int result1) mutable {
      auto task2_on_main = transfer(std::move(task2), main_thread);
      return std::move(task2_on_main) | then([&, result1](int result2) {
        std::println("  Got task2 result on main: {}", result2);
        std::println("  Combining on thread: {}", getThreadId());
        return result1 + result2;
      });
    });

    auto result = syncWait(std::move(combined));
    std::println("  Combined result: {}", std::get<0>(*result));
  }

  // =========================================================================
  // Example 4: Error handling across threads
  // =========================================================================
  std::println("\n--- Example 4: Error Handling ---");
  {
    SingleThreadScheduler worker;
    InlineScheduler main_thread;

    auto work_on_worker = worker.schedule() | then([&] {
      std::println("  Worker computing...");
      return 42;
    });

    auto sender = transfer(std::move(work_on_worker), main_thread) |
                  uponError([&](std::error_code ec) {
                    std::println("  Error handled on main thread: {}", ec.message());
                    return -1;  // Fallback value
                  }) |
                  then([&](int value) {
                    std::println("  Final value on main: {}", value);
                    return value;
                  });

    auto result = syncWait(std::move(sender));
    std::println("  Result: {}", std::get<0>(*result));
  }

  std::println("\n=== Transfer Demo Complete ===");
  std::println("\nKey Takeaways:");
  std::println("  1. transfer(sender, scheduler) moves execution to a new thread");
  std::println("  2. Data automatically flows through the transfer");
  std::println("  3. Chain multiple transfers for complex threading patterns");
  std::println("  4. Always transfer() back to main before syncWait()");

  return 0;
}
