#include "synchronization/hierarchical_mutex.h"

#include <mutex>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "HierarchicalMutex"_test = [] {
    HierarchicalMutex mutex1{1};
    HierarchicalMutex mutex2{2};

    // Test locking and unlocking at different levels
    mutex1.lock();
    mutex2.lock();  // Should succeed, as level 2 > level 1
    mutex2.unlock();
    mutex1.unlock();

    // Test trying to lock at a lower level
    bool success = mutex1.try_lock();  // Should succeed
    assert(success);
    // mutex2.try_lock();        // Should fail, as level 1 < level 2
  };
}
