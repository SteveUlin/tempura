// Tests for scheme/fold_unique.h - constexpr catamorphism with DAG identity tracking
#include "symbolic4/core.h"
#include "symbolic4/let.h"
#include "symbolic4/operators.h"
#include "symbolic4/scheme/fold_unique.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// ============================================================================
// Test Interpreter: Count unique nodes (constexpr)
// ============================================================================
// Counts how many times each node type is visited. Shared LetNode
// subexpressions are only counted once (identity returns 0).

struct CountNodes {
  using result_type = int;

  template <typename T, typename Visited>
  static constexpr auto terminal(T, Visited) -> int {
    return 1;
  }

  template <typename Visited, typename Op, typename... Rs>
  static constexpr auto combine(Visited, Op, Rs... child_counts) -> int {
    if constexpr (sizeof...(Rs) == 0) {
      return 1;
    } else {
      return 1 + (child_counts + ...);
    }
  }

  // identity() returns 0 - don't count revisits
  template <typename Visited>
  static constexpr auto identity(Visited) -> int {
    return 0;
  }
};

// ============================================================================
// Test Interpreter: Sum constants (constexpr)
// ============================================================================

struct SumConstants {
  using result_type = int;

  template <typename T, typename Visited>
  static constexpr auto terminal(T, Visited) -> int {
    if constexpr (is_constant_v<T>) {
      return static_cast<int>(T::value);
    } else {
      return 0;
    }
  }

  template <typename Visited, typename Op, typename... Rs>
  static constexpr auto combine(Visited, Op, Rs... child_sums) -> int {
    if constexpr (sizeof...(Rs) == 0) {
      return 0;
    } else {
      return (child_sums + ...);
    }
  }

  template <typename Visited>
  static constexpr auto identity(Visited) -> int {
    return 0;  // Don't double-count shared subexpressions
  }
};

auto main() -> int {
  Symbol<struct X> x;

  "foldUnique on terminal"_test = [&] {
    constexpr auto result = foldUniqueValue<CountNodes>(Symbol<struct X>{});
    static_assert(result == 1);
    expectEq(result, 1);
  };

  "foldUnique on simple expression"_test = [&] {
    auto expr = x + Constant<5>{};
    constexpr auto result =
        foldUniqueValue<CountNodes>(Symbol<struct X>{} + Constant<5>{});
    // root (+) + 2 children = 3
    static_assert(result == 3);
    expectEq(result, 3);
  };

  "foldUnique on LetNode - single reference"_test = [] {
    // let t = x + 1 in t (single reference, no sharing)
    auto t = let<struct T>(Symbol<struct X>{} + one);
    constexpr auto result =
        foldUniqueValue<CountNodes>(let<struct T1>(Symbol<struct X>{} + one));
    // Visits: x, one, (+) = 3
    static_assert(result == 3);
    expectEq(result, 3);
  };

  "foldUnique on shared LetNode - computed once"_test = [] {
    // let t = x * x in t + t
    // Without dedup: would visit x*x twice = 6 nodes
    // With dedup: x*x visited once, second t returns 0
    auto t = let<struct T2>(Symbol<struct X>{} * Symbol<struct X>{});
    auto expr = t + t;

    constexpr auto check = [] {
      auto t = let<struct T2>(Symbol<struct X>{} * Symbol<struct X>{});
      return foldUniqueValue<CountNodes>(t + t);
    }();

    // Structure: (+) with two LetNode<T2> children
    // First LetNode<T2>: visits x, x, (*) = 3 nodes
    // Second LetNode<T2>: already visited, returns identity (0)
    // Root (+): 1 + 3 + 0 = 4
    static_assert(check == 4);
    expectEq(check, 4);
  };

  "foldUnique on triple shared reference"_test = [] {
    constexpr auto check = [] {
      auto t = let<struct T3>(Symbol<struct X>{} * Symbol<struct X>{});
      return foldUniqueValue<CountNodes>(t + t + t);  // Parses as (t + t) + t
    }();

    // First t: x, x, (*) = 3 nodes
    // Second t: identity = 0
    // Third t: identity = 0
    // Two (+) operators = 2 nodes
    // Total: 3 + 0 + 0 + 1 + 1 = 5
    static_assert(check == 5);
    expectEq(check, 5);
  };

  "foldUnique sums constants correctly"_test = [] {
    constexpr auto check = [] {
      // let t = 3 + 5 in t + t
      auto t = let<struct T4>(Constant<3>{} + Constant<5>{});
      return foldUniqueValue<SumConstants>(t + t);
    }();

    // First t: 3 + 5 = 8
    // Second t: identity = 0 (don't double count)
    // Root +: 8 + 0 = 8
    static_assert(check == 8);
    expectEq(check, 8);
  };

  "foldUnique with nested LetNodes"_test = [] {
    constexpr auto check = [] {
      // let a = x + 1 in let b = a * a in b + b
      auto a = let<struct A>(Symbol<struct X>{} + one);
      auto b = let<struct B>(a * a);
      return foldUniqueValue<CountNodes>(b + b);
    }();

    // First b: evaluates a * a
    //   First a: x + 1 = 3 nodes (x, 1, +)
    //   Second a (in a*a): identity = 0
    //   (*) = 1 node
    // Second b: identity = 0
    // Root (+) = 1 node
    // Total: 3 + 0 + 1 + 0 + 1 = 5
    static_assert(check == 5);
    expectEq(check, 5);
  };

  "foldUnique respects multiple independent LetNodes"_test = [] {
    constexpr auto check = [] {
      auto a = let<struct A2>(Symbol<struct Y>{} + one);
      auto b = let<struct B2>(Symbol<struct Y>{} + two);
      return foldUniqueValue<CountNodes>(a + b + a + b);
    }();

    // First a: y, 1, (+) = 3
    // First b: y, 2, (+) = 3
    // Second a: identity = 0
    // Second b: identity = 0
    // Three (+) = 3
    // Total: 9
    static_assert(check == 9);
    expectEq(check, 9);
  };

  "foldUnique with TEMPURA_LET macro"_test = [&] {
    // Note: TEMPURA_LET creates unique Id per expansion, can't use in constexpr
    // lambda easily, so test at runtime
    TEMPURA_LET(squared, x * x);
    auto expr = squared + squared;
    auto result = foldUniqueValue<CountNodes>(expr);
    // Same as "shared LetNode" test = 4
    expectEq(result, 4);
  };

  "IdSet compile-time membership"_test = [] {
    using Empty = IdSet<>;
    using WithA = id_set_insert_t<struct A, Empty>;
    using WithAB = id_set_insert_t<struct B, WithA>;

    static_assert(!id_set_contains_v<struct A, Empty>);
    static_assert(id_set_contains_v<struct A, WithA>);
    static_assert(!id_set_contains_v<struct B, WithA>);
    static_assert(id_set_contains_v<struct A, WithAB>);
    static_assert(id_set_contains_v<struct B, WithAB>);
    static_assert(!id_set_contains_v<struct C, WithAB>);

    // Inserting existing element is no-op
    using StillWithAB = id_set_insert_t<struct A, WithAB>;
    static_assert(std::is_same_v<WithAB, StillWithAB>);
  };

  "foldUnique returns updated visited set"_test = [] {
    constexpr auto check = [] {
      auto t = let<struct T5>(Symbol<struct X>{} + one);
      auto [result, visited] = foldUnique<CountNodes>(t + t, IdSet<>{});
      // After traversal, T5 should be in visited set
      return id_set_contains_v<struct T5, decltype(visited)>;
    }();
    static_assert(check);
  };

  return TestRegistry::result();
}
