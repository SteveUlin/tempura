#include "meta/manual_lifetime.h"

#include <string>

#include "unit.h"

using namespace tempura;

// Test type: non-moveable with destruction tracking
struct NonMoveable {
  explicit NonMoveable(int v) : value(v) { construct_count++; }

  ~NonMoveable() { destruct_count++; }

  NonMoveable(const NonMoveable&) = delete;
  NonMoveable(NonMoveable&&) = delete;
  auto operator=(const NonMoveable&) -> NonMoveable& = delete;
  auto operator=(NonMoveable&&) -> NonMoveable& = delete;

  int value;
  static inline int construct_count = 0;
  static inline int destruct_count = 0;

  static void reset() {
    construct_count = 0;
    destruct_count = 0;
  }
};

// Factory returning non-moveable type (via guaranteed copy elision)
auto makeNonMoveable(int x) -> NonMoveable { return NonMoveable{x}; }

auto main() -> int {
  "basic construction and access"_test = [] {
    ManualLifetime<int> storage;
    storage.construct(42);

    expectEq(storage.get(), 42);
    expectEq(*storage, 42);

    storage.destruct();
  };

  "explicit destruction (no automatic cleanup)"_test = [] {
    NonMoveable::reset();

    {
      ManualLifetime<NonMoveable> storage;
      storage.construct(99);
      expectEq(storage->value, 99);
      expectEq(NonMoveable::destruct_count, 0);

      storage.destruct();
      expectEq(NonMoveable::destruct_count, 1);
    }

    // Verify no double-destruction when ManualLifetime goes out of scope
    expectEq(NonMoveable::destruct_count, 1);
  };

  "intentional leak (ownership transfer simulation)"_test = [] {
    NonMoveable::reset();

    {
      ManualLifetime<NonMoveable> storage;
      storage.construct(123);
      // NOT calling destruct() - simulates transferring ownership elsewhere
    }

    expectEq(NonMoveable::destruct_count, 0);  // No automatic cleanup
  };

  "non-moveable type via constructWith (key use case)"_test = [] {
    NonMoveable::reset();

    ManualLifetime<NonMoveable> storage;
    storage.constructWith([] { return makeNonMoveable(42); });

    expectEq(storage->value, 42);
    expectEq(NonMoveable::construct_count, 1);

    storage.destruct();
    expectEq(NonMoveable::destruct_count, 1);
  };

  "pointer-like interface (const and mutable)"_test = [] {
    ManualLifetime<std::string> storage;
    storage.construct("hello");

    expectEq(storage->length(), 5u);
    expectEq(storage.get(), std::string{"hello"});
    expectEq(*storage, std::string{"hello"});

    const auto& const_ref = storage;
    expectEq(*const_ref, std::string{"hello"});
    expectEq(const_ref.get(), std::string{"hello"});

    storage.destruct();
  };

  return 0;
}
