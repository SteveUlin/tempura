#include "guarded.h"

#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Constructor"_test = [] {
    Guarded<int> guarded{5};
    expectEq(5, guarded.get());
  };

  "Emplace args"_test = [] {
    Guarded<std::vector<int>> guarded{1, 2, 3, 4, 5};
    expectEq(5UL, guarded.get().size());
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

  "Arrow Operator"_test = [] {
    Guarded<std::vector<int>> guarded{1, 2, 3, 4, 5};
    expectEq(5UL, guarded.get().size());
  };

  "RAII Locks"_test = [] {
    Guarded<int> guarded{0};
    {
      GuardedHandle<int> handle = guarded.acquire();
      expectEq(guarded.try_lock(), false);
    }
    expectEq(guarded.try_lock(), true);
  };

  "withLock Locks"_test = [] {
    Guarded<int> guarded{0};
    guarded.withLock([&guarded](int&) {
      expectEq(guarded.try_lock(), false);
    });
    expectEq(guarded.try_lock(), true);
  };

  "const withLock Locks"_test = [] {
    const Guarded<int> guarded{0};
    guarded.withLock([&guarded](const int&) {
      expectEq(guarded.try_lock(), false);
    });
    expectEq(guarded.try_lock(), true);
  };

  "lock multiple"_test = [] {
    Guarded<int> a{5};
    Guarded<int> b{6};
    {
      auto [handle_a, handle_b] = acquire(a, b);
      expectEq(a.try_lock(), false);
      expectEq(b.try_lock(), false);
    }
    expectEq(a.try_lock(), true);
    expectEq(b.try_lock(), true);
  };

  return 0;
}
