# unit.h Design Goals

## Philosophy
Minimal, focused testing framework for tempura. Implements just enough functionality to be useable without external dependencies. Not attempting to replace GoogleTest/Catch2 for general use.

## Core Requirements

### 1. Test Definition Syntax
- **Status:** ✅ Complete
- Clean UDL syntax: `"test name"_test = [] { ... }`
- No macros, no global registration boilerplate
- constexpr-compatible where possible (for compile-time assertions)

### 2. Basic Matchers
- **Status:** ✅ Complete (needs consistency fixes)
- `expectEq`, `expectNeq`, `expectTrue`, `expectFalse`
- Comparison matchers: `expectLT`, `expectLE`, `expectGT`, `expectGE`
- Floating-point: `expectNear` (with configurable delta)
- Range matchers: `expectRangeEq`, `expectRangeNear`
- Source location tracking (automatic via `std::source_location`)

### 3. Output Control
- **Status:** 🚧 Planned
- **Default mode:** Output on error only (quiet success)
- **Verbose mode:** Show all test execution (current behavior)
- **Rationale:** Fast feedback loop for CI/development, detailed output when needed

**Implementation approach:**
```cpp
// Environment variable or command-line flag
UNIT_VERBOSE=1 ./test_binary
// or
./test_binary --verbose
```

### 4. Test Filtering
- **Status:** 🚧 Planned
- Filter tests by name pattern (glob or regex)
- **Rationale:** Run subset of tests without recompiling

**Implementation approach:**
```cpp
// Environment variable
UNIT_FILTER="bayes_*" ./test_binary
UNIT_FILTER="*distribution*" ./test_binary
```

### 5. Final Output Summary
- **Status:** 🚧 Planned
- Summary report at end of test run:
  - Total tests run
  - Passed/failed counts
  - Failed test names
  - Execution time (optional)

**Example output:**
```
========================================
Test Summary
========================================
Total:  42 tests
Passed: 40 tests
Failed:  2 tests

Failed tests:
  - bayes_normal_test: expectNear failed at bayes_normal_test.cpp:123
  - bayes_gamma_test: expectEq failed at bayes_gamma_test.cpp:456

Execution time: 0.342s
========================================
```

### 6. Log/Error Capturing
- **Status:** 🚧 Planned
- Capture stdout/stderr during test execution
- Only display captured output if test fails
- **Rationale:** Debug prints shouldn't pollute output on success, but should be visible on failure

**Implementation considerations:**
- RAII guard to redirect stdout/stderr to buffer
- Platform-specific (dup2 on Unix, _dup2 on Windows)
- May want to make this optional (some users want to see prints immediately)

## Non-Goals

### Exception Testing
- **Rationale:** Tempura doesn't use exceptions (assert/abort on errors)
- Not worth the complexity

### Test Fixtures / Setup-Teardown
- **Rationale:** Just use RAII helpers in test lambdas
- Minimal value add over manual setup

### Sub-Test Sections
- **Rationale:** Refactor into multiple tests instead
- Keeps framework simple

### Benchmarking
- **Rationale:** Separate concern, separate tool
- May add minimal timing info to summary, but not performance testing

### Death Tests
- **Rationale:** Hard to implement portably, rarely needed
- Can validate with external tools if required

## Implementation Status

### Completed
- [x] Test definition syntax
- [x] Basic matchers (eq, neq, true, false, comparisons)
- [x] Floating-point comparison (expectNear)
- [x] Range matchers
- [x] Source location tracking
- [x] Exception handling in test runner (catches unexpected exceptions)

### Must Fix (Correctness)
- [x] Remove fake `constexpr` from matchers (they modify global state)
- [x] Fix missing space in error output
- [x] Consistent error formatting (iostream vs std::print)
- [x] Consistent TestContext recording across all matchers

### Should Fix (Design)
- [ ] Unify TestContext and TestRegistry (eliminate redundant state)
- [ ] Remove or use comparison.h
- [x] Named constant for default delta with documentation
- [ ] Add [[nodiscard]] to matcher return values

### Planned Features
- [ ] Output control (quiet by default, verbose on flag)
- [ ] Test filtering by name pattern
- [ ] Final summary report
- [ ] Log/error capturing (optional, with flag to disable)
- [ ] Relative error comparison for floating-point
- [ ] Better NaN/infinity handling in expectNear

## Design Decisions

### State Management
**Current state:** Dual system (TestContext + TestRegistry) with redundancy.

**Planned refactoring:**
- Keep TestContext as single source of truth for per-test state
- TestRegistry holds summary statistics only
- TestContext.failures_ should feed into final summary report

### Error Reporting
**Decision:** Standardize on `std::print` (aligns with CLAUDE.md preferences)
- Consistent formatting
- Type-safe
- Easier to format complex output

### Return Values
**Decision:** Keep `bool` returns, add `[[nodiscard]]`
- Allows chaining: `if (expectEq(...) && expectEq(...)) { ... }`
- Forces acknowledgment of result
- Supports early return patterns

### Floating-Point Comparison
**Current:** Absolute error only with `expectNear(lhs, rhs, delta)`

**Future enhancement:**
```cpp
// Relative error support
expectNearRel(lhs, rhs, rel_tol);  // |lhs - rhs| < rel_tol * max(|lhs|, |rhs|)

// Combined absolute + relative (like numpy.isclose)
expectClose(lhs, rhs, rel_tol=1e-9, abs_tol=0.0);
```

## Configuration Model

Environment variables (checked at runtime):
- `UNIT_VERBOSE={0,1}` - Enable verbose output (default: 0)
- `UNIT_FILTER=pattern` - Run tests matching glob pattern (default: "*")
- `UNIT_CAPTURE_LOGS={0,1}` - Capture stdout/stderr (default: 1)
- `UNIT_SHOW_TIMING={0,1}` - Show execution time in summary (default: 0)

No command-line parsing (keep it simple, use env vars only).

## Code Size Budget
Target: < 500 lines for core functionality (excluding matchers)
Current: ~390 lines
Budget remaining: ~110 lines for planned features
