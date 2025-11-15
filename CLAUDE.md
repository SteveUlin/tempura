# Tempura - C++26 numerical methods & system design library

## Core Principles

- Correct over fast (algorithmic efficiency, not micro-optimization)
- constexpr-by-default (maximum compile-time evaluation)
- Zero STL dependencies in critical paths
- Using GCC C++26 (not all features implemented yet; clangd may have issues)

## Code Style

**Naming:**

- Types: `PascalCase`
- Functions/methods: `camelCase` (verb-first)
- Variables: `snake_case`
- Class members: `snake_case_` (trailing underscore)
- Struct members: `snake_case` (no trailing underscore)
- Constants/enums: `kPascalCase`
- Macros: `SCREAMING_SNAKE_CASE` (avoid)
- Files: `snake_case.h`

**Other:**

- Single namespace `tempura` (no `::internal` or `::detail`)
- Classes: `public` before `private`
- Unicode/emojis encouraged (α, β, ∂, ∑, ✓, ✗, ⚠️)
- Comments: minimal, timeless, design-focused
- Use PRECONDITION/POSTCONDITION/DANGER as needed
- No copyright statements

## Development Guidelines

- Do NOT use `/tmp` directory for test files or experiments
- Create test files in project-local directories (e.g., `build/`, test subdirectories)
- Keep all development artifacts within the project tree
