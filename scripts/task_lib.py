"""
Minimal task management library for Tempura

Provides core CRUD operations on tasks.json for use by both
scripts/task CLI and AI tools.
"""

import json
from pathlib import Path
from datetime import date
from typing import Optional, Dict, Any, List

# Paths
SCRIPT_DIR = Path(__file__).parent.absolute()
ROOT_DIR = SCRIPT_DIR.parent
TASKS_FILE = ROOT_DIR / ".tasks" / "tasks.json"

# Priority ordering for sorting
PRIORITY_ORDER = {"critical": 0, "high": 1, "medium": 2, "low": 3}


def init_tasks() -> None:
    """Initialize tasks file if it doesn't exist"""
    if not TASKS_FILE.exists():
        TASKS_FILE.parent.mkdir(exist_ok=True)
        save_tasks({"version": "1.0", "tasks": []})


def load_tasks() -> Dict[str, Any]:
    """Load tasks from JSON file"""
    init_tasks()
    with open(TASKS_FILE, 'r') as f:
        return json.load(f)


def save_tasks(data: Dict[str, Any]) -> None:
    """Save tasks to JSON file"""
    with open(TASKS_FILE, 'w') as f:
        json.dump(data, f, indent=2)


def next_id() -> int:
    """Get next available task ID"""
    data = load_tasks()
    if not data["tasks"]:
        return 1
    return max(task["id"] for task in data["tasks"]) + 1


def find_task(task_id: int) -> Optional[Dict[str, Any]]:
    """Find task by ID"""
    data = load_tasks()
    for task in data["tasks"]:
        if task["id"] == task_id:
            return task
    return None


def add_task(task: Dict[str, Any]) -> int:
    """
    Add a new task to the task list.

    Args:
        task: Task dict with required fields (title, description, component, priority)
              Optional fields will be filled with defaults.

    Returns:
        The ID of the newly created task
    """
    data = load_tasks()
    task_id = next_id()

    # Build task with defaults
    new_task = {
        "id": task_id,
        "title": task["title"],
        "description": task.get("description", ""),
        "component": task["component"],
        "priority": task.get("priority", "medium"),
        "tags": task.get("tags", []),
        "status": "pending",
        "created": str(date.today()),
        "started": None,
        "notes": task.get("notes", ""),
        "acceptance": task.get("acceptance", [])
    }

    data["tasks"].append(new_task)
    save_tasks(data)

    return task_id


def update_task(task_id: int, updates: Dict[str, Any]) -> bool:
    """
    Update fields of an existing task.

    Args:
        task_id: ID of task to update
        updates: Dict of fields to update

    Returns:
        True if task was found and updated, False otherwise
    """
    data = load_tasks()

    for task in data["tasks"]:
        if task["id"] == task_id:
            task.update(updates)
            save_tasks(data)
            return True

    return False


def remove_task(task_id: int) -> Optional[Dict[str, Any]]:
    """
    Remove task from the task list.

    Args:
        task_id: ID of task to remove

    Returns:
        The removed task dict, or None if not found
    """
    data = load_tasks()

    for i, task in enumerate(data["tasks"]):
        if task["id"] == task_id:
            removed = data["tasks"].pop(i)
            save_tasks(data)
            return removed

    return None


def filter_tasks(
    component: Optional[str] = None,
    tag: Optional[str] = None,
    status: Optional[str] = None,
    priority: Optional[str] = None
) -> List[Dict[str, Any]]:
    """
    Filter tasks by criteria.

    Args:
        component: Filter by component name
        tag: Filter by tag (must be in task's tags list)
        status: Filter by status
        priority: Filter by priority

    Returns:
        List of matching tasks, sorted by priority then creation date
    """
    data = load_tasks()
    tasks = data["tasks"]

    if component:
        tasks = [t for t in tasks if t["component"] == component]

    if tag:
        tasks = [t for t in tasks if tag in t.get("tags", [])]

    if status:
        tasks = [t for t in tasks if t["status"] == status]

    if priority:
        tasks = [t for t in tasks if t["priority"] == priority]

    # Sort by priority, then creation date
    tasks.sort(key=lambda t: (PRIORITY_ORDER.get(t["priority"], 999), t["created"]))

    return tasks
