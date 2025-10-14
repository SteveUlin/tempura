# Symbolic3 Cleanup and Improvement Plan

This document outlines a plan to clean up and improve the `symbolic3` library. The goal is to reduce code duplication, improve naming consistency, and make the simplification strategies more robust and easier to use.

## High-Level Goals

- **Consolidate Redundant Code:** Merge the functionality of `pattern_matching.h` and `rewriting.h` into a single, cohesive pattern matching and rewriting engine.
- **Improve Naming and Consistency:** Ensure that file names, concepts, and coding styles are consistent throughout the library.
- **Streamline Simplification Strategies:** Refine the simplification pipelines in `simplify.h` to provide a clear and powerful primary `simplify` function, while retaining flexibility for specialized use cases.
- **Enhance Documentation:** Update and improve code comments to reflect the current state of the library and provide better context for readers.
- **Clean Up Build System:** Remove commented-out and unused code from `CMakeLists.txt`.

## Actionable Items

---

### **ID:** `CLEANUP-1` ✅ **COMPLETED**

**Title:** Consolidate `pattern_matching.h` and `rewriting.h`
**Description:** The files `pattern_matching.h` and `rewriting.h` have significant overlap in functionality, including definitions for `PatternVar`, `Rewrite`, `RewriteSystem`, `match`, and `substitute`. This redundancy can lead to confusion and maintenance issues. The plan is to merge the superior features of both into a single, canonical `pattern_matching.h` and remove `rewriting.h`. `pattern_matching.h` appears to be the more modern and feature-complete of the two.
**Files to Modify:**

- `src/symbolic3/pattern_matching.h` (to become the canonical implementation)
- `src/symbolic3/rewriting.h` (to be deleted)
- `src/symbolic3/simplify.h` (to update includes and usage)
- `src/symbolic3/symbolic3.h` (to update includes)
  **Impact:** High
  **Priority:** High
  **Dependencies:** None
  **Status:** ✅ **COMPLETED** - `rewriting.h` has been removed, `pattern_matching.h` is the canonical implementation, all includes have been updated, and all 18 symbolic3 tests pass successfully.
  **Verification:** All tests in `build/test/symbolic3/` should pass after the consolidation. Specifically, `simplification_basic_test`, `simplification_advanced_test`, and `simplification_pipeline_test` are critical.

---

### **ID:** `CLEANUP-2` ✅ **COMPLETED**

**Title:** Clean Up `CMakeLists.txt`
**Description:** The `CMakeLists.txt` file in `src/symbolic3/` contains a large number of commented-out `add_executable` and `add_test` commands for old or consolidated tests. This makes the file difficult to read and maintain. The plan is to remove all commented-out test targets and reorganize the file for clarity.
**Files to Modify:**

- `src/symbolic3/CMakeLists.txt`
  **Impact:** Low
  **Priority:** High
  **Dependencies:** None
  **Status:** ✅ **COMPLETED** - Removed all commented-out code and duplicate test definitions. The file is now clean and well-organized with clear section headers. All 18 symbolic3 tests pass successfully.
  **Verification:** The project should build and all tests should run successfully after the cleanup. Run `cmake -B build -G Ninja && cmake --build build && cd build && ctest`.

---

### **ID:** `CLEANUP-3` ✅ **COMPLETED**

**Title:** Refine and Unify Simplification Pipelines
**Description:** Consolidated multiple simplification pipelines into a single canonical `simplify` function. Removed `full_simplify`, `algebraic_simplify_recursive`, `bottomup_simplify`, `topdown_simplify`, `trig_aware_simplify`, `simplify_bounded`, and related redundant functions. The `simplify` function now uses the two-stage pipeline architecture. Retained `algebraic_simplify` as a low-level building block for custom pipelines. Reduced code by 46% (~564 lines) while maintaining all functionality.
**Files Modified:**

- `src/symbolic3/simplify.h` - Consolidated pipelines and documentation
- `src/symbolic3/symbolic3.h` - Simplified exports to single `simplify` function
- `src/symbolic3/derivative.h` - Updated to use `simplify`
- `src/symbolic3/test/*.cpp` - Updated all test files
- `examples/*.cpp` - Updated all example files
  **Impact:** Medium
  **Priority:** Medium
  **Dependencies:** `CLEANUP-1`
  **Status:** ✅ **COMPLETED** - All 18 symbolic3 tests pass successfully. The API is now significantly simpler with a single canonical `simplify(expr, ctx)` interface.
  **Verification:** All simplification tests pass. The two-stage pipeline handles nested expressions, term collection, canonical forms, and transcendental functions correctly.

---

### **ID:** `CLEANUP-4`

**Title:** Integrate `dsl.h` and `smart_traversal.h` into the Main Simplification Pipeline
**Description:** The `dsl.h` and `smart_traversal.h` files introduce powerful concepts for composing strategies, but they are not fully utilized in the main `simplify` pipeline. The two-stage simplification pipeline in `simplify.h` could be re-implemented using the operators from `dsl.h` (e.g., `~>` for flow, `?>` for try). This would make the pipeline definition more declarative and readable.
**Files to Modify:**

- `src/symbolic3/simplify.h`
- `src/symbolic3/dsl.h`
  **Impact:** Medium
  **Priority:** Low
  **Dependencies:** `CLEANUP-3`
  **Verification:** The behavior of the `simplify` function should not change. All simplification tests must pass.

---

### **ID:** `CLEANUP-5`

**Title:** Improve Code Comments and Remove Outdated References
**Description:** Some code comments refer to `symbolic2`, which appears to be a previous version of the library. Other comments are overly verbose or could be clarified. The plan is to review and update comments in all major header files, ensuring they are concise, accurate, and provide valuable context. The excellent comments in `simplify.h` regarding the two-stage pipeline should be used as a model.
**Files to Modify:**

- All header files in `src/symbolic3/`.
  **Impact:** Low
  **Priority:** Low
  **Dependencies:** None
  **Verification:** This is a qualitative change, but a review of the updated comments should confirm their clarity and accuracy.

---

### **ID:** `CLEANUP-6`

**Title:** Consolidate `matching.h`
**Description:** The file `matching.h` has some overlap with the `match` functions in `pattern_matching.h` and `rewriting.h`. After `CLEANUP-1` is complete, we should review `matching.h` and merge its unique functionality into the new canonical `pattern_matching.h`, then remove `matching.h`.
**Files to Modify:**

- `src/symbolic3/matching.h` (to be deleted)
- `src/symbolic3/pattern_matching.h` (to absorb functionality)
- `src/symbolic3/derivative.h` (and any other files that include `matching.h`)
  **Impact:** Medium
  **Priority:** Medium
  **Dependencies:** `CLEANUP-1`
  **Verification:** All tests that rely on matching functionality should pass. This includes `matching_test`, `pattern_binding_test`, and the simplification tests.
