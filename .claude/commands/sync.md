# Sync Agent Branch with Main

Sync the current agent branch with the latest changes from main to prevent drift and conflicts.

## Process

1. Check current branch and worktree location
2. Fetch latest changes (if remote configured)
3. Check if main has advanced beyond current branch
4. If main has new commits:
   - Rebase current branch onto main
   - Report what was synced
5. If already up to date:
   - Report current status
6. Show git log to confirm sync

## Important

- Only sync if working tree is clean (no uncommitted changes)
- If rebase conflicts occur, report them and stop - user must resolve
- This is a **safe operation** - it updates your branch with main's changes

Run this before starting new work to ensure you're building on the latest code.
