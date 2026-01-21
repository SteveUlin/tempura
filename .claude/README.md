# Claude Code Configuration

This directory contains Claude Code configuration files for the tempura project.

## Slash Commands

### `/improve-dist <file>`

Systematically reviews and improves a probability distribution implementation.

**Usage:**
```
/improve-dist src/bayes/beta.h
/improve-dist src/bayes/binomial.h
```

**What it does:**
1. Code review (bugs, quality, type safety)
2. Apply fixes following project patterns
3. Create comprehensive test suite
4. Verify all tests pass
5. Check-in before committing

See `.claude/prompts/improve_bayes_distribution.md` for the full workflow.

## Hooks

### `pre-commit.sh`

Automatically runs bayes tests when committing changes to `src/bayes/` files.

**Behavior:**
- Detects if staged files are in `src/bayes/`
- Runs `ctest --test-dir build -L bayes`
- Blocks commit if tests fail
- Shows which files triggered the tests

**Configuration:**

Hooks should be automatically detected by Claude Code. If they're not running, check your Claude Code hooks settings.

## Prompts

### `improve_bayes_distribution.md`

Detailed workflow documentation for improving distribution implementations. Used by the `/improve-dist` command.

**Covers:**
- Code review checklist
- Fix patterns and best practices
- Test coverage requirements
- Extension point usage
- Style guidelines
- Before/after examples

## Agents

### `plan` (Plan Agent)

Software architect agent for designing implementation plans before writing code.

**Key principles:**
- **Context first** - Refuses to plan without understanding "why"
- **Asks questions liberally** - Uses AskUserQuestion to clarify ambiguities
- **Explores before proposing** - Deep codebase investigation
- **Presents alternatives** - Shows 2-3 approaches with tradeoffs

**Activated automatically when:**
- User requests feature implementation
- Architectural changes are needed
- Complex refactoring tasks

**Output:** Writes structured plan to `.claude/plans/<name>.md`

### `commit-and-integrate`

Two-step workflow: creates a clean commit, then integrates to main branch.

## Directory Structure

```
.claude/
├── README.md                              # This file
├── agents/
│   ├── commit-and-integrate.md            # Commit + integrate workflow
│   └── plan.md                            # Planning agent
├── commands/
│   └── improve-dist.md                    # Slash command for distribution improvement
├── hooks/
│   └── pre-commit.sh                      # Auto-run bayes tests on commit
├── prompts/
│   └── improve_bayes_distribution.md      # Detailed workflow documentation
└── settings.local.json                    # Permissions and configuration
```
