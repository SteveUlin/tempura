#!/usr/bin/env bash
# Quick script to run clangd check on a file

set -euo pipefail

if [ $# -eq 0 ]; then
    echo "Usage: $0 <file.cpp>"
    echo "Example: $0 examples/string_display_debugging.cpp"
    exit 1
fi

FILE="$1"

if [ ! -f "$FILE" ]; then
    echo "Error: File '$FILE' not found"
    exit 1
fi

echo "Running clangd check on: $FILE"
echo "========================================"

# Run clangd check and capture output
clangd --check="$FILE" 2>&1 | {
    # Filter for the summary line
    grep -E "(All checks completed|error:|Error:)" || true
}

# Also show if there were any real errors (not clangd internal errors)
echo ""
echo "Checking for compilation errors..."
clangd --check="$FILE" 2>&1 | {
    if grep -q "\[dependent_non_type_arg_in_partial_spec\]"; then
        echo "Note: Template specialization warnings (clangd strictness, not real errors)"
    fi

    if grep -q "ExpandDeducedType"; then
        echo "Note: Code action failures (not compilation errors)"
    fi

    # Look for real syntax/semantic errors
    if grep -E "error: |fatal error:" > /dev/null; then
        echo "❌ Found real compilation errors!"
        return 1
    else
        echo "✅ No real compilation errors found"
        return 0
    fi
}

EXIT_CODE=$?
echo ""

if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ File passes clangd checks"
else
    echo "❌ File has issues - see details above"
fi

exit $EXIT_CODE
