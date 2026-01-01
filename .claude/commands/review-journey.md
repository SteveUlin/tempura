---
description: Generate a guided reading order for reviewing current changes
argument-hint: [revision] (default: @)
---

## Context

- Changed files: !`jj diff --stat -r ${1:-@}`
- Full diff: !`jj diff -r ${1:-@}`

## Your Task

Generate a guided code review reading order for the changes above.

### Output Format

#### Summary
[3-5 sentences describing what this change does, why, and the overall approach taken]

#### Reading Order

1. **filename** - [brief note if helpful]
2. **filename**
3. ...

#### Callouts (optional)
[Only include if there's something tricky, risky, or worth extra attention. Skip this section if the change is straightforward.]

### Guidelines

- Group related files logically (impl before its test, etc.)
- Keep it scannable - this is a roadmap, not the review
- Only add callouts for genuinely notable things
