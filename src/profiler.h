#pragma once

#include <chrono>
#include <deque>
#include <iostream>
#include <source_location>

namespace tempura {

using namespace std::chrono_literals;

// Basic single-threaded profiler.
//
// Supports Scoped RAII tracking with the TEMPURA_TRACE() macro.
//
// This profiler is not highly precise as it allocates new anchor points as
// needed. However, it is sufficiently accurate for most tasks. Allocating a new
// anchor may add a few nanoseconds to a trace.

// Concept:
//
// The profiler maintains a "current tracing anchor" that accrues "exclusive"
// time.
//
// When a new tracing anchor is encountered:
//   - Store a pointer to the previous anchor
//   - Add "exclusive" time to the previous anchor
//   - Start accruing "exclusive" time for the new anchor
//
// When an anchor goes out of scope:
//   - Add "exclusive" time to the parent anchor
//   - Log the total time since construction as "inclusive" time
//   - Set the current anchor to the parent anchor
//
// If the parent anchor is the same as the current anchor, do nothing.
// Just log a hit on construction.

class Profiler {
 public:
  // Ends profiling and prints statistics.
  static auto endAndPrintStats() -> void;

  Profiler() {
    Anchor& anchor = anchors_.emplace_back(std::source_location::current());
    anchor.hits = 1;
    global_current_ = &anchor;
    global_start_ = std::chrono::high_resolution_clock::now();
    atexit(endAndPrintStats);
  }

  // Information about an anchor point, i.e., TEMPURA_TRACE();
  struct Anchor {
    explicit Anchor(std::source_location location) : location(location) {}
    std::source_location location;
    std::chrono::nanoseconds inclusive = 0ns;
    std::chrono::nanoseconds exclusive = 0ns;
    uint64_t hits = 0;
  };

  // RAII class for tracing a block of code.
  class Tracer {
   public:
    // Starts the timer and sets the global current anchor.
    [[nodiscard]] explicit Tracer(Anchor& anchor);

    // Stops the timer and sets the global current anchor to the parent.
    ~Tracer();

   private:
    Anchor& anchor_;
    Anchor& parent_anchor_;

    // Cache the previous inclusive time to avoid double counting during
    // recursive calls.
    std::chrono::nanoseconds previous_inclusive_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
  };

  // Creates a new anchor point.
  static auto getNewAnchor(std::source_location location =
                               std::source_location::current()) -> Anchor& {
    Anchor& anchor = instance_.anchors_.emplace_back(location);
    return anchor;
  }

 private:
  static Profiler& instance_;
  std::deque<Anchor> anchors_;

  // The anchor currently being traced.
  Anchor* global_current_;
  std::chrono::time_point<std::chrono::high_resolution_clock> global_start_;
};
inline Profiler& Profiler::instance_{*(new Profiler{})};

#define TEMPURA_TRACE()                                        \
  static auto& __anchor = ::tempura::Profiler::getNewAnchor(); \
  auto __tracer = ::tempura::Profiler::Tracer(__anchor);

}  // namespace tempura
