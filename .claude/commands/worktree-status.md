# Worktree Status Overview

Show the status of all git worktrees and their branches to understand the current state of parallel development.

## Process

1. Run `git worktree list` to show all worktrees and their branches
2. For each worktree, show:
   - Current branch
   - Last commit message
   - How many commits ahead/behind main
   - Working tree status (clean or dirty)
3. Display a summary table showing:
   - Worktree location
   - Branch name
   - Status relative to main
   - Clean/dirty state

## Information Displayed

**For each worktree:**
- Location on disk
- Current branch
- Latest commit
- Commits ahead of main (if any)
- Commits behind main (if any)
- Uncommitted changes (if any)

**Overall summary:**
- Which agents are ahead of main (have work to integrate)
- Which agents need syncing (behind main)
- Which agents have uncommitted work

Use this to get a quick overview of all parallel development work.
