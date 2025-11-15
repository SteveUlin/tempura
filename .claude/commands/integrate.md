# Integrate Agent Work to Main

Merge the current agent branch's work into main and reset the agent branch for the next task.

## Process

1. Verify current branch is an agent branch (agent1, agent2, agent3)
2. Check that working tree is clean (no uncommitted changes)
3. Verify there are commits to integrate (agent branch ahead of main)
4. Show what will be merged (git log main..HEAD)
5. **WAIT for user approval** before proceeding
6. Merge agent branch to main:
   - Change to tempura worktree (where main is checked out)
   - Run `git merge <agent-branch> --ff-only`
   - Verify merge succeeded
7. Reset agent branch to main:
   - Return to agent worktree
   - Run `git reset --hard main`
8. Verify final state:
   - Show git log confirming main and agent branch are synced
   - Show git status confirming clean state

## Safety Checks

- Fail if working tree has uncommitted changes
- Fail if not on an agent branch
- Fail if no commits to integrate
- Require user approval before merging
- Use --ff-only to ensure clean merge (no merge commits)

This completes the agent workflow: Work → Integrate → Ready for next task.
