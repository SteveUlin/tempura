# Query Tasks

Help the user query and explore tasks in the database.

## Query Types

**List all tasks:**
```bash
./scripts/task list
```

**Priority-sorted tasks (pending only):**
```bash
./scripts/task fetch --priority
```

**Filter by component:**
```bash
./scripts/task fetch --component bayes
```

**Filter by tag:**
```bash
./scripts/task fetch --tag distribution
```

**Show specific task:**
```bash
./scripts/task show <id>
```

**Stats summary:**
Read `.tasks/tasks.json` and calculate:
- Total tasks
- Tasks by status (pending, in_progress)
- Tasks by priority
- Tasks by component
- Most common tags

Use Python with task_lib:
```python
from scripts.task_lib import load_tasks

data = load_tasks()
tasks = data['tasks']

print(f"Total: {len(tasks)}")

# Group by status
from collections import Counter
status_counts = Counter(t['status'] for t in tasks)
priority_counts = Counter(t['priority'] for t in tasks)
component_counts = Counter(t['component'] for t in tasks)

# Show formatted summary
```

## Workflow

1. Determine what user wants to see
2. Execute appropriate query using script commands
3. Present results clearly with formatting
4. Offer follow-up actions:
   - Start a task (/task-next)
   - See more details
   - Run another query

**Key Points:**
- Use script commands for queries (fetch, show, list)
- Direct JSON reading for stats calculations
- Clear formatting and actionable next steps
