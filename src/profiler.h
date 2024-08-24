#pragma once

#include <chrono>
#include <deque>
#include <iostream>
#include <source_location>

namespace tempura {

using namespace std::chrono_literals;

// Basic single-threaded profiler.
//
// Supports Scoped RAII tracking:
// TEMPURA_TRACE();
//
// Not super high fidelity as the Profiler allocates new anchor points as
// needed, but this is more than accurate enough for most tasks. Allocating
// a new anchor may add a few nanoseconds to a trace.

// Idea:
//
// We always have a "current tracing anchor" that is accruing "exclusive" time.
//
// When we hit another tracing anchor:
//   - store a pointer to the previous anchor
//   - add "exclusive" time to the previous anchor
//   - start accruing "exclusive" time for the new anchor
//
// When an anchor goes out of scope:
//   - add "exclusive" time to the parent anchor
//   - log total time since construction, "inclusive" time
//   - set the current anchor to the parent anchor
//
// If the parent anchor is ever the same as the current anchor, don't do
// anything. Just log a hit on construction.

class Profiler {
 public:
  Profiler() {
    Anchor& anchor =
        anchors_.emplace_back(std::source_location::current());
    anchor.hits = 1;
    global_current_ = &anchor;
    global_start_ = std::chrono::high_resolution_clock::now();
    atexit(endAndPrintStats);
  }
  // Information about an anchor point. i.e. TEMPURA_TRACE();
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
    // Automatically starts the timer and sets the global current anchor.
    [[nodiscard]] explicit Tracer(Anchor& anchor);

    // Automatically stops the timer and sets the global current anchor to the
    // parent.
    ~Tracer();

   private:
    Anchor& anchor_;
    Anchor& parent_anchor_;

    // Cache the previous inclusive time so we avoid double counting when
    // handling recursive calls.
    std::chrono::nanoseconds previous_inclusive_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
  };

  static auto getNewAnchor(std::source_location location =
                               std::source_location::current()) -> Anchor& {
    Anchor& anchor = instance_.anchors_.emplace_back(location);
    return anchor;
  }

  static auto endAndPrintStats() -> void;

 private:
  static Profiler& instance_;
  std::deque<Anchor> anchors_;

  // The anchor that we are currently tracing.
  Anchor* global_current_;
  std::chrono::time_point<std::chrono::high_resolution_clock> global_start_;
};
inline Profiler& Profiler::instance_{*(new Profiler{})};

#define TEMPURA_TRACE()                                        \
  static auto& __anchor = ::tempura::Profiler::getNewAnchor(); \
  auto __tracer = ::tempura::Profiler::Tracer(__anchor);

}  // namespace tempura
