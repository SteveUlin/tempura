---
description: Review and improve a bayes distribution with comprehensive tests
---

Follow the workflow in `.claude/prompts/improve_bayes_distribution.md` to improve {{args}}.

**Process:**
1. Read and review the distribution file
2. Identify critical bugs and quality issues
3. Apply fixes following the documented patterns
4. Create comprehensive test file
5. Build and verify all tests pass
6. Present summary and check-in before committing

**Key Requirements:**
- Fix critical bugs first (compilation errors, logic bugs, type safety)
- Use ADL patterns: `using std::log; log(x);`
- Use `numeric_infinity(T{})` from `bayes/numeric_traits.h`
- Add static_assert for integer rejection on continuous distributions
- Write judicious comments explaining *why*, not *what*
- Create comprehensive tests (~15+ test cases)
- Get approval before committing

Start by reading the file and performing the code review.
