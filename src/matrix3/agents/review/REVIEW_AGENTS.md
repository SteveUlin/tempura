# Matrix3 Comprehensive Review System

A multi-agent system for systematic, read-only quality assessment of the matrix3 library.

## Quick Start

### Option 1: Full Automated Review

```
Execute the matrix3 comprehensive review using subagents.

Read src/matrix3/agents/review/REVIEW_ARCHITECTURE.md for system design.
Read src/matrix3/agents/review/prompts/review_director.md for workflow.

For each file in matrix3:
  Spawn parallel Tasks (subagent_type: general-purpose) for each specialist:
    - Mathematics Reviewer (prompts/mathematics_reviewer.md)
    - Code Quality Reviewer (prompts/code_quality_reviewer.md)
    - Architecture Fit Reviewer (prompts/architecture_reviewer.md)
    - Test Quality Reviewer (prompts/test_quality_reviewer.md)
    - Feature Completeness Reviewer (prompts/feature_reviewer.md)

Collect all reports, then spawn Meta-Reviewer to synthesize.

Output: FINAL_REPORT.md + TASK_LIST.md
```

### Option 2: Single File Review

```
Review src/matrix3/[filename].h using the comprehensive review system.

Spawn 5 specialist reviewers in parallel (see review/prompts/).
Combine results and provide summary.
```

### Option 3: Single Aspect Review

```
Review src/matrix3/*.h for [mathematics|code quality|architecture|tests|features] only.

Use the [specialist]_reviewer.md prompt for each file.
Compile findings into a focused report.
```

## System Overview

```
┌──────────────────────────────────────────────────────────────┐
│                     Review Director                          │
│              (Orchestrates full review)                      │
└─────────────────────────┬────────────────────────────────────┘
                          │
    ┌─────────────────────┼─────────────────────┐
    │                     │                     │
    ▼                     ▼                     ▼
┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐
│  Math  │ │  Code  │ │  Arch  │ │  Test  │ │Feature │
│Reviewer│ │Reviewer│ │Reviewer│ │Reviewer│ │Reviewer│
└────┬───┘ └────┬───┘ └────┬───┘ └────┬───┘ └────┬───┘
     │          │          │          │          │
     └──────────┴──────────┴──────────┴──────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │    Meta-Reviewer      │
              │  (Synthesizes all)    │
              └───────────┬───────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │   FINAL_REPORT.md     │
              │   TASK_LIST.md        │
              └───────────────────────┘
```

## Specialist Reviewers

| Reviewer | Focus | Prompt File |
|----------|-------|-------------|
| **Mathematics** | Algorithm correctness, numerical stability | `mathematics_reviewer.md` |
| **Code Quality** | Style, clarity, constexpr usage | `code_quality_reviewer.md` |
| **Architecture** | Design patterns, API consistency | `architecture_reviewer.md` |
| **Test Quality** | Coverage, edge cases, organization | `test_quality_reviewer.md` |
| **Feature** | Missing functionality, integration | `feature_reviewer.md` |

## Files to Review

### Core (5 files)
- `base.h`, `extents.h`, `layouts.h`, `accessors.h`, `matrix.h`

### Storage (6 files)
- `inline_coordinate_list.h`, `banded.h`, `block.h`
- `complex.h`, `permutation.h`, `permuted.h`

### Algorithms (5 files)
- `addition.h`, `multiplication.h`
- `gauss_jordan.h`, `lu_decomposition.h`, `kronecker_product.h`

### Views & Utilities (3 files)
- `transpose.h`, `submatrix.h`, `to_string.h`

**Total: 19 files** (15 with tests)

## Output Files

| File | Purpose |
|------|---------|
| `REVIEW_STATUS.md` | Progress tracking |
| `reports/*.md` | Individual specialist reports |
| `FINAL_REPORT.md` | Comprehensive quality assessment |
| `TASK_LIST.md` | Prioritized improvement tasks |

## Invocation Pattern

Each specialist is invoked as a Task subagent:

```xml
<Task>
  subagent_type: general-purpose
  prompt: |
    You are the Mathematics Reviewer for matrix3.

    [Contents of prompts/mathematics_reviewer.md]

    FILE TO REVIEW: extents.h

    [File contents here]

    Produce your review report. DO NOT modify any code.
</Task>
```

## Key Principles

1. **Read-Only**: No code modifications during review
2. **Systematic**: Every file, every aspect
3. **Specific**: Cite file:line for all issues
4. **Prioritized**: Not everything needs fixing
5. **Actionable**: Clear recommendations

## Expected Duration

- Single file (5 specialists): ~2-3 minutes
- Full library (19 files): ~30-45 minutes
- Meta-review synthesis: ~5 minutes

## Customization

### Skip Certain Reviewers
If you only care about certain aspects:
```
Review matrix3 for mathematics correctness only.
Skip code quality, architecture, test, and feature reviews.
```

### Focus on Specific Files
```
Review only the algorithm files (addition.h, multiplication.h,
gauss_jordan.h, lu_decomposition.h, kronecker_product.h).
```

### Adjust Severity Thresholds
Meta-reviewer can be instructed to:
```
Only include issues of MAJOR severity or higher.
Ignore MINOR and SUGGESTION level findings.
```

## Troubleshooting

### Review Taking Too Long
- Reduce parallelism (fewer files at once)
- Skip less critical reviewers (e.g., feature completeness)
- Focus on specific file subsets

### Conflicting Recommendations
- Meta-reviewer should flag and resolve
- When in doubt, prioritize correctness over style

### Missing Context
- Specialists may need related files for context
- Provide architecture overview when reviewing algorithms

## Related Documentation

- `REVIEW_ARCHITECTURE.md` - Detailed system design
- `prompts/*.md` - Individual agent prompts
- `../ARCHITECTURE.md` - Migration agent architecture (different system)
- `../MEMORIES.md` - Accumulated knowledge from migration
