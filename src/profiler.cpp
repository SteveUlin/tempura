#include "profiler.h"

#include <cassert>
#include <format>
#include <iostream>
#include <ranges>

namespace tempura {

namespace {

auto toHumanReadable(std::chrono::nanoseconds duration) -> std::string {
  using namespace std::chrono_literals;
  if (duration < 1ms) {
    return std::format("{} ns", duration.count());
  }
  if (duration < 10s) {
    return std::format(
        "{:.2f} ms",
        std::chrono::duration<double, std::chrono::milliseconds::period>(
            duration)
            .count());
  }
  if (duration < 5min) {
    return std::format(
        "{:.2f} s",
        std::chrono::duration<double, std::chrono::seconds::period>(duration)
            .count());
  }
  if (duration < 120min) {
    return std::format(
        "{:.2f} s",
        std::chrono::duration<double, std::chrono::hours::period>(duration)
            .count());
  }
  return std::format(
      "{:.2f} h",
      std::chrono::duration<double, std::chrono::hours::period>(duration)
          .count());
}

};  // namespace

Profiler& Profiler::instance_ = *(new Profiler{});

Profiler::Tracer::Tracer(Anchor& anchor)
    : anchor_(anchor),
      parent_anchor_(*instance_.global_current_),
      previous_inclusive_(anchor.inclusive),
      start_(std::chrono::high_resolution_clock::now()) {
  instance_.global_current_ = &anchor_;
}

Profiler::Tracer::~Tracer() {
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::nanoseconds elapsed = end - start_;
  parent_anchor_.exclusive -= elapsed;
  anchor_.exclusive += elapsed;
  anchor_.inclusive = previous_inclusive_ + elapsed;
  anchor_.hits++;

  instance_.global_current_ = &parent_anchor_;
}

auto Profiler::endAndPrintStats() -> void {
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::nanoseconds elapsed = end - instance_.global_start_;
  instance_.anchors_[0].inclusive = elapsed;
  instance_.anchors_[0].exclusive += elapsed;

  for (const Anchor& anchor : instance_.anchors_ | std::views::drop(1)) {
    {
      auto percent = (static_cast<double>(anchor.inclusive.count()) /
                      static_cast<double>(elapsed.count())) *
                     100;
      auto avg = anchor.inclusive / anchor.hits;

      std::cout << std::format(
          "{}:{} {}[{}]: {} {:.2f}% avg: {}\n", anchor.location.file_name(),
          anchor.location.line(), anchor.location.function_name(), anchor.hits,
          toHumanReadable(anchor.inclusive), percent, toHumanReadable(avg));
    }
    {
      auto percent = (static_cast<double>(anchor.exclusive.count()) /
                      static_cast<double>(elapsed.count())) *
                     100;
      auto avg = anchor.exclusive / anchor.hits;
      std::cout << std::format("\tw/o children: {} ns, {:.2f}% avg: {}\n",
                               toHumanReadable(anchor.exclusive), percent,
                               toHumanReadable(avg));
    }
  }
}

}  // namespace tempura
