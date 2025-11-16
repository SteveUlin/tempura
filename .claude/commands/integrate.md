# Integrate Agent Work to Main

Merge the current agent branch's work into main and reset the agent branch for the next task.

## Process

1. Verify current branch is an agent branch (agent1, agent2, agent3)
2. Check that working tree state (build artifacts are ok, source changes need stashing)
3. Verify there are commits to integrate (agent branch ahead of main)
4. Show what will be merged (git log main..HEAD with --stat)
5. Stash any uncommitted changes if present:
   - Run `git stash push -u -m "Temporary stash for integration rebase"`
6. Rebase agent branch onto main (in case branches diverged):
   - Run `git rebase main`
   - This ensures we can fast-forward merge
7. Merge agent branch to main:
   - Change to tempura worktree (where main is checked out)
   - Run `git merge <agent-branch> --ff-only`
   - Verify merge succeeded
8. Reset agent branch to main:
   - Return to agent worktree
   - Run `git reset --hard main`
9. Clean up:
   - Drop the temporary stash if created
10. Verify final state:
    - Show git log confirming main and agent branch are synced
    - Show git status confirming clean state

## Safety Checks

- Fail if not on an agent branch
- Fail if no commits to integrate
- Use --ff-only to ensure clean merge (no merge commits)
- Rebase before merge to handle diverged branches

This completes the agent workflow: Work → Integrate → Ready for next task.
