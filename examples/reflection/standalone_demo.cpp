// Standalone C++26 Reflection Demo
//
// This example compiles without standard library dependencies.
// Compile with: clang++ -std=c++26 -freflection standalone_demo.cpp
//
// Bloomberg's clang-p2996 uses ^^ as the reflection operator.

// The info type - opaque handle to compile-time metadata
using info = decltype(^^void);

// ============================================================================
// Example types to reflect upon
// ============================================================================

struct Point {
  int x;
  int y;
  int z;
};

enum class Color { Red, Green, Blue };

namespace math {
struct Vector3 { float x, y, z; };
}

// ============================================================================
// Basic reflection - get info about types
// ============================================================================

constexpr info point_info = ^^Point;
constexpr info color_info = ^^Color;
constexpr info int_info = ^^int;
constexpr info vector_info = ^^math::Vector3;

// ============================================================================
// Template using reflection
// ============================================================================

template <typename T>
consteval info reflect_type() {
  return ^^T;
}

template <info R>
consteval bool is_same_reflected(info other) {
  return R == other;
}

// ============================================================================
// Comparing reflections
// ============================================================================

static_assert(^^int == ^^int, "Same types should compare equal");
static_assert(^^int != ^^double, "Different types should compare unequal");
static_assert(^^Point != ^^Color, "Point != Color");

// ============================================================================
// Splicing - turn reflection back into code
// ============================================================================

template <info TypeInfo>
struct Wrapper {
  [:TypeInfo:] value;  // Splice the type
};

// Wrapper<^^int> has an int member
// Wrapper<^^Point> has a Point member

// ============================================================================
// Member reflection (requires <experimental/meta> header)
// ============================================================================
//
// With the full std::meta library, you can also:
//   - Enumerate struct members with nonstatic_data_members_of()
//   - Get names with name_of()
//   - Use 'template for' for compile-time iteration
//
// See the other examples for full std::meta usage.

// ============================================================================
// Main - everything is compile-time, so main is trivial
// ============================================================================

int main() {
  // All the interesting work happens at compile time!
  // The static_asserts above verify everything works.

  // Create a wrapped int using splicing
  Wrapper<^^int> w;
  w.value = 42;

  // Create a Point using splicing
  Wrapper<^^Point> wp;
  wp.value = {1, 2, 3};

  return 0;
}
