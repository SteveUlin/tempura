#!/usr/bin/env bash
# Pre-commit hook: Run bayes tests when bayes files are modified

set -e  # Exit on error

# Check if any bayes files are being committed
bayes_files_changed=$(git diff --cached --name-only | grep "^src/bayes/" || true)

if [ -n "$bayes_files_changed" ]; then
  echo "Bayes files modified, running bayes test suite..."
  echo "Changed files:"
  echo "$bayes_files_changed" | sed 's/^/  - /'
  echo ""

  # Ensure build directory exists
  if [ ! -d "build" ]; then
    echo "Error: build directory not found. Run: cmake -B build -G Ninja"
    exit 1
  fi

  # Run bayes tests
  if ! ctest --test-dir build -L bayes --output-on-failure; then
    echo ""
    echo "❌ Bayes tests failed. Fix the issues before committing."
    exit 1
  fi

  echo ""
  echo "✓ All bayes tests passed"
fi

exit 0
