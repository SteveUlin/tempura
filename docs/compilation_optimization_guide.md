# C++ Compilation Performance: A Practical Guide

This guide explains how to diagnose and fix slow C++ compilation times, using the tempura/symbolic4 codebase as a case study.

---

## Table of Contents

1. [How C++ Compilation Works](#1-how-c-compilation-works)
2. [Measuring Compilation Time](#2-measuring-compilation-time)
3. [Diagnosing Bottlenecks](#3-diagnosing-bottlenecks)
4. [Understanding Include Dependencies](#4-understanding-include-dependencies)
5. [Optimization Strategies](#5-optimization-strategies)
6. [Case Study: symbolic4](#6-case-study-symbolic4)
7. [Quick Reference](#7-quick-reference)

---

## 1. How C++ Compilation Works

When you compile a C++ file, the compiler goes through several phases:

```
┌─────────────────────────────────────────────────────────────────┐
│                        YOUR SOURCE FILE                         │
│                         (myfile.cpp)                            │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ PHASE 1: PREPROCESSING                                          │
│                                                                 │
│ • Expands all #include directives (recursively)                 │
│ • Processes #define macros                                      │
│ • Handles #ifdef conditional compilation                        │
│                                                                 │
│ Output: One giant "translation unit" with all headers inlined   │
│ Typical cost: 5-10% of total time                               │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ PHASE 2: PARSING                                                │
│                                                                 │
│ • Lexical analysis (breaking code into tokens)                  │
│ • Syntax analysis (building Abstract Syntax Tree)               │
│ • Name resolution (finding what each identifier refers to)      │
│ • Overload resolution (picking which function to call)          │
│                                                                 │
│ Output: Complete AST with all names resolved                    │
│ Typical cost: 15-30% of total time                              │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ PHASE 3: TEMPLATE INSTANTIATION                                 │
│                                                                 │
│ • For each unique use of a template (e.g., vector<int>),        │
│   generates the actual code for that specific type              │
│ • Checks concept constraints (C++20)                            │
│ • Evaluates constexpr expressions                               │
│                                                                 │
│ Output: Fully instantiated code, no more templates              │
│ Typical cost: 10-40% of total time (highly variable!)           │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ PHASE 4: OPTIMIZATION                                           │
│                                                                 │
│ • Inlining (replacing function calls with function body)        │
│ • Constant folding (computing 2+2 at compile time)              │
│ • Dead code elimination                                         │
│ • Loop optimizations                                            │
│ • Interprocedural analysis (IPA) - analyzing across functions   │
│                                                                 │
│ Output: Optimized intermediate representation                   │
│ Typical cost: 30-50% of total time                              │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ PHASE 5: CODE GENERATION                                        │
│                                                                 │
│ • Converts IR to assembly                                       │
│ • Register allocation                                           │
│ • Instruction scheduling                                        │
│ • Writes the .o (object) file                                   │
│                                                                 │
│ Output: Object file ready for linking                           │
│ Typical cost: 10-15% of total time                              │
└─────────────────────────────────────────────────────────────────┘
```

### Why This Matters

Understanding the phases helps you diagnose problems:

| Symptom | Likely Cause | Phase |
|---------|--------------|-------|
| Slow even with `-O0` | Too many includes or templates | Parsing, Instantiation |
| Much slower with `-O2`/`-O3` | Complex code for optimizer | Optimization |
| Uses lots of RAM | Deep template nesting | Instantiation |
| Parallel builds don't help | Single large file | Any |

---

## 2. Measuring Compilation Time

### Basic Timing

The simplest measurement:

```bash
# Time a single target
time cmake --build build --target my_target

# Output:
# real    0m5.432s   ← Wall clock time (what you wait)
# user    0m5.891s   ← CPU time in user space
# sys     0m0.312s   ← CPU time in kernel
```

**Interpreting the numbers:**
- `user > real` → Parallel compilation happened
- `user ≈ real` → Single-threaded compilation
- `sys` high → Lots of file I/O (many headers)

### Forcing a Clean Rebuild

To get consistent measurements, delete the object file first:

```bash
# Find and delete the object file
rm build/path/to/CMakeFiles/target.dir/file.cpp.o

# Now time the rebuild
time cmake --build build --target my_target
```

### GCC's Built-in Profiler: `-ftime-report`

GCC can tell you exactly where time is spent:

```bash
# Compile with timing report
g++ -ftime-report myfile.cpp -o myfile 2> timing.log

# View the report
cat timing.log
```

Example output:
```
Time variable                                  wall           GGC
 phase parsing                      :   0.49 ( 20%)   164M ( 55%)
 phase lang. deferred               :   0.25 ( 10%)    47M ( 16%)
 phase opt and generate             :   1.66 ( 69%)    82M ( 28%)
 template instantiation             :   0.27 ( 11%)    66M ( 22%)
 TOTAL                              :   2.40          297M
```

**Reading the columns:**
- `wall` = seconds of wall clock time
- `GGC` = memory used (GCC's Garbage Collector)
- Percentages show proportion of total

**Key rows to watch:**
- `phase parsing` — Reading and understanding code
- `template instantiation` — Generating code for templates
- `phase opt and generate` — Optimization passes
- `TOTAL` — Overall time and memory

---

## 3. Diagnosing Bottlenecks

### Step 1: Compare Fast vs Slow Files

Find two test files — one fast, one slow — and compare:

```bash
# Time the fast one
time g++ -c fast_test.cpp -o /dev/null

# Time the slow one
time g++ -c slow_test.cpp -o /dev/null
```

If there's a big difference (e.g., 2s vs 50s), you have a localized problem to investigate.

### Step 2: Get Phase Breakdowns

```bash
# Fast file
g++ -ftime-report -c fast_test.cpp -o /dev/null 2>&1 | grep -E "(TOTAL|template|phase)"

# Slow file
g++ -ftime-report -c slow_test.cpp -o /dev/null 2>&1 | grep -E "(TOTAL|template|phase)"
```

Compare the outputs to see which phase is the bottleneck.

### Step 3: Measure Include-Only Overhead

Create a test file that just includes headers, no actual code:

```bash
# Create test file
echo '#include "heavy_header.h"' > /tmp/test.cpp
echo 'int main() {}' >> /tmp/test.cpp

# Measure parse time only (no codegen)
time g++ -fsyntax-only /tmp/test.cpp
```

The `-fsyntax-only` flag skips code generation, measuring just parsing and template instantiation.

### Step 4: Compare Include Chains

```bash
# Lightweight header
echo '#include "core.h"' > /tmp/a.cpp && echo 'int main(){}' >> /tmp/a.cpp
time g++ -fsyntax-only -I src /tmp/a.cpp

# Heavyweight header
echo '#include "model.h"' > /tmp/b.cpp && echo 'int main(){}' >> /tmp/b.cpp
time g++ -fsyntax-only -I src /tmp/b.cpp
```

If `model.h` takes 5x longer than `core.h` just to parse, you've found an include bloat problem.

---

## 4. Understanding Include Dependencies

### The Include Explosion Problem

When you write:
```cpp
#include "model.h"
```

You might think you're including one file. But `model.h` might include 10 other headers, each of which includes 10 more, and so on.

### Visualizing the Include Tree

GCC's `-H` flag shows every included file:

```bash
g++ -H myfile.cpp -c -o /dev/null 2>&1 | head -50
```

Output:
```
. model.h                          ← Direct include (depth 1)
.. core.h                          ← Included by model.h (depth 2)
... type_traits                    ← Included by core.h (depth 3)
.. mcmc/plate_transforms.h         ← Another include from model.h
... mcmc/transforms.h              ← Depth 3
... mcmc/samples.h
... mcmc/state.h
... distributions/indexed_node.h
.... distributions/wrappers.h      ← Depth 4
```

The dots indicate nesting depth. More dots = deeper in the include chain.

### Counting Lines of Code

To understand the "weight" of an include:

```bash
# Count lines in a header and its dependencies
wc -l header.h                           # Just the header
wc -l header.h dep1.h dep2.h dep3.h     # Header + known deps

# Or use the preprocessor to count everything
g++ -E myfile.cpp | wc -l                # Total lines after preprocessing
```

### The Transitive Include Problem

```
You write:              What actually gets compiled:

#include "model.h"  →   model.h (400 lines)
                        + mcmc/plate_transforms.h (1400 lines)
                        + mcmc/transforms.h (600 lines)
                        + mcmc/samples.h (350 lines)
                        + distributions/*.h (2800 lines)
                        + strategy/diff.h (500 lines)
                        + ... (many more)
                        ─────────────────────────────────
                        Total: ~10,000 lines!
```

**Every file that includes `model.h` compiles all 10,000 lines.**

---

## 5. Optimization Strategies

### Strategy A: Forward Declaration Headers

**Problem:** Code that only needs to *name* a type still includes the full definition.

**Solution:** Create a lightweight header with just declarations.

```cpp
// model_fwd.h (NEW - 20 lines)
#pragma once
namespace mylib {
    template <typename... RVs> class Model;           // Just declare
    template <typename... RVs> auto model(RVs...);    // Just declare
}

// model.h (existing - 400+ lines)
#pragma once
#include "heavy_dep1.h"
#include "heavy_dep2.h"
// ... full implementation
```

**Usage:**
```cpp
// Code that only needs the type name
#include "model_fwd.h"    // 20 lines, instant

// Code that needs the implementation
#include "model.h"        // 400+ lines
```

**When it helps:** Headers that are included widely but only need type names.

### Strategy B: Header Splitting

**Problem:** One 1,400-line header does 5 different things. Every user pays for all 5.

**Solution:** Split into focused headers.

```
BEFORE:
    plate_transforms.h (1400 lines)
        - Type definitions
        - Scalar parameter handling
        - Indexed parameter handling
        - Jacobian computation
        - HMC integration

AFTER:
    plate_transforms_types.h   (200 lines)  ← Just types
    plate_transforms_scalar.h  (300 lines)  ← Scalar logic
    plate_transforms_indexed.h (400 lines)  ← Indexed logic
    plate_transforms.h         (500 lines)  ← Aggregate (includes all above)
```

**When it helps:** Large headers (500+ lines) with distinct logical sections.

### Strategy C: Extern Template Instantiation

**Problem:** Multiple `.cpp` files use `vector<string>`. Each one instantiates the template independently.

**Solution:** Instantiate once, share across all files.

```cpp
// extern_templates.h (declare)
#pragma once
#include <vector>
#include <string>

extern template class std::vector<std::string>;  // "Don't instantiate here"

// extern_templates.cpp (define once)
#include <vector>
#include <string>

template class std::vector<std::string>;  // "Instantiate here"
```

**Usage:**
```cpp
// In every other file
#include "extern_templates.h"  // Gets the declaration
#include <vector>
std::vector<std::string> v;    // Uses pre-instantiated version
```

**When it helps:** Templates that are instantiated identically in many files.

### Strategy D: Precompiled Headers (PCH)

**Problem:** Every `.cpp` file re-parses `<vector>`, `<string>`, `<algorithm>`, etc.

**Solution:** Parse them once, save the result.

```cmake
# CMakeLists.txt
target_precompile_headers(my_target PRIVATE
    <vector>
    <string>
    <algorithm>
    "my_common_header.h"
)
```

**When it helps:** Standard library headers, widely-used project headers.

### Strategy E: Reducing Template Depth

**Problem:** Deeply nested templates like `Expression<Add, Expression<Mul, Expression<...>>>` create exponentially many types.

**Solutions:**
1. **Type erasure** — Use `std::function` or `std::any` to hide template complexity
2. **Explicit instantiation limits** — Don't nest beyond N levels
3. **Runtime dispatch** — Move some decisions to runtime

**Trade-off:** These often sacrifice performance for compile time.

---

## 6. Case Study: symbolic4

### The Problem

In the tempura codebase:

| Test | Compile Time | Ratio |
|------|--------------|-------|
| `core_test` | 2.8s | 1× |
| `model_test` | 59s | **21×** |

Both are single `.cc` files. Why the huge difference?

### Diagnosis

**Step 1: Check includes**

```bash
# core_test.cc
grep '#include' src/symbolic4/core_test.cc
```
```
#include "symbolic4/core.h"
#include "symbolic4/constants.h"
#include "symbolic4/operators.h"
#include "unit.h"
```

```bash
# model_test.cc
grep '#include' src/symbolic4/model_test.cc
```
```
#include "symbolic4/model.h"
#include "symbolic4/distributions/distributions.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/interpreter/to_string.h"
#include "unit.h"
```

`model_test` has more includes, but that's not the whole story.

**Step 2: Check transitive includes**

```bash
grep '#include' src/symbolic4/model.h
```
```
#include "symbolic4/core.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/mcmc/plate_transforms.h"    ← This is the problem
```

```bash
wc -l src/symbolic4/mcmc/plate_transforms.h
```
```
1436 src/symbolic4/mcmc/plate_transforms.h
```

**Step 3: Get phase breakdown**

```bash
g++ -ftime-report -c model_test.cc 2>&1 | grep -E "(TOTAL|template|phase)"
```
```
 phase parsing                      :   7.20 ( 27%)  1804M
 phase opt and generate             :  18.89 ( 70%)  2181M
 template instantiation             :   5.66 ( 21%)  1482M
 TOTAL                              :  27.12         4162M
```

### Analysis

| Metric | core_test | model_test | Ratio |
|--------|-----------|------------|-------|
| Header LOC | ~1,100 | ~10,000 | 9× |
| Parse time | 0.49s | 7.20s | 15× |
| Template time | 0.27s | 5.66s | 21× |
| Optimization | 1.66s | 18.89s | 11× |
| Memory | 300 MB | 4.2 GB | 14× |

**Conclusions:**
1. **Header bloat** — `model.h` pulls in 9× more code
2. **Template explosion** — Complex distribution/transform types
3. **Optimizer overload** — 4.2 GB of RAM processing inline expansions

### Experimental Results (2026-02)

We tested several optimization strategies on `model_test`:

| Strategy | Result | Notes |
|----------|--------|-------|
| **Disable LTO** | ❌ Worse (62s vs 56s) | LTO helps by deferring codegen to link time (10% faster) |
| **-O0** | ❌ Worse (72s vs 56s) | Disables LTO's deferred codegen, forces immediate work |
| **-O1** | ⚠️ Minimal (~3%) | Not worth the complexity |
| **Precompiled Headers** | ❌ Worse (61s vs 56s) | PCH overhead > parsing benefit |
| **Extern templates** | ❌ N/A | Not viable — each expression creates unique types |

**Key insight:** The "optimization" phase (71%) is dominated by **code generation**, not traditional optimizer passes. LTO actually **helps** by batching codegen at link time. Strategies that disable or work around LTO make things worse.

**LTO timing breakdown:**
| Config | Compile | Link | Total |
|--------|---------|------|-------|
| With LTO | ~52s | 4.3s | **56.6s** |
| Without LTO | ~61s | 1.1s | **62.5s** |

Counterintuitively, LTO is 10% faster overall because deferred codegen at link-time is more efficient.

**Why most strategies don't work for expression templates:**
- Each test expression creates a unique type (`Expression<AddOp, Constant<1.0>, Constant<2.0>>`)
- These types are specific to each test — nothing to share
- Forward declarations only help parsing (25%), not codegen (71%)
- PCH only helps parsing, not template instantiation

**What might actually help:**
1. **Type erasure** — trade compile time for runtime overhead (against design philosophy)
2. **Test consolidation** — fewer tests = fewer unique types (reduces coverage)
3. **ccache** — caches compile results for incremental development
4. **Parallel builds** — distribute compile time across cores (already using)
5. **Faster hardware** — more RAM, faster CPU (the honest answer)

### Original Recommendations (may not work as expected)

| Fix | Effort | Expected Improvement |
|-----|--------|---------------------|
| Create `model_fwd.h` | 1 hour | 20-30% for light users |
| Split `plate_transforms.h` | 6-8 hours | 20% |
| Extern templates for `evaluate<>` | 4-5 hours | 10-15% |
| Precompiled headers | 1 hour | 15% |

**Note:** These estimates were theoretical. Actual testing showed minimal-to-negative impact for expression template codebases.

---

## 7. Quick Reference

### Commands

```bash
# Basic timing
time cmake --build build --target TARGET

# GCC phase breakdown
g++ -ftime-report FILE.cpp 2>&1 | grep -E "(TOTAL|template|phase)"

# Parse-only timing (no codegen)
time g++ -fsyntax-only FILE.cpp

# Show include tree
g++ -H FILE.cpp 2>&1 | head -50

# Count preprocessed lines
g++ -E FILE.cpp | wc -l

# Force clean rebuild
rm build/.../CMakeFiles/TARGET.dir/FILE.o
```

### Red Flags

| Symptom | Likely Problem |
|---------|----------------|
| Parse time > 30% | Too many includes |
| Template time > 40% | Deep template nesting |
| Optimization > 60% | Too much inlined code |
| Memory > 2 GB | All of the above |
| Single file > 30s | Needs splitting |

### Rules of Thumb

1. **Headers should be < 500 lines** — Split larger ones
2. **Don't include what you don't use** — Audit includes regularly
3. **Forward declare when possible** — Full definition only when needed
4. **Watch transitive includes** — One bad header pollutes everything
5. **Measure before optimizing** — Use `-ftime-report` to find the real problem

---

## Further Reading

- [GCC Manual: Compilation Options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)
- [Include What You Use (IWYU) tool](https://include-what-you-use.org/)
- [C++ Modules (C++20)](https://en.cppreference.com/w/cpp/language/modules) — The long-term solution
- [Precompiled Headers in CMake](https://cmake.org/cmake/help/latest/command/target_precompile_headers.html)
