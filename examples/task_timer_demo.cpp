// Demonstration of repeating timer-based tasks with updating terminal output

#include "task/task.h"

#include <chrono>
#include <iostream>
#include <print>

using namespace tempura;
using namespace std::chrono_literals;

// Spinner animation frames
constexpr std::array<std::string_view, 8> kSpinner = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧"
};

// Progress bar helper
auto progressBar(int current, int total, int width = 30) -> std::string {
  float progress = static_cast<float>(current) / static_cast<float>(total);
  int filled = static_cast<int>(progress * static_cast<float>(width));

  std::string bar;
  bar += "│";
  for (int i = 0; i < width; ++i) {
    if (i < filled) {
      bar += "█";
    } else if (i == filled) {
      bar += "▓";
    } else {
      bar += "░";
    }
  }
  bar += "│";
  return bar;
}

auto main() -> int {
  std::println("╔════════════════════════════════════════╗");
  std::println("║     Task Library Timer Demo            ║");
  std::println("╚════════════════════════════════════════╝\n");

  // Create timer queue and run it on a background thread
  TimerQueue timer_queue;
  std::thread timer_thread([&timer_queue] { timer_queue.run(); });

  TimerScheduler timer{timer_queue};

  // =========================================================================
  // Example 1: Countdown using repeatEffectUntil
  // =========================================================================
  std::println("Example 1: Countdown Timer (using repeatEffectUntil)");
  std::println("─────────────────────────────────────────────────────");

  {
    int countdown = 5;
    // Use delay() instead of scheduleAfter() - delay computes time at start(),
    // so each repeat iteration waits the full 500ms from when it starts
    auto countdown_work = timer.delay(500ms)
      | then([&countdown] {
          if (countdown > 0) {
            std::print("\r  Launching in {} seconds... {}  ",
                       countdown, kSpinner[countdown % 8]);
            std::cout.flush();
          } else {
            std::println("\r  🚀 Liftoff!                      ");
          }
          return --countdown;
        })
      | repeatEffectUntil([&countdown] { return countdown < 0; });

    syncWait(std::move(countdown_work));
  }

  std::println("");

  // =========================================================================
  // Example 2: Progress bar using repeatN
  // =========================================================================
  std::println("Example 2: Progress Bar (using repeatN)");
  std::println("────────────────────────────────────────");

  {
    constexpr int kTotalSteps = 20;
    int step = 0;

    auto progress_work = timer.delay(150ms)
      | then([&step, kTotalSteps] {
          int percent = (step * 100) / kTotalSteps;
          std::print("\r  Processing: {} {:3}%  ",
                     progressBar(step, kTotalSteps), percent);
          std::cout.flush();
          return ++step;
        })
      | repeatN(kTotalSteps + 1);  // 0 to 20 inclusive

    syncWait(std::move(progress_work));
  }

  std::println("\n  ✓ Complete!\n");

  // =========================================================================
  // Example 3: Spinner with status messages using nested repeatN
  // =========================================================================
  std::println("Example 3: Status Updates (using repeatN)");
  std::println("──────────────────────────────────────────");

  {
    std::array<std::string_view, 5> statuses = {
        "Initializing system...",
        "Loading configuration...",
        "Connecting to server...",
        "Synchronizing data...",
        "Finalizing setup..."
    };

    size_t status_idx = 0;
    int frame = 0;

    // Outer repeat for each status, inner logic handles 6 frames per status
    auto status_work = timer.delay(100ms)
      | then([&] {
          std::print("\r  {} {}   ", kSpinner[frame], statuses[status_idx]);
          std::cout.flush();
          ++frame;
          if (frame >= 6) {
            frame = 0;
            ++status_idx;
          }
          return frame;
        })
      | repeatN(statuses.size() * 6);  // 5 statuses × 6 frames

    syncWait(std::move(status_work));
  }

  std::println("\r  ✓ All systems ready!            \n");

  // =========================================================================
  // Example 4: Parallel timers with whenAll
  // =========================================================================
  std::println("Example 4: Parallel Operations");
  std::println("───────────────────────────────");

  std::println("  Starting 3 parallel tasks...");

  auto task_a = timer.scheduleAfter(800ms) | then([] {
    std::println("    [A] Database query complete");
    return "data";
  });

  auto task_b = timer.scheduleAfter(600ms) | then([] {
    std::println("    [B] API call complete");
    return 42;
  });

  auto task_c = timer.scheduleAfter(1000ms) | then([] {
    std::println("    [C] File I/O complete");
    return 3.14;
  });

  auto parallel_work = whenAll(std::move(task_a), std::move(task_b), std::move(task_c))
    | then([](std::tuple<const char*> a, std::tuple<int> b, std::tuple<double> c) {
        std::println("  All tasks finished!");
        std::println("    Results: A=\"{}\", B={}, C={:.2f}",
                     std::get<0>(a), std::get<0>(b), std::get<0>(c));
        return true;
      });

  syncWait(std::move(parallel_work));
  std::println("");

  // =========================================================================
  // Example 5: Sensor readings using repeatN
  // =========================================================================
  std::println("Example 5: Simulated Sensor Readings (using repeatN)");
  std::println("─────────────────────────────────────────────────────");

  {
    int reading_count = 0;
    double accumulated = 0.0;

    auto sensor_work = timer.delay(300ms)
      | then([&] {
          // Simulate a sensor reading with some variation
          double value = 23.5 + static_cast<double>(reading_count % 3) * 0.3
                       - 0.15 * (reading_count % 2);
          ++reading_count;
          accumulated += value;

          double avg = accumulated / static_cast<double>(reading_count);
          std::print("\r  Sensor #{:2}: {:.2f}°C  (avg: {:.2f}°C)  ",
                     reading_count, value, avg);
          std::cout.flush();
          return value;
        })
      | repeatN(10);

    syncWait(std::move(sensor_work));
  }

  std::println("\n  ✓ Sensor monitoring complete\n");

  // =========================================================================
  // Cleanup
  // =========================================================================
  std::println("╔════════════════════════════════════════╗");
  std::println("║          Demo Complete                 ║");
  std::println("╚════════════════════════════════════════╝");

  timer_queue.stop();
  timer_thread.join();

  return 0;
}
