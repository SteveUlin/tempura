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
4. Stage relevant files with `git add`
5. Commit with the drafted message
6. Run `git status` after commit to verify
7. Report to user:
   - List of files committed
   - The commit message used

**Important:**
- Do NOT add promotional footers
- Do NOT use `--no-verify` unless explicitly requested
- Do NOT commit untracked config files (.claude/settings.json) unless requested
- Automatically commit without waiting for approval

Start by reviewing the current git status and changes.
