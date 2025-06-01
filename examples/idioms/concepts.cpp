#include <print>
#include <type_traits>

// Concepts
//
// Concepts let you restrict template parameters to types that satisfy certain
// requirements.
//
// Sometimes you may want to enable a function for all types that fit a
// concept. But usually this isn't restrictive enough. I want to restrict
// my custom concept to only the types that:
//   - Meet the requirements of the concept
//   - Opt into the concept
//
// Below I demonstrate how to restrict a custom concept to only the types that
// either:
//   - Derive from a tag type
//   - Override a type trait
//
// Driving from a tag type provides a nice api for classes that know they
// want to opt into the concept.
//
// Whereas overriding a type trait allows you to opt into the concept
// without modifying the class definition, which is useful for third-party
// libraries


// Define a tag type
struct TagType {};

// Define a type trait
template <typename T>
struct IsOptedIn : std::is_base_of<TagType, T> {};

// Custom concept that checks if a type derives from TagType or overrides IsOptedIn
template <typename T>
concept CustomConcept = IsOptedIn<T>::value;

// Example class that derives from TagType
struct DerivedFromTag : TagType {};

// Example class that overrides the type trait
struct OverridesTrait {};
template <>
struct IsOptedIn<OverridesTrait> : std::true_type {};

// Function that uses the custom concept
template <CustomConcept T>
void exampleFunction(const T&) {
    std::println("CustomConcept satisfied by type: {}", typeid(T).name());
}

auto main() -> int {
    DerivedFromTag derived;
    OverridesTrait overridden;

    exampleFunction(derived);      // Should work
    exampleFunction(overridden);   // Should work

    // Uncommenting the following line will cause a compilation error
    // int notOptedIn = 42;
    // exampleFunction(notOptedIn);

    return 0;
}
