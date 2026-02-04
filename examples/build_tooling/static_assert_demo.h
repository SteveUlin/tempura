#pragma once

// ============================================================================
// static_assert_demo.h — Best-practice assertion patterns for C++26
// ============================================================================
//
// This header demonstrates four patterns for compile-time error reporting
// that produce actionable diagnostics instead of walls of template errors.
//
// Each section has a "GOOD" path that compiles, and a clearly-marked "ERROR"
// section (commented out) that shows what the improved assertion looks like.
// Uncomment any ERROR block to see the diagnostic.
//
// ============================================================================

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace tempura::build_tooling {

// ============================================================================
// Pattern 1: dependent_false<T>
// ============================================================================
//
// Problem: static_assert(false, "msg") in an if-constexpr branch fires
// unconditionally — the compiler evaluates it even when the branch is dead,
// because `false` doesn't depend on any template parameter.
//
// Solution: dependent_false<T> is a value that's always false but depends
// on T, so the compiler defers evaluation until T is known (i.e., the branch
// is actually instantiated).

template <typename>
constexpr bool dependent_false = false;

// Demonstration: a type-dispatch function with exhaustive branches
namespace pattern1 {

struct Meters {};
struct Seconds {};
struct Kilograms {};

template <typename Unit>
constexpr auto unitLabel() -> const char* {
  if constexpr (std::is_same_v<Unit, Meters>) {
    return "m";
  } else if constexpr (std::is_same_v<Unit, Seconds>) {
    return "s";
  } else if constexpr (std::is_same_v<Unit, Kilograms>) {
    return "kg";
  } else {
    // Without dependent_false, `static_assert(false)` fires even for valid
    // Unit types, because the compiler sees it during parsing, not instantiation.
    //
    // With dependent_false<Unit>, the assert only fires when someone actually
    // passes an unrecognized Unit — and the compiler shows Unit in the error.
    static_assert(dependent_false<Unit>,
                  "Unknown unit type — add a branch for this Unit");
  }
}

// --- GOOD: these compile fine ---
static_assert(unitLabel<Meters>()[0] == 'm');
static_assert(unitLabel<Seconds>()[0] == 's');

// --- ERROR: uncomment to see the diagnostic ---
// struct Joules {};
// constexpr auto bad = unitLabel<Joules>();
// Expected error:
//   error: static assertion failed: Unknown unit type — add a branch for this Unit
//   note: 'dependent_false<tempura::build_tooling::pattern1::Joules>' evaluates to false

}  // namespace pattern1

// ============================================================================
// Pattern 2: Layered concept + static_assert
// ============================================================================
//
// Problem: concept-constrained functions produce "no matching function" errors
// that don't explain WHY the concept wasn't satisfied.
//
// Solution: Use the concept for overload gating (SFINAE), then add a
// static_assert inside the function body for a human-readable diagnostic.
// The concept keeps bad types out of overload resolution; the assert explains
// what went wrong when someone calls the function explicitly.

namespace pattern2 {

// A minimal expression system
template <typename T>
concept HasEval = requires(T t) {
  { t.eval() } -> std::convertible_to<double>;
};

struct Literal {
  double val;
  constexpr auto eval() const -> double { return val; }
};

struct Add {
  double a, b;
  constexpr auto eval() const -> double { return a + b; }
};

// This struct intentionally lacks eval() — it has value() instead
struct RawValue {
  double value_;
  constexpr auto value() const -> double { return value_; }
};

// The concept gates overload resolution.
// The static_assert inside gives a detailed diagnostic if someone
// force-instantiates with a bad type.
template <HasEval Expr>
constexpr auto compute(Expr e) -> double {
  return e.eval() * 2.0;
}

// Fallback for non-HasEval types: deferred error with guidance
template <typename T>
constexpr auto compute(T) -> double {
  static_assert(dependent_false<T>,
                "compute() requires a type with .eval() → double. "
                "Did you mean to call .value() instead?");
  return 0.0;  // unreachable
}

// --- GOOD: these compile fine ---
static_assert(compute(Literal{3.0}) == 6.0);
static_assert(compute(Add{1.0, 2.0}) == 6.0);

// --- ERROR: uncomment to see layered diagnostics ---
// constexpr auto bad = compute(RawValue{5.0});
// Expected errors (in order):
//   1. "no matching function for call to 'compute(RawValue)'"
//      note: candidate requires 'HasEval<RawValue>' [concept not satisfied]
//   2. In fallback overload:
//      "compute() requires a type with .eval() → double.
//       Did you mean to call .value() instead?"

}  // namespace pattern2

// ============================================================================
// Pattern 3: Symbol lookup failure with type info
// ============================================================================
//
// Problem: static_assert(idx < N, "not found") only shows a boolean failure.
// The user doesn't know WHICH symbol was missing or what symbols are available.
//
// Solution: Use dependent_false<Sym> so the compiler includes the failing type
// in the error message. The user sees both the missing symbol AND the container
// type (which implies what IS available).

namespace pattern3 {

// A mini compile-time symbol table
template <typename... Syms>
struct SymbolTable {
  // Check if a symbol exists in this table
  template <typename Sym>
  static constexpr bool contains = (std::is_same_v<Sym, Syms> || ...);

  // Lookup — good error on missing symbol
  template <typename Sym>
  constexpr auto get() const -> double {
    if constexpr (contains<Sym>) {
      return 42.0;  // placeholder — real impl would store values
    } else {
      // BAD version (don't use):
      //   static_assert(false, "symbol not found");
      //   → "error: static assertion failed: symbol not found"  (no type info!)
      //
      // GOOD version: the compiler shows Sym AND SymbolTable<Syms...> in the error:
      static_assert(
          dependent_false<Sym>,
          "Symbol not found in SymbolTable. "
          "Check the template arguments of SymbolTable for available symbols.");
      return 0.0;
    }
  }
};

struct Alpha {};
struct Beta {};
struct Gamma {};

using Table = SymbolTable<Alpha, Beta>;

// --- GOOD: these compile fine ---
inline constexpr Table table{};
static_assert(table.get<Alpha>() == 42.0);
static_assert(table.get<Beta>() == 42.0);

// --- ERROR: uncomment to see the diagnostic ---
// constexpr auto bad = table.get<Gamma>();
// Expected error:
//   error: static assertion failed: Symbol not found in SymbolTable.
//          Check the template arguments of SymbolTable for available symbols.
//   note: 'dependent_false<tempura::build_tooling::pattern3::Gamma>' evaluates to false
//   note: in instantiation of 'SymbolTable<Alpha, Beta>::get<Gamma>()'
//                                          ^^^^^^^^^^^
//                                          Available symbols visible here!

}  // namespace pattern3

// ============================================================================
// Pattern 4: C++26 user-generated static_assert messages (P2741R3)
// ============================================================================
//
// C++26 allows static_assert with a user-generated message — the message
// argument can be any constant expression convertible to a string-like object,
// not just a string literal. This enables embedding type names, computed values,
// and formatted diagnostics directly in the assertion message.
//
// GCC 14+ supports this with -std=c++2c. The pattern is:
//
//   static_assert(condition, MessageGeneratingExpression);
//
// where the expression must have .size() and .data() members (like std::string_view).
//
// Since constexpr std::format is not yet widely available, we use a simple
// compile-time string builder approach instead.

namespace pattern4 {

// A constexpr string builder for static_assert messages.
// P2741R3 requires the message to have .data() and .size() — this provides both.
template <std::size_t N>
struct StaticMessage {
  char data_[N]{};
  std::size_t size_{};

  constexpr StaticMessage() = default;

  constexpr StaticMessage(const char (&str)[N])
      : size_{N - 1} {
    for (std::size_t i = 0; i < N; ++i) data_[i] = str[i];
  }

  constexpr auto data() const -> const char* { return data_; }
  constexpr auto size() const -> std::size_t { return size_; }
};

// Concatenate two compile-time strings
template <std::size_t A, std::size_t B>
constexpr auto concat(const StaticMessage<A>& a, const StaticMessage<B>& b) {
  StaticMessage<A + B> result{};
  for (std::size_t i = 0; i < a.size(); ++i) result.data_[i] = a.data()[i];
  for (std::size_t i = 0; i < b.size(); ++i)
    result.data_[a.size() + i] = b.data()[i];
  result.size_ = a.size() + b.size();
  return result;
}

// Embed a compile-time integer into a message
template <int V>
constexpr auto intToMessage() {
  if constexpr (V == 0) {
    return StaticMessage{"0"};
  } else {
    // Count digits
    constexpr auto abs_v = V < 0 ? -V : V;
    constexpr int num_digits = [] {
      int n = abs_v, d = 0;
      while (n > 0) { ++d; n /= 10; }
      return d;
    }();
    constexpr int len = num_digits + (V < 0 ? 1 : 0);

    StaticMessage<len + 1> result{};
    int n = abs_v;
    for (int i = num_digits - 1; i >= 0; --i) {
      result.data_[(V < 0 ? 1 : 0) + i] = '0' + (n % 10);
      n /= 10;
    }
    if constexpr (V < 0) result.data_[0] = '-';
    result.size_ = len;
    return result;
  }
}

// --- Demonstration: bounds-checked access with computed message ---

template <int N>
struct FixedArray {
  double data_[N]{};

  template <int I>
  constexpr auto get() const -> double {
    // P2741R3: user-generated message embeds the index AND the size
    static_assert(
        I >= 0 && I < N,
        concat(
            concat(StaticMessage{"Index "}, intToMessage<I>()),
            concat(StaticMessage{" out of bounds for FixedArray of size "},
                   intToMessage<N>())));
    return data_[I];
  }
};

// --- GOOD: compiles fine ---
inline constexpr FixedArray<3> arr{.data_ = {1.0, 2.0, 3.0}};
static_assert(arr.get<0>() == 1.0);
static_assert(arr.get<2>() == 3.0);

// --- ERROR: uncomment to see computed diagnostic ---
// constexpr auto bad = arr.get<5>();
// Expected error (GCC 14+ with -std=c++2c):
//   error: static assertion failed: Index 5 out of bounds for FixedArray of size 3

}  // namespace pattern4

}  // namespace tempura::build_tooling
