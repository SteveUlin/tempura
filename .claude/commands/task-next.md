# Work on Next Priority Task

Fetch and start working on the highest priority task from the task database:

1. **Fetch the highest priority task:**
   - Run: `./scripts/task fetch --priority`
   - Show only the first task found

2. **Show detailed task information:**
   - Run: `./scripts/task show <id>`
   - Display title, description, acceptance criteria, and notes

3. **Ask user for confirmation:**
   - "Ready to start Task #<id>? [yes/skip/remove]"
   - If user says "yes" or similar, proceed with step 4
   - If user says "skip" or "no", show the next task
   - If user says "remove", run `./scripts/task remove <id>` to delete the task permanently (no archive)

4. **Start the task:**
   - Run: `./scripts/task start <id>`
   - This marks the task as in_progress

5. **Implement the task:**
   - Follow the description and acceptance criteria closely
   - Use constexpr where possible (Tempura philosophy)
   - Follow existing patterns in the component
   - Run tests incrementally as you implement
   - Use the testing framework in `src/tempura/unit.h`

6. **During implementation:**
   - Build frequently: `ninja -C build`
   - **IMPORTANT: Run tests through ctest ONLY:** `ctest --test-dir build -R <component>`
   - **NEVER run test binaries directly** (e.g., `./build/test/foo_test`) - this requires user approval
   - Always use `ctest` or `ninja -C build <test_target>` instead

7. **Before marking complete:**
   - Verify all acceptance criteria are met
   - Run full build: `ninja -C build`
   - Run component tests: `ctest --test-dir build -R <component>`
   - **Use ctest, NOT direct binary execution**
   - Check that all tests pass
   - Review code quality and documentation
   - Ask user to review the changes

8. **On user approval:**
   - Get the commit SHA: `git log -1 --format=%H`
   - Run: `./scripts/task done <id> <commit-sha>`
   - Task will be removed from tasks.json and archived to git history

9. **Ask user:**
   - "Task #<id> complete! Work on next task? [yes/no]"
   - If yes, repeat from step 1

**Important:**
- Only mark tasks as "done" after user approval
- **Always run tests through `ctest`, NEVER run test binaries directly**
- Direct binary execution (e.g., `./build/test/foo_test`) requires user approval and slows workflow
- Follow Tempura coding conventions (see CLAUDE.md)
- Use Unicode symbols freely (α, β, ∂, etc.)
- Prefer compile-time computation (constexpr-by-default)
