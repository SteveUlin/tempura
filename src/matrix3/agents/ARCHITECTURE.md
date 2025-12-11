# Matrix Migration Multi-Agent System

## Overview

This system orchestrates the migration of code from `matrix` and `matrix2` to `matrix3` using a hierarchical multi-agent architecture with file-based shared memory.

## Architecture Pattern: Hierarchical Supervisor

Based on modern agent orchestration patterns, we use a **Hierarchical Supervisor** model where:
- A Director Agent coordinates task assignment and workflow
- Specialized agents handle implementation, review, and iteration
- Shared memory files enable asynchronous communication

```
                    ┌─────────────┐
                    │  Director   │
                    │   Agent     │
                    └──────┬──────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
    ┌──────▼──────┐ ┌──────▼──────┐ ┌──────▼──────┐
    │ Implementer │ │  Reviewer   │ │  Responder  │
    │   Agent     │ │   Agent     │ │   Agent     │
    └─────────────┘ └─────────────┘ └─────────────┘
                           │
                    ┌──────▼──────┐
                    │   Memory    │
                    │  Curator    │
                    └─────────────┘
```

## Agent Roles

### 1. Director Agent
**Purpose**: Orchestrates the migration workflow, assigns tasks, monitors progress

**Responsibilities**:
- Read TASKS.md to understand pending work
- Update STATUS.md with current assignments
- Decide which component to migrate next
- Trigger Implementer Agent for each task
- Monitor completion and handle blockers

**Triggers**: Autonomous - runs continuously until checkpoint

### 2. Implementer Agent
**Purpose**: Creates commits with migrated code and tests

**Responsibilities**:
- Read assigned task from STATUS.md
- Analyze source code in matrix/matrix2
- Implement equivalent in matrix3 following existing patterns
- Write comprehensive tests using unit.h framework
- Create atomic commits with clear messages
- Update STATUS.md on completion

**Output**: jj commits with code + tests

### 3. Reviewer Agent
**Purpose**: Reviews PRs/commits for quality and correctness

**Responsibilities**:
- Review code changes for correctness
- Check test coverage and quality
- Verify adherence to CLAUDE.md style guidelines
- Check constexpr-by-default principle
- Leave structured feedback in STATUS.md

**Output**: Review comments with actionable feedback

### 4. Responder Agent
**Purpose**: Addresses review feedback

**Responsibilities**:
- Read review feedback from STATUS.md
- Implement requested changes
- Create follow-up commits
- Mark feedback as addressed

**Output**: Fix commits

### 5. Memory Curator Agent
**Purpose**: Maintains quality of shared memory

**Responsibilities**:
- Review updates to MEMORIES.md
- Consolidate duplicate information
- Remove stale/irrelevant entries
- Ensure conciseness (target: <100 lines)
- Promote patterns that prove useful

**Triggers**: After every 3-5 migration tasks

## File Communication Protocol

### Static Files (Update Infrequently)
- `ARCHITECTURE.md` - This file; system design
- `TASKS.md` - Migration task breakdown

### Mutable Files (Active Communication)
- `STATUS.md` - Current state, assignments, blocking issues
- `MEMORIES.md` - Accumulated knowledge, patterns, gotchas

### Update Rules

1. **Atomic Updates**: Each agent makes one logical update per interaction
2. **Timestamp Everything**: All status entries include ISO timestamps
3. **Clear Ownership**: Each section has a single responsible agent
4. **No Conflicts**: Agents write to designated sections only

## Workflow (Autonomous)

```
1. Director reads TASKS.md, selects next task
2. Director updates STATUS.md with assignment
3. Implementer creates commit with jj, updates STATUS.md
4. Reviewer posts feedback to STATUS.md
5. Review Loop (max 3 iterations):
   a. If APPROVE: exit loop, task complete
   b. If REQUEST_CHANGES:
      - Responder addresses feedback
      - Responder creates fix commit with jj
      - Back to Reviewer (iteration++)
   c. If iteration > 3: merge anyway, log unresolved issues
6. Director marks task complete
7. Every 5 tasks: invoke Memory Curator
8. Repeat until 5 commits made, then checkpoint
```

### Review Loop Termination

The review loop exits when:
- **APPROVE**: Reviewer approves (normal exit)
- **Max Iterations (3)**: After 3 rounds of changes, merge with issues noted
- **Trivial Changes Only**: Minor style issues don't block

Large refactors requested by Reviewer:
1. Responder implements the refactor
2. Goes back to Reviewer for re-review
3. This counts as one iteration
4. If refactor spawns new issues, those get another iteration

## Design Decisions

### Why File-Based Communication?
- **Persistence**: State survives across sessions
- **Transparency**: Humans can inspect/edit state
- **Simplicity**: No external infrastructure needed
- **Version Control**: Git tracks all changes

### Why Hierarchical vs Swarm?
- Migration has clear sequential dependencies
- Reduces coordination complexity
- Easier to debug and audit
- Matches the structured nature of code migration

### Why Separate Review/Response Agents?
- Separation of concerns
- Review can be more critical without self-bias
- Response focuses purely on addressing feedback
- Mimics human code review workflow

## References

Architecture patterns informed by:
- [MarkTechPost: Top 5 AI Agent Architectures 2025](https://www.marktechpost.com/2025/11/15/comparing-the-top-5-ai-agent-architectures-in-2025-hierarchical-swarm-meta-learning-modular-evolutionary/)
- [MongoDB: Memory Engineering for Multi-Agent Systems](https://www.mongodb.com/company/blog/technical/why-multi-agent-systems-need-memory-engineering)
- [AGENTS.md Standard](https://agents.md/)
- [Botpress: AI Agent Orchestration](https://botpress.com/blog/ai-agent-orchestration)
