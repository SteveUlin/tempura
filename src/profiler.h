#pragma once

#include <chrono>
#include <deque>

namespace tempura {

using namespace std::chrono_literals;

// Basic single-threaded profiler.
//
// Supports Scoped RAII tracking:
// TEMPURA_TRACE("Some Label");
//
// Not super high fidelity as the Profiler allocates new anchor points as
// needed, but this is more than accurate enough for most tasks. Allocating
// a new anchor may add a few nanoseconds to a trace.

class Profiler {
 public:
  // Information about an anchor point. i.e. TEMPURA_TRACE("Some Label");
  struct Anchor {
    const char* label;
    std::chrono::nanoseconds inclusive = 0ns;
    std::chrono::nanoseconds exclusive = 0ns;
    uint64_t hits = 0;
  };

  // RAII class for tracing a block of code.
  class Tracer {
   public:
    // Automatically starts the timer and sets the global current anchor.
    explicit Tracer(Anchor& anchor);

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

  static auto now()
      -> std::chrono::time_point<std::chrono::high_resolution_clock>;

  static auto beginTracing() -> void;

  static auto getNewAnchor(const char* label) -> Anchor&;

  static auto endAndPrintStats() -> void;

 private:
  static Profiler& instance_;
  std::deque<Anchor> anchors_;

  // The anchor that we are currently tracing.
  Anchor* global_current_;
  std::chrono::time_point<std::chrono::high_resolution_clock> global_start_;
};

#define TEMPURA_TRACE(label)                                           \
  static auto __kLabel = label;                                        \
  static auto& __anchor = ::tempura::Profiler::getNewAnchor(__kLabel); \
  auto __tracer = ::tempura::Profiler::Tracer(__anchor);

}  // namespace tempura
