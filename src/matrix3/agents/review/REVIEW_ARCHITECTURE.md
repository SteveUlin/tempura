# Matrix3 Comprehensive Review System

## Overview

A multi-agent review system for systematic, file-by-file quality assessment of the matrix3 library. Unlike the migration agents (which modify code), this system is **read-only** - it produces analysis and prioritized task lists, not code changes.

## Architecture: Specialist Review Pipeline

```
                         ┌──────────────────┐
                         │  Review Director │
                         │   (Orchestrator) │
                         └────────┬─────────┘
                                  │
          ┌───────────────────────┼───────────────────────┐
          │                       │                       │
          ▼                       ▼                       ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Mathematics   │     │  Code Quality   │     │  Architecture   │
│    Reviewer     │     │    Reviewer     │     │   Fit Reviewer  │
└────────┬────────┘     └────────┬────────┘     └────────┬────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐     ┌─────────────────┐
│  Test Quality   │     │    Feature      │
│    Reviewer     │     │  Completeness   │
└────────┬────────┘     └────────┬────────┘
         │                       │
         └───────────┬───────────┘
                     ▼
          ┌─────────────────────┐
          │   Meta-Reviewer     │
          │  (Synthesis Agent)  │
          └──────────┬──────────┘
                     │
                     ▼
          ┌─────────────────────┐
          │   Final Report &    │
          │   Prioritized Tasks │
          └─────────────────────┘
```

## Agent Roles

### 1. Review Director (Orchestrator)
**Purpose**: Coordinates the review workflow, manages file iteration

**Responsibilities**:
- Enumerate all files to review (headers + tests)
- Dispatch specialist reviewers per file
- Track review progress in REVIEW_STATUS.md
- Aggregate results for meta-reviewer

**Invocation**: Main conversation or master agent

### 2. Mathematics Reviewer
**Purpose**: Validate mathematical correctness

**Focus Areas**:
- Algorithm correctness (formulas, indices, edge cases)
- Numerical stability (overflow, underflow, precision)
- Mathematical properties preserved (commutativity, associativity, etc.)
- Reference to established algorithms (cite sources)
- Complexity analysis (time/space)

**Output**: Math issues with severity and citations

### 3. Code Quality Reviewer
**Purpose**: Assess local code quality

**Focus Areas**:
- CLAUDE.md style compliance
- constexpr usage
- Error handling (asserts, preconditions)
- Code clarity and readability
- Performance (algorithmic, not micro-optimization)
- Memory safety

**Output**: Code quality issues with specific locations

### 4. Architecture Fit Reviewer
**Purpose**: Evaluate integration with matrix3 design

**Focus Areas**:
- GenericMatrix inheritance pattern
- Extents/Layout/Accessor decomposition
- Consistency with other matrix3 components
- API design coherence
- Zero-STL compliance in critical paths
- Include dependencies

**Output**: Architecture conformance issues

### 5. Test Quality Reviewer
**Purpose**: Evaluate test comprehensiveness

**Focus Areas**:
- Coverage of public API
- Edge case testing (empty, single, max)
- static_assert usage for compile-time
- Test organization and clarity
- Redundant vs missing tests
- Mathematical correctness demonstration

**Output**: Test gaps and simplification opportunities

### 6. Feature Completeness Reviewer
**Purpose**: Identify missing functionality

**Focus Areas**:
- Comparison with industry standard (BLAS, Eigen, etc.)
- Missing operations for the storage type
- Integration gaps with other matrix3 components
- Documentation needs
- Example/demo coverage

**Output**: Feature wishlist with priorities

### 7. Meta-Reviewer (Synthesis Agent)
**Purpose**: Synthesize specialist reviews into actionable output

**Responsibilities**:
- Deduplicate overlapping issues
- Prioritize by impact and effort
- Categorize (bug, enhancement, refactor, docs)
- Identify cross-cutting concerns
- Generate final report and task list

**Output**:
- Comprehensive quality report
- Prioritized improvement tasks
- Cross-file observations

## File Communication

### Input Files
- `src/matrix3/*.h` - Headers to review
- `src/matrix3/*_test.cpp` - Tests to review
- `src/matrix3/agents/MEMORIES.md` - Context from migration

### Output Files
- `review/REVIEW_STATUS.md` - Progress tracking
- `review/reports/` - Per-file specialist reports
- `review/FINAL_REPORT.md` - Comprehensive analysis
- `review/TASK_LIST.md` - Prioritized improvements

## Review Output Format

### Specialist Review Format
```markdown
## [Reviewer Type]: [filename]

**Overall Grade**: A/B/C/D/F

### Issues Found

#### [SEVERITY] Issue Title
- **Location**: file.h:123
- **Description**: What's wrong
- **Impact**: Why it matters
- **Suggestion**: How to fix (optional)

### Positive Observations
- [What's done well]

### Summary Stats
- Critical: N
- Major: N
- Minor: N
- Suggestions: N
```

### Severity Levels
- **CRITICAL**: Incorrect behavior, crashes, security
- **MAJOR**: Missing important functionality, significant style violation
- **MINOR**: Small improvements, minor style issues
- **SUGGESTION**: Nice-to-have, not required

## Workflow

### Phase 1: File-by-File Specialist Review
```
For each file in src/matrix3/:
  For each specialist (Math, Code, Arch, Test, Feature):
    Spawn Task with specialist prompt
    Collect report → review/reports/{file}_{specialist}.md
```

### Phase 2: Meta-Review & Synthesis
```
Spawn Meta-Reviewer Task with all reports
Generate:
  - review/FINAL_REPORT.md
  - review/TASK_LIST.md
```

### Phase 3: Comprehensive Quality Assessment
```
Read all reports + FINAL_REPORT
Generate overall quality narrative
Identify systemic patterns
Suggest library-wide improvements
```

## Invocation Pattern

Each specialist reviewer is invoked as a subagent:

```
<Task>
  subagent_type: general-purpose
  prompt: |
    You are the [SPECIALIST] Reviewer for matrix3.

    [Contents of prompts/review_[specialist].md]

    FILE TO REVIEW: [filename]

    [File contents]

    [Test file contents if applicable]

    Produce your review report following the format in the prompt.
    DO NOT modify any code. This is a read-only review.
</Task>
```

## Configuration

### Files to Review
The following files should be reviewed:

**Core Infrastructure**:
- base.h
- extents.h
- layouts.h
- accessors.h
- matrix.h

**Storage Types**:
- inline_coordinate_list.h
- banded.h
- block.h
- complex.h
- permutation.h
- permuted.h

**Algorithms**:
- addition.h
- multiplication.h
- gauss_jordan.h
- lu_decomposition.h
- kronecker_product.h

**Views**:
- transpose.h
- submatrix.h

**Utilities**:
- to_string.h

### Review Order
1. Core infrastructure first (dependencies flow outward)
2. Then storage types
3. Then algorithms
4. Finally utilities

## Rules

1. **No Code Changes**: This system produces analysis only
2. **Cite Locations**: Always reference file:line
3. **Be Specific**: Concrete issues, not vague concerns
4. **Prioritize**: Not everything needs fixing
5. **Consider Context**: matrix3 is a learning/reference library
