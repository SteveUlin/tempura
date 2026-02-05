# C++26 Reflection Examples

These examples demonstrate C++26 static reflection (P2996) using Bloomberg's clang-p2996.

## Current Status (February 2026)

✅ **Reflection is available via Bloomberg's clang-p2996 fork:**

```bash
# Enter the reflection development shell
nix develop .#reflection

# Compile examples
clang++ -std=c++26 -freflection basic_reflection.cpp -o basic_reflection
```

**Note:** Bloomberg's clang-p2996 uses `^^` (double caret) as the reflection operator, not the single `^` from the P2996 paper. This is an implementation detail that may change in upstream compilers.

## Examples Overview

These examples are written to the P2996 specification and will work once GCC merges the reflection branch:

### 1. `basic_reflection.cpp`
Fundamental reflection primitives:
- `^T` - reflect operator (creates `std::meta::info`)
- `[:r:]` - splice operator (reifies reflection back to code)
- `meta::name_of` - get names of types/entities
- `meta::qualified_name_of` - get fully qualified names

### 2. `struct_iteration.cpp`
Iterate over struct members at compile time:
- `meta::nonstatic_data_members_of` - enumerate data members
- `template for` - compile-time expansion loop
- Tuple-like `getMember<I>()` access pattern
- Field-wise comparison

### 3. `enum_to_string.cpp`
Automatic enum ↔ string conversion (no macros!):
- `meta::enumerators_of` - get all enumerator values
- `enumToString(E)` - convert enum to name
- `stringToEnum<E>(name)` - parse string to enum
- Works entirely at compile time

### 4. `json_serialization.cpp`
Practical example: automatic JSON generation:
- No macros or code generators needed
- Handles nested structs recursively
- Handles vectors
- Pretty-printing with indentation

### 5. `define_aggregate.cpp`
Advanced: create new types programmatically:
- `meta::define_aggregate` - construct new struct types
- Builder pattern generated via reflection
- Filter/transform member lists

### 6. `function_reflection.cpp`
Reflect on functions and their parameters:
- `meta::parameters_of` - get parameter list
- `meta::return_type_of` - get return type
- Class member function reflection
- Automatic logging wrappers

## Compilation (once GCC P2996 is merged)

```bash
# Enter development shell with GCC trunk
nix develop .#trunk

# Compile examples
g++ -std=c++26 -freflection -o basic_reflection basic_reflection.cpp
g++ -std=c++26 -freflection -o enum_to_string enum_to_string.cpp
# etc.
```

## Key Concepts

### The Reflect Operator (`^^`)
```cpp
constexpr auto r = ^^int;        // Reflect on a type
constexpr auto r = ^^MyStruct;   // Reflect on a class
constexpr auto r = ^^myFunction; // Reflect on a function
```

**Note:** The P2996 paper specifies `^` but Bloomberg's implementation uses `^^`.

### The Splice Operator (`[::]`)
```cpp
constexpr auto r = ^^int;
[:r:] x = 42;  // Equivalent to: int x = 42;
```

### Template For (Compile-time Iteration)
```cpp
template for (constexpr auto member : meta::nonstatic_data_members_of(^^T)) {
  // This block is instantiated once per member
  std::println("{}", meta::name_of(member));
}
```

## Why Reflection Matters

C++26 reflection eliminates entire categories of boilerplate:

| Before (Macros/Codegen) | After (Reflection) |
|------------------------|-------------------|
| `NLOHMANN_DEFINE_TYPE_INTRUSIVE(...)` | Just write the struct |
| Hand-written `toString()` for enums | `enumToString(value)` works for any enum |
| Macro-based struct iteration | `template for` over members |
| External code generators | Compile-time metaprogramming |

## References

- [P2996 - Reflection for C++26](https://wg21.link/p2996)
- [P3294 - Code Injection with Token Sequences](https://wg21.link/p3294)
- [GCC P2996 Branch](https://gcc.gnu.org/git/?p=gcc.git;a=shortlog;h=refs/heads/devel/c%2B%2B/p2996)
- [Bloomberg P2996 Reference Implementation](https://github.com/bloomberg/clang-p2996)
