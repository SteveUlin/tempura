#include "profiler.h"

#include <cassert>
#include <print>

namespace tempura {

Profiler& Profiler::instance_ = *(new Profiler());

Profiler::Tracer::Tracer(Anchor &anchor)
    : anchor_(anchor), parent_anchor_(*instance_.global_current_),
      previous_inclusive_(anchor.inclusive), start_(now()) {
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

auto Profiler::now()
    -> std::chrono::time_point<std::chrono::high_resolution_clock> {
  return std::chrono::high_resolution_clock::now();
}

auto Profiler::beginTracing() -> void {
  Anchor& anchor = instance_.anchors_.emplace_back();
  anchor.label = "Global";
  anchor.hits = 1;
  instance_.global_current_ = &anchor;
  instance_.global_start_ = now();
}

auto Profiler::getNewAnchor(const char* label) -> Anchor& {
  Anchor& anchor = instance_.anchors_.emplace_back();
  anchor.label = label;
  return anchor;
}

auto Profiler::endAndPrintStats() -> void {
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::nanoseconds elapsed = end - instance_.global_start_;
  instance_.anchors_[0].inclusive = elapsed;
  instance_.anchors_[0].exclusive += elapsed;

  for (const Anchor& anchor : instance_.anchors_) {
    {
      auto percent =
          (static_cast<double>(anchor.inclusive.count()) / elapsed.count()) * 100;
      auto avg = static_cast<double>(anchor.inclusive.count()) / anchor.hits;

      std::print("{}[{}]: {} ns, {:.2f}%, avg: {:.2f} ns",
                 anchor.label, anchor.hits,  anchor.inclusive.count(), percent, avg);
    }
    {
      auto percent =
          (static_cast<double>(anchor.exclusive.count()) / elapsed.count()) *
          100;
      auto avg = static_cast<double>(anchor.exclusive.count()) / anchor.hits;
      std::println(", w/o children: {} ns, {:.2f}%, avg: {:.2f} ns",
                   anchor.exclusive.count(), percent, avg);
    }
  }
}

}  // namespace

