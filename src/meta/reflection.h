#pragma once

// ============================================================================
// reflection.h — C++26 P2996 reflection utilities for tempura
// ============================================================================
//
// Provides the std::meta interface for compile-time type introspection.
// Bloomberg's clang-p2996 ships <experimental/meta> — we include that directly.
//
// For GCC (once P2996 lands), replace the fallback with #include <meta>.
//
// Key primitives used by this codebase:
//   ^^T                                    - reflect on type T → std::meta::info
//   [:r:]                                  - splice info r back to code
//   template for (auto x : range)          - compile-time expansion loop
//   std::meta::template_of(^^T)            - get base template of a specialization
//   std::meta::template_arguments_of(^^T)  - get template args as info range
//   std::meta::substitute(^^T, args)       - instantiate template with new args

#if __has_include(<experimental/meta>)
#include <experimental/meta>
#else
#error "reflection.h requires <experimental/meta> — use nix develop .#reflection"
#endif

#include <vector>

namespace tempura::reflect {

// ============================================================================
// Template introspection — needed for TypeList ops and core.h type traits
// ============================================================================

// Check if T is a template specialization (e.g., TypeList<int, double>)
template <typename T>
consteval bool hasTemplateArguments() {
  return std::meta::has_template_arguments(^^T);
}

// Get the base template of a specialization.
// E.g., templateOf<TypeList<int>>() == ^^TypeList
template <typename T>
consteval auto templateOf() -> std::meta::info {
  return std::meta::template_of(^^T);
}

// Get template arguments as a vector of info values.
// E.g., templateArgsOf<TypeList<int, double>>() returns {^^int, ^^double}
template <typename T>
consteval auto templateArgsOf() -> std::vector<std::meta::info> {
  return std::meta::template_arguments_of(^^T);
}

// Instantiate a template with new arguments.
// E.g., substitute(^^TypeList, {^^int, ^^double}) → ^^TypeList<int, double>
consteval auto substitute(std::meta::info tmpl,
                          std::vector<std::meta::info> args) -> std::meta::info {
  return std::meta::substitute(tmpl, args);
}

// ============================================================================
// Type identity — use ^^T for free type equality
// ============================================================================

// Check if two types are the same using reflection (no template specialization)
template <typename T, typename U>
consteval bool isSameType() {
  return ^^T == ^^U;
}

// Check if T is a specialization of a specific template
// E.g., isSpecializationOf<TypeList<int, double>, TypeList>() → true
template <typename T, template <typename...> typename Template>
consteval bool isSpecializationOf() {
  if constexpr (!std::meta::has_template_arguments(^^T)) return false;
  else return std::meta::template_of(^^T) == ^^Template;
}

// ============================================================================
// Member introspection
// ============================================================================

// Iterate over nonstatic data members at compile time.
// Useful for struct-to-tuple, JSON serialization, etc.
template <typename T>
consteval auto dataMembers() {
  return std::meta::nonstatic_data_members_of(^^T);
}

template <typename T>
consteval auto dataMemberCount() -> std::size_t {
  return std::meta::nonstatic_data_members_of(^^T).size();
}

// ============================================================================
// Name queries
// ============================================================================

template <typename T>
consteval auto nameOf() -> std::string_view {
  return std::meta::name_of(^^T);
}

template <typename T>
consteval auto displayNameOf() -> std::string_view {
  return std::meta::display_name_of(^^T);
}

}  // namespace tempura::reflect
