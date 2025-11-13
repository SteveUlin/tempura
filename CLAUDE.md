# CLAUDE.md

## Project Overview

**Tempura** is a C++26 numerical methods library and system design library
emphasizing compile-time computation through heavy template metaprogramming.

**Core Principles:**

- Correct over fast (algorithmic efficiency, not micro-optimization)
- constexpr-by-default (maximum compile-time evaluation)
- Zero STL dependencies in critical paths for pure compile-time metaprogramming
- Unicode embraced (code uses α, β, ∂ freely)
- Incremental development: **If functionality is missing, add it** - don't work
  around it
- We're working in GCC's c++26, but not all C++26 feature have been implemented
  yet. Likewise, clangd might yield inaccurate diagnostics.

## Code Style

Follow Google C++ Style Guide naming conventions:

### Naming Conventions

- **Type Names** (classes, structs, typedefs, templates): `PascalCase`
  - Examples: `MyExcitingClass`, `MyTemplate`
- **Function Names**: `PascalCase`
  - Examples: `DoSomething()`, `CalculateValue()`
  - Exception: `snake_case` allowed for getters/setters
- **Variable Names**: `snake_case`
  - Examples: `my_variable`, `loop_counter`
- **Class Member Variables**: `snake_case` with trailing underscore
  - Examples: `private_member_`, `data_`
- **Struct Member Variables**: `snake_case` without trailing underscore
  - Examples: `x`, `vertex_count`
- **Constant Names**: `kPascalCase`
  - Examples: `kDaysInAWeek`, `kMaxIterations`
- **Namespace Names**: `snake_case`
  - Examples: `namespace my_namespace`, `namespace tempura`
- **Enumerator Names**: `kPascalCase` (like constants)
  - Examples: `kRed`, `kSuccess`
- **Macro Names**: `SCREAMING_SNAKE_CASE` (avoid when possible)
  - Examples: `MY_MACRO`, `MAX_BUFFER_SIZE`
- **File Names**: lowercase with underscores or dashes (underscores preferred)
  - Examples: `my_file.h`, `task_system.cpp`

### Unicode and Emojis

Unicode characters and emojis are encouraged when they improve code readability and are allowed in comments as well.

- Examples: Use `α`, `β`, `∂`, `∑` for mathematical symbols in variable names
- Examples: Use `✓`, `✗`, `⚠️` in comments to highlight status or warnings
