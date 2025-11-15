#include "guarded.h"

#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Constructor"_test = [] {
    Guarded<int> guarded{5};
    expectEq(5, *guarded.acquire());
  };

  "Emplace args"_test = [] {
    Guarded<std::vector<int>> guarded{1, 2, 3, 4, 5};
    expectEq(5UL, guarded.acquire()->size());
  };

  "Handle Dereference"_test = [] {
    Guarded<int> guarded{5};
    GuardedHandle<int> handle = guarded.acquire();
    expectEq(5, *handle);
  };

  "const Handle Dereference"_test = [] {
    const Guarded<int> guarded{5};
    GuardedHandle<const int> handle = guarded.acquire();
    expectEq(5, *handle);
  };

  "RAII Locks"_test = [] {
    Guarded<int> guarded{0};
    {
      auto handle1 = guarded.acquire();
      auto handle2 = guarded.acquire(std::try_to_lock);
      expectEq(handle2.owns_lock(), false);  // Can't acquire while held
    }
    auto handle3 = guarded.acquire(std::try_to_lock);
    expectEq(handle3.owns_lock(), true);  // Can acquire after release
  };

  "withLock Locks"_test = [] {
    Guarded<int> guarded{5};
    guarded.withLock([&guarded](int& value) {
      auto handle = guarded.acquire(std::try_to_lock);
      expectEq(handle.owns_lock(), false);  // Can't acquire while held
      expectEq(value, 5);
    });
    auto handle = guarded.acquire(std::try_to_lock);
    expectEq(handle.owns_lock(), true);  // Can acquire after release
  };

  "const withLock Locks"_test = [] {
    const Guarded<int> guarded{5};
    guarded.withLock([&guarded](const int& value) {
      auto handle = guarded.acquire(std::try_to_lock);
      expectEq(handle.owns_lock(), false);  // Can't acquire while held
      expectEq(value, 5);
    });
    auto handle = guarded.acquire(std::try_to_lock);
    expectEq(handle.owns_lock(), true);  // Can acquire after release
  };

  return 0;
}
