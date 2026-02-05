// Reflection header shim
//
// This file provides the std::meta interface for C++26 reflection (P2996).
// Once GCC's <experimental/meta> ships, this can be replaced with:
//   #include <experimental/meta>
//
// The reflection primitives (^, [::], template for) are compiler built-ins.
// This header just declares the std::meta namespace interface.

#pragma once

// Try the standard experimental header first
#if __has_include(<experimental/meta>)
#include <experimental/meta>
#else

// Fallback: declare the std::meta interface using GCC's __reflect builtin
// This will work once GCC's P2996 implementation is complete

#include <string_view>
#include <vector>

namespace std::meta {

// The fundamental reflection type - an opaque handle to compile-time metadata
// Bloomberg's clang-p2996 uses ^^ (double caret) as the reflection operator
using info = decltype(^^void);

// ============================================================================
// Name queries
// ============================================================================

consteval auto name_of(info r) -> std::string_view {
  return __reflect(name_of, r);
}

consteval auto qualified_name_of(info r) -> std::string_view {
  return __reflect(qualified_name_of, r);
}

consteval auto display_name_of(info r) -> std::string_view {
  return __reflect(display_name_of, r);
}

// ============================================================================
// Type queries
// ============================================================================

consteval auto type_of(info r) -> info {
  return __reflect(type_of, r);
}

consteval auto return_type_of(info r) -> info {
  return __reflect(return_type_of, r);
}

// ============================================================================
// Type predicates
// ============================================================================

consteval auto is_type(info r) -> bool { return __reflect(is_type, r); }
consteval auto is_class_type(info r) -> bool { return __reflect(is_class_type, r); }
consteval auto is_enum_type(info r) -> bool { return __reflect(is_enum_type, r); }
consteval auto is_union_type(info r) -> bool { return __reflect(is_union_type, r); }
consteval auto is_function(info r) -> bool { return __reflect(is_function, r); }
consteval auto is_variable(info r) -> bool { return __reflect(is_variable, r); }
consteval auto is_namespace(info r) -> bool { return __reflect(is_namespace, r); }

// ============================================================================
// Access specifiers
// ============================================================================

consteval auto is_public(info r) -> bool { return __reflect(is_public, r); }
consteval auto is_protected(info r) -> bool { return __reflect(is_protected, r); }
consteval auto is_private(info r) -> bool { return __reflect(is_private, r); }

// ============================================================================
// Qualifiers and specifiers
// ============================================================================

consteval auto is_const(info r) -> bool { return __reflect(is_const, r); }
consteval auto is_volatile(info r) -> bool { return __reflect(is_volatile, r); }
consteval auto is_static(info r) -> bool { return __reflect(is_static, r); }
consteval auto is_virtual(info r) -> bool { return __reflect(is_virtual, r); }
consteval auto is_noexcept(info r) -> bool { return __reflect(is_noexcept, r); }

// ============================================================================
// Type transformations
// ============================================================================

consteval auto add_const(info r) -> info { return __reflect(add_const, r); }
consteval auto remove_const(info r) -> info { return __reflect(remove_const, r); }
consteval auto add_volatile(info r) -> info { return __reflect(add_volatile, r); }
consteval auto remove_volatile(info r) -> info { return __reflect(remove_volatile, r); }
consteval auto add_pointer(info r) -> info { return __reflect(add_pointer, r); }
consteval auto remove_pointer(info r) -> info { return __reflect(remove_pointer, r); }
consteval auto add_lvalue_reference(info r) -> info { return __reflect(add_lvalue_reference, r); }
consteval auto add_rvalue_reference(info r) -> info { return __reflect(add_rvalue_reference, r); }

// ============================================================================
// Member queries (return ranges)
// ============================================================================

consteval auto members_of(info r) { return __reflect(members_of, r); }
consteval auto nonstatic_data_members_of(info r) { return __reflect(nonstatic_data_members_of, r); }
consteval auto static_data_members_of(info r) { return __reflect(static_data_members_of, r); }
consteval auto bases_of(info r) { return __reflect(bases_of, r); }
consteval auto enumerators_of(info r) { return __reflect(enumerators_of, r); }
consteval auto parameters_of(info r) { return __reflect(parameters_of, r); }

// ============================================================================
// Offset and size
// ============================================================================

consteval auto offset_of(info r) -> std::size_t { return __reflect(offset_of, r); }
consteval auto size_of(info r) -> std::size_t { return __reflect(size_of, r); }
consteval auto alignment_of(info r) -> std::size_t { return __reflect(alignment_of, r); }

// ============================================================================
// Define new types (code injection)
// ============================================================================

struct data_member_options_t {
  std::string_view name = {};
  std::size_t alignment = 0;
  int width = -1;  // bit-field width, -1 means not a bit-field
};

consteval auto data_member_spec(info type, data_member_options_t opts = {}) -> info {
  return __reflect(data_member_spec, type, opts.name, opts.alignment, opts.width);
}

template <typename... Members>
consteval auto define_aggregate(info, Members...) -> info;  // Implementation-defined

// ============================================================================
// Identifier construction
// ============================================================================

consteval auto identifier(std::string_view name) -> info {
  return __reflect(identifier, name);
}

}  // namespace std::meta

#endif  // __has_include(<experimental/meta>)
