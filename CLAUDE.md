# Tempura - C++26 numerical methods & system design library

## Core Principles

- Don't sacrifice correctness for micro-optimizations
- constexpr-by-default (maximum compile-time evaluation)
- Prefer `std::` by default unless there's a reason to use a custom implementation
- Using **GCC trunk** with `-freflection` for C++26 reflection
- C++26 reflection (`^^T`, `[:r:]`, `template for`) used throughout symbolic5
- `nix develop .#trunk` for dev shell; `cmake --preset gcc-trunk` to configure

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
- Prefer `{}` over `()` for initialization (including ctor init lists)
- `static_assert` tests belong in test files, not headers
- Unicode/emojis encouraged (α, β, ∂, ∑, ✓, ✗, ⚠️)
- Document preconditions/postconditions/dangers in comments where non-obvious
- No copyright statements
- `[[nodiscard]]` on types only, never on functions - too noisy
- `noexcept` only when needed for performance (move constructors, swap) - codebase is exception-free by design, don't sprinkle it everywhere

**Comments:**

- Assume the reader is a competent C++ developer - be instructive, not condescending
- Explain *why this specific approach* - what constraint or insight drove the decision
- Keep comments concise - one line when possible
- Good: `// API requires positive input, so take abs() and negate output if needed`
- Good: `// Robin Hood reduces probe length variance at cost of more swaps`
- Good: `// Variance = p(1-p), maximized at p=0.5`
- Bad: `// Now print the output` (obvious from code)
- Bad: `// This function returns the probability` (obvious from signature)

## Testing

- Use `unit.h` framework: `"test name"_test = [] { ... };`
- Use `expect*` helpers (expectEq, expectTrue, etc.) not manual checks
- Return `TestRegistry::result()` from main
- Prefer `static_assert` for compile-time validation

**Test simplification:**

- Group related assertions in one test (e.g., test `prob` for true/false together)
- Don't test math identities (e.g., "PMF sums to 1" tests math, not code)
- Don't create custom types just to test genericity unless truly needed
- Include edge cases inline with normal cases, not as separate tests
- For statistical tests, document the tolerance derivation:
  ```cpp
  // Standard error = √(p(1-p)/N) ≈ 0.0046 for p=0.7, N=10k
  // Tolerance of 0.1 is ~20 standard errors (effectively never fails)
  expectNear(0.7, empirical_p, 0.1);
  ```

## Development Guidelines

- Do NOT use `/tmp` directory for test files or experiments
- Create test files in project-local directories (e.g., `build/`, test subdirectories)
- Keep all development artifacts within the project tree

## Build & Test

- Prefer `cmake --build <dir>` over calling `make`/`ninja` directly (generator-agnostic)
- Prefer `ctest` for running tests (handles test discovery, labels, output capture)
- Examples:
  ```bash
  cmake --build build                    # Build all targets
  cmake --build build --target foo_test  # Build specific target
  ctest --test-dir build -R foo          # Run tests matching "foo"
  ctest --test-dir build -L containers   # Run tests with label "containers"
  ```

## Command Execution

- AVOID using `$` variables in bash commands when possible
- Commands with `$` trigger user approval prompts which slow down workflow
- Use absolute paths or relative paths instead of environment variables
- Example: use `cd build-asan && cmake --build .` NOT `cd $BUILD_DIR && make`

## Error Handling

**Fail loudly, not silently:**

- Use `assert()` for invariant violations and "impossible" conditions
- Never silently return or skip operations on bad input - crash instead
- Silent failures hide bugs and make debugging harder
- Example: if resize detects overflow, assert rather than silently refusing to grow

**Safety checks and performance:**

- Bounds checks and asserts throughout code are encouraged - minor perf hit is acceptable
- Removing checks for performance requires measurement and documentation:
  ```cpp
  // PERF: bounds check removed - see benchmark results in maps/README.md
  ```
- Better to crash than return wrong results

## Code Organization

**Locality vs DRY tradeoffs:**

- **Concepts and type traits**: Deduplicate into shared headers - these are stable abstractions
- **Implementation details** (e.g., SlotState enums per container): Keep local to each file
  - Locality aids comprehension for humans and AI
  - Similar implementations can diverge independently
  - Avoids coupling between files that happen to share patterns today
- **Rule of thumb**: Share stable abstractions, duplicate volatile implementation details
