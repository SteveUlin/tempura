# Matrix3 Migration Agents

This folder contains a multi-agent system for migrating code from `matrix`/`matrix2` to `matrix3`.

## Quick Start (Autonomous Mode with Subagents)

Run the full pipeline using Task tool subagents:

```
Execute the matrix3 migration using subagents. For each step, spawn a Task with:
- subagent_type: "general-purpose"
- Include the relevant prompt file content in the task prompt

Workflow: Director → Implementer → Reviewer → (Responder if needed) → loop
Use jj for commits. Stop after 5 commits for checkpoint.
```

## Subagent Invocation Pattern

Each agent should be invoked as a subagent using the Task tool:

```
<Task>
  subagent_type: general-purpose
  prompt: |
    You are acting as the [AGENT_NAME] for matrix3 migration.

    [Contents of prompts/[agent].md]

    Current STATUS.md state:
    [Contents of STATUS.md]

    Execute your role and report results.
</Task>
```

## File Structure

```
agents/
├── AGENTS.md           # This file - usage guide
├── ARCHITECTURE.md     # System design (update rarely)
├── TASKS.md            # Migration tasks (update on completion)
├── STATUS.md           # Current state (update frequently)
├── MEMORIES.md         # Shared knowledge (curate periodically)
└── prompts/
    ├── director.md     # Orchestrator
    ├── implementer.md  # Code writer
    ├── reviewer.md     # Code reviewer
    ├── responder.md    # Feedback handler
    └── memory_curator.md  # Memory quality agent
```

## Workflow (Autonomous with Subagents)

```
     ┌─────────────────┐
     │ Task: Director  │ ─── Selects task, updates STATUS.md
     └────────┬────────┘
              ▼
   ┌────────────────────┐
   │ Task: Implementer  │ ─── Writes code + tests, jj commit
   └────────┬───────────┘
            ▼
    ┌─────────────────┐
    │ Task: Reviewer  │ ─── Reviews, posts feedback
    └───────┬─────────┘
            │
      ┌─────▼─────┐
      │ APPROVE?  │───yes──► Task complete, increment commit count
      └─────┬─────┘
            │no (max 3 iterations)
            ▼
    ┌─────────────────┐
    │ Task: Responder │ ─── Addresses feedback, jj fix commit
    └───────┬─────────┘
            │
            └──────► Back to Reviewer (iteration++)

After iteration 3: merge anyway, log unresolved issues

Every 5 tasks: Task: Memory Curator
After 5 commits: CHECKPOINT - pause for human review
```

## Orchestrator Pattern

The main conversation acts as orchestrator, spawning subagents:

```python
# Pseudocode for orchestrator logic
commits = 0
while commits < 5:
    # 1. Spawn Director subagent
    task = director_agent.select_task()

    # 2. Spawn Implementer subagent
    implementer_agent.implement(task)
    commits += 1

    # 3. Review loop
    iterations = 0
    while iterations < 3:
        # Spawn Reviewer subagent
        result = reviewer_agent.review()
        if result == "APPROVE":
            break
        # Spawn Responder subagent
        responder_agent.address_feedback()
        iterations += 1

    # 4. Periodically curate
    if commits % 5 == 0:
        memory_curator_agent.curate()

# CHECKPOINT
```

## Communication Protocol

Agents communicate via files (read before acting, write after acting):
- **STATUS.md**: Task assignments, review feedback, blocking issues
- **MEMORIES.md**: Patterns, gotchas, useful discoveries

## Checkpoints

System runs autonomously until checkpoint triggers:

1. **5 commits made**: Pause for human review of all changes
2. **Unresolvable blocker**: Something prevents progress
3. **3 review iterations exhausted**: Merged with issues logged

At checkpoint, human reviews:
- `jj log` for commit history
- STATUS.md for any logged issues
- MEMORIES.md for accumulated knowledge
