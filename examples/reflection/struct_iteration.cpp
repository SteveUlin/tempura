// Struct Member Iteration with C++26 Reflection
//
// Demonstrates iterating over struct members at compile time:
// - meta::nonstatic_data_members_of - get all data members
// - template for - compile-time expansion loop
// - meta::offset_of - get byte offset of a member

#include "meta.h"

#include <print>
#include <string_view>

namespace meta = std::meta;

// ============================================================================
// Example struct to reflect upon
// ============================================================================

struct Person {
  std::string_view name;
  int age;
  double height;
  bool is_student;
};

// ============================================================================
// Print all member names and types
// ============================================================================

template <typename T>
void printMembers() {
  std::println("Members of {}:", meta::name_of(^^T));

  // template for: expands at compile time for each member
  template for (constexpr auto member : meta::nonstatic_data_members_of(^^T)) {
    std::println("  {} : {}",
                 meta::name_of(member),
                 meta::name_of(meta::type_of(member)));
  }
}

// ============================================================================
// Count members at compile time
// ============================================================================

template <typename T>
consteval auto memberCount() -> std::size_t {
  return meta::nonstatic_data_members_of(^^T).size();
}

// ============================================================================
// Get member by index (tuple-like access)
// ============================================================================

template <std::size_t I, typename T>
constexpr auto& getMember(T& obj) {
  constexpr auto members = meta::nonstatic_data_members_of(^^T);
  return obj.[:members[I]:];
}

template <std::size_t I, typename T>
constexpr auto const& getMember(T const& obj) {
  constexpr auto members = meta::nonstatic_data_members_of(^^T);
  return obj.[:members[I]:];
}

// ============================================================================
// Print all values of a struct instance
// ============================================================================

template <typename T>
void printValues(T const& obj) {
  std::println("Values of {} instance:", meta::name_of(^^T));

  template for (constexpr auto member : meta::nonstatic_data_members_of(^^T)) {
    std::println("  .{} = {}", meta::name_of(member), obj.[:member:]);
  }
}

// ============================================================================
// Check if struct has a specific member name
// ============================================================================

template <typename T>
consteval auto hasMember(std::string_view name) -> bool {
  template for (constexpr auto member : meta::nonstatic_data_members_of(^^T)) {
    if (meta::name_of(member) == name) {
      return true;
    }
  }
  return false;
}

// ============================================================================
// Compare two structs field-by-field
// ============================================================================

template <typename T>
constexpr auto fieldwiseEqual(T const& a, T const& b) -> bool {
  template for (constexpr auto member : meta::nonstatic_data_members_of(^^T)) {
    if (a.[:member:] != b.[:member:]) {
      return false;
    }
  }
  return true;
}

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::println("=== Struct Member Iteration ===\n");

  // Print member info
  printMembers<Person>();

  // Member count
  std::println("\nPerson has {} members", memberCount<Person>());

  // Check for specific members
  std::println("\nMember existence:");
  std::println("  has 'name': {}", hasMember<Person>("name"));
  std::println("  has 'age': {}", hasMember<Person>("age"));
  std::println("  has 'salary': {}", hasMember<Person>("salary"));

  // Print values
  Person alice{.name = "Alice", .age = 30, .height = 1.65, .is_student = false};
  std::println("");
  printValues(alice);

  // Tuple-like access
  std::println("\nTuple-like access:");
  std::println("  getMember<0>(alice) = {}", getMember<0>(alice));  // name
  std::println("  getMember<1>(alice) = {}", getMember<1>(alice));  // age

  // Modify via getMember
  getMember<1>(alice) = 31;  // Happy birthday!
  std::println("  After birthday: age = {}", alice.age);

  // Field-wise comparison
  Person alice_clone{.name = "Alice", .age = 31, .height = 1.65, .is_student = false};
  Person bob{.name = "Bob", .age = 25, .height = 1.80, .is_student = true};

  std::println("\nFieldwise equality:");
  std::println("  alice == alice_clone: {}", fieldwiseEqual(alice, alice_clone));
  std::println("  alice == bob: {}", fieldwiseEqual(alice, bob));

  return 0;
}
