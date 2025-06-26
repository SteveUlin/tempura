#pragma once

#include <cassert>
#include <mutex>

namespace tempura {

// HierarchicalMutex is a mutex that enforces a hierarchy of locks to prevent
// deadlocks.
//
// You may only lock mutexes with higher levels than the currently
// held mutex. This ensures that if a thread holds a mutex at level N, it cannot
// lock a mutex at level M <= N, thus preventing circular dependencies.
//
// You must release higher level mutexes before lower level ones.
//
// Holding no mutexes is considered to be at level 0.
template <typename Mutex = std::mutex>
class HierarchicalMutex {
 public:
  explicit HierarchicalMutex(unsigned int level) : level_{level} {}

  void lock() {
    assert(current_level_ < level_ &&
            "Trying to lock a hierarchical mutex at a lower level than the "
            "current level is not allowed.");
    mutex_.lock();
    previous_level_ = current_level_;
    current_level_ = level_;
  }

  void unlock() {
    assert(current_level_ == level_ &&
           "Unlocking a hierarchical mutex at a different level than the "
           "current level is not allowed.");
    mutex_.unlock();
    current_level_ = previous_level_;
  }

  auto try_lock() -> bool {
    assert(current_level_ < level_ &&
           "Trying to lock a hierarchical mutex at a lower level than the "
           "current level is not allowed.");
    if (!mutex_.try_lock()) {
      return false;
    }
    current_level_ = level_;
    return true;
  }

 private:
  unsigned int level_;
  unsigned int previous_level_ = 0;
  inline static thread_local unsigned int current_level_ = 0;

  Mutex mutex_ = {};
};

}  // namespace tempura
