---
description: Create a simple one-line commit without self-promotion
---

Create a git commit with the following requirements:

**Commit Message Format:**
- Single line, concise summary (50-72 characters recommended)
- NO multi-line messages
- NO footers or signatures
- NO "Generated with Claude Code" or similar promotion
- Follow conventional commit style when appropriate (e.g., "fix:", "feat:", "docs:")

**Process:**
1. Run `git status` to see current changes
2. Run `git diff` to review unstaged changes
3. Analyze changes and draft a clear, descriptive one-line commit message
4. Present to user:
   - List of modified files to be committed
   - Brief summary of changes in each file
   - Proposed commit message
5. WAIT for user approval before committing
6. If approved, stage relevant files with `git add` and commit with the approved message
7. Run `git status` after commit to verify

**Important:**
- Do NOT commit without user approval
- Do NOT add promotional footers
- Do NOT use `--no-verify` unless explicitly requested
- Do NOT commit untracked config files (.claude/settings.json) unless requested

Start by reviewing the current git status and changes.
